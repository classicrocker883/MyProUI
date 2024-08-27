/**
 * Bed Level Tools for Pro UI
 *
 * Based on the original work of: Henri-J-Norden
 * https://github.com/Jyers/Marlin/pull/126
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../../inc/MarlinConfigPre.h"

#if ALL(DWIN_LCD_PROUI, HAS_LEVELING)

#include "../../marlinui.h"
#include "../../../feature/bedlevel/bedlevel.h"
#include "../../../module/probe.h"
#include "../../../gcode/gcode.h"
#include "../../../module/planner.h"

#include "bedlevel_tools.h"

#define DEBUG_OUT ENABLED(DEBUG_LEVELING_FEATURE)
#include "../../../core/debug_out.h"

BedLevelToolsClass bedLevelTools;

bool BedLevelToolsClass::goto_mesh_value = false;
uint8_t BedLevelToolsClass::mesh_x = 0;
uint8_t BedLevelToolsClass::mesh_y = 0;
uint8_t BedLevelToolsClass::tilt_grid = 2;

bool drawing_mesh = false;

#if ENABLED(AUTO_BED_LEVELING_UBL)
#include "../../../libs/least_squares_fit.h"
#include "../../../libs/vector_3.h"

  bool BedLevelToolsClass::create_plane_from_mesh() {
    struct linear_fit_data lsf_results;
    incremental_LSF_reset(&lsf_results);
    GRID_LOOP(x, y) {
      const float z = bedlevel.z_values[x][y];
      if (!isnan(z)) {
        xy_pos_t rpos = { bedlevel.get_mesh_x(x), bedlevel.get_mesh_y(y) };
        incremental_LSF(&lsf_results, rpos, z);
      }
    }

    if (finish_incremental_LSF(&lsf_results)) {
      SERIAL_ECHOPGM("Could not complete LSF!");
      return true;
    }

    bedlevel.set_all_mesh_points_to_value(0);

    matrix_3x3 rotation = matrix_3x3::create_look_at(vector_3(lsf_results.A, lsf_results.B, 1));
    GRID_LOOP(i, j) {
      float mx = bedlevel.get_mesh_x(i), my = bedlevel.get_mesh_y(j), mz = bedlevel.z_values[i][j];

      #if DEBUG_OUT
        if (DEBUGGING(LEVELING)) {
          DEBUG_ECHOLN(F("before rotation = ["), p_float_t(mx, 7), C(','), p_float_t(my, 7), C(','), p_float_t(mz, 7), F("]   ---> "));
          DEBUG_DELAY(20);
        }
      #endif

      rotation.apply_rotation_xyz(mx, my, mz);

      #if DEBUG_OUT
        if (DEBUGGING(LEVELING)) {
          DEBUG_ECHOLN(F("after rotation = ["), p_float_t(mx, 7), C(','), p_float_t(my, 7), C(','), p_float_t(mz, 7), F("]   ---> "));
          DEBUG_DELAY(20);
        }
      #endif

      bedlevel.z_values[i][j] = mz - lsf_results.D;
    }
    return false;
  }

#endif

void BedLevelToolsClass::manual_value_update(const uint8_t mesh_x, const uint8_t mesh_y, bool reset/*=false*/) {
  float zval;
  if (reset) { zval = 0; }
  else { zval = current_position.z; }
  gcode.process_subcommands_now(TS(F("M421I"), mesh_x, F("J"), mesh_y, F("Z"), p_float_t(zval, 3)));
  planner.synchronize();
}

void BedLevelToolsClass::manual_move(const uint8_t mesh_x, const uint8_t mesh_y, bool zmove/*=false*/) {
  gcode.process_subcommands_now(F("G28O"));
  if (!zmove) {
    DWIN_Draw_Popup(ICON_BLTouch, F("Moving to Point"), F("Please wait until done."));
    gcode.process_subcommands_now(TS(F("G0F600Z"), p_float_t(Z_CLEARANCE_BETWEEN_PROBES, 3))); //gcode.process_subcommands_now(F("G0F600Z" STRINGIFY(Z_CLEARANCE_BETWEEN_PROBES)));
    gcode.process_subcommands_now(TS(F("G42F4000I"), mesh_x, F("J"), mesh_y));
  }
  planner.synchronize();
  current_position.z = goto_mesh_value ? bedlevel.z_values[mesh_x][mesh_y] : Z_CLEARANCE_BETWEEN_PROBES;
  planner.buffer_line(current_position, homing_feedrate(Z_AXIS), active_extruder);
  planner.synchronize();
  if (!zmove) HMI_ReturnScreen();
}

