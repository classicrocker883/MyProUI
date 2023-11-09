/**
 * Mesh Viewer for PRO UI
 * Author: Miguel A. Risco-Castillo (MRISCOC)
 * version: 4.2.1
 * Date: 2023/05/05
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../../inc/MarlinConfigPre.h"

#if ALL(DWIN_LCD_PROUI, HAS_MESH)

#include "../../../core/types.h"
#include "../../marlinui.h"
#include "dwin.h"
#include "dwin_popup.h"
#include "../../../feature/bedlevel/bedlevel.h"
#include "meshviewer.h"

#if USE_GRID_MESHVIEWER
  #include "bedlevel_tools.h"
#endif

bool meshredraw;                            // Redraw mesh points
uint8_t sizex, sizey;                       // Mesh XY size
uint8_t rmax;                               // Maximum radius
#define margin 25                           // XY Margins
#define rmin 5                              // Minimum radius
#define zmin -20                            // rmin at z=-0.20
#define zmax  20                            // rmax at z= 0.20
#define width DWIN_WIDTH - 2 * margin
#define r(z) ((z - zmin) * (rmax - rmin) / (zmax - zmin) + rmin)
#define px(xp) (margin + (xp) * (width) / (sizex - 1))
#define py(yp) (30 + DWIN_WIDTH - margin - (yp) * (width) / (sizey - 1))

#if ENABLED(TJC_DISPLAY)
  #define meshfont font8x16
#else
  #define meshfont font6x12
#endif

MeshViewerClass MeshViewer;

float MeshViewerClass::max, MeshViewerClass::min;

void MeshViewerClass::DrawMeshGrid(const uint8_t csizex, const uint8_t csizey) {
  sizex = csizex;
  sizey = csizey;
  rmax = _MIN(margin - 2, 0.5*(width)/(sizex - 1));
  min = 100;
  max = -100;
  DWINUI::ClearMainArea();
  DWIN_Draw_Rectangle(0, HMI_data.PopupTxt_Color, px(0), py(0), px(sizex - 1), py(sizey - 1));
  for (uint8_t x = 1; x < sizex - 1; ++x) DWIN_Draw_VLine(HMI_data.PopupBg_Color, px(x), py(sizey - 1), width);
  for (uint8_t y = 1; y < sizey - 1; ++y) DWIN_Draw_HLine(HMI_data.PopupBg_Color, px(0), py(y), width);
}

void MeshViewerClass::DrawMeshPoint(const uint8_t x, const uint8_t y, const float z) {
  if (isnan(z)) return;
  #if LCD_BACKLIGHT_TIMEOUT_MINS
    ui.refresh_backlight_timeout();
  #endif
  const uint8_t fs = DWINUI::fontWidth(meshfont);
  const int16_t v = round(z * 100);
  NOLESS(max, z); NOMORE(min, z);
  const uint16_t color = DWINUI::RainbowInt(v, zmin, zmax);
  DWINUI::Draw_FillCircle(color, px(x), py(y), r(_MAX(_MIN(v, zmax), zmin)));
  TERN_(TJC_DISPLAY, delay(100));
  const uint16_t fy = py(y) - fs;
  if (sizex < TERN(TJC_DISPLAY, 8, 9)) {
    if (v == 0) DWINUI::Draw_Float(meshfont, 1, 2, px(x) - 2 * fs, fy, 0);
    else DWINUI::Draw_Signed_Float(meshfont, 1, 2, px(x) - 3 * fs, fy, z);
  }
  else {
    char msg[9]; msg[0] = '\0';
    switch (v) {
      case -999 ... -100:  // -9.99 .. -1.00 || 1.00 .. 9.99
      case  100 ...  999: DWINUI::Draw_Signed_Float(meshfont, 1, 1, px(x) - 3 * fs, fy, z); break;
      case  -99 ...   -1: sprintf_P(msg, PSTR("-.%2i"), -v); break; // -0.99 .. -0.01 mm
      case    1 ...   99: sprintf_P(msg, PSTR( ".%2i"),  v); break; //  0.01 ..  0.99 mm
      default:
        DWIN_Draw_String(false, meshfont, DWINUI::textcolor, DWINUI::backcolor, px(x) - 4, fy, "0");
        return;
    }
    DWIN_Draw_String(false, meshfont, DWINUI::textcolor, DWINUI::backcolor, px(x) - 2 * fs, fy, msg);
  }
  SERIAL_FLUSH();
}

void MeshViewerClass::DrawMesh(const bed_mesh_t zval, const uint8_t csizex, const uint8_t csizey) {
  DrawMeshGrid(csizex, csizey);
  for (uint8_t y = 0; y < csizey; ++y) {
    hal.watchdog_refresh();
    for (uint8_t x = 0; x < csizex; ++x) DrawMeshPoint(x, y, zval[x][y]);
  }
}

void MeshViewerClass::Draw(const bool withsave/*=false*/, const bool redraw/*=true*/) {
  Title.ShowCaption(GET_TEXT_F(MSG_MESH_VIEWER));

  const bool see_mesh = TERN0(USE_GRID_MESHVIEWER, bedLevelTools.view_mesh);
  if (see_mesh) {
    #if USE_GRID_MESHVIEWER
      DWINUI::ClearMainArea();
      bedLevelTools.Draw_Bed_Mesh(-1, 1, 8, 10 + TITLE_HEIGHT);
    #endif
  }
  else {
    if (redraw) DrawMesh(bedlevel.z_values, GRID_MAX_POINTS_X, GRID_MAX_POINTS_Y);
    else DWINUI::Draw_Box(1, HMI_data.Background_Color, {89,305,99,38});
  }

  if (withsave) {
    DWIN_Draw_Box(1, HMI_data.Background_Color, 120, 300, 33, 48); // draw black box to fill previous button select_box
    DWINUI::Draw_Button(BTN_Save, 26, 305);
    DWINUI::Draw_Button(BTN_Continue, 146, 305);
    Draw_Select_Highlight(HMI_flag.select_flag, 305);
  }
  else {
    DWINUI::Draw_Button(BTN_Continue, 86, 305, true);
  }

  if (see_mesh) {
    TERN_(USE_GRID_MESHVIEWER, bedLevelTools.Set_Mesh_Viewer_Status();)
    }
  else {
    ui.set_status_and_level(MString<32>(
      F("Zmin: "), p_float_t(min, 3), F(" | "), p_float_t(max, 3), F("+"), F(" :Zmax"))
    );
  }
}

void Draw_MeshViewer() { MeshViewer.Draw(true, meshredraw); }

void onClick_MeshViewer() { if (HMI_flag.select_flag) SaveMesh(); HMI_ReturnScreen(); }

void Goto_MeshViewer(const bool redraw) {
  meshredraw = redraw;
  if (leveling_is_valid()) { Goto_Popup(Draw_MeshViewer, onClick_MeshViewer); }
  else { HMI_ReturnScreen(); }
}

#endif // DWIN_LCD_PROUI && HAS_MESH
