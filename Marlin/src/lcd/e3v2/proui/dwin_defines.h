/**
 * DWIN general defines and data structs for PRO UI
 * Based on the original work of: Miguel A. Risco-Castillo (MRISCOC)
 * Version: 3.13.3
 * Date: 2022/05/18
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
#pragma once

#include "../../../inc/MarlinConfigPre.h"

//#define DEBUG_DWIN 1
//#define TJC_DISPLAY         // Enable for TJC display
//#define DACAI_DISPLAY       // Enable for DACAI display
#define TITLE_CENTERED        // Center Menu Title Text

#if HAS_MESH
  #define PROUI_MESH_EDIT     // Add a menu to edit mesh inset + points
  #if ENABLED(PROUI_MESH_EDIT)
    #define Z_OFFSET_MIN -3.0 // (mm)
    #define Z_OFFSET_MAX  3.0 // (mm)
  #endif
#endif

#if ENABLED(HYBRID_THRESHOLD)
  #define HYBRID_THRESHOLD_MENU // Enable Hybrid Threshold menu
#endif
#if HAS_STEALTHCHOP
  #define STEALTHCHOP_MENU      // Enable StealthChop menu (352 bytes)
#endif

#if defined(__STM32F1__) || defined(STM32F1) || MB(CREALITY_V24S1_301, CREALITY_V24S1_301F4)
  #define DASH_REDRAW 1
#endif

#if !HAS_BED_PROBE && ENABLED(BABYSTEPPING)
  #define JUST_BABYSTEP 1
#endif

#if (ALT_COLOR_MENU == 2)
  #define Def_Background_Color  RGB( 0, 8, 6) // Dark Green/Blue
  #define Def_TitleBg_Color     RGB( 0, 23, 16) // Orient Blue
#else
  #define Def_Background_Color  Color_Bg_Black //
  #define Def_TitleBg_Color     Color_Voxelab_Red //
#endif
#define Def_Cursor_Color      Color_Cyan //
#define Def_TitleTxt_Color    Color_White
#define Def_Text_Color        Color_White
#define Def_Selected_Color    Select_Color
#define Def_SplitLine_Color   Color_White //
#define Def_Highlight_Color   Color_White
#define Def_StatusBg_Color    Color_Bg_Black //
#define Def_StatusTxt_Color   Color_Yellow
#define Def_PopupBg_Color     Color_Bg_Window
#define Def_PopupTxt_Color    Popup_Text_Color
#define Def_AlertBg_Color     Color_Bg_Red
#define Def_AlertTxt_Color    Color_Yellow
#define Def_PercentTxt_Color  Percent_Color
#define Def_Barfill_Color     BarFill_Color
#define Def_Indicator_Color   Color_Cyan //
#define Def_Coordinate_Color  Color_Brown //
#define Def_Bottom_Color      Color_Silver //

#if ALL(LED_CONTROL_MENU, HAS_COLOR_LEDS)
  #define Def_Leds_Color      LEDColorWhite()
#endif

#if CASELIGHT_USES_BRIGHTNESS
  #define Def_CaseLight_Brightness 255
#endif

#if ALL(INDIVIDUAL_AXIS_HOMING_SUBMENU, MESH_BED_LEVELING)
  #ifdef Z_AFTER_HOMING
    #define DEF_Z_AFTER_HOMING Z_AFTER_HOMING
  #else
    #define DEF_Z_AFTER_HOMING 10
  #endif
#endif

#ifndef PREHEAT_1_TEMP_HOTEND
  #define PREHEAT_1_TEMP_HOTEND 210
#endif
#ifndef PREHEAT_1_TEMP_BED
  #define PREHEAT_1_TEMP_BED 60
#endif
#ifndef PREHEAT_1_TEMP_CHAMBER
  #define PREHEAT_1_TEMP_CHAMBER 35
#endif
#define DEF_HOTENDPIDT PREHEAT_1_TEMP_HOTEND
#define DEF_BEDPIDT PREHEAT_1_TEMP_BED
#define DEF_CHAMBERPIDT PREHEAT_1_TEMP_CHAMBER
#define DEF_PIDCYCLES 5
#define EXT active_extruder

//=============================================================================
// Only for Professional Firmware UI extensions
//=============================================================================

#if PROUI_EX

  #include <stddef.h>
  #include "../../../core/types.h"

  #define HAS_TOOLBAR 1
  #if HAS_TOOLBAR
    constexpr uint8_t TBMaxOpt = 5;      // Amount of shortcuts on screen
    #if HAS_BED_PROBE
      #define DEF_TBOPT {1, 7, 6, 2, 4}  // Default shorcuts for ABL/UBL
    #else
      #define DEF_TBOPT {1, 5, 4, 2, 3}; // Default shortcuts for MM
    #endif
  #endif

  #include "proui.h"

  #undef X_BED_SIZE
  #undef Y_BED_SIZE
  #undef X_MIN_POS
  #undef Y_MIN_POS
  #undef X_MAX_POS
  #undef Y_MAX_POS
  #undef Z_MAX_POS
  #undef NOZZLE_PARK_POINT
  #define X_BED_SIZE (float)PRO_data.x_bed_size
  #define Y_BED_SIZE (float)PRO_data.y_bed_size
  #define X_MIN_POS  (float)PRO_data.x_min_pos
  #define Y_MIN_POS  (float)PRO_data.y_min_pos
  #define X_MAX_POS  (float)PRO_data.x_max_pos
  #define Y_MAX_POS  (float)PRO_data.y_max_pos
  #define Z_MAX_POS  (float)PRO_data.z_max_pos
  #define NOZZLE_PARK_POINT {(float)PRO_data.Park_point.x, (float)PRO_data.Park_point.y, (float)PRO_data.Park_point.z}
  #if HAS_MESH
    #undef  GRID_MAX_POINTS_X
    #undef  GRID_MAX_POINTS_Y
    #undef  GRID_MAX_POINTS
    #define GRID_MAX_POINTS_X PRO_data.grid_max_points
    #define GRID_MAX_POINTS_Y PRO_data.grid_max_points
    #define GRID_MAX_POINTS  (PRO_data.grid_max_points * PRO_data.grid_max_points)
  #endif
  #if HAS_BED_PROBE
    #undef  Z_PROBE_FEEDRATE_SLOW
    #define Z_PROBE_FEEDRATE_SLOW PRO_data.zprobefeedslow
  #endif
  #if HAS_EXTRUDERS
    #undef  INVERT_E0_DIR
    #define INVERT_E0_DIR PRO_data.Invert_E0
  #endif
#if ENABLED(PROUI_MESH_EDIT)
  #undef  MESH_MIN_X
  #undef  MESH_MAX_X
  #undef  MESH_MIN_Y
  #undef  MESH_MAX_Y
  #define MESH_MIN_X (float)PRO_data.mesh_min_x
  #define MESH_MAX_X (float)PRO_data.mesh_max_x
  #define MESH_MIN_Y (float)PRO_data.mesh_min_y
  #define MESH_MAX_Y (float)PRO_data.mesh_max_y
#endif

#else

  #if HAS_MESH
    #define PROUI_GRID_PNTS 1
  #endif

  #include <stddef.h>
  #include "../../../core/types.h"
  #include "proui.h"

// ProUI extra feature redefines
  #if PROUI_GRID_PNTS
    #undef  GRID_MAX_POINTS_X
    #undef  GRID_MAX_POINTS_Y
    #undef  GRID_MAX_POINTS
    #define GRID_MAX_POINTS_X HMI_data.grid_max_points
    #define GRID_MAX_POINTS_Y HMI_data.grid_max_points
    #define GRID_MAX_POINTS  (HMI_data.grid_max_points * HMI_data.grid_max_points)
  #endif
  #if HAS_BED_PROBE
    #undef  Z_PROBE_FEEDRATE_SLOW
    #define Z_PROBE_FEEDRATE_SLOW HMI_data.zprobefeedslow
  #endif
  #if HAS_EXTRUDERS
    #undef  INVERT_E0_DIR
    #define INVERT_E0_DIR HMI_data.Invert_E0
  #endif
#if ENABLED(PROUI_MESH_EDIT)
  #undef  MESH_MIN_X
  #undef  MESH_MAX_X
  #undef  MESH_MIN_Y
  #undef  MESH_MAX_Y
  #define MESH_MIN_X (float)meshSet.mesh_min_x
  #define MESH_MAX_X (float)meshSet.mesh_max_x
  #define MESH_MIN_Y (float)meshSet.mesh_min_y
  #define MESH_MAX_Y (float)meshSet.mesh_max_y
#endif

#endif // PROUI_EX