// Move / Probe methods
void BedLevelToolsClass::MoveToXYZ() {
  goto_mesh_value = true;
  manual_move(mesh_x, mesh_y, false);
}
void BedLevelToolsClass::MoveToXY() {
  goto_mesh_value = false;
  manual_move(mesh_x, mesh_y, false);
}
void BedLevelToolsClass::MoveToZ() {
  goto_mesh_value = true;
  manual_move(mesh_x, mesh_y, true);
}
void BedLevelToolsClass::ProbeXY() {
  gcode.process_subcommands_now(
    TS(
      F("G28O\nG0Z"), uint16_t(Z_CLEARANCE_DEPLOY_PROBE),
      F("\nG30X"), p_float_t(bedlevel.get_mesh_x(mesh_x), 2),
      F("Y"), p_float_t(bedlevel.get_mesh_y(mesh_y), 2)
    )
  );
}

void BedLevelToolsClass::mesh_reset() {
  set_bed_leveling_enabled(false);
  ZERO(bedlevel.z_values);
  TERN_(AUTO_BED_LEVELING_BILINEAR, bedlevel.refresh_bed_level();)
}

// Accessors
float BedLevelToolsClass::get_max_value() {
  float max = -(__FLT_MAX__);
  GRID_LOOP(x, y) {
    const float z = bedlevel.z_values[x][y]; if (!isnan(z)) NOLESS(max, z);
  }
  return max;
}

float BedLevelToolsClass::get_min_value() {
  float min = __FLT_MAX__;
  GRID_LOOP(x, y) {
    const float z = bedlevel.z_values[x][y]; if (!isnan(z)) NOMORE(min, z);
  }
  return min;
}

// Return 'true' if mesh is good and within LCD limits
bool BedLevelToolsClass::meshValidate() {
  TERN_(PROUI_MESH_EDIT, if ((MESH_MAX_X <= MESH_MIN_X) || (MESH_MAX_Y <= MESH_MIN_Y)) return false;)
  GRID_LOOP(x, y) {
    const float z = bedlevel.z_values[x][y];
    if (isnan(z) || !WITHIN(z, Z_OFFSET_MIN, Z_OFFSET_MAX)) return false;
  }
  return true;
}

#if USE_GRID_MESHVIEWER
  #include "meshviewer.h"
  bool BedLevelToolsClass::view_mesh = false;

  void BedLevelToolsClass::Draw_Bed_Mesh(int16_t selected/*=-1*/, uint8_t gridline_width/*=1*/, uint16_t padding_x/*=8*/, uint16_t padding_y_top/*=(40 + 53 - 7)*/) {
    drawing_mesh = true;
    const uint16_t total_width_px = DWIN_WIDTH - padding_x - padding_x,
                   cell_width_px  = total_width_px / (GRID_MAX_POINTS_X),
                   cell_height_px = total_width_px / (GRID_MAX_POINTS_Y);
    const float v_max = abs(get_max_value()), v_min = abs(get_min_value()), rmax = _MAX(v_min, v_max);

    // Clear background from previous selection and select new square
    DWIN_Draw_Rectangle(1, DWINUI::backcolor, _MAX(0, padding_x - gridline_width), _MAX(0, padding_y_top - gridline_width), padding_x + total_width_px, padding_y_top + total_width_px);
    if (selected >= 0) {
      const auto selected_y = selected / (GRID_MAX_POINTS_X);
      const auto selected_x = selected - (GRID_MAX_POINTS_X) * selected_y;
      const auto start_y_px = padding_y_top + selected_y * cell_height_px;
      const auto start_x_px = padding_x + selected_x * cell_width_px;
      DWIN_Draw_Rectangle(1, DWINUI::textcolor, _MAX(0, start_x_px - gridline_width), _MAX(0, start_y_px - gridline_width), start_x_px + cell_width_px, start_y_px + cell_height_px);
    }

    // Draw value square grid
    GRID_LOOP(x, y) {
      const auto start_x_px = padding_x + x * cell_width_px;
      const auto end_x_px   = start_x_px + cell_width_px - 1 - gridline_width;
      const auto start_y_px = padding_y_top + ((GRID_MAX_POINTS_Y) - y - 1) * cell_height_px;
      const auto end_y_px   = start_y_px + cell_height_px - 1 - gridline_width;
      const float z = bedlevel.z_values[x][y];
      const uint16_t color = isnan(z) ? Color_Grey : (   // Gray if undefined
        (z > 0 ? uint16_t(round(0x1F *  z / rmax)) << 11 // Red for positive mesh point
               : uint16_t(round(0x3F * -z / rmax)) << 5) // Green for negative mesh point
               | _MIN(0x1F, (uint8_t(abs(z) * 0.4)))     // + Blue stepping for every mm
      );                                                 // RGB565 colors: https://rgbcolorpicker.com/565

      /*
      const int16_t v = round(z * 100);
      const uint16_t color = isnan(z) ? Color_Grey : DWINUI::RainbowInt(v, -rmax, rmax);
      const uint16_t color = isnan(z) ? Color_Grey : (   // Gray if undefined
        (z > 0 ? uint16_t(round(0x1F *  z / rmax)) << 11 // Red for positive mesh point
               : uint16_t(round(0x3F * -z / rmax)) << 5) // Green for negative mesh point
               | _MIN(0x1F, (uint8_t(abs(z) * 0.4)))     // + Blue stepping for every mm
      );                                                 // RGB565 colors: https://rgbcolorpicker.com/565
      */

      DWIN_Draw_Rectangle(1, color, start_x_px, start_y_px, end_x_px, end_y_px);
      safe_delay(10);
      LCD_SERIAL.flushTX();

      // Draw value text on
      const uint8_t fs = DWINUI::fontWidth(MeshViewer.meshfont);
      const int8_t offset_y = cell_height_px / 2 - fs;
      if (isnan(z)) { // Undefined
        DWIN_Draw_String(false, MeshViewer.meshfont, DWINUI::textcolor, DWINUI::backcolor, start_x_px + cell_width_px / 2 - 5, start_y_px + offset_y, F("X"));
      }
      else {          // Has value
        MString<12> msg;
        if ((GRID_MAX_POINTS_X) >= TERN(TJC_DISPLAY, 8, 10))
          msg.setf(F("%02i"), uint16_t(z * 100) % 100);
        else
          msg.set(p_float_t(abs(z), 2));
        const int8_t offset_x = cell_width_px / 2 - (fs / 2) * msg.length() - 2;
        if ((GRID_MAX_POINTS_X) >= TERN(TJC_DISPLAY, 8, 10))
          DWIN_Draw_String(false, MeshViewer.meshfont, DWINUI::textcolor, DWINUI::backcolor, start_x_px - 2 + offset_x, start_y_px + offset_y, F("."));
        DWIN_Draw_String(false, MeshViewer.meshfont, DWINUI::textcolor, DWINUI::backcolor, start_x_px + 1 + offset_x, start_y_px + offset_y, msg);
      }
      safe_delay(10);
      LCD_SERIAL.flushTX();
    } // GRID_LOOP
  }

  void BedLevelToolsClass::Set_Mesh_Viewer_Status() {
    /// TODO: Draw gradient with values as a legend instead
    float v_max = abs(get_max_value()), v_min = abs(get_min_value()), rmax = _MAX(v_min, v_max), rmin = _MIN(v_min, v_max);
    if (rmax > 3e+10f) { rmax = 0.0000001f; }
    if (rmin > 3e+10f) { rmin = 0.0000001f; }
    ui.set_status(
      &MString<47>(
        F("Green "), p_float_t(-rmin, 3),
        F("..0.."),  p_float_t(rmax, 3),
        F("+ Red")
      )
    );
    drawing_mesh = false;
  }

#endif // USE_GRID_MESHVIEWER

#endif // DWIN_LCD_PROUI && HAS_LEVELING
