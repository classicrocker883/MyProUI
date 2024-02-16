/**
 * DWIN Enhanced implementation for PRO UI
 * Author: Miguel A. Risco-Castillo (MRISCOC)
 * Version: 3.25.3
 * Date: 2023/05/18
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

#include "../../../inc/MarlinConfig.h"

#if ENABLED(DWIN_LCD_PROUI)

#include "dwin_popup.h"
#include "menus.h"
#include "../../utf8.h"
#include "../../marlinui.h"
#include "../../../core/macros.h"
#include "../../../core/serial.h"
#include "../../../gcode/gcode.h"
#include "../../../gcode/queue.h"
#include "../../../libs/numtostr.h"
#include "../../../module/motion.h"
#include "../../../module/planner.h"
#include "../../../module/printcounter.h"
#include "../../../module/stepper.h"
#include "../../../module/temperature.h"

#if NEED_HEX_PRINT
  #include "../../../libs/hex_print.h"
#endif

#if HAS_FILAMENT_SENSOR
  #include "../../../feature/runout.h"
#endif

#if ENABLED(EEPROM_SETTINGS)
  #include "../../../module/settings.h"
#endif

#if ENABLED(HOST_ACTION_COMMANDS)
  #include "../../../feature/host_actions.h"
#endif

#if ANY(HAS_MESH, HAS_BED_PROBE)
  #include "../../../feature/bedlevel/bedlevel.h"
  #include "bedlevel_tools.h"
#endif

#if HAS_BED_PROBE
  #include "../../../module/probe.h"
#endif

#if ENABLED(BLTOUCH)
  #include "../../../feature/bltouch.h"
#endif

#if ANY(BABYSTEP_ZPROBE_OFFSET, JUST_BABYSTEP)
  #include "../../../feature/babystep.h"
#endif

#if ENABLED(POWER_LOSS_RECOVERY)
  #include "../../../feature/powerloss.h"
#endif

#if ENABLED(PRINTCOUNTER)
  #include "printstats.h"
#endif

#if ENABLED(CASE_LIGHT_MENU)
  #include "../../../feature/caselight.h"
#endif

#if ENABLED(LED_CONTROL_MENU)
  #include "../../../feature/leds/leds.h"
#endif

#if HAS_TRINAMIC_CONFIG
  #include "../../../feature/tmc_util.h"
#endif

#if ANY(HAS_GCODE_PREVIEW, CV_LASER_MODULE)
  #include "gcode_preview.h"
#endif

#if HAS_TOOLBAR
  #include "toolbar.h"
#endif

#if HAS_ESDIAG
  #include "endstop_diag.h"
#endif

#if PROUI_TUNING_GRAPH
  #include "plot.h"
#endif

#if HAS_MESH
  #include "meshviewer.h"
#endif

#if HAS_LOCKSCREEN
  #include "lockscreen.h"
#endif

#if HAS_SOUND
  #include "../../../libs/buzzer.h"
#endif

#define DEBUG_OUT ENABLED(DEBUG_DWIN)
#include "../../../core/debug_out.h"

#define PAUSE_HEAT

// Load and Unload limits
#ifndef EXTRUDE_MAXLENGTH
  #ifdef FILAMENT_CHANGE_UNLOAD_LENGTH
    #define EXTRUDE_MAXLENGTH (FILAMENT_CHANGE_UNLOAD_LENGTH + 10)
  #else
    #define EXTRUDE_MAXLENGTH 500
  #endif
#endif

// Juntion deviation limits
#define MIN_JD_MM             0.001
#define MAX_JD_MM             TERN(LIN_ADVANCE, 0.3f, 0.5f)

#if HAS_TRINAMIC_CONFIG
  #define MIN_TMC_CURRENT 100
  #define MAX_TMC_CURRENT 3000
#endif

// Editable temperature limits
#define MIN_ETEMP   0
#define MAX_ETEMP   thermalManager.hotend_max_target(EXT)
#define MIN_BEDTEMP 0
#define MAX_BEDTEMP BED_MAX_TARGET

#define DWIN_VAR_UPDATE_INTERVAL         500
#define DWIN_UPDATE_INTERVAL             1000

#define BABY_Z_VAR TERN(HAS_BED_PROBE, probe.offset.z, HMI_data.ManualZOffset)

// Structs
HMI_value_t HMI_value;
HMI_flag_t HMI_flag{0};
HMI_data_t HMI_data;

enum SelectItem : uint8_t {
  PAGE_PRINT = 0,
  PAGE_PREPARE,
  PAGE_CONTROL,
  PAGE_ADVANCE,
  OPTITEM(HAS_TOOLBAR, PAGE_TOOLBAR)
  PAGE_COUNT,
  PRINT_SETUP = 0,
  PRINT_PAUSE_RESUME,
  PRINT_STOP,
  PRINT_COUNT
};

typedef struct {
  uint8_t now, last;
  void set(uint8_t v) { now = last = v; }
  void reset() { set(0); }
  bool changed() { bool c = (now != last); if (c) { last = now; } return c; }
  bool dec() { if (now) { now--; } return changed(); }
  bool inc(uint8_t v) { if (now < (v - 1)) { now++; } else { now = (v - 1); } return changed(); }
} select_t;
select_t select_page{0}, select_print{0};

bool hash_changed = true; // Flag to know if message status was changed
bool blink = false;
uint8_t checkkey = 255, last_checkkey = MainMenu;

char DateTime[16+1] =
{
  // YY year
  __DATE__[7], __DATE__[8],__DATE__[9], __DATE__[10],
  // First month letter, Oct Nov Dec = '1' otherwise '0'
  (__DATE__[0] == 'O' || __DATE__[0] == 'N' || __DATE__[0] == 'D') ? '1' : '0',
  // Second month letter
  (__DATE__[0] == 'J') ? ( (__DATE__[1] == 'a') ? '1' :       // Jan, Jun or Jul
                          ((__DATE__[2] == 'n') ? '6' : '7') ) :
  (__DATE__[0] == 'F') ? '2' :                                // Feb
  (__DATE__[0] == 'M') ? (__DATE__[2] == 'r') ? '3' : '5' :   // Mar or May
  (__DATE__[0] == 'A') ? (__DATE__[1] == 'p') ? '4' : '8' :   // Apr or Aug
  (__DATE__[0] == 'S') ? '9' :                                // Sep
  (__DATE__[0] == 'O') ? '0' :                                // Oct
  (__DATE__[0] == 'N') ? '1' :                                // Nov
  (__DATE__[0] == 'D') ? '2' :                                // Dec
  0,
  // First day letter, replace space with digit
  __DATE__[4]==' ' ? '0' : __DATE__[4],
  // Second day letter
  __DATE__[5],
  // Separator
  ' ','-',' ',
  // Time
  __TIME__[0],__TIME__[1],__TIME__[2],__TIME__[3],__TIME__[4],
  '\0'
};

// New menu system pointers
MenuClass *FileMenu = nullptr;
MenuClass *PrepareMenu = nullptr;
MenuClass *TrammingMenu = nullptr;
MenuClass *MoveMenu = nullptr;
MenuClass *ControlMenu = nullptr;
MenuClass *AdvancedMenu = nullptr;
MenuClass *AdvancedSettings = nullptr;
#if HAS_HOME_OFFSET
  MenuClass *HomeOffMenu = nullptr;
#endif
#if HAS_BED_PROBE
  MenuClass *ProbeSetMenu = nullptr;
#endif
MenuClass *FilSetMenu = nullptr;
MenuClass *SelectColorMenu = nullptr;
MenuClass *GetColorMenu = nullptr;
MenuClass *TuneMenu = nullptr;
MenuClass *MotionMenu = nullptr;
MenuClass *FilamentMenu = nullptr;
#if ENABLED(MESH_BED_LEVELING)
  MenuClass *ManualMesh = nullptr;
#endif
#if HAS_PREHEAT
  MenuClass *PreheatMenu = nullptr;
#endif
MenuClass *TemperatureMenu = nullptr;
MenuClass *MaxSpeedMenu = nullptr;
MenuClass *MaxAccelMenu = nullptr;
#if ENABLED(CLASSIC_JERK)
  MenuClass *MaxJerkMenu = nullptr;
#endif
MenuClass *StepsMenu = nullptr;
MenuClass *PIDMenu = nullptr;
#if ANY(MPC_EDIT_MENU, MPC_AUTOTUNE_MENU)
  MenuClass *HotendMPCMenu = nullptr;
#endif
#if ENABLED(PIDTEMP) && ANY(PID_EDIT_MENU, PID_AUTOTUNE_MENU)
  MenuClass *HotendPIDMenu = nullptr;
#endif
#if ENABLED(PIDTEMPBED) && ANY(PID_EDIT_MENU, PID_AUTOTUNE_MENU)
  MenuClass *BedPIDMenu = nullptr;
#endif
#if CASELIGHT_USES_BRIGHTNESS
  MenuClass *CaseLightMenu = nullptr;
#endif
#if ENABLED(LED_CONTROL_MENU)
  MenuClass *LedControlMenu = nullptr;
#endif
#if HAS_ZOFFSET_ITEM
  MenuClass *ZOffsetWizMenu = nullptr;
#endif
#if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
  MenuClass *HomingMenu = nullptr;
#endif
#if ENABLED(FWRETRACT)
  MenuClass *FWRetractMenu = nullptr;
#endif
#if PROUI_EX
  #if ENABLED(NOZZLE_PARK_FEATURE)
     MenuClass *ParkPosMenu = nullptr;
  #endif
  MenuClass *PhySetMenu = nullptr;
#endif
#if HAS_TOOLBAR
  MenuClass *TBSetupMenu = nullptr;
#endif
#if HAS_MESH
  MenuClass *MeshMenu = nullptr;
  #if ENABLED(PROUI_MESH_EDIT)
    MenuClass *EditMeshMenu = nullptr;
  #endif
  MenuClass *MeshInsetMenu = nullptr;
#endif
#if ENABLED(SHAPING_MENU)
  MenuClass *InputShapingMenu = nullptr;
#endif
#if HAS_TRINAMIC_CONFIG
  MenuClass *TrinamicConfigMenu = nullptr;
#endif
#if ENABLED(CV_LASER_MODULE)
  MenuClass *LaserSettings = nullptr;
  MenuClass *LaserPrintMenu = nullptr;
#endif

// Updatable menuitems pointers
MenuItemClass *HotendTargetItem = nullptr;
MenuItemClass *BedTargetItem = nullptr;
MenuItemClass *FanSpeedItem = nullptr;
TERN_(MESH_BED_LEVELING, MenuItemClass *MMeshMoveZItem = nullptr;)
TERN_(PROUI_MESH_EDIT, MenuItemClass *EditZValueItem = nullptr;)

//-----------------------------------------------------------------------------
// Main Buttons
//-----------------------------------------------------------------------------

void ICON_Button(const bool selected, const int iconid, const frame_rect_t &ico, FSTR_P caption) {
  DWINUI::Draw_IconWB(iconid + selected, ico.x, ico.y);
  if (selected) {
    DWINUI::Draw_Box(0, HMI_data.Cursor_Color, ico);
    DWINUI::Draw_Box(0, HMI_data.Cursor_Color, DWINUI::ReduceFrame(ico, 1));
  }
  const uint16_t x = ico.x + (ico.w - strlen_P(FTOP(caption)) * DWINUI::fontWidth()) / 2,
                 y = (ico.y + ico.h - 20) - DWINUI::fontHeight() / 2;
  DWINUI::Draw_String(x, y, caption);
}

//
// Main Menu: "Print"
//
void ICON_Print() {
  constexpr frame_rect_t ico = { 17, 110 - TERN0(HAS_TOOLBAR, TBYOFFSET), 110, 100};
  ICON_Button(select_page.now == PAGE_PRINT, ICON_Print_0, ico, GET_TEXT_F(MSG_BUTTON_PRINT));
}

//
// Main Menu: "Prepare"
//
void ICON_Prepare() {
  constexpr frame_rect_t ico = { 145, 110 - TERN0(HAS_TOOLBAR, TBYOFFSET), 110, 100};
  ICON_Button(select_page.now == PAGE_PREPARE, ICON_Prepare_0, ico, GET_TEXT_F(MSG_PREPARE));
}

//
// Main Menu: "Control"
//
void ICON_Control() {
  constexpr frame_rect_t ico = { 17, 226 - TERN0(HAS_TOOLBAR, TBYOFFSET), 110, 100};
  ICON_Button(select_page.now == PAGE_CONTROL, ICON_Control_0, ico, GET_TEXT_F(MSG_CONTROL));
}

//
// Main Menu: "Level" || "Advanced Settings" if no leveling
//
//
void ICON_AdvSettings() {
  constexpr frame_rect_t ico = { 145, 226 - TERN0(HAS_TOOLBAR, TBYOFFSET), 110, 100};
  #if ANY(AUTO_BED_LEVELING_BILINEAR, AUTO_BED_LEVELING_UBL, MESH_BED_LEVELING)
  ICON_Button(select_page.now == PAGE_ADVANCE, ICON_Leveling_0, ico, GET_TEXT_F(MSG_BUTTON_LEVEL));
  #else
  ICON_Button(select_page.now == PAGE_ADVANCE, ICON_Info_0, ico, GET_TEXT_F(MSG_BUTTON_ADVANCED));
  #endif
}

//
// Printing: "Tune"
//
void ICON_Tune() {
  constexpr frame_rect_t ico = { 8, 232, 80, 100 };
  ICON_Button(select_print.now == PRINT_SETUP, ICON_Setup_0, ico, GET_TEXT_F(MSG_TUNE));
}

//
// Printing: "Pause"
//
void ICON_Pause() {
  constexpr frame_rect_t ico = { 96, 232, 80, 100 };
  ICON_Button(select_print.now == PRINT_PAUSE_RESUME, ICON_Pause_0, ico, GET_TEXT_F(MSG_BUTTON_PAUSE));
}

//
// Printing: "Resume"
//
void ICON_Resume() {
  constexpr frame_rect_t ico = { 96, 232, 80, 100 };
  ICON_Button(select_print.now == PRINT_PAUSE_RESUME, ICON_Continue_0, ico, GET_TEXT_F(MSG_BUTTON_RESUME));
}

//
// Printing: "Stop"
//
void ICON_Stop() {
  constexpr frame_rect_t ico = { 184, 232, 80, 100 };
  ICON_Button(select_print.now == PRINT_STOP, ICON_Stop_0, ico, GET_TEXT_F(MSG_BUTTON_STOP));
}

//
//PopUps
//
void Popup_window_PauseOrStop() {
  switch (select_print.now) {
    case PRINT_PAUSE_RESUME:
      DWIN_Popup_ConfirmCancel(ICON_Pause_1, GET_TEXT_F(MSG_PAUSE_PRINT));
      break;
    case PRINT_STOP:
      DWIN_Popup_ConfirmCancel(ICON_Stop_1, GET_TEXT_F(MSG_STOP_PRINT));
      break;
    default: break;
  }
}

#if HAS_HOTEND || HAS_HEATED_BED
  void DWIN_Popup_Temperature(const int_fast8_t heater_id, const bool toohigh) {
    FSTR_P const heaterstr = (heater_id == H_BED) ? GET_TEXT_F(MSG_BED_TEMPERATURE) : GET_TEXT_F(MSG_NOZZLE_TEMPERATURE),
                 lowhighstr = toohigh ? GET_TEXT_F(MSG_TOO_HIGH) : GET_TEXT_F(MSG_TOO_LOW);
    HMI_SaveProcessID(WaitResponse);
    DWIN_Show_Popup((toohigh ? ICON_TempTooHigh : ICON_TempTooLow), heaterstr, lowhighstr, BTN_Continue);
  }
#endif

//
// Draw status line
//
void DWIN_DrawStatusLine(const char *text) {
  DWIN_Draw_Rectangle(1, HMI_data.StatusBg_Color, 0, STATUS_Y, DWIN_WIDTH, STATUS_Y + 20);
  if (text) { DWINUI::Draw_CenteredString(HMI_data.StatusTxt_Color, STATUS_Y + 2, text); }
}

// Clear & reset status line
void DWIN_ResetStatusLine() {
  ui.status_message.clear();
  DWIN_CheckStatusMessage();
}

// Djb2 hash algorithm
uint32_t GetHash(char * str) {
  uint32_t hash = 5381;
  for (char c; (c = *str++);) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  return hash;
}

// Check for a change in the status message
void DWIN_CheckStatusMessage() {
  static MString<>::hash_t old_hash = 0x0000;
  const MString<>::hash_t hash = ui.status_message.hash();
  hash_changed = hash != old_hash;
  old_hash = hash;
}

void DWIN_DrawStatusMessage() {
  #if ENABLED(STATUS_MESSAGE_SCROLLING)

    // Get the UTF8 character count of the string
    uint8_t slen = ui.status_message.glyphs();

    // If the string fits the status line do not scroll it
    if (slen <= LCD_WIDTH) {
      if (hash_changed) {
        DWIN_DrawStatusLine(ui.status_message);
        hash_changed = false;
      }
    }
    else {
      // String is larger than the available line space

      // Get a pointer to the next valid UTF8 character
      // and the string remaining length
      uint8_t rlen;
      const char *stat = ui.status_and_len(rlen);
      DWIN_Draw_Rectangle(1, HMI_data.StatusBg_Color, 0, STATUS_Y, DWIN_WIDTH, STATUS_Y + 20);
      DWINUI::MoveTo(0, STATUS_Y + 2);
      DWINUI::Draw_String(HMI_data.StatusTxt_Color, stat, LCD_WIDTH);

      // If the string doesn't completely fill the line...
      if (rlen < LCD_WIDTH) {
        DWINUI::Draw_Char(HMI_data.StatusTxt_Color, '.');  // Always at 1+ spaces left, draw a dot
        uint8_t chars = LCD_WIDTH - rlen;                  // Amount of space left in characters
        if (--chars) {                                     // Draw a second dot if there's space
          DWINUI::Draw_Char(HMI_data.StatusTxt_Color, '.');
          if (--chars) {                                   // Print a second copy of the message
            DWINUI::Draw_String(HMI_data.StatusTxt_Color, ui.status_message, chars);
          }
        }
      }
      ui.advance_status_scroll();
    }

  #else

    if (hash_changed) {
      ui.status_message.trunc(LCD_WIDTH);
      DWIN_DrawStatusLine(ui.status_message);
      hash_changed = false;
    }

  #endif
}

void Draw_Print_Labels() {
  DWINUI::Draw_String( 46, 173, GET_TEXT_F(MSG_INFO_PRINT_TIME));
  DWINUI::Draw_String(181, 173, GET_TEXT_F(MSG_REMAINING_TIME));
  TERN_(SHOW_INTERACTION_TIME, DWINUI::Draw_String(100, 215, F("Until Filament Change"));)
}

static uint8_t _percent_done = 100;
void Draw_Print_ProgressBar() {
  DWINUI::Draw_IconWB(ICON_Bar, 15, 93);
  DWIN_Draw_Rectangle(1, HMI_data.Barfill_Color, 15 + (_percent_done * 242) / 100, 93, 257, 113);
  DWINUI::Draw_String(HMI_data.PercentTxt_Color, HMI_data.Background_Color, 117, 133, pcttostrpctrj(_percent_done));
}

duration_t _printtime = print_job_timer.duration();
void Draw_Print_ProgressElapsed() {
  char buf[10];
  const bool has_days = (_printtime.value > 60*60*24L);
  _printtime.toDigital(buf, has_days);
  DWINUI::Draw_String(HMI_data.Text_Color, HMI_data.Background_Color, 45, 192, buf);
}

#if ENABLED(SHOW_REMAINING_TIME)
  duration_t _remain_time = 0;
  void Draw_Print_ProgressRemain() {
    char buf[10];
    const bool has_days = (_remain_time.value > 60*60*24L);
    _remain_time.toDigital(buf, has_days);
    DWINUI::Draw_String(HMI_data.Text_Color, HMI_data.Background_Color, 181, 192, buf);
  }
#endif

/// Not ready
#if ENABLED(SHOW_INTERACTION_TIME)
static uint32_t _interact_time = 0;
  void Draw_Print_ProgressInteract() {
    uint32_t interact_time = _interact_time;
      MString<12> buf;
      buf.setf(F("%02i:%02i "), interact_time / 3600, (interact_time % 3600) / 60);
      DWINUI::Draw_String(HMI_data.Text_Color, HMI_data.Background_Color, 251, 192, buf);
  }
#endif

void ICON_ResumeOrPause() {
  if (checkkey == PrintProcess) { (print_job_timer.isPaused() || HMI_flag.pause_flag) ? ICON_Resume() : ICON_Pause(); }
}

// Update filename on print
void DWIN_Print_Header(const char *text=nullptr) {
  static char headertxt[31] = "";  // Print header text
  if (text) {
    const int8_t size = _MIN(30U, strlen_P(text));
    for (uint8_t i = 0; i < size; ++i) headertxt[i] = text[i];
    headertxt[size] = '\0';
  }
  if (checkkey == PrintProcess || checkkey == PrintDone) {
    DWIN_Draw_Rectangle(1, HMI_data.Background_Color, 0, 60, DWIN_WIDTH, 60+16);
    DWINUI::Draw_CenteredString(60, headertxt);
  }
}

void Draw_PrintProcess() {
  #if ENABLED(CV_LASER_MODULE)
    Title.ShowCaption(laser_device.is_laser_device() ? GET_TEXT_F(MSG_ENGRAVING) : GET_TEXT_F(MSG_PRINTING));
  #else
    Title.ShowCaption(GET_TEXT_F(MSG_PRINTING));
  #endif
  DWINUI::ClearMainArea();
  DWIN_Print_Header(nullptr);
  Draw_Print_Labels();
  DWINUI::Draw_Icon(ICON_PrintTime, 15, 171);
  DWINUI::Draw_Icon(ICON_RemainTime, 150, 171);
  Draw_Print_ProgressBar();
  Draw_Print_ProgressElapsed();
  Draw_Print_ProgressRemain();
  TERN_(SHOW_INTERACTION_TIME, Draw_Print_ProgressInteract();)
  ICON_Tune();
  ICON_ResumeOrPause();
  ICON_Stop();
}

void Goto_PrintProcess() {
  if (checkkey == PrintProcess) { ICON_ResumeOrPause(); }
  else {
    checkkey = PrintProcess;
    Draw_PrintProcess();
    TERN_(DASH_REDRAW, DWIN_RedrawDash();)
  }
  DWIN_UpdateLCD();
}

void Draw_PrintDone() {
  ui.set_progress_done();
  ui.reset_remaining_time();
  Title.ShowCaption(GET_TEXT_F(MSG_PRINT_DONE));
  DWINUI::ClearMainArea();
  DWIN_Print_Header(nullptr);
  #if HAS_GCODE_PREVIEW
    const bool haspreview = preview.valid();
    if (haspreview) {
      preview.show();
      DWINUI::Draw_Button(BTN_Continue, 86, 295, true);
    }
  #else
    constexpr bool haspreview = false;
  #endif

  if (!haspreview) {
    Draw_Print_ProgressBar();
    Draw_Print_Labels();
    DWINUI::Draw_Icon(ICON_PrintTime, 15, 171);
    DWINUI::Draw_Icon(ICON_RemainTime, 150, 171);
    Draw_Print_ProgressElapsed();
    Draw_Print_ProgressRemain();
    TERN_(SHOW_INTERACTION_TIME, Draw_Print_ProgressInteract();)
    DWINUI::Draw_Button(BTN_Continue, 86, 273, true);
  }
}

void Goto_PrintDone() {
  DEBUG_ECHOLNPGM("Goto_PrintDone");
  wait_for_user = true;
  if (checkkey != PrintDone) {
    checkkey = PrintDone;
    Draw_PrintDone();
    DWIN_UpdateLCD();
  }
}

void Draw_Main_Menu() {
  DWINUI::ClearMainArea();
  #if ENABLED(CV_LASER_MODULE)
    Title.ShowCaption(laser_device.is_laser_device() ? F("Laser Engraver") : CUSTOM_MACHINE_NAME);
  #else
    Title.ShowCaption(CUSTOM_MACHINE_NAME);
  #endif
  DWINUI::Draw_Icon(ICON_LOGO, 71, 52);  // CREALITY logo
  ICON_Print();
  ICON_Prepare();
  ICON_Control();
  ICON_AdvSettings();
  TERN_(HAS_TOOLBAR, Draw_ToolBar();)
}

void Goto_Main_Menu() {
  if (checkkey == MainMenu) return;
  checkkey = MainMenu;
  Draw_Main_Menu();
  DWIN_UpdateLCD();
}

// Draw X, Y, Z and blink if in an un-homed or un-trusted state
void _update_axis_value(const AxisEnum axis, const uint16_t x, const uint16_t y, const bool force) {
  const bool draw_qmark = axis_should_home(axis),
             draw_empty = NONE(HOME_AFTER_DEACTIVATE, DISABLE_REDUCED_ACCURACY_WARNING) && !draw_qmark && !axis_is_trusted(axis);

  // Check for a position change
  static xyz_pos_t oldpos = { -1, -1, -1 };

  #if ALL(IS_FULL_CARTESIAN, SHOW_REAL_POS)
    const float p = planner.get_axis_position_mm(axis);
  #else
    const float p = current_position[axis];
  #endif

  const bool changed = oldpos[axis] != p;
  if (changed) { oldpos[axis] = p; }

  if (force || changed || draw_qmark || draw_empty) {
    if (blink && draw_qmark)
      { DWINUI::Draw_String(HMI_data.Coordinate_Color, HMI_data.Background_Color, x, y, F("  - ? -")); }
    else if (blink && draw_empty)
      { DWINUI::Draw_String(HMI_data.Coordinate_Color, HMI_data.Background_Color, x, y, F("       ")); }
    else
      { DWINUI::Draw_Signed_Float(HMI_data.Coordinate_Color, HMI_data.Background_Color, 3, 2, x, y, p); }
  }
}

void _draw_iconblink(bool &flag, const bool sensor, const uint8_t icon1, const uint8_t icon2, const uint16_t x, const uint16_t y) {
  #if DISABLED(NO_BLINK_IND)
    if (flag != sensor) {
      flag = sensor;
      if (!flag) {
        DWIN_Draw_Box(1, HMI_data.Background_Color, x, y, 20, 20);
        DWINUI::Draw_Icon(icon1, x, y);
      }
    }
    if (flag) {
      DWIN_Draw_Box(1, blink ? HMI_data.Selected_Color : HMI_data.Background_Color, x, y, 20, 20);
      DWINUI::Draw_Icon(icon2, x, y);
    }
  #else
    if (flag != sensor) {
      flag = sensor;
      DWIN_Draw_Box(1, HMI_data.Background_Color, x, y, 20, 20);
      DWINUI::Draw_Icon(flag ? icon2 : icon1, x, y);
    }
  #endif
}

void _draw_ZOffsetIcon() {
  #if HAS_LEVELING
    static bool _leveling_active = false;
    _draw_iconblink(_leveling_active, planner.leveling_active, ICON_Zoffset, ICON_SetZOffset, 187, 416);
  #else
    DWINUI::Draw_Icon(ICON_SetZOffset, 187, 416);
  #endif
}

#if ALL(HAS_FILAMENT_SENSOR, PROUI_EX)
  void _draw_runout_icon() {
    static bool _runout_active = false;
    if (runout.enabled) { _draw_iconblink(_runout_active, FilamentSensorDevice::poll_runout_state(0), ICON_StepE, ICON_Version, 113, 416); }
    else {
      DWIN_Draw_Box(1, HMI_data.Background_Color, 113, 416, 20, 20);
      DWINUI::Draw_Icon(ICON_StepE, 113, 416);
    }
  }
#endif

void _draw_feedrate() {
  #if ENABLED(SHOW_SPEED_IND)
    static bool _should_redraw;
    if (HMI_data.SpdInd) {
      int16_t _value;
      if (blink) {
        _value = feedrate_percentage;
        DWINUI::Draw_String(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 116 + 4 * STAT_CHR_W + 2, 384, F(" %"));
      }
      else {
        _value = CEIL(MMS_SCALED(feedrate_mm_s));
        DWIN_Draw_Box(1, HMI_data.Background_Color, 116 + 4 * STAT_CHR_W + 2, 384, 30, 20);
      }
      DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 116 + 2 * STAT_CHR_W, 384, _value);
      _should_redraw = true;
    }
    else {
      static int16_t _feedrate = 100;
      if (blink && _should_redraw == true) {
        DWINUI::Draw_String(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 116 + 4 * STAT_CHR_W + 2, 384, F(" %"));
        DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 116 + 2 * STAT_CHR_W, 384, feedrate_percentage);
      }
      else if (_feedrate != feedrate_percentage) {
        _feedrate = feedrate_percentage;
        DWINUI::Draw_String(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 116 + 4 * STAT_CHR_W + 2, 384, F(" %"));
        DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 116 + 2 * STAT_CHR_W, 384, _feedrate);
      }
    }
  #else
    static int16_t _feedrate = 100;
    if (_feedrate != feedrate_percentage) {
      _feedrate = feedrate_percentage;
      DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 116 + 2 * STAT_CHR_W, 384, _feedrate);
    }
  #endif
}

void _draw_xyz_position(const bool force) {
  _update_axis_value(X_AXIS,  27, 457, force);
  _update_axis_value(Y_AXIS, 112, 457, force);
  _update_axis_value(Z_AXIS, 197, 457, force);
}

void update_variable() {
  TERN_(DEBUG_DWIN, DWINUI::Draw_Int(Color_Light_Red, Color_Bg_Black, 2, DWIN_WIDTH - 6 * DWINUI::fontWidth(), 6, checkkey);)
  TERN_(DEBUG_DWIN, DWINUI::Draw_Int(Color_Yellow, Color_Bg_Black, 2, DWIN_WIDTH - 3 * DWINUI::fontWidth(), 6, last_checkkey);)

  _draw_xyz_position(false);

  TERN_(CV_LASER_MODULE, if (laser_device.is_laser_device()) return;)

  #if HAS_HOTEND
    static celsius_t _hotendtemp = 0, _hotendtarget = 0;
    const celsius_t hc = thermalManager.wholeDegHotend(EXT),
                    ht = thermalManager.degTargetHotend(EXT);
    const bool _new_hotend_temp = _hotendtemp != hc,
               _new_hotend_target = _hotendtarget != ht;
    if (_new_hotend_temp) { _hotendtemp = hc; }
    if (_new_hotend_target) { _hotendtarget = ht; }

    // if hotend is near target, or heating, ICON indicates hot
    if (thermalManager.degHotendNear(EXT, ht) || thermalManager.isHeatingHotend(EXT)) {
      DWIN_Draw_Box(1, HMI_data.Background_Color, 9, 383, 20, 20);
      DWINUI::Draw_Icon(ICON_SetEndTemp, 9, 383);
    }
    else {
      DWIN_Draw_Box(1, HMI_data.Background_Color, 9, 383, 20, 20);
      DWINUI::Draw_Icon(ICON_HotendTemp, 9, 383);
    }
  #endif
  #if HAS_HEATED_BED
    static celsius_t _bedtemp = 0, _bedtarget = 0;
    const celsius_t bc = thermalManager.wholeDegBed(),
                    bt = thermalManager.degTargetBed();
    const bool _new_bed_temp = _bedtemp != bc,
               _new_bed_target = _bedtarget != bt;
    if (_new_bed_temp) { _bedtemp = bc; }
    if (_new_bed_target) { _bedtarget = bt; }

    // if bed is near target, heating, or if degrees > 44, ICON indicates hot
    if (thermalManager.degBedNear(bt) || thermalManager.isHeatingBed() || (bc > 44)) {
      DWIN_Draw_Box(1, HMI_data.Background_Color, 9, 416, 20, 20);
      DWINUI::Draw_Icon(ICON_BedTemp, 9, 416);
    }
    else {
      DWIN_Draw_Box(1, HMI_data.Background_Color, 9, 416, 20, 20);
      DWINUI::Draw_Icon(ICON_SetBedTemp, 9, 416);
    }
  #endif
  #if HAS_FAN
    static uint8_t _fanspeed = 0;
    const bool _new_fanspeed = _fanspeed != thermalManager.fan_speed[EXT];
    if (_new_fanspeed) { _fanspeed = thermalManager.fan_speed[EXT]; }
  #endif

  if (IsMenu(TuneMenu) || IsMenu(TemperatureMenu)) {
    // Tune page temperature update
    TERN_(HAS_HOTEND, if (_new_hotend_target) { HotendTargetItem->redraw(); })
    TERN_(HAS_HEATED_BED, if (_new_bed_target) { BedTargetItem->redraw(); })
    TERN_(HAS_FAN, if (_new_fanspeed) { FanSpeedItem->redraw(); })
  }

  // Bottom temperature update

  #if HAS_HOTEND
    if (_new_hotend_temp)
      { DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 28, 384, _hotendtemp); }
    if (_new_hotend_target)
      { DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 25 + 4 * STAT_CHR_W + 6, 384, _hotendtarget); }

    static int16_t _flow = planner.flow_percentage[EXT];
    if (_flow != planner.flow_percentage[EXT]) {
      _flow = planner.flow_percentage[EXT];
      DWINUI::Draw_Signed_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 116 + 2 * STAT_CHR_W, 417, _flow);
    }
  #endif

  #if HAS_HEATED_BED
    if (_new_bed_temp)
      { DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 28, 417, _bedtemp); }
    if (_new_bed_target)
      { DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 25 + 4 * STAT_CHR_W + 6, 417, _bedtarget); }
  #endif

  _draw_feedrate();

  #if HAS_FAN
    if (_new_fanspeed)
      { DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 195 + 2 * STAT_CHR_W, 384, _fanspeed); }
  #endif

  static float _offset = 0;
  if (BABY_Z_VAR != _offset) {
    _offset = BABY_Z_VAR;
    DWINUI::Draw_Signed_Float(DWIN_FONT_STAT, HMI_data.Indicator_Color,  HMI_data.Background_Color, 2, 2, 204, 417, _offset);
  }

  #if ALL(HAS_FILAMENT_SENSOR, PROUI_EX)
    _draw_runout_icon();
  #endif

  _draw_ZOffsetIcon();

}

/**
 * Memory card and file management
 */

bool DWIN_lcd_sd_status = false;

#if ENABLED(PROUI_MEDIASORT)
  void SetMediaSort() {
    Toggle_Chkb_Line(HMI_data.MediaSort);
    card.setSortOn(HMI_data.MediaSort ? TERN(SDSORT_REVERSE, AS_REV, AS_FWD) : AS_OFF);
  }
#endif

void SetMediaAutoMount() { Toggle_Chkb_Line(HMI_data.MediaAutoMount); }

inline uint16_t nr_sd_menu_items() {
  return _MIN(card.get_num_items() + !card.flag.workDirIsRoot, MENU_MAX_ITEMS);
}

void make_name_without_ext(char *dst, char *src, size_t maxlen=MENU_CHAR_LIMIT) {
  size_t pos = strlen(src); // index of ending nul

  // For files, remove the extension
  // which may be .gcode, .gco, or .g
  if (!card.flag.filenameIsDir) {
    while (pos && src[pos] != '.') pos--; // find last '.' (stop at 0)
  }

  if (!pos) { pos = strlen(src); } // pos = 0 ('.' not found) restore pos

  size_t len = pos;     // nul or '.'
  if (len > maxlen) {   // Keep the name short
    pos = len = maxlen; // move nul down
    dst[--pos] = '.';   // insert dots
    dst[--pos] = '.';
    dst[--pos] = '.';
  }

  dst[len] = '\0';      // end it

  // Copy down to 0
  while (pos--) dst[pos] = src[pos];
}

void SDCard_Up() {
  card.cdup();
  DWIN_lcd_sd_status = false; // On next DWIN_Update
}

void SDCard_Folder(char * const dirname) {
  card.cd(dirname);
  DWIN_lcd_sd_status = false; // On next DWIN_Update
}

void onClickSDItem() {
  const uint16_t hasUpDir = !card.flag.workDirIsRoot;
  if (hasUpDir && CurrentMenu->selected == 1) return SDCard_Up();
  else {
    const uint16_t filenum = CurrentMenu->selected - 1 - hasUpDir;
    card.selectFileByIndexSorted(filenum);

    // Enter that folder!
    if (card.flag.filenameIsDir) return SDCard_Folder(card.filename);

    if (card.fileIsBinary())
      return DWIN_Popup_Continue(ICON_Error, GET_TEXT_F(MSG_CHECK_FILENAME), GET_TEXT_F(MSG_ONLY_GCODE));
    else {
      DWIN_Print_Header(card.longest_filename()); // Save filename
      return Goto_ConfirmToPrint();
    }
  }
}

#if ENABLED(SCROLL_LONG_FILENAMES)
  char shift_name[LONG_FILENAME_LENGTH + 1] = "";

  void Draw_SDItem_Shifted(uint8_t &shift) {
    // Shorten to the available space
    const size_t lastchar = shift + MENU_CHAR_LIMIT;
    const char c = shift_name[lastchar];
    shift_name[lastchar] = '\0';

    const uint8_t row = FileMenu->line();
    Erase_Menu_Text(row);
    Draw_Menu_Line(row, 0, &shift_name[shift]);

    shift_name[lastchar] = c;
  }

  void FileMenuIdle(bool reset=false) {
    static bool hasUpDir = false;
    static uint8_t last_itemselected = 0;
    static int8_t shift_amt = 0, shift_len = 0;
    if (reset) {
      last_itemselected = 0;
      hasUpDir = !card.flag.workDirIsRoot; // is a SubDir
      return;
    }
    const uint8_t selected = FileMenu->selected;
    if (last_itemselected != selected) {
      if (last_itemselected >= 1 + hasUpDir) { FileMenu->Items()[last_itemselected]->redraw(true); }
      last_itemselected = selected;
      if (selected >= 1 + hasUpDir) {
        const int8_t filenum = selected - 1 - hasUpDir; // Skip "Back" and ".."
        card.selectFileByIndexSorted(filenum);
        make_name_without_ext(shift_name, card.longest_filename(), LONG_FILENAME_LENGTH);
        shift_len = strlen(shift_name);
        shift_amt = 0;
      }
    }
    else if ((selected >= 1 + hasUpDir) && (shift_len > MENU_CHAR_LIMIT)) {
      uint8_t shift_new = _MIN(shift_amt + 1, shift_len - MENU_CHAR_LIMIT); // Try to shift by...
      Draw_SDItem_Shifted(shift_new); // Draw the item
      if (shift_new == shift_amt) {   // Scroll reached the end
        shift_new = -1;               // Reset
      }
      shift_amt = shift_new;          // Set new scroll
    }
  }
#else
  char shift_name[FILENAME_LENGTH + 1] = "";
#endif

void onDrawFileName(MenuItemClass* menuitem, int8_t line) {
  const bool is_subdir = !card.flag.workDirIsRoot;
  if (is_subdir && menuitem->pos == 1) {
    Draw_Menu_Line(line, ICON_ReadEEPROM, ".. Back");
  }
  else {
    uint8_t icon;
    card.selectFileByIndexSorted(menuitem->pos - is_subdir - 1);
    make_name_without_ext(shift_name, card.longest_filename());
    icon = card.flag.filenameIsDir ? ICON_Folder : card.fileIsBinary() ? ICON_Binary : ICON_File;
    Draw_Menu_Line(line, icon, shift_name);
  }
}

void Draw_Print_File_Menu() {
  checkkey = Menu;
  if (card.isMounted()) {
    if (SET_MENU(FileMenu, MSG_MEDIA_MENU, nr_sd_menu_items() + 1)) {
      MenuItemAdd(ICON_Back, GET_TEXT_F(MSG_EXIT_MENU), onDrawMenuItem, Goto_Main_Menu);
      for (uint8_t i = 0; i < nr_sd_menu_items(); ++i) {
        MenuItemAdd(onDrawFileName, onClickSDItem);
      }
    }
    UpdateMenu(FileMenu);
    TERN_(DASH_REDRAW, DWIN_RedrawDash();)
  }
  else {
    if (SET_MENU(FileMenu, MSG_MEDIA_MENU, 1)) { BACK_ITEM(Goto_Main_Menu); }
    UpdateMenu(FileMenu);
    DWIN_Draw_Rectangle(1, HMI_data.AlertBg_Color, 10, MBASE(3) - 10, DWIN_WIDTH - 10, MBASE(4));
    DWINUI::Draw_CenteredString(font12x24, HMI_data.AlertTxt_Color, MBASE(3), GET_TEXT_F(MSG_MEDIA_NOT_INSERTED));
  }
  TERN_(SCROLL_LONG_FILENAMES, FileMenuIdle(true);)
}

//
// Watch for media mount / unmount
//
void HMI_SDCardUpdate() {
  if (checkkey == Homing) return;
  if (DWIN_lcd_sd_status != card.isMounted()) {
    DWIN_lcd_sd_status = card.isMounted();
    ResetMenu(FileMenu);
    if (IsMenu(FileMenu)) {
      CurrentMenu = nullptr;
      Draw_Print_File_Menu();
    }
    if (!DWIN_lcd_sd_status && SD_Printing()) { ui.abort_print(); } // Media removed while printing
  }
}

/**
 * Dash board and indicators
 */

void DWIN_Draw_Dashboard() {
  DWIN_Draw_Rectangle(1, HMI_data.Background_Color, 0, STATUS_Y + 21, DWIN_WIDTH, DWIN_HEIGHT - 1);
  DWIN_Draw_Rectangle(1, HMI_data.Bottom_Color, 0, 449, DWIN_WIDTH, 450);

  DWINUI::Draw_Icon(ICON_MaxSpeedX,  10, 454);
  DWINUI::Draw_Icon(ICON_MaxSpeedY,  95, 454);
  DWINUI::Draw_Icon(ICON_MaxSpeedZ, 180, 454);
  _draw_xyz_position(true);

  DWIN_Draw_Rectangle(1, HMI_data.Bottom_Color, 0, 478, DWIN_WIDTH, 479); //changed added 2nd line

  TERN_(CV_LASER_MODULE, if (laser_device.is_laser_device()) return;)

  #if HAS_HOTEND
    DWINUI::Draw_Icon(ICON_HotendTemp, 9, 383);
    DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 28, 384, thermalManager.wholeDegHotend(EXT));
    DWINUI::Draw_String(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 25 + 3 * STAT_CHR_W + 5, 384, F("/"));
    DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 25 + 4 * STAT_CHR_W + 6, 384, thermalManager.degTargetHotend(EXT));
    DWIN_Draw_DegreeSymbol(HMI_data.Indicator_Color, 25 + 4 * STAT_CHR_W + 39, 384);

    DWINUI::Draw_Icon(ICON_StepE, 113, 416);
    DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 116 + 2 * STAT_CHR_W, 417, planner.flow_percentage[EXT]);
    DWINUI::Draw_String(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 116 + 5 * STAT_CHR_W + 2, 417, F("%"));
  #endif

  #if HAS_HEATED_BED
    DWINUI::Draw_Icon(ICON_SetBedTemp, 9, 416);
    DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 28, 417, thermalManager.wholeDegBed());
    DWINUI::Draw_String(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 25 + 3 * STAT_CHR_W + 5, 417, F("/"));
    DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 25 + 4 * STAT_CHR_W + 6, 417, thermalManager.degTargetBed());
    DWIN_Draw_DegreeSymbol(HMI_data.Indicator_Color, 25 + 4 * STAT_CHR_W + 39, 417);
  #endif

  DWINUI::Draw_Icon(ICON_Speed, 113, 383);
  DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 116 + 2 * STAT_CHR_W, 384, feedrate_percentage);
  TERN_(SHOW_SPEED_IND, if (!HMI_data.SpdInd)) DWINUI::Draw_String(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 116 + 5 * STAT_CHR_W + 2, 384, F("%"));

  #if HAS_FAN
    DWINUI::Draw_Icon(ICON_FanSpeed, 187, 383);
    DWINUI::Draw_Int(DWIN_FONT_STAT, HMI_data.Indicator_Color, HMI_data.Background_Color, 3, 195 + 2 * STAT_CHR_W, 384, thermalManager.fan_speed[EXT]);
  #endif

  #if HAS_ZOFFSET_ITEM
    DWINUI::Draw_Icon(planner.leveling_active ? ICON_SetZOffset : ICON_Zoffset, 187, 416);
    DWINUI::Draw_Signed_Float(DWIN_FONT_STAT, HMI_data.Indicator_Color,  HMI_data.Background_Color, 2, 2, 204, 417, BABY_Z_VAR);
  #endif

}

void Draw_Info_Menu() {
  DWINUI::ClearMainArea();
  Title.ShowCaption(GET_TEXT_F(MSG_INFO_SCREEN));
  Draw_Menu_Line(0, ICON_Back, GET_TEXT_F(MSG_BACK), false, true);
  char machine_size[21];
  machine_size[0] = '\0';
  sprintf_P(machine_size, PSTR("%ix%ix%i"), (int16_t)X_BED_SIZE, (int16_t)Y_BED_SIZE, (int16_t)Z_MAX_POS);

  DWINUI::Draw_CenteredString(92,  GET_TEXT_F(MSG_INFO_MACHINENAME));
  DWINUI::Draw_CenteredString(112, CUSTOM_MACHINE_NAME);
  DWINUI::Draw_CenteredString(145, GET_TEXT_F(MSG_INFO_SIZE));
  DWINUI::Draw_CenteredString(165, machine_size);

  for (uint8_t i = 0; i < TERN(PROUI_EX, 2, 4); ++i) {
    DWINUI::Draw_Icon(ICON_Step + i, ICOX, 90 + i * MLINE);
    DWIN_Draw_HLine(HMI_data.SplitLine_Color, 16, MYPOS(i + 2), 240);
  }

  #if PROUI_EX
    Init();
  #else
    DWINUI::Draw_CenteredString(198, GET_TEXT_F(MSG_INFO_FWVERSION));
    DWINUI::Draw_CenteredString(218, F(SHORT_BUILD_VERSION));
    DWINUI::Draw_CenteredString(251, GET_TEXT_F(MSG_INFO_BUILD));
    DWINUI::Draw_CenteredString(271, F(DateTime));
  #endif
    // Draw the face
    // DWINUI::Draw_FillCircle(0xFF0F, 128, 320, 50);
    // Draw the eyes
    // DWIN_Draw_Point(0x00FF, 15, 15, 75+28, 75+218);
    // DWIN_Draw_Point(0x00FF, 15, 15, 110+28, 75+218);

    // Draw the mouth
    // DWIN_Draw_Line(0x0000, 80+28, 115+218, 120+28, 115+218);
    // DWIN_Draw_Line(0x0000, 80+28, 115+218, 89+28, 124+218);
    // DWIN_Draw_Line(0x0000, 111+28, 124+218, 120+28, 115+218);
    // DWIN_Draw_Line(0x0000, 111+28, 124+218, 89+28, 124+218);
}

// Main Process
void HMI_MainMenu() {
  EncoderState encoder_diffState = get_encoder_state();
  if (encoder_diffState == ENCODER_DIFF_NO) return;

  if (encoder_diffState == ENCODER_DIFF_CW) {
    if (select_page.inc(PAGE_COUNT)) {
      switch (select_page.now) {
        case PAGE_PRINT:   ICON_Print(); break;
        case PAGE_PREPARE: ICON_Print();   ICON_Prepare(); break;
        case PAGE_CONTROL: ICON_Prepare(); ICON_Control(); break;
        case PAGE_ADVANCE: ICON_Control(); ICON_AdvSettings(); break;
        OPTCODE(HAS_TOOLBAR,
        case PAGE_TOOLBAR: ICON_AdvSettings(); Goto_ToolBar();  break)
      }
    }
  }
  else if (encoder_diffState == ENCODER_DIFF_CCW) {
    if (select_page.dec()) {
      switch (select_page.now) {
        case PAGE_PRINT:   ICON_Print();   ICON_Prepare(); break;
        case PAGE_PREPARE: ICON_Prepare(); ICON_Control(); break;
        case PAGE_CONTROL: ICON_Control(); ICON_AdvSettings(); break;
        case PAGE_ADVANCE: ICON_AdvSettings(); break;
      }
    }
  }
  else if (encoder_diffState == ENCODER_DIFF_ENTER) {
    switch (select_page.now) {
      case PAGE_PRINT:
        if (HMI_data.MediaAutoMount) {
          card.mount();
          safe_delay(800);
        }
        Draw_Print_File_Menu();
        break;
      case PAGE_PREPARE: Draw_Prepare_Menu(); break;
      case PAGE_CONTROL: Draw_Control_Menu(); break;
      case PAGE_ADVANCE: Draw_AdvancedSettings_Menu(); break;
    }
  }
  DWIN_UpdateLCD();
}

// Pause or Stop popup
void OnClick_PauseOrStop() {
  switch (select_print.now) {
    case PRINT_PAUSE_RESUME: if (HMI_flag.select_flag) { ui.pause_print(); } break; // confirm pause
    case PRINT_STOP: if (HMI_flag.select_flag) { ui.abort_print(); } break; // stop confirmed then abort print
    default: break;
  }
  return Goto_PrintProcess();
}

// Printing
void HMI_Printing() {
  EncoderState encoder_diffState = get_encoder_state();
  if (encoder_diffState == ENCODER_DIFF_NO) return;
  // Avoid flicker by updating only the previous menu
  if (encoder_diffState == ENCODER_DIFF_CW) {
    if (select_print.inc(PRINT_COUNT)) {
      switch (select_print.now) {
        case PRINT_SETUP: ICON_Tune(); break;
        case PRINT_PAUSE_RESUME: ICON_Tune(); ICON_ResumeOrPause(); break;
        case PRINT_STOP: ICON_ResumeOrPause(); ICON_Stop(); break;
      }
    }
  }
  else if (encoder_diffState == ENCODER_DIFF_CCW) {
    if (select_print.dec()) {
      switch (select_print.now) {
        case PRINT_SETUP: ICON_Tune(); ICON_ResumeOrPause(); break;
        case PRINT_PAUSE_RESUME: ICON_ResumeOrPause(); ICON_Stop(); break;
        case PRINT_STOP: ICON_Stop(); break;
      }
    }
  }
  else if (encoder_diffState == ENCODER_DIFF_ENTER) {
    switch (select_print.now) {
      case PRINT_SETUP: Draw_Tune_Menu(); break;
      case PRINT_PAUSE_RESUME:
        if (print_job_timer.isPaused()) {  // if printer is already in pause
          ui.resume_print();
          break;
        }
        else {
          return Goto_Popup(Popup_window_PauseOrStop, OnClick_PauseOrStop);
        }
      case PRINT_STOP:
        return Goto_Popup(Popup_window_PauseOrStop, OnClick_PauseOrStop);
      default: break;
    }
  }
  DWIN_UpdateLCD();
}

void Draw_Main_Area() {
  switch (checkkey) {
    case MainMenu:               Draw_Main_Menu(); break;
    case PrintProcess:           Draw_PrintProcess(); break;
    case PrintDone:              Draw_PrintDone(); break;
    OPTCODE(HAS_ESDIAG,
    case ESDiagProcess:          Draw_EndStopDiag(); break)
    OPTCODE(PROUI_ITEM_PLOT,
    case PlotProcess:
      if (HMI_value.tempControl == PID_BED_START) { drawBPlot(); }
      else { drawHPlot(); } break)
    case Popup:                  Draw_Popup(); break;
    OPTCODE(HAS_LOCKSCREEN,
    case Locked:                 lockScreen.draw(); break)
    case Menu:
      #if HAS_TOOLBAR
        if (CurrentMenu == &ToolBar) { Draw_Main_Menu(); } else { ReDrawMenu(); }
      #else
        ReDrawMenu();
      #endif
      break;
    case SetInt:
    case SetPInt:
    case SetIntNoDraw:
    case SetFloat:
    case SetPFloat:              ReDrawMenu(true); break;
    default: break;
  }
}

void HMI_WaitForUser() {
  EncoderState encoder_diffState = get_encoder_state();
  if ((encoder_diffState != ENCODER_DIFF_NO) && !ui.backlight) {
    if (checkkey == WaitResponse) { HMI_ReturnScreen(); }
    //EachMomentUpdate();
    //idle();
    return ui.refresh_brightness();
  }
  if (!wait_for_user) {
    switch (checkkey) {
      case PrintDone:
        select_page.reset();
        Goto_Main_Menu();
        break;
      #if HAS_BED_PROBE
        case Leveling:
          HMI_flag.cancel_lev = 1;
          DWIN_DrawStatusLine("Canceling auto leveling...");
          DWIN_UpdateLCD();
          break;
      #endif
      case NothingToDo:
        break;
      default:
        HMI_ReturnScreen();
        break;
    }
  }
}

// Draws boot screen
void HMI_Init() {
  #if ENABLED(SHOW_BOOTSCREEN)
    #ifndef BOOTSCREEN_TIMEOUT
      #define BOOTSCREEN_TIMEOUT 1100
    #endif
    DWINUI::Draw_Box(1, Color_Black, { 5, 220, DWIN_WIDTH - 5, DWINUI::fontHeight() });
    DWINUI::Draw_CenteredString(3, Color_White, 220, F(MACHINE_NAME));
    for (uint16_t t = 15; t <= 257; t += 10) {
      DWINUI::Draw_Icon(ICON_Bar, 15, 260);
      DWIN_Draw_Rectangle(1, HMI_data.Background_Color, t, 260, 257, 280);
      DWIN_UpdateLCD();
      safe_delay((BOOTSCREEN_TIMEOUT) / 22);
    }
  #endif
}

void EachMomentUpdate() {
  static millis_t next_var_update_ms = 0, next_rts_update_ms = 0, next_status_update_ms = 0;
  const millis_t ms = millis();

  #if HAS_BACKLIGHT_TIMEOUT
    if (ui.backlight_off_ms && ELAPSED(ms, ui.backlight_off_ms)) {
      TurnOffBacklight(); // Backlight off
      ui.backlight_off_ms = 0;
    }
  #endif

  if (ELAPSED(ms, next_var_update_ms)) {
    next_var_update_ms = ms + DWIN_VAR_UPDATE_INTERVAL;
    blink = !blink;
    update_variable();
    #if HAS_ESDIAG
      if (checkkey == ESDiagProcess) { esDiag.update(); }
    #endif
    #if PROUI_TUNING_GRAPH
      if (checkkey == PidProcess) {
        TERN_(PIDTEMP, if (HMI_value.tempControl == PID_EXTR_START) { plot.update(thermalManager.wholeDegHotend(EXT)); })
        TERN_(PIDTEMPBED, if (HMI_value.tempControl == PID_BED_START) { plot.update(thermalManager.wholeDegBed()); })
      }
      if (checkkey == MPCProcess) {
        TERN_(MPCTEMP, if (HMI_value.tempControl == MPCTEMP_START) { plot.update(thermalManager.wholeDegHotend(EXT)); })
      }
      #if ENABLED(PROUI_ITEM_PLOT)
        if (checkkey == PlotProcess) {
          TERN_(PIDTEMP, if (HMI_value.tempControl == PID_EXTR_START) { plot.update(thermalManager.wholeDegHotend(EXT)); })
          TERN_(PIDTEMPBED, if (HMI_value.tempControl == PID_BED_START) { plot.update(thermalManager.wholeDegBed()); })
          TERN_(MPCTEMP, if (HMI_value.tempControl == MPCTEMP_START) { plot.update(thermalManager.wholeDegHotend(EXT)); })
          if (HMI_flag.abort_flag || HMI_flag.pause_flag || print_job_timer.isPaused()) {
            HMI_ReturnScreen();
          }
        }
      #endif
    #endif
  }

  #if HAS_STATUS_MESSAGE_TIMEOUT
    bool did_expire = ui.status_reset_callback && (*ui.status_reset_callback)();
    did_expire |= ui.status_message_expire_ms && ELAPSED(ms, ui.status_message_expire_ms);
    if (did_expire) ui.reset_status();
  #endif

  if (ELAPSED(ms, next_status_update_ms)) {
    next_status_update_ms = ms + DWIN_VAR_UPDATE_INTERVAL;
    DWIN_DrawStatusMessage();
    #if ENABLED(SCROLL_LONG_FILENAMES)
      if (IsMenu(FileMenu)) { FileMenuIdle(); }
    #endif
  }

  if (!PENDING(ms, next_rts_update_ms)) {
    next_rts_update_ms = ms + DWIN_UPDATE_INTERVAL;

    if ((HMI_flag.printing_flag != Printing()) && (checkkey != Homing) TERN_(HAS_BED_PROBE, && (checkkey != Leveling))) {
      HMI_flag.printing_flag = Printing();
      DEBUG_ECHOLNPGM("printing_flag: ", HMI_flag.printing_flag);
      if (HMI_flag.printing_flag) { DWIN_Print_Started(); }
      else if (HMI_flag.abort_flag) { DWIN_Print_Aborted(); }
      else { DWIN_Print_Finished(); }
    }

    if ((HMI_flag.pause_flag != printingIsPaused()) && (checkkey != Homing)) {
      HMI_flag.pause_flag = printingIsPaused();
      DEBUG_ECHOLNPGM("pause_flag: ", HMI_flag.pause_flag);
      if (HMI_flag.pause_flag) { DWIN_Print_Pause(); }
      else if (HMI_flag.abort_flag) { DWIN_Print_Aborted(); }
      else { DWIN_Print_Resume(); }
    }

    if (checkkey == PrintProcess) { // print process

      // Progress percent
      if (_percent_done != card.percentDone()) {
        _percent_done = card.percentDone();
        Draw_Print_ProgressBar();
      }

      // Remaining time
      #if ENABLED(SHOW_REMAINING_TIME)
        if (_remain_time != ui.get_remaining_time()) {
          _remain_time = ui.get_remaining_time();
          Draw_Print_ProgressRemain();
        }
      #endif

      #if ENABLED(SHOW_INTERACTION_TIME)
        // Interaction time
        if (_interact_time != ui.get_interaction_time()) {
          _interact_time = ui.get_interaction_time();
          Draw_Print_ProgressInteract();
        }
      #endif

      // Elapsed print time
      const uint16_t min = print_job_timer.duration();
      if (_printtime.minute() + 1 <= min) { // 1 minute update
        _printtime.value = min;
        Draw_Print_ProgressElapsed();
      }

    }
    #if ENABLED(POWER_LOSS_RECOVERY)
      else if (DWIN_lcd_sd_status && recovery.dwin_flag) { // resume print before power off
        return Goto_PowerLossRecovery();
      }
    #endif // POWER_LOSS_RECOVERY
  }
  DWIN_UpdateLCD();
}

#if ENABLED(POWER_LOSS_RECOVERY)
  void Popup_PowerLossRecovery() {
    DWINUI::ClearMainArea();
    Draw_Popup_Bkgd();
    DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 70, GET_TEXT_F(MSG_OUTAGE_RECOVERY));
    DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 147, F("It looks like the last"));
    DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 167, F("file was interrupted."));
    DWINUI::Draw_Button(BTN_Cancel, 26, 280);
    DWINUI::Draw_Button(BTN_Continue, 146, 280);
    MediaFile *dir = nullptr;
    const char * const filename = card.diveToFile(true, dir, recovery.info.sd_filename);
    card.selectFileByName(filename);
    DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 207, card.longest_filename());
    DWIN_Print_Header(card.longest_filename()); // Save filename
    Draw_Select_Highlight(HMI_flag.select_flag);
    DWIN_UpdateLCD();
  }

  void OnClick_PowerLossRecovery() {
    if (HMI_flag.select_flag) {
      queue.inject(F("M1000C"));
      select_page.reset();
      return Goto_Main_Menu();
    }
    else {
      HMI_SaveProcessID(NothingToDo);
      select_print.set(PRINT_SETUP);
      queue.inject(F("M1000"));
    }
  }

  void Goto_PowerLossRecovery() {
    recovery.dwin_flag = false;
    LCD_MESSAGE(MSG_CONTINUE_PRINT_JOB);
    Goto_Popup(Popup_PowerLossRecovery, OnClick_PowerLossRecovery);
  }

#endif // POWER_LOSS_RECOVERY

#if ENABLED(AUTO_BED_LEVELING_UBL)

  void ApplyUBLSlot() { bedlevel.storage_slot = MenuData.Value; }
  void SetUBLSlot() { SetIntOnClick(0, settings.calc_num_meshes() - 1, bedlevel.storage_slot, ApplyUBLSlot); }
  void onDrawUBLSlot(MenuItemClass* menuitem, int8_t line) {
    NOLESS(bedlevel.storage_slot, 0);
    onDrawIntMenu(menuitem, line, bedlevel.storage_slot);
  }

  void ApplyUBLTiltGrid() { bedLevelTools.tilt_grid = MenuData.Value; }
  void SetUBLTiltGrid() { SetIntOnClick(1, 3, bedLevelTools.tilt_grid, ApplyUBLTiltGrid); }

  void UBLMeshTilt() {
    NOLESS(bedlevel.storage_slot, 0);
    if (bedLevelTools.tilt_grid > 1) {
      gcode.process_subcommands_now(TS(F("G29J"), bedLevelTools.tilt_grid));
    }
    else {
      gcode.process_subcommands_now(F("G29J"));
    }
    LCD_MESSAGE(MSG_UBL_MESH_TILTED);
  }

  void UBLSmartFillMesh() {
    for (uint8_t x = 0; x < GRID_MAX_POINTS_X; ++x) bedlevel.smart_mesh_fill();
    LCD_MESSAGE(MSG_UBL_MESH_FILLED);
  }

  void UBLMeshSave() {
    NOLESS(bedlevel.storage_slot, 0);
    settings.store_mesh(bedlevel.storage_slot);
    ui.status_printf(0, GET_TEXT_F(MSG_MESH_SAVED), bedlevel.storage_slot);
    DONE_BUZZ(true);
  }

  void UBLMeshLoad() {
    NOLESS(bedlevel.storage_slot, 0);
    settings.load_mesh(bedlevel.storage_slot);
  }

#endif  // AUTO_BED_LEVELING_UBL

void DWIN_HandleScreen() {
  switch (checkkey) {
    case MainMenu:        HMI_MainMenu(); break;
    case Menu:            HMI_Menu(); break;
    case SetInt:          HMI_SetDraw(); break;
    case SetFloat:        HMI_SetDraw(); break;
    case SetPInt:         HMI_SetPInt(); break;
    case SetPFloat:       HMI_SetPFloat(); break;
    case SetIntNoDraw:    HMI_SetNoDraw(); break;
    case PrintProcess:    HMI_Printing(); break;
    case Popup:           HMI_Popup(); break;
    OPTCODE(HAS_LOCKSCREEN,
    case Locked:          HMI_LockScreen(); break)

    #if ALL(HAS_BED_PROBE, PROUI_EX)
      case Leveling:
    #endif
    case PrintDone:
    TERN_(HAS_ESDIAG,
    case ESDiagProcess:)
    TERN_(PROUI_ITEM_PLOT,
    case PlotProcess:)
    case WaitResponse:    HMI_WaitForUser(); break;
    default: break;
  }
}

bool IDisPopUp() {    // If ID is popup...
  switch (checkkey) {
    case NothingToDo:
    case WaitResponse:
    case Popup:
    case Homing:
    TERN_(HAS_BED_PROBE,
    case Leveling:)
    TERN_(HAS_PID_HEATING,
    case PidProcess:)
    TERN_(MPCTEMP,
    case MPCProcess:)
    TERN_(HAS_ESDIAG,
    case ESDiagProcess:)
    TERN_(PROUI_ITEM_PLOT,
    case PlotProcess:)
      return true;
    default: break;
  }
  return false;
}

void HMI_SaveProcessID(const uint8_t id) {
  if (checkkey == id) return;
  if (!IDisPopUp()) { last_checkkey = checkkey; } // if previous is not a popup
  checkkey = id;
  switch (id) {
    case Popup:
    TERN_(HAS_ESDIAG,
    case ESDiagProcess:)
    case PrintDone:
    TERN_(HAS_BED_PROBE,
    case Leveling:)
    TERN_(PROUI_ITEM_PLOT,
    case PlotProcess:)
    case WaitResponse:
      wait_for_user = true;
    default: break;
  }
}

void HMI_ReturnScreen() {
  checkkey = last_checkkey;
  wait_for_user = false;
  Draw_Main_Area();
}

void DWIN_HomingStart() {
  DEBUG_ECHOLNPGM("DWIN_HomingStart");
  if (checkkey != NothingToDo || checkkey != Leveling) { HMI_SaveProcessID(Homing); }
  Title.ShowCaption(GET_TEXT_F(MSG_HOMING));
  DWIN_Show_Popup(TERN(TJC_DISPLAY, ICON_BLTouch, ICON_Printer_0), GET_TEXT_F(MSG_HOMING), GET_TEXT_F(MSG_PLEASE_WAIT));
}

void DWIN_HomingDone() {
  DEBUG_ECHOLNPGM("DWIN_HomingDone");
  #if DISABLED(HAS_BED_PROBE) && ANY(BABYSTEP_ZPROBE_OFFSET, JUST_BABYSTEP)
    planner.synchronize();
    babystep.add_mm(Z_AXIS, HMI_data.ManualZOffset);
  #endif
  #if ENABLED(CV_LASER_MODULE)
    if (laser_device.is_laser_device()) { laser_device.laser_home(); }
  #endif
  if (last_checkkey == PrintDone) { Goto_PrintDone(); }
  else if (checkkey != NothingToDo || checkkey != Leveling) { HMI_ReturnScreen(); }
}

#if HAS_LEVELING
  void DWIN_LevelingStart() {
    DEBUG_ECHOLNPGM("DWIN_LevelingStart");
    #if HAS_BED_PROBE
      HMI_flag.cancel_lev = 0;
      HMI_SaveProcessID(Leveling);
      Title.ShowCaption(GET_TEXT_F(MSG_BED_LEVELING));
      MeshViewer.DrawMeshGrid(GRID_MAX_POINTS_X, GRID_MAX_POINTS_Y);
      DWINUI::Draw_Button(BTN_Cancel, 86, 305, true);
    #elif ENABLED(MESH_BED_LEVELING)
      Draw_AdvancedSettings_Menu();
    #endif

    #if ENABLED(PREHEAT_BEFORE_LEVELING)
      #if HAS_BED_PROBE
        if (!DEBUGGING(DRYRUN)) { probe.preheat_for_probing(LEVELING_NOZZLE_TEMP, HMI_data.BedLevT); }
      #else
        #if HAS_HOTEND
          if (!DEBUGGING(DRYRUN) && thermalManager.degTargetHotend(EXT) < LEVELING_NOZZLE_TEMP) {
            thermalManager.setTargetHotend(LEVELING_NOZZLE_TEMP, 0);
            thermalManager.wait_for_hotend(EXT);
          }
        #endif
        #if HAS_HEATED_BED
          if (!DEBUGGING(DRYRUN) && thermalManager.degTargetBed() < HMI_data.BedLevT) {
            thermalManager.setTargetBed(HMI_data.BedLevT);
            thermalManager.wait_for_bed_heating();
          }
        #endif
      #endif
    #endif
  }

  #if ALL(HAS_MESH, HAS_BED_PROBE)
    void DWIN_LevelingDone() {
      DEBUG_ECHOLNPGM("DWIN_LevelingDone");
        if (HMI_flag.cancel_lev) {
          probe.stow();
          reset_bed_level();
          HMI_ReturnScreen();
          DWIN_UpdateLCD();
          ui.set_status(F("Mesh was cancelled"));
        }
        else {
          Goto_MeshViewer(true);
        }
    }
  #endif
#endif // HAS_LEVELING

#if HAS_MESH
  void DWIN_MeshUpdate(const int8_t cpos, const int8_t tpos, const_float_t zval) {
    ui.set_status(
      &MString<32>(GET_TEXT_F(MSG_PROBING_POINT), ' ', cpos, '/', tpos, F(" Z="), p_float_t(zval, 2))
    );
  }
#endif

// PID/MPC process

#if PROUI_TUNING_GRAPH
  celsius_t _maxtemp, _target;
  void DWIN_Draw_PID_MPC_Popup() {
    constexpr frame_rect_t gfrm = { 30, 150, DWIN_WIDTH - 60, 160 };
    DWINUI::ClearMainArea();
    Draw_Popup_Bkgd();
    // Draw labels, Values
    switch (HMI_value.tempControl) {
      default: return;
      #if ENABLED(MPC_AUTOTUNE)
        case MPCTEMP_START:
          DWINUI::Draw_CenteredString(2,HMI_data.PopupTxt_Color, 70, GET_TEXT_F(MSG_MPC_AUTOTUNE));
          DWINUI::Draw_String(HMI_data.PopupTxt_Color, gfrm.x, gfrm.y - DWINUI::fontHeight() - 4, GET_TEXT_F(MSG_MPC_TARGET));
          DWINUI::Draw_CenteredString(2, HMI_data.PopupTxt_Color, 92, GET_TEXT_F(MSG_FOR_NOZZLE));
          _maxtemp = thermalManager.hotend_max_target(EXT);
          _target = 200;
          break;
      #endif
      #if ENABLED(PIDTEMP)
        case PID_EXTR_START:
          DWINUI::Draw_CenteredString(2, HMI_data.PopupTxt_Color, 70, GET_TEXT_F(MSG_PID_AUTOTUNE));
          DWINUI::Draw_String(HMI_data.PopupTxt_Color, gfrm.x, gfrm.y - DWINUI::fontHeight() - 4, GET_TEXT_F(MSG_PID_TARGET));
          DWINUI::Draw_CenteredString(2, HMI_data.PopupTxt_Color, 92, GET_TEXT_F(MSG_FOR_NOZZLE));
          _maxtemp = thermalManager.hotend_max_target(EXT);
          _target = HMI_data.HotendPidT;
          break;
      #endif
      #if ENABLED(PIDTEMPBED)
        case PID_BED_START:
          DWINUI::Draw_CenteredString(2, HMI_data.PopupTxt_Color, 70, GET_TEXT_F(MSG_PID_AUTOTUNE));
          DWINUI::Draw_String(HMI_data.PopupTxt_Color, gfrm.x, gfrm.y - DWINUI::fontHeight() - 4, GET_TEXT_F(MSG_PID_TARGET));
          DWINUI::Draw_CenteredString(2, HMI_data.PopupTxt_Color, 92, GET_TEXT_F(MSG_FOR_BED));
          _maxtemp = BED_MAX_TARGET;
          _target = HMI_data.BedPidT;
          break;
      #endif
    }
    plot.draw(gfrm, _maxtemp, _target);
    DWINUI::Draw_Int(false, 2, HMI_data.StatusTxt_Color, HMI_data.PopupBg_Color, 3, gfrm.x + 92, gfrm.y - DWINUI::fontHeight() - 6, _target);
  }

  // Plot Temperature Graph (PID Tuning Graph)
  #if ENABLED(PROUI_ITEM_PLOT)

    void dwinDrawPlot(tempcontrol_t result) {
      HMI_value.tempControl = result;
      constexpr frame_rect_t gfrm = { 30, 135, DWIN_WIDTH - 60, 160 };
      DWINUI::ClearMainArea();
      Draw_Popup_Bkgd();
      HMI_SaveProcessID(PlotProcess);

      switch (result) {
        #if ENABLED(MPCTEMP)
          case MPCTEMP_START:
        #elif ENABLED(PIDTEMP)
          case PID_EXTR_START:
        #endif
            Title.ShowCaption(GET_TEXT_F(MSG_HOTEND_TEMP_GRAPH));
            DWINUI::Draw_CenteredString(3, HMI_data.PopupTxt_Color, 75, GET_TEXT_F(MSG_NOZZLE_TEMPERATURE));
            _maxtemp = thermalManager.hotend_max_target(EXT);
            _target = thermalManager.degTargetHotend(EXT);
            break;
        #if ENABLED(PIDTEMPBED)
          case PID_BED_START:
            Title.ShowCaption(GET_TEXT_F(MSG_BED_TEMP_GRAPH));
            DWINUI::Draw_CenteredString(3, HMI_data.PopupTxt_Color, 75, GET_TEXT_F(MSG_BED_TEMPERATURE));
            _maxtemp = BED_MAX_TARGET;
            _target = thermalManager.degTargetBed();
            break;
        #endif
        default:
          break;
      }

      DWIN_Draw_String(false, 2, HMI_data.PopupTxt_Color, HMI_data.PopupBg_Color, gfrm.x, gfrm.y - DWINUI::fontHeight() - 4, GET_TEXT_F(MSG_TARGET));
      plot.draw(gfrm, _maxtemp, _target);
      DWINUI::Draw_Int(false, 2, HMI_data.StatusTxt_Color, HMI_data.PopupBg_Color, 3, gfrm.x + 80, gfrm.y - DWINUI::fontHeight() - 4, _target);
      DWINUI::Draw_Button(BTN_Continue, 86, 305, true);
    }

    void drawHPlot() {
      TERN_(PIDTEMP, dwinDrawPlot(PID_EXTR_START);)
      TERN_(MPCTEMP, dwinDrawPlot(MPCTEMP_START);)
    }
    void drawBPlot() {
      TERN_(PIDTEMPBED, dwinDrawPlot(PID_BED_START);)
    }

  #endif // PROUI_ITEM_PLOT
#endif // PROUI_TUNING_GRAPH

#if HAS_PID_HEATING

  void DWIN_M303(const bool seenC, const int c, const bool seenS, const heater_id_t hid, const celsius_t temp) {
    if (seenC) { HMI_data.PidCycles = c; }
    if (seenS) {
      switch (hid) {
        OPTCODE(PIDTEMP,    case 0 ... HOTENDS - 1: HMI_data.HotendPidT = temp; break)
        OPTCODE(PIDTEMPBED, case H_BED:             HMI_data.BedPidT = temp;    break)
        default: break;
      }
    }
  }

  void DWIN_PidTuning(tempcontrol_t result) {
    HMI_value.tempControl = result;
    switch (result) {
      #if ENABLED(PIDTEMPBED)
        case PID_BED_START:
          HMI_SaveProcessID(PidProcess);
          #if PROUI_TUNING_GRAPH
            DWIN_Draw_PID_MPC_Popup();
          #else
            DWIN_Draw_Popup(ICON_TempTooHigh, GET_TEXT_F(MSG_PID_AUTOTUNE), GET_TEXT_F(MSG_BED_RUN));
          #endif
          break;
      #endif
      #if ENABLED(PIDTEMP)
        case PID_EXTR_START:
          HMI_SaveProcessID(PidProcess);
          #if PROUI_TUNING_GRAPH
            DWIN_Draw_PID_MPC_Popup();
          #else
            DWIN_Draw_Popup(ICON_TempTooHigh, GET_TEXT_F(MSG_PID_AUTOTUNE), GET_TEXT_F(MSG_NOZZLE_RUN));
          #endif
          break;
      #endif
      case PID_BAD_HEATER_ID:
        checkkey = last_checkkey;
        DWIN_Popup_Continue(ICON_TempTooLow, GET_TEXT_F(MSG_PID_AUTOTUNE_FAILED), GET_TEXT_F(MSG_BAD_HEATER_ID));
        break;
      case PID_TUNING_TIMEOUT:
        checkkey = last_checkkey;
        DWIN_Popup_Continue(ICON_TempTooHigh, GET_TEXT_F(MSG_ERROR), GET_TEXT_F(MSG_PID_TIMEOUT));
        break;
      case PID_TEMP_TOO_HIGH:
        checkkey = last_checkkey;
        DWIN_Popup_Continue(ICON_TempTooHigh, GET_TEXT_F(MSG_PID_AUTOTUNE_FAILED), GET_TEXT_F(MSG_TEMP_TOO_HIGH));
        break;
      case AUTOTUNE_DONE:
        checkkey = last_checkkey;
        DWIN_Popup_Continue(ICON_TempTooLow, GET_TEXT_F(MSG_PID_AUTOTUNE), GET_TEXT_F(MSG_BUTTON_DONE));
        break;
      default:
        checkkey = last_checkkey;
        DWIN_Popup_Continue(ICON_Info_0, GET_TEXT_F(MSG_ERROR), GET_TEXT_F(MSG_STOPPING));
        break;
    }
  }

#endif // HAS_PID_HEATING

#if ENABLED(MPC_AUTOTUNE)

  void DWIN_MPCTuning(tempcontrol_t result) {
    HMI_value.tempControl = result;
    switch (result) {
      case MPCTEMP_START:
        HMI_SaveProcessID(MPCProcess);
        #if PROUI_TUNING_GRAPH
          DWIN_Draw_PID_MPC_Popup();
        #else
          DWIN_Draw_Popup(ICON_TempTooHigh, GET_TEXT_F(MSG_MPC_AUTOTUNE), GET_TEXT_F(MSG_NOZZLE_RUN));
        #endif
        break;
      case MPC_TEMP_ERROR:
        checkkey = last_checkkey;
        DWIN_Popup_Continue(ICON_TempTooHigh, GET_TEXT_F(MSG_PID_AUTOTUNE_FAILED), F(STR_MPC_TEMPERATURE_ERROR));
        ui.reset_alert_level();
        break;
      case MPC_INTERRUPTED:
        checkkey = last_checkkey;
        DWIN_Popup_Continue(ICON_TempTooHigh, GET_TEXT_F(MSG_ERROR), F(STR_MPC_AUTOTUNE_INTERRUPTED));
        ui.reset_alert_level();
        break;
      case AUTOTUNE_DONE:
        checkkey = last_checkkey;
        DWIN_Popup_Continue(ICON_TempTooLow, GET_TEXT_F(MSG_MPC_AUTOTUNE), GET_TEXT_F(MSG_BUTTON_DONE));
        ui.reset_alert_level();
        break;
      default:
        checkkey = last_checkkey;
        ui.reset_alert_level();
        break;
    }
  }

#endif // MPC_AUTOTUNE

// Started a Print Job
void DWIN_Print_Started() {
  DEBUG_ECHOLNPGM("DWIN_Print_Started: ", SD_Printing());
  TERN_(HAS_GCODE_PREVIEW, if (Host_Printing()) { preview.invalidate(); })
  ui.progress_reset();
  ui.reset_remaining_time();
  HMI_flag.pause_flag = false;
  HMI_flag.abort_flag = false;
  select_print.reset();
  #if ALL(PROUI_EX, HAS_GCODE_PREVIEW)
    if (!fileprop.isConfig)
  #endif
  Goto_PrintProcess();
}

// Pause a print job
void DWIN_Print_Pause() {
  DEBUG_ECHOLNPGM("DWIN_Print_Pause");
  ICON_ResumeOrPause();
}

// Resume print job
void DWIN_Print_Resume() {
  DEBUG_ECHOLNPGM("DWIN_Print_Resume");
  ICON_ResumeOrPause();
  LCD_MESSAGE(MSG_RESUME_PRINT);
}

// Ended print job
void DWIN_Print_Finished() {
  DEBUG_ECHOLNPGM("DWIN_Print_Finished");
  if (all_axes_homed()) {
    #ifdef SD_FINISHED_RELEASECOMMAND
      queue.inject(F(SD_FINISHED_RELEASECOMMAND));
    #else
      const int16_t zpos = current_position.z + TERN(NOZZLE_PARK_FEATURE,
      NOZZLE_PARK_Z_RAISE_MIN, Z_POST_CLEARANCE);
      _MIN(zpos, Z_MAX_POS);
      const int16_t ypos = TERN(NOZZLE_PARK_FEATURE, TERN(PROUI_EX, PRO_data.Park_point.y, DEF_NOZZLE_PARK_POINT.y), Y_MAX_POS);
      MString<32> cmd;
      cmd.setf(cmd, F("G0F3000Z%i\nG0F2000Y%i"), zpos, ypos);
      queue.inject(&cmd);
    #endif
  }
  if (!HMI_flag.abort_flag) {
    set_bed_leveling_enabled(false);
    DisableMotors();
  }
  HMI_flag.abort_flag = false;
  HMI_flag.pause_flag = false;
  wait_for_heatup = false;
  #if ALL(PROUI_EX, HAS_GCODE_PREVIEW)
    if (!fileprop.isConfig)
  #endif
  Goto_PrintDone();
  OPTCODE(HAS_GCODE_PREVIEW, fileprop.clears())
}

// Print was aborted
void DWIN_Print_Aborted() {
  DEBUG_ECHOLNPGM("DWIN_Print_Aborted");
  RaiseHead();
  #ifdef EVENT_GCODE_SD_ABORT
    queue.inject(F(EVENT_GCODE_SD_ABORT));
  #endif
  TERN_(HOST_PROMPT_SUPPORT, hostui.notify(GET_TEXT_F(MSG_PRINT_ABORTED)));
  LCD_MESSAGE_F("Print Aborted");
  DWIN_Print_Finished();
}

#if (ALT_COLOR_MENU == 1) // 1 = Alternate Aquila
  void DWIN_SetColorDefaults() {
    HMI_data.Background_Color = Def_Background_Color;
    HMI_data.Cursor_Color     = Def_Cursor_Color;
    HMI_data.TitleBg_Color    = Def_TitleBg_Color;
    HMI_data.TitleTxt_Color   = Def_TitleTxt_Color;
    HMI_data.Text_Color       = Def_Text_Color;
    HMI_data.Selected_Color   = Def_Selected_Color;
    HMI_data.SplitLine_Color  = Def_SplitLine_Color;
    HMI_data.Highlight_Color  = Def_Highlight_Color;
    HMI_data.StatusBg_Color   = Def_StatusBg_Color;
    HMI_data.StatusTxt_Color  = Def_StatusTxt_Color;
    HMI_data.PopupBg_Color    = Def_PopupBg_Color;
    HMI_data.PopupTxt_Color   = Def_PopupTxt_Color;
    HMI_data.AlertBg_Color    = Def_AlertBg_Color;
    HMI_data.AlertTxt_Color   = Def_AlertTxt_Color;
    HMI_data.PercentTxt_Color = Def_PercentTxt_Color;
    HMI_data.Barfill_Color    = Def_Barfill_Color;
    HMI_data.Indicator_Color  = Def_Indicator_Color;
    HMI_data.Coordinate_Color = Def_Coordinate_Color;
    HMI_data.Bottom_Color     = Def_Bottom_Color;
  }
#elif (ALT_COLOR_MENU == 2) // 2 = Ender3V2 Default
  void DWIN_SetColorDefaults() {
    HMI_data.Background_Color = Def_Background_Color;
    HMI_data.Cursor_Color     = RGB(20, 49, 31); // Grey
    HMI_data.TitleBg_Color    = Def_TitleBg_Color;
    HMI_data.TitleTxt_Color   = Def_TitleTxt_Color;
    HMI_data.Text_Color       = Def_Text_Color;
    HMI_data.Selected_Color   = RGB(6, 29, 27); // Royal Blue
    HMI_data.SplitLine_Color  = RGB(0, 23, 16); // Orient Blue
    HMI_data.Highlight_Color  = Def_Highlight_Color;
    HMI_data.StatusBg_Color   = RGB(0, 23, 16); // Orient Blue
    HMI_data.StatusTxt_Color  = RGB(31, 63, 0); // Yellow
    HMI_data.PopupBg_Color    = Color_Bg_Window;
    HMI_data.PopupTxt_Color   = Popup_Text_Color;
    HMI_data.AlertBg_Color    = RGB(30, 0, 15); // BG_Red
    HMI_data.AlertTxt_Color   = RGB(31, 63, 0); // Yellow
    HMI_data.PercentTxt_Color = RGB(31, 49, 9); // Dark_Yellow
    HMI_data.Barfill_Color    = BarFill_Color;
    HMI_data.Indicator_Color  = Color_White;
    HMI_data.Coordinate_Color = Color_White;
    HMI_data.Bottom_Color     = RGB(0, 23, 16); // Orient Blue
  }
#else // 0 = Voxelab Default
  void DWIN_SetColorDefaults() {
    HMI_data.Background_Color = Def_Background_Color;
    HMI_data.Cursor_Color     = Def_Text_Color;
    HMI_data.TitleBg_Color    = Def_TitleBg_Color;
    HMI_data.TitleTxt_Color   = Def_TitleTxt_Color;
    HMI_data.Text_Color       = Def_Text_Color;
    HMI_data.Selected_Color   = Def_Selected_Color;
    HMI_data.SplitLine_Color  = Def_SplitLine_Color;
    HMI_data.Highlight_Color  = Def_Highlight_Color;
    HMI_data.StatusBg_Color   = Def_StatusBg_Color;
    HMI_data.StatusTxt_Color  = Def_StatusTxt_Color;
    HMI_data.PopupBg_Color    = Def_PopupBg_Color;
    HMI_data.PopupTxt_Color   = Def_PopupTxt_Color;
    HMI_data.AlertBg_Color    = Def_AlertBg_Color;
    HMI_data.AlertTxt_Color   = Def_AlertTxt_Color;
    HMI_data.PercentTxt_Color = Def_PercentTxt_Color;
    HMI_data.Barfill_Color    = Def_Barfill_Color;
    HMI_data.Indicator_Color  = Def_Text_Color;
    HMI_data.Coordinate_Color = Def_Text_Color;
    HMI_data.Bottom_Color     = Def_TitleBg_Color;
  }
#endif

void DWIN_SetDataDefaults() {
  DEBUG_ECHOLNPGM("DWIN_SetDataDefaults");
  DWIN_SetColorDefaults();
  DWINUI::SetColors(HMI_data.Text_Color, HMI_data.Background_Color, HMI_data.TitleBg_Color);
  TERN_(PIDTEMP, HMI_data.HotendPidT = DEF_HOTENDPIDT;)
  TERN_(PIDTEMPBED, HMI_data.BedPidT = DEF_BEDPIDT;)
  TERN_(HAS_PID_HEATING, HMI_data.PidCycles = DEF_PIDCYCLES;)
  #if ENABLED(PREVENT_COLD_EXTRUSION)
    HMI_data.ExtMinT = EXTRUDE_MINTEMP;
    ApplyExtMinT();
  #endif
  #if ALL(HAS_HEATED_BED, PREHEAT_BEFORE_LEVELING)
    HMI_data.BedLevT = LEVELING_BED_TEMP;
  #endif
  TERN_(BAUD_RATE_GCODE, HMI_data.Baud250K = (BAUDRATE == 250000);)
  HMI_data.CalcAvg = true;
  TERN_(SHOW_SPEED_IND, HMI_data.SpdInd = false;)
  HMI_data.FullManualTramming = false;
  #if ENABLED(PROUI_MEDIASORT)
    HMI_data.MediaSort = true;
    card.setSortOn(TERN(SDSORT_REVERSE, AS_REV, AS_FWD));
  #else
    card.setSortOn(TERN(SDSORT_REVERSE, AS_REV, AS_FWD));
  #endif
  HMI_data.MediaAutoMount = DISABLED(PROUI_EX);
  #if ALL(INDIVIDUAL_AXIS_HOMING_SUBMENU, MESH_BED_LEVELING)
    HMI_data.z_after_homing = DEF_Z_AFTER_HOMING;
  #endif
  IF_DISABLED(HAS_BED_PROBE, HMI_data.ManualZOffset = 0;)
  #if ALL(LED_CONTROL_MENU, HAS_COLOR_LEDS)
    #if ENABLED(LED_COLOR_PRESETS)
      leds.set_default();
      ApplyLEDColor();
    #else
      HMI_data.Led_Color = Def_Leds_Color;
      leds.set_color(
        (HMI_data.Led_Color >> 16) & 0xFF,
        (HMI_data.Led_Color >>  8) & 0xFF,
        (HMI_data.Led_Color >>  0) & 0xFF
        OPTARG(HAS_WHITE_LED, (HMI_data.Led_Color >> 24) & 0xFF)
      );
    #endif
  #endif
  TERN_(ADAPTIVE_STEP_SMOOTHING, HMI_data.AdaptiveStepSmoothing = true;)
  TERN_(HAS_GCODE_PREVIEW, HMI_data.EnablePreview = true;)
  #if PROUI_EX
    PRO_data.x_bed_size = DEF_X_BED_SIZE;
    PRO_data.y_bed_size = DEF_Y_BED_SIZE;
    PRO_data.x_min_pos  = DEF_X_MIN_POS;
    PRO_data.y_min_pos  = DEF_Y_MIN_POS;
    PRO_data.x_max_pos  = DEF_X_MAX_POS;
    PRO_data.y_max_pos  = DEF_Y_MAX_POS;
    PRO_data.z_max_pos  = DEF_Z_MAX_POS;
    TERN_(HAS_MESH, PRO_data.grid_max_points = DEF_GRID_MAX_POINTS;)
    #if HAS_BED_PROBE
      PRO_data.zprobefeedslow = DEF_Z_PROBE_FEEDRATE_SLOW;
      IF_DISABLED(BD_SENSOR, PRO_data.multiple_probing = MULTIPLE_PROBING;)
    #endif
    TERN_(HAS_EXTRUDERS, PRO_data.Invert_E0 = DEF_INVERT_E0_DIR;)
    TERN_(NOZZLE_PARK_FEATURE, PRO_data.Park_point = DEF_NOZZLE_PARK_POINT;)
    #if HAS_FILAMENT_SENSOR
      PRO_data.Runout_active_state = FIL_RUNOUT_STATE;
      PRO_data.FilamentMotionSensor = DEF_FIL_MOTION_SENSOR;
    #endif
    PRO_data.hotend_maxtemp = HEATER_0_MAXTEMP;
    #if HAS_TOOLBAR
      const uint8_t _def[] = DEF_TBOPT;
      for (uint8_t i = 0; i < TBMaxOpt; ++i) PRO_data.TBopt[i] = _def[i];
    #endif
    ProEx.SetData();
  #else
    #if HAS_BED_PROBE
      HMI_data.zprobeFeed = DEF_Z_PROBE_FEEDRATE_SLOW;
      IF_DISABLED(BD_SENSOR, HMI_data.multiple_probing = MULTIPLE_PROBING;)
    #endif
    TERN_(PROUI_GRID_PNTS, HMI_data.grid_max_points = DEF_GRID_MAX_POINTS;)
  #endif
}

void DWIN_CopySettingsTo(char * const buff) {
  DEBUG_ECHOLNPGM("DWIN_CopySettingsTo");
  DEBUG_ECHOLNPGM("HMI_data: ", sizeof(HMI_data_t));
  memcpy(buff, &HMI_data, sizeof(HMI_data_t));
  #if PROUI_EX
    DEBUG_ECHOLNPGM("PRO_data: ", sizeof(PRO_data_t));
    memcpy(buff + sizeof(HMI_data_t), &PRO_data, sizeof(PRO_data_t));
  #endif
}

void DWIN_CopySettingsFrom(const char * const buff) {
  DEBUG_ECHOLNPGM("DWIN_CopySettingsFrom");
  memcpy(&HMI_data, buff, sizeof(HMI_data_t));
  #if PROUI_EX
    memcpy(&PRO_data, buff + sizeof(HMI_data_t), sizeof(PRO_data_t));
    ProEx.LoadSettings();
  #endif
  DWINUI::SetColors(HMI_data.Text_Color, HMI_data.Background_Color, HMI_data.TitleBg_Color);
  TERN_(PREVENT_COLD_EXTRUSION, ApplyExtMinT();)
  feedrate_percentage = 100;
  TERN_(BAUD_RATE_GCODE, if (HMI_data.Baud250K) { SetBaud250K(); } else { SetBaud115K(); })
  TERN_(PROUI_MEDIASORT, card.setSortOn(HMI_data.MediaSort ? TERN(SDSORT_REVERSE, AS_REV, AS_FWD) : AS_OFF);)
  #if ALL(LED_CONTROL_MENU, HAS_COLOR_LEDS)
    leds.set_color(
      (HMI_data.Led_Color >> 16) & 0xFF,
      (HMI_data.Led_Color >>  8) & 0xFF,
      (HMI_data.Led_Color >>  0) & 0xFF
      OPTARG(HAS_WHITE_LED, (HMI_data.Led_Color >> 24) & 0xFF)
    );
    leds.update();
  #endif
}

// Initialize or re-initialize the LCD
void MarlinUI::init_lcd() {
  DEBUG_ECHOLNPGM("MarlinUI::init_lcd");
  delay(750);   // wait to wakeup screen
  const bool hs = DWIN_Handshake(); UNUSED(hs);
  #if ENABLED(DEBUG_DWIN)
    DEBUG_ECHOPGM("DWIN_Handshake ");
    DEBUG_ECHOLN(hs ? F("ok.") : F("error."));
  #endif
  DWIN_Frame_SetDir(1);
  DWIN_UpdateLCD();
  encoderConfiguration();
}

void Init(){

  uint16_t uVar2;
  uint16_t uVar3;
  uint16_t uVar4;

  uVar4 = 25;
  uVar2 = 65;

  do {
    DWIN_Draw_Box(1, 0, uVar2, uVar2 + 200, uVar4 + 117, uVar4);
    uVar3 = uVar2 - 5;
    DWIN_Draw_Box(0, Rectangle_Color, uVar2, uVar2 + 200, uVar4 + 117, uVar4);
    DWIN_UpdateLCD();
    uVar4 += 10;
    safe_delay(20);
    uVar2 = uVar3;
  } while (uVar3 != 15);
  char ver[25];
  sprintf(ver, "Version: %s", SHORT_BUILD_VERSION);
  DWINUI::Draw_CenteredString(2, Color_Cyan, 230, TERN(PROUI_EX, F("MRiscoC ProUI"), F("MRiscoC NoPro"))) ;
  DWINUI::Draw_CenteredString((fontid_t)2, Color_White, 260, F(ver));
  DWINUI::Draw_CenteredString(false, 1, Color_White, DWINUI::backcolor, 280, DateTime);
  DWINUI::Draw_CenteredString(2, 0xffe0, 305, F("ClassicRocker883"));
  DWIN_UpdateLCD();
  safe_delay(300);
}

void DWIN_InitScreen() {
  DEBUG_ECHOLNPGM("DWIN_InitScreen");
  HMI_Init();
  DWIN_UpdateLCD();
  #if PROUI_EX
    ProEx.Init();
  #endif
  Init();
  safe_delay(2000);
  DWINUI::init();
  DWINUI::onTitleDraw = Draw_Title;
  InitMenu();
  checkkey = 255;
  hash_changed = true;
  DWIN_DrawStatusLine("");
  DWIN_Draw_Dashboard();
  Goto_Main_Menu();
  #if ENABLED(AUTO_BED_LEVELING_UBL)
    UBLMeshLoad();
  #elif ENABLED(AUTO_BED_LEVELING_BILINEAR)
    //bedLevelTools.mesh_reset();
    (void)settings.load();
  #endif
  #if ENABLED(LASER_SYNCHRONOUS_M106_M107)  // Zero fans speed at boot with LASER_SYNCHRONOUS_M106_M107
    thermalManager.zero_fan_speeds();
    planner.buffer_sync_block(BLOCK_BIT_SYNC_FANS);
  #endif
  LCD_MESSAGE(WELCOME_MSG);
}

void MarlinUI::update() {
  HMI_SDCardUpdate();   // SD card update
  EachMomentUpdate();   // Status update
  DWIN_HandleScreen();  // Rotary encoder update
}

void MarlinUI::refresh() { /* Nothing to see here */ }

#if HAS_LCD_BRIGHTNESS
  void MarlinUI::_set_brightness() { DWIN_LCD_Brightness(backlight ? brightness : 0); }
#endif

void MarlinUI::kill_screen(FSTR_P const lcd_error, FSTR_P const) {
  DWIN_Draw_Popup(TERN(TJC_DISPLAY, ICON_BLTouch, ICON_Printer_0), GET_TEXT_F(MSG_PRINTER_KILLED), lcd_error);
  DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 270, GET_TEXT_F(MSG_TURN_OFF));
  DWIN_UpdateLCD();
}

void DWIN_RebootScreen() {
  DWIN_Frame_Clear(Color_Black);
  DWIN_JPG_ShowAndCache(0);
  DWINUI::Draw_CenteredString(Color_White, 220, GET_TEXT_F(MSG_PLEASE_WAIT_REBOOT));
  DWIN_UpdateLCD();
  safe_delay(500);
}

void DWIN_RedrawDash() {
  hash_changed = true;
  DWIN_DrawStatusMessage();
  DWIN_Draw_Dashboard();
}

void DWIN_RedrawScreen() {
  Draw_Main_Area();
  DWIN_RedrawDash();
}

#if ENABLED(ADVANCED_PAUSE_FEATURE)

  void DWIN_Popup_Pause(FSTR_P const fmsg, uint8_t button/*=0*/) {
    HMI_SaveProcessID(button ? WaitResponse : NothingToDo);
    DWIN_Show_Popup(ICON_Pause_1, GET_TEXT_F(MSG_ADVANCED_PAUSE), fmsg, button);
  }

  void MarlinUI::pause_show_message(const PauseMessage message, const PauseMode mode/*=PAUSE_MODE_SAME*/, const uint8_t extruder/*=EXT*/) {
    pause_mode = mode;
    switch (message) {
      case PAUSE_MESSAGE_PARKING:  DWIN_Popup_Pause(GET_TEXT_F(MSG_PAUSE_PRINT_PARKING));    break;                // M125
      case PAUSE_MESSAGE_CHANGING: DWIN_Popup_Pause(GET_TEXT_F(MSG_FILAMENT_CHANGE_INIT));   break;                // pause_print (M125, M600)
      case PAUSE_MESSAGE_WAITING:  DWIN_Popup_Pause(GET_TEXT_F(MSG_ADVANCED_PAUSE_WAITING), BTN_Continue); break;
      case PAUSE_MESSAGE_UNLOAD:   DWIN_Popup_Pause(GET_TEXT_F(MSG_FILAMENT_CHANGE_UNLOAD)); break;                // Unload of pause and Unload of M702
      case PAUSE_MESSAGE_INSERT:   DWIN_Popup_Pause(GET_TEXT_F(MSG_FILAMENT_CHANGE_INSERT), BTN_Continue); break;
      case PAUSE_MESSAGE_LOAD:     DWIN_Popup_Pause(GET_TEXT_F(MSG_FILAMENT_CHANGE_LOAD));   break;
      case PAUSE_MESSAGE_PURGE:    DWIN_Popup_Pause(GET_TEXT_F(TERN(ADVANCED_PAUSE_CONTINUOUS_PURGE, MSG_FILAMENT_CHANGE_CONT_PURGE, MSG_FILAMENT_CHANGE_PURGE))); break;
      case PAUSE_MESSAGE_OPTION:   Goto_FilamentPurge(); break;
      case PAUSE_MESSAGE_RESUME:   DWIN_Popup_Pause(GET_TEXT_F(MSG_FILAMENT_CHANGE_RESUME)); break;
      case PAUSE_MESSAGE_STATUS:   HMI_ReturnScreen(); break;                                                      // Exit from Pause, Load and Unload
      case PAUSE_MESSAGE_HEAT:     DWIN_Popup_Pause(GET_TEXT_F(MSG_FILAMENT_CHANGE_HEAT), BTN_Continue);   break;
      case PAUSE_MESSAGE_HEATING:  DWIN_Popup_Pause(GET_TEXT_F(MSG_FILAMENT_CHANGE_HEATING)); break;
      default: break;
    }
  }

  void Draw_Popup_FilamentPurge() {
    DWIN_Draw_Popup(ICON_AutoLeveling, GET_TEXT_F(MSG_ADVANCED_PAUSE), GET_TEXT_F(MSG_FILAMENT_CHANGE_PURGE_CONTINUE));
    DWINUI::Draw_Button(BTN_Purge, 26, 280);
    DWINUI::Draw_Button(BTN_Continue, 146, 280);
    Draw_Select_Highlight(true);
  }

  void OnClick_FilamentPurge() {
    if (HMI_flag.select_flag) {
      pause_menu_response = PAUSE_RESPONSE_EXTRUDE_MORE;  // "Purge More" button
    }
    else {
      HMI_SaveProcessID(NothingToDo);
      pause_menu_response = PAUSE_RESPONSE_RESUME_PRINT;  // "Continue" button
    }
  }

  void Goto_FilamentPurge() {
    pause_menu_response = PAUSE_RESPONSE_WAIT_FOR;
    Goto_Popup(Draw_Popup_FilamentPurge, OnClick_FilamentPurge);
  }

#endif // ADVANCED_PAUSE_FEATURE

#if HAS_MESH

  void DWIN_MeshViewer() {
    if (!leveling_is_valid()) {
      DWIN_Popup_Continue(ICON_Leveling_1, GET_TEXT_F(MSG_MESH_VIEWER), GET_TEXT_F(MSG_NO_VALID_MESH));
    }
    else {
      HMI_SaveProcessID(WaitResponse);
      MeshViewer.Draw(false, true);
    }
  }

#endif // HAS_MESH


#if HAS_LOCKSCREEN

  void DWIN_LockScreen() {
    if (checkkey != Locked) {
      lockScreen.rprocess = checkkey;
      checkkey = Locked;
      lockScreen.init();
    }
  }

  void DWIN_UnLockScreen() {
    if (checkkey == Locked) {
      checkkey = lockScreen.rprocess;
      Draw_Main_Area();
    }
  }

  void HMI_LockScreen() {
    EncoderState encoder_diffState = get_encoder_state();
    if (encoder_diffState == ENCODER_DIFF_NO) return;
    lockScreen.onEncoder(encoder_diffState);
    if (lockScreen.isUnlocked()) { DWIN_UnLockScreen(); }
  }

#endif // HAS_LOCKSCREEN

#if HAS_GCODE_PREVIEW

  void SetPreview() { Toggle_Chkb_Line(HMI_data.EnablePreview); }

  void OnClick_ConfirmToPrint() {
    DWIN_ResetStatusLine();

    if (HMI_flag.select_flag) {     // Confirm
      return card.openAndPrintFile(card.filename);
    }
    else {
      HMI_ReturnScreen();
    }
  }

#endif

void Goto_ConfirmToPrint() {
  #if ENABLED(CV_LASER_MODULE)
    if (fileprop.isConfig) return card.openAndPrintFile(card.filename);
    if (fileprop.isLaser) {
      if (laser_device.is_laser_device())
        return Draw_LaserPrint_Menu();
      else
        return Draw_LaserSettings_Menu();
    }
    else
      LaserOn(false); // If it is not laser file turn off laser mode
  #endif
  #if HAS_GCODE_PREVIEW
    if (HMI_data.EnablePreview) return Goto_Popup(preview.drawFromSD, OnClick_ConfirmToPrint);
  #endif
  card.openAndPrintFile(card.filename); // Direct print SD file
}

#if HAS_ESDIAG
  void Draw_EndStopDiag() {
    HMI_SaveProcessID(ESDiagProcess);
    esDiag.draw();
  }
#endif

//=============================================================================
// MENU SUBSYSTEM
//=============================================================================

// Tool functions

#if ENABLED(EEPROM_SETTINGS)

  void WriteEeprom() {
    DWIN_DrawStatusLine(GET_TEXT_F(MSG_STORE_EEPROM));
    safe_delay(500);
    DWIN_UpdateLCD();
    DONE_BUZZ(settings.save());
  }

  void ReadEeprom() {
    const bool success = settings.load();
    DWIN_RedrawScreen();
    DONE_BUZZ(success);
  }

  void ResetEeprom() {
    settings.reset();
    DWIN_RedrawScreen();
    DONE_BUZZ(true);
  }

  #if HAS_MESH
    void SaveMesh() {
      TERN_(MESH_BED_LEVELING, ManualMeshSave();)
      TERN(AUTO_BED_LEVELING_UBL, UBLMeshSave(), WriteEeprom());
    }
  #endif

#endif

// Reset Printer
void RebootPrinter() {
  wait_for_heatup = wait_for_user = false;    // Stop waiting for heating/user
  thermalManager.disable_all_heaters();
  planner.finish_and_disable();
  DWIN_RebootScreen();
  hal.reboot();
}

void Goto_Info_Menu() {
  Draw_Info_Menu();
  DWIN_UpdateLCD();
  HMI_SaveProcessID(WaitResponse);
}

void DisableMotors() { queue.inject(F("M84")); }

void AutoHome() { queue.inject_P(G28_STR); }

#if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
  void HomeX() { queue.inject(F("G28X")); }
  void HomeY() { queue.inject(F("G28Y")); }
  void HomeZ() { queue.inject(F("G28Z")); }
  #if ALL(INDIVIDUAL_AXIS_HOMING_SUBMENU, MESH_BED_LEVELING)
    void ApplyZAfterHoming() { HMI_data.z_after_homing = MenuData.Value; }
    void SetZAfterHoming() { SetIntOnClick(0, 20, HMI_data.z_after_homing, ApplyZAfterHoming); }
  #endif
#endif

#if HAS_HOME_OFFSET
  // Apply workspace offset, making the current position 0,0,0
  void SetHome() {
    queue.inject(F("G92X0Y0Z0"));
    DONE_BUZZ(true);
  }
#endif

#if HAS_ZOFFSET_ITEM

  void ApplyZOffset() { TERN_(EEPROM_SETTINGS, settings.save()); }
  void LiveZOffset() {
    #if ANY(BABYSTEP_ZPROBE_OFFSET, JUST_BABYSTEP)
      const_float_t step_zoffset = round((MenuData.Value / 100.0f) * planner.settings.axis_steps_per_mm[Z_AXIS]) - babystep.accum;
      if (BABYSTEP_ALLOWED()) { babystep.add_steps(Z_AXIS, step_zoffset); }
    #endif
  }
  void SetZOffset() {
    #if ANY(BABYSTEP_ZPROBE_OFFSET, JUST_BABYSTEP)
      babystep.accum = round(planner.settings.axis_steps_per_mm[Z_AXIS] * BABY_Z_VAR);
    #endif
    SetPFloatOnClick(PROBE_OFFSET_ZMIN, PROBE_OFFSET_ZMAX, 2, ApplyZOffset, LiveZOffset);
  }

  void SetMoveZto0() {
    TERN_(HAS_LEVELING, set_bed_leveling_enabled(false));
    gcode.process_subcommands_now(TS(F("G28XYO\nG28Z\nG0F5000X"), X_CENTER, F("Y"), Y_CENTER, F("\nG0Z0F300\nM400")));
    ui.reset_status();
  }

  #if !HAS_BED_PROBE
    void HomeZandDisable() {
      HomeZ();
      DisableMotors();
    }
  #endif

#endif // HAS_ZOFFSET_ITEM

#if HAS_PREHEAT
  #define _DoPreheat(N) void DoPreheat##N() { ui.preheat_all(N-1); }
  REPEAT_1(PREHEAT_COUNT, _DoPreheat)
#endif

void DoCoolDown() { thermalManager.cooldown(); }

bool EnableLiveMove = false;
void SetLiveMove() { Toggle_Chkb_Line(EnableLiveMove); }
void AxisMove(AxisEnum axis) {
  #if HAS_HOTEND
    if (axis == E_AXIS && thermalManager.tooColdToExtrude(EXT)) {
      gcode.process_subcommands_now(F("G92E0"));  // reset extruder position
      return DWIN_Popup_Continue(ICON_TempTooLow, GET_TEXT_F(MSG_HOTEND_TOO_COLD), GET_TEXT_F(MSG_PLEASE_PREHEAT));
    }
  #endif
  planner.synchronize();
  if (!planner.is_full()) { planner.buffer_line(current_position, manual_feedrate_mm_s[axis]); }
}
void LiveMove() {
  if (!EnableLiveMove) return;
  *MenuData.P_Float = MenuData.Value / MINUNITMULT;
  AxisMove(HMI_value.axis);
}
void ApplyMove() {
  if (EnableLiveMove) return;
  AxisMove(HMI_value.axis);
}

#if ENABLED(CV_LASER_MODULE)
  void SetMoveX() {
    HMI_value.axis = X_AXIS;
    if (!laser_device.is_laser_device()) { SetPFloatOnClick(X_MIN_POS, X_MAX_POS, UNITFDIGITS, ApplyMove, LiveMove); }
    else { SetPFloatOnClick(X_MIN_POS - laser_device.homepos.x, X_MAX_POS - laser_device.homepos.x, UNITFDIGITS, ApplyMove, LiveMove); }
  }
  void SetMoveY() {
    HMI_value.axis = Y_AXIS;
    if (!laser_device.is_laser_device()) SetPFloatOnClick(Y_MIN_POS, Y_MAX_POS, UNITFDIGITS, ApplyMove, LiveMove);
    else { SetPFloatOnClick(Y_MIN_POS - laser_device.homepos.y, Y_MAX_POS - laser_device.homepos.y, UNITFDIGITS, ApplyMove, LiveMove); }
  }
  void SetMoveZ() { HMI_value.axis = Z_AXIS; SetPFloatOnClick(laser_device.is_laser_device() ? -Z_MAX_POS : Z_MIN_POS, Z_MAX_POS, UNITFDIGITS, ApplyMove, LiveMove); }
#else
  void SetMoveX() { HMI_value.axis = X_AXIS; SetPFloatOnClick(X_MIN_POS, X_MAX_POS, UNITFDIGITS, ApplyMove, LiveMove); }
  void SetMoveY() { HMI_value.axis = Y_AXIS; SetPFloatOnClick(Y_MIN_POS, Y_MAX_POS, UNITFDIGITS, ApplyMove, LiveMove); }
  void SetMoveZ() { HMI_value.axis = Z_AXIS; SetPFloatOnClick(Z_MIN_POS, Z_MAX_POS, UNITFDIGITS, ApplyMove, LiveMove); }
#endif

#if HAS_HOTEND
  void SetMoveE() {
    #define E_MIN_POS (current_position.e - (EXTRUDE_MAXLENGTH))
    #define E_MAX_POS (current_position.e + (EXTRUDE_MAXLENGTH))
    HMI_value.axis = E_AXIS; SetPFloatOnClick(E_MIN_POS, E_MAX_POS, UNITFDIGITS, ApplyMove, LiveMove);
  }
#endif

#if ENABLED(POWER_LOSS_RECOVERY)
  void SetPwrLossr() {
    Toggle_Chkb_Line(recovery.enabled);
    recovery.changed();
  }
#endif

#if ENABLED(BAUD_RATE_GCODE)
  void SetBaud115K() { queue.inject(F("M575B115")); }
  void SetBaud250K() { queue.inject(F("M575B250")); }
  void SetBaudRate() {
    Toggle_Chkb_Line(HMI_data.Baud250K);
    if (HMI_data.Baud250K) { SetBaud250K(); } else { SetBaud115K(); }
  }
#endif

#if HAS_LCD_BRIGHTNESS
  void ApplyBrightness() { ui.set_brightness(MenuData.Value); }
  void LiveBrightness() { DWIN_LCD_Brightness(MenuData.Value); }
  void SetBrightness() { SetIntOnClick(LCD_BRIGHTNESS_MIN, LCD_BRIGHTNESS_MAX, ui.brightness, ApplyBrightness, LiveBrightness); }
  void TurnOffBacklight() { HMI_SaveProcessID(WaitResponse); ui.set_brightness(0); DWIN_RedrawScreen(); }
#endif

#if ENABLED(CASE_LIGHT_MENU)
  void SetCaseLight() {
    Toggle_Chkb_Line(caselight.on);
    caselight.update_enabled();
  }
  #if CASELIGHT_USES_BRIGHTNESS
    void ApplyCaseLightBrightness() { caselight.brightness = MenuData.Value; }
    void LiveCaseLightBrightness() { caselight.update_brightness(); }
    void SetCaseLightBrightness() { SetIntOnClick(0, 255, caselight.brightness, ApplyCaseLightBrightness, LiveCaseLightBrightness)); }
  #endif
#endif

#if ENABLED(LED_CONTROL_MENU)
  #if !ALL(CASE_LIGHT_MENU, CASE_LIGHT_USE_NEOPIXEL)
    void SetLedStatus() {
      leds.toggle();
      Show_Chkb_Line(leds.lights_on);
    }
  #endif
  #if HAS_COLOR_LEDS
    void ApplyLEDColor() { HMI_data.Led_Color = LEDColor( {leds.color.r, leds.color.g, leds.color.b OPTARG(HAS_WHITE_LED, leds.color.w) } ); }
    void LiveLEDColor(uint8_t *color) { *color = MenuData.Value; leds.update(); }
    void LiveLEDColorR() { LiveLEDColor(&leds.color.r); }
    void LiveLEDColorG() { LiveLEDColor(&leds.color.g); }
    void LiveLEDColorB() { LiveLEDColor(&leds.color.b); }
    void SetLEDColorR() { SetIntOnClick(0, 255, leds.color.r, ApplyLEDColor, LiveLEDColorR); }
    void SetLEDColorG() { SetIntOnClick(0, 255, leds.color.g, ApplyLEDColor, LiveLEDColorG); }
    void SetLEDColorB() { SetIntOnClick(0, 255, leds.color.b, ApplyLEDColor, LiveLEDColorB); }
    #if HAS_WHITE_LED
      void LiveLEDColorW() { LiveLEDColor(&leds.color.w); }
      void SetLEDColorW() { SetIntOnClick(0, 255, leds.color.w, ApplyLEDColor, LiveLEDColorW); }
    #endif
  #endif
#endif

#if ENABLED(SOUND_MENU_ITEM)
  void SetEnableSound() { Toggle_Chkb_Line(ui.sound_on); }
  void SetEnableTick() { Toggle_Chkb_Line(ui.tick_on); }
#endif

#if ALL(HAS_MESH, USE_GRID_MESHVIEWER)
  void SetViewMesh() { Toggle_Chkb_Line(bedLevelTools.view_mesh); }
#endif

#if HAS_HOME_OFFSET
  void ApplyHomeOffset() { set_home_offset(HMI_value.axis, MenuData.Value / MINUNITMULT); }
  void SetHomeOffsetX() { HMI_value.axis = X_AXIS; SetPFloatOnClick(-50, 50, UNITFDIGITS, ApplyHomeOffset); }
  void SetHomeOffsetY() { HMI_value.axis = Y_AXIS; SetPFloatOnClick(-50, 50, UNITFDIGITS, ApplyHomeOffset); }
  void SetHomeOffsetZ() { HMI_value.axis = Z_AXIS; SetPFloatOnClick( -2,  2, UNITFDIGITS, ApplyHomeOffset); }
#endif

#if HAS_BED_PROBE
  void SetProbeOffsetX() { SetPFloatOnClick(-60, 60, UNITFDIGITS, TERN(PROUI_EX, ProEx.ApplyPhySet, nullptr)); }
  void SetProbeOffsetY() { SetPFloatOnClick(-60, 60, UNITFDIGITS, TERN(PROUI_EX, ProEx.ApplyPhySet, nullptr)); }
  void SetProbeOffsetZ() { SetPFloatOnClick(-10, 10, 2); }

  #if PROUI_EX
    void SetProbeZSpeed()  { SetPIntOnClick(60, 1000); }
    #if DISABLED(BD_SENSOR)
      void ApplyProbeMultiple() { PRO_data.multiple_probing = MenuData.Value; }
      void SetProbeMultiple()  { SetIntOnClick(1, 4, PRO_data.multiple_probing, ApplyProbeMultiple); }
    #endif
  #else
    void SetProbeZSpeed()  { SetPIntOnClick(60, 1000); }
    #if DISABLED(BD_SENSOR)
      void ApplyProbeMultiple() { HMI_data.multiple_probing = MenuData.Value; }
      void SetProbeMultiple()  { SetIntOnClick(1, 4, HMI_data.multiple_probing, ApplyProbeMultiple); }
    #endif
  #endif

  #if ENABLED(Z_MIN_PROBE_REPEATABILITY_TEST)
    void ProbeTest() {
      DEBUG_ECHOLNPGM("M48 Probe Test");
      LCD_MESSAGE(MSG_M48_TEST);
      queue.inject(F("G28XYO\nG28Z\nM48 P5"));
    }
  #endif

  void ProbeStow() { probe.stow(); }
  void ProbeDeploy() { probe.deploy(); }

  #if ALL(HAS_BLTOUCH_HS_MODE, HS_MENU_ITEM)
    void SetHSMode() { Toggle_Chkb_Line(bltouch.high_speed_mode); }
  #endif

  // Auto Bed Leveling
  void AutoLev() {
    queue.inject(F(TERN(AUTO_BED_LEVELING_UBL, "G29P1", "G29")));
  }

  // Mesh Popup
  void PopUp_StartAutoLev() { DWIN_Popup_ConfirmCancel(ICON_Leveling_1, F("Start Auto Bed Leveling?")); }
  void OnClick_StartAutoLev() {
    if (HMI_flag.select_flag) { AutoLev(); }
    else { HMI_ReturnScreen(); }
  }
  void AutoLevStart() { Goto_Popup(PopUp_StartAutoLev, OnClick_StartAutoLev); }
#endif

#if ENABLED(EDITABLE_DISPLAY_TIMEOUT)
  void ApplyTimer() { ui.backlight_timeout_minutes = MenuData.Value; }
  void SetTimer() { SetIntOnClick(ui.backlight_timeout_min, ui.backlight_timeout_max, ui.backlight_timeout_minutes, ApplyTimer); }
#endif

#if ALL(PROUI_EX, NOZZLE_PARK_FEATURE)
  void SetParkPosX()   { SetPIntOnClick(X_MIN_POS, X_MAX_POS); }
  void SetParkPosY()   { SetPIntOnClick(Y_MIN_POS, Y_MAX_POS); }
  void SetParkZRaise() { SetPIntOnClick(Z_MIN_POS, 50); }
#endif

#if HAS_FILAMENT_SENSOR
  void SetRunoutEnable() {
    runout.reset();
    Toggle_Chkb_Line(runout.enabled);
  }

  #if PROUI_EX
    void LiveRunoutActive() { ProEx.DrawRunoutActive(true); }
    void SetRunoutActive() {
      uint8_t val;
      val = PRO_data.FilamentMotionSensor ? 2 : PRO_data.Runout_active_state ? 1 : 0;
      SetOnClick(SetIntNoDraw, 0, 2, 0, val, ProEx.ApplyRunoutActive, LiveRunoutActive);
      ProEx.DrawRunoutActive(true);
    }
  #endif

  #if HAS_FILAMENT_RUNOUT_DISTANCE
    void ApplyRunoutDistance() { runout.set_runout_distance(MenuData.Value / MINUNITMULT); }
    void SetRunoutDistance() { SetFloatOnClick(0, 999, UNITFDIGITS, runout.runout_distance(), ApplyRunoutDistance); }
  #endif
#endif

#if ENABLED(CONFIGURE_FILAMENT_CHANGE)
  void SetFilLoad()   { SetPFloatOnClick(0, EXTRUDE_MAXLENGTH, UNITFDIGITS); }
  void SetFilUnload() { SetPFloatOnClick(0, EXTRUDE_MAXLENGTH, UNITFDIGITS); }
#endif

#if ENABLED(PREVENT_COLD_EXTRUSION)
  void ApplyExtMinT() { thermalManager.extrude_min_temp = HMI_data.ExtMinT; thermalManager.allow_cold_extrude = (HMI_data.ExtMinT == 0); }
  void SetExtMinT() { SetPIntOnClick(MIN_ETEMP, MAX_ETEMP, ApplyExtMinT); }
#endif

#if HAS_FEEDRATE_EDIT
  void SetSpeed() { SetPIntOnClick(SPEED_EDIT_MIN, SPEED_EDIT_MAX); }
#endif

#if HAS_FLOW_EDIT
  void SetFlow() { SetPIntOnClick(FLOW_EDIT_MIN, FLOW_EDIT_MAX, []{ planner.refresh_e_factor(EXT); }); }
#endif

#if HAS_HOTEND
  void ApplyHotendTemp() { thermalManager.setTargetHotend(MenuData.Value, H_E0); }
  void SetHotendTemp() { SetIntOnClick(MIN_ETEMP, MAX_ETEMP, thermalManager.degTargetHotend(EXT), ApplyHotendTemp); }
#endif

#if HAS_HEATED_BED
  void ApplyBedTemp() { thermalManager.setTargetBed(MenuData.Value); }
  void SetBedTemp() { SetIntOnClick(MIN_BEDTEMP, MAX_BEDTEMP, thermalManager.degTargetBed(), ApplyBedTemp); }
#endif

#if HAS_FAN
  void ApplyFanSpeed() { thermalManager.set_fan_speed(0, MenuData.Value); TERN_(LASER_SYNCHRONOUS_M106_M107, planner.buffer_sync_block(BLOCK_BIT_SYNC_FANS));}
  void SetFanSpeed() { SetIntOnClick(0, 255, thermalManager.fan_speed[EXT], ApplyFanSpeed); }
#endif

#if ENABLED(SHOW_SPEED_IND)
  void SetSpdInd() { Toggle_Chkb_Line(HMI_data.SpdInd); }
#endif

#if ENABLED(NOZZLE_PARK_FEATURE)
  void ParkHead() {
    LCD_MESSAGE(MSG_FILAMENT_PARK_ENABLED);
    queue.inject(F("G28O\nG27 P1"));
  }
  void RaiseHead() {
    char msg[20] = "";
    sprintf_P(msg, PSTR("Raise Z by %i"), NOZZLE_PARK_Z_RAISE_MIN);
    LCD_MESSAGE_F(msg);
    queue.inject(F("G27 P3"));
  }
#else
  void RaiseHead() {
    LCD_MESSAGE(MSG_TOOL_CHANGE_ZLIFT);
    char cmd[20] = "";
    const int16_t zpos = current_position.z + Z_POST_CLEARANCE;
    if (axis_is_trusted(Z_AXIS)) _MIN(zpos, Z_MAX_POS);
    sprintf_P(cmd, PSTR("G0 F3000 Z%i"), zpos);
    queue.inject(cmd);
  }
#endif

#if ENABLED(ADVANCED_PAUSE_FEATURE)

  void ChangeFilament() {
    HMI_SaveProcessID(NothingToDo);
    queue.inject(F("M600 B2"));
  }

  #if ENABLED(FILAMENT_LOAD_UNLOAD_GCODES)
    void UnloadFilament() {
      LCD_MESSAGE(MSG_FILAMENTUNLOAD);
      queue.inject(F("M702 Z20"));
    }

    void LoadFilament() {
      LCD_MESSAGE(MSG_FILAMENTLOAD);
      queue.inject(F("M701 Z20"));
    }
  #endif

#endif // ADVANCED_PAUSE_FEATURE

// Bed Tramming
TERN(HAS_BED_PROBE, float, void) tram(uint8_t point OPTARG(HAS_BED_PROBE, bool stow_probe/*=true*/)) {
  #if ENABLED(LCD_BED_TRAMMING)
    constexpr float lfrb[] = BED_TRAMMING_INSET_LFRB;
  #else
    const_float_t lfrb[] = { ui.screw_pos, ui.screw_pos, TERN(HAS_BED_PROBE, _MAX(((X_BED_SIZE - X_MAX_POS) - probe.offset.x), ui.screw_pos), ui.screw_pos), TERN(HAS_BED_PROBE, _MAX(((Y_BED_SIZE - Y_MAX_POS) - probe.offset.y), ui.screw_pos), ui.screw_pos) };
  #endif
  OPTCODE(HAS_BED_PROBE, static bool inLev = false)
  OPTCODE(HAS_BED_PROBE, if (inLev) return NAN)
  float xpos = 0, ypos = 0 OPTARG(HAS_BED_PROBE, zval = 0);
  gcode.process_subcommands_now(F("G28O"));
  ui.reset_status(true);
  switch (point) {
    case 0: LCD_MESSAGE(MSG_TRAM_FL);
      xpos = lfrb[0];
      ypos = lfrb[1];
      break;
    case 1: LCD_MESSAGE(MSG_TRAM_FR);
      xpos = X_BED_SIZE - lfrb[2];
      ypos = lfrb[1];
      break;
    case 2: LCD_MESSAGE(MSG_TRAM_BR);
      xpos = X_BED_SIZE - lfrb[2];
      ypos = Y_BED_SIZE - lfrb[3];
      break;
    case 3: LCD_MESSAGE(MSG_TRAM_BL);
      xpos = lfrb[0];
      ypos = Y_BED_SIZE - lfrb[3];
      break;
    case 4: LCD_MESSAGE(MSG_TRAM_C);
      xpos = X_CENTER;
      ypos = Y_CENTER;
      break;
  }
  #if HAS_BED_PROBE
    if (HMI_data.FullManualTramming) {
      TERN_(HAS_LEVELING, set_bed_leveling_enabled(false);)
      gcode.process_subcommands_now(MString<100>(
        #if ENABLED(LCD_BED_TRAMMING)
          F("M420S0\nG90\nG0F300Z" STRINGIFY(BED_TRAMMING_Z_HOP) "\nG0F5000X"), p_float_t(xpos, 1), 'Y', p_float_t(ypos, 1), F("\nG0F300Z" STRINGIFY(BED_TRAMMING_HEIGHT))
        #else
          F("M420S0\nG90\nG0F300Z" STRINGIFY(Z_CLEARANCE_BETWEEN_PROBES) "\nG0F5000X"), p_float_t(xpos, 1), 'Y', p_float_t(ypos, 1), F("\nG0F300Z0")
        #endif
      ));
    }
    else {
      TERN_(HAS_LEVELING, set_bed_leveling_enabled(false);)
      if (stow_probe) { probe.stow(); }
      inLev = true;
      zval = probe.probe_at_point(xpos, ypos, (stow_probe ? PROBE_PT_STOW : PROBE_PT_RAISE));
      if (!isnan(zval)) {
        ui.set_status(TS(F("X:"), p_float_t(xpos, 1), F(" Y:"), p_float_t(ypos, 1), F(" Z:"), p_float_t(zval, 3)));
      }
      else { LCD_MESSAGE(MSG_ZPROBE_OUT); }
      inLev = false;
    }
    return zval;
  #else // !HAS_BED_PROBE
    queue.inject(MString<100>(
      #if ENABLED(LCD_BED_TRAMMING)
        F("M420S0\nG28O\nG90\nG0F300Z" STRINGIFY(BED_TRAMMING_Z_HOP) "\nG0F5000X"), p_float_t(xpos, 1), 'Y', p_float_t(ypos, 1), F("\nG0F300Z" STRINGIFY(BED_TRAMMING_HEIGHT))
      #else
        F("M420S0\nG28O\nG90\nG0F300Z" STRINGIFY(Z_CLEARANCE_BETWEEN_PROBES) "\nG0F5000X"), p_float_t(xpos, 1), 'Y', p_float_t(ypos, 1), F("\nG0F300Z0")
      #endif
    ));
  #endif // HAS_BED_PROBE
} // Bed Tramming

#if ALL(HAS_BED_PROBE, PROUI_ITEM_TRAM)

  void Trammingwizard() {
    if (HMI_data.FullManualTramming) {
      LCD_MESSAGE(MSG_DISABLE_MANUAL_TRAMMING);
      return;
    }
    else LCD_MESSAGE(MSG_TRAMMING_WIZARD_START);
    DWINUI::ClearMainArea();
    static bed_mesh_t zval = {};
    probe.stow();
    checkkey = NothingToDo;       // Before home disable user input
    zval[0][0] = tram(0, false);  // First tram point can do Homing
    MeshViewer.DrawMeshGrid(2, 2);
    MeshViewer.DrawMeshPoint(0, 0, zval[0][0]);
    zval[1][0] = tram(1, false);
    MeshViewer.DrawMeshPoint(1, 0, zval[1][0]);
    zval[1][1] = tram(2, false);
    MeshViewer.DrawMeshPoint(1, 1, zval[1][1]);
    zval[0][1] = tram(3, false);
    MeshViewer.DrawMeshPoint(0, 1, zval[0][1]);
    probe.stow();

    if (HMI_data.CalcAvg) {
      DWINUI::Draw_CenteredString(140, F("Calculating average"));
      DWINUI::Draw_CenteredString(160, F("and relative heights"));
      safe_delay(1000);
      float avg = 0.0f;
      for (uint8_t x = 0; x < 2; ++x) for (uint8_t y = 0; y < 2; ++y) avg += zval[x][y];
      avg /= 4.0f;
      for (uint8_t x = 0; x < 2; ++x) for (uint8_t y = 0; y < 2; ++y) zval[x][y] -= avg;
      MeshViewer.DrawMesh(zval, 2, 2);
    }
    else { DWINUI::Draw_CenteredString(100, F("Finding True value")); }
    safe_delay(1000);
    ui.reset_status();

    #ifndef BED_TRAMMING_PROBE_TOLERANCE
      #define BED_TRAMMING_PROBE_TOLERANCE 0.06f
    #endif

    uint8_t p = 0;
    float max = 0.0f;
    FSTR_P plabel;
    bool s = true;
    for (uint8_t x = 0; x < 2; ++x) for (uint8_t y = 0; y < 2; ++y) {
      const float d = fabsf(zval[x][y]);
      if (max < d) {
        s = (zval[x][y] >= 0);
        max = d;
        p = y + 2 * x;
      }
    }
    if (fabsf(MeshViewer.max - MeshViewer.min) < BED_TRAMMING_PROBE_TOLERANCE || UNEAR_ZERO(max)) {
      DWINUI::Draw_CenteredString(140, F("Corners leveled"));
      DWINUI::Draw_CenteredString(160, F("Tolerance achieved!"));
    }
    else {
      switch (p) {
        case 0b00 : plabel = GET_TEXT_F(MSG_TRAM_FL); break;
        case 0b01 : plabel = GET_TEXT_F(MSG_TRAM_BL); break;
        case 0b10 : plabel = GET_TEXT_F(MSG_TRAM_FR); break;
        case 0b11 : plabel = GET_TEXT_F(MSG_TRAM_BR); break;
        default   : plabel = F(""); break;
      }
      DWINUI::Draw_CenteredString(120, F("Corners not leveled"));
      DWINUI::Draw_CenteredString(140, F("Knob adjustment required"));
      DWINUI::Draw_CenteredString((s ? Color_Green : Color_Error_Red), 160, (s ? GET_TEXT_F(MSG_TRAMWIZ_LOWER) : GET_TEXT_F(MSG_TRAMWIZ_RAISE)));
      DWINUI::Draw_CenteredString(HMI_data.StatusTxt_Color, 180, plabel);
    }
    DWINUI::Draw_Button(BTN_Continue, 86, 305, true);
    checkkey = Menu;
    HMI_SaveProcessID(WaitResponse);
  }

  void SetManualTramming() {
    Toggle_Chkb_Line(HMI_data.FullManualTramming);
  }

  void SetCalcAvg() {
    Toggle_Chkb_Line(HMI_data.CalcAvg);
  }

  // Trammingwizard Popup
  void PopUp_StartTramwiz() { DWIN_Popup_ConfirmCancel(TERN(TJC_DISPLAY, ICON_BLTouch, ICON_Printer_0), F("Start Tramming Wizard?")); }
  void OnClick_StartTramwiz() {
    if (HMI_flag.select_flag) {
      if (HMI_data.FullManualTramming) {
        LCD_MESSAGE_F("Disable manual tramming");
        HMI_ReturnScreen();
        return;
      }
      else { Trammingwizard(); }
    }
    else { HMI_ReturnScreen(); }
  }
  void TramwizStart() { Goto_Popup(PopUp_StartTramwiz, OnClick_StartTramwiz); }

#endif // HAS_BED_PROBE && PROUI_ITEM_TRAM
// TrammingWizard

#if ENABLED(MESH_BED_LEVELING)

  void ManualMeshStart() {
    LCD_MESSAGE(MSG_UBL_BUILD_MESH_MENU);
    gcode.process_subcommands_now(F("G28XYO\nG28Z\nM211S0\nG29S1"));
    #ifdef MANUAL_PROBE_START_Z
      const uint8_t line = CurrentMenu->line(MMeshMoveZItem->pos);
      DWINUI::Draw_Signed_Float(HMI_data.Text_Color, HMI_data.Background_Color, 3, 2, VALX - 2 * DWINUI::fontWidth(DWIN_FONT_MENU), MBASE(line), MANUAL_PROBE_START_Z);
    #endif
  }

  void LiveMeshMoveZ() {
    *MenuData.P_Float = MenuData.Value / POW(10, 2);
    if (!planner.is_full()) {
      planner.synchronize();
      planner.buffer_line(current_position, manual_feedrate_mm_s[Z_AXIS]);
    }
  }
  void SetMMeshMoveZ() { SetPFloatOnClick(-1, 1, 2, planner.synchronize, LiveMeshMoveZ); }

  void ManualMeshContinue() {
    gcode.process_subcommands_now(F("G29S2"));
    MMeshMoveZItem->redraw();
  }

  void ManualMeshSave() {
    LCD_MESSAGE(MSG_UBL_STORAGE_MESH_MENU);
    queue.inject(F("M211S1"));
  }

#endif // MESH_BED_LEVELING

#if HAS_PREHEAT
  #if HAS_HOTEND
    void SetPreheatEndTemp() { SetPIntOnClick(MIN_ETEMP, MAX_ETEMP); }
  #endif
  #if HAS_HEATED_BED
    void SetPreheatBedTemp() { SetPIntOnClick(MIN_BEDTEMP, MAX_BEDTEMP); }
  #endif
  #if HAS_FAN
    void SetPreheatFanSpeed() { SetPIntOnClick(0, 255); }
  #endif
#endif

void ApplyMaxSpeed() { planner.set_max_feedrate(HMI_value.axis, MenuData.Value / MINUNITMULT); }
#if HAS_X_AXIS
  void SetMaxSpeedX() { HMI_value.axis = X_AXIS, SetFloatOnClick(min_feedrate_edit_values.x, max_feedrate_edit_values.x, UNITFDIGITS, planner.settings.max_feedrate_mm_s[X_AXIS], ApplyMaxSpeed); }
#endif
#if HAS_Y_AXIS
  void SetMaxSpeedY() { HMI_value.axis = Y_AXIS, SetFloatOnClick(min_feedrate_edit_values.y, max_feedrate_edit_values.y, UNITFDIGITS, planner.settings.max_feedrate_mm_s[Y_AXIS], ApplyMaxSpeed); }
#endif
#if HAS_Z_AXIS
  void SetMaxSpeedZ() { HMI_value.axis = Z_AXIS, SetFloatOnClick(min_feedrate_edit_values.z, max_feedrate_edit_values.z, UNITFDIGITS, planner.settings.max_feedrate_mm_s[Z_AXIS], ApplyMaxSpeed); }
#endif
#if HAS_HOTEND
  void SetMaxSpeedE() { HMI_value.axis = E_AXIS; SetFloatOnClick(min_feedrate_edit_values.e, max_feedrate_edit_values.e, UNITFDIGITS, planner.settings.max_feedrate_mm_s[E_AXIS], ApplyMaxSpeed); }
#endif

void ApplyMaxAccel() { planner.set_max_acceleration(HMI_value.axis, MenuData.Value); }
#if HAS_X_AXIS
  void SetMaxAccelX() { HMI_value.axis = X_AXIS, SetIntOnClick(min_acceleration_edit_values.x, max_acceleration_edit_values.x, planner.settings.max_acceleration_mm_per_s2[X_AXIS], ApplyMaxAccel); }
#endif
#if HAS_Y_AXIS
  void SetMaxAccelY() { HMI_value.axis = Y_AXIS, SetIntOnClick(min_acceleration_edit_values.y, max_acceleration_edit_values.y, planner.settings.max_acceleration_mm_per_s2[Y_AXIS], ApplyMaxAccel); }
#endif
#if HAS_Z_AXIS
  void SetMaxAccelZ() { HMI_value.axis = Z_AXIS, SetIntOnClick(min_acceleration_edit_values.z, max_acceleration_edit_values.z, planner.settings.max_acceleration_mm_per_s2[Z_AXIS], ApplyMaxAccel); }
#endif
#if HAS_HOTEND
  void SetMaxAccelE() { HMI_value.axis = E_AXIS; SetIntOnClick(min_acceleration_edit_values.e, max_acceleration_edit_values.e, planner.settings.max_acceleration_mm_per_s2[E_AXIS], ApplyMaxAccel); }
#endif

#if ENABLED(CLASSIC_JERK)
  void ApplyMaxJerk() { planner.set_max_jerk(HMI_value.axis, MenuData.Value / MINUNITMULT); }
  #if HAS_X_AXIS
    void SetMaxJerkX() { HMI_value.axis = X_AXIS, SetFloatOnClick(min_jerk_edit_values.x, max_jerk_edit_values.x, UNITFDIGITS, planner.max_jerk.x, ApplyMaxJerk); }
  #endif
  #if HAS_Y_AXIS
    void SetMaxJerkY() { HMI_value.axis = Y_AXIS, SetFloatOnClick(min_jerk_edit_values.y, max_jerk_edit_values.y, UNITFDIGITS, planner.max_jerk.y, ApplyMaxJerk); }
  #endif
  #if HAS_Z_AXIS
    void SetMaxJerkZ() { HMI_value.axis = Z_AXIS, SetFloatOnClick(min_jerk_edit_values.z, max_jerk_edit_values.z, UNITFDIGITS, planner.max_jerk.z, ApplyMaxJerk); }
  #endif
  #if HAS_HOTEND
    void SetMaxJerkE() { HMI_value.axis = E_AXIS; SetFloatOnClick(min_jerk_edit_values.e, max_jerk_edit_values.e, UNITFDIGITS, planner.max_jerk.e, ApplyMaxJerk); }
  #endif
#elif HAS_JUNCTION_DEVIATION
  void ApplyJDmm() { TERN_(LIN_ADVANCE, planner.recalculate_max_e_jerk()); }
  void SetJDmm() { SetPFloatOnClick(MIN_JD_MM, MAX_JD_MM, 3, ApplyJDmm); }
#endif

#if ENABLED(LIN_ADVANCE)
  void SetLA_K() { SetPFloatOnClick(0, 10, 3); }
#endif

#if HAS_X_AXIS
  void SetStepsX() { HMI_value.axis = X_AXIS, SetPFloatOnClick( min_steps_edit_values.x, max_steps_edit_values.x, 2); }
#endif
#if HAS_Y_AXIS
  void SetStepsY() { HMI_value.axis = Y_AXIS, SetPFloatOnClick( min_steps_edit_values.y, max_steps_edit_values.y, 2); }
#endif
#if HAS_Z_AXIS
  void SetStepsZ() { HMI_value.axis = Z_AXIS, SetPFloatOnClick( min_steps_edit_values.z, max_steps_edit_values.z, 2); }
#endif
#if HAS_HOTEND
  void SetStepsE() { HMI_value.axis = E_AXIS; SetPFloatOnClick( min_steps_edit_values.e, max_steps_edit_values.e, 2); }
#endif

#if PROUI_EX
  void SetBedSizeX() { HMI_value.axis = NO_AXIS_ENUM, SetPIntOnClick(X_BED_MIN, X_MAX_POS, ProEx.ApplyPhySet); }
  void SetBedSizeY() { HMI_value.axis = NO_AXIS_ENUM, SetPIntOnClick(Y_BED_MIN, Y_MAX_POS, ProEx.ApplyPhySet); }
  void SetMinPosX()  { HMI_value.axis = X_AXIS,       SetPIntOnClick(     -100,       100, ProEx.ApplyPhySet); }
  void SetMinPosY()  { HMI_value.axis = Y_AXIS,       SetPIntOnClick(     -100,       100, ProEx.ApplyPhySet); }
  void SetMaxPosX()  { HMI_value.axis = X_AXIS,       SetPIntOnClick(X_BED_MIN,       999, ProEx.ApplyPhySet); }
  void SetMaxPosY()  { HMI_value.axis = Y_AXIS,       SetPIntOnClick(Y_BED_MIN,       999, ProEx.ApplyPhySet); }
  void SetMaxPosZ()  { HMI_value.axis = Z_AXIS,       SetPIntOnClick(      100,       999, ProEx.ApplyPhySet); }
#endif

#if ALL(PROUI_EX, HAS_EXTRUDERS)
  void SetInvertE0() {
    stepper.disable_e_steppers();
    Toggle_Chkb_Line(PRO_data.Invert_E0);
    current_position.e = 0;
    sync_plan_position_e();
  }
#endif

#if ENABLED(FWRETRACT)
  void Return_FWRetract_Menu() { (PreviousMenu == FilamentMenu) ? Draw_FilamentMan_Menu() : Draw_Tune_Menu(); }
  void SetRetractLength() { SetPFloatOnClick( 0, 10, UNITFDIGITS); }
  void SetRetractSpeed()  { SetPFloatOnClick( 1, 90, UNITFDIGITS); }
  void SetZRaise()        { SetPFloatOnClick( 0, 2, 2); }
  void SetAddRecover()    { SetPFloatOnClick(-5, 5, UNITFDIGITS); }
#else
  void SetRetractSpeed()  { SetPFloatOnClick( 1, 90, UNITFDIGITS); }
#endif

#if ENABLED(ENC_MENU_ITEM)
  void SetEncRateA() { SetPIntOnClick( ui.enc_rateB + 1, 1000 ); }
  void SetEncRateB() { SetPIntOnClick( 11, ui.enc_rateA - 1); }
#endif

#if HAS_TOOLBAR
  void LiveTBSetupItem() {
    UpdateTBSetupItem(static_cast<MenuItemClass*>(CurrentMenu->SelectedItem()), MenuData.Value);
    DrawTBSetupItem(true);
  }
  void ApplyTBSetupItem() {
    DrawTBSetupItem(false);
    if (static_cast<MenuItemClass*>(CurrentMenu->SelectedItem())->icon) {
      uint8_t *Pint = (uint8_t *)static_cast<MenuItemPtrClass*>(CurrentMenu->SelectedItem())->value;
      *Pint = MenuData.Value;
    }
  }
  void SetTBSetupItem() {
    const uint8_t val = *(uint8_t *)static_cast<MenuItemPtrClass*>(CurrentMenu->SelectedItem())->value;
    SetOnClick(SetIntNoDraw, 0, ToolBar.OptCount() - 1, 0, val, ApplyTBSetupItem, LiveTBSetupItem);
    DrawTBSetupItem(true);
  }
  void onDrawTBSetupItem(MenuItemClass* menuitem, int8_t line) {
    uint8_t val = *(uint8_t *)static_cast<MenuItemPtrClass*>(menuitem)->value;
    UpdateTBSetupItem(menuitem, val);
    onDrawMenuItem(menuitem, line);
  }
#endif // HAS_TOOLBAR

// Special Menuitem Drawing functions =================================================

void onDrawSelColorItem(MenuItemClass* menuitem, int8_t line) {
  const uint16_t color = *(uint16_t*)static_cast<MenuItemPtrClass*>(menuitem)->value;
  DWIN_Draw_Rectangle(0, HMI_data.Highlight_Color, ICOX + 1, MBASE(line) - 1 + 1, ICOX + 18, MBASE(line) - 1 + 18);
  DWIN_Draw_Rectangle(1, color, ICOX + 2, MBASE(line) - 1 + 2, ICOX + 17, MBASE(line) - 1 + 17);
  onDrawMenuItem(menuitem, line);
}

void onDrawGetColorItem(MenuItemClass* menuitem, int8_t line) {
  const uint8_t i = menuitem->icon;
  uint16_t color;
  switch (i) {
    case 0:  color = RGB(31, 0, 0); break; // Red
    case 1:  color = RGB(0, 63, 0); break; // Green
    case 2:  color = RGB(0, 0, 31); break; // Blue
    default: color = 0; break;
  }
  DWIN_Draw_Rectangle(0, HMI_data.Highlight_Color, ICOX + 1, MBASE(line) - 1 + 1, ICOX + 18, MBASE(line) - 1 + 18);
  DWIN_Draw_Rectangle(1, color, ICOX + 2, MBASE(line) - 1 + 2, ICOX + 17, MBASE(line) - 1 + 17);
  DWINUI::Draw_String(LBLX, MBASE(line) - 1, menuitem->caption);
  Draw_Menu_IntValue(HMI_data.Background_Color, line, 4, HMI_value.Color[i]);
  DWIN_Draw_HLine(HMI_data.SplitLine_Color, 16, MYPOS(line + 1), 240);
}

#if ALL(HAS_FILAMENT_SENSOR, PROUI_EX)
  void onDrawRunoutActive(MenuItemClass* menuitem, int8_t line) {
    onDrawMenuItem(menuitem, line);
    if (PRO_data.FilamentMotionSensor)
      { DWINUI::Draw_String(VALX - MENU_CHR_W, MBASE(line), GET_TEXT_F(MSG_MOTION)); }
    else
      { DWINUI::Draw_String(VALX + MENU_CHR_W, MBASE(line), PRO_data.Runout_active_state ? GET_TEXT_F(MSG_HIGH) : GET_TEXT_F(MSG_LOW)); }
  }
#endif

#if ALL(HAS_MESH, PROUI_EX)
  void drawMeshPoints(bool selected, int8_t line, int8_t value) {
    char mpmsg[10];
    sprintf(mpmsg, "%ix%i", value, value);
    if (selected) { DWINUI::Draw_String(DWINUI::textcolor, HMI_data.Selected_Color, VALX + MENU_CHR_H, MBASE(line), mpmsg); }
    else { DWINUI::Draw_String(VALX + MENU_CHR_H, MBASE(line), mpmsg); }
  }

  void onDrawMeshPoints(MenuItemClass* menuitem, int8_t line) {
    onDrawMenuItem(menuitem, line);
    drawMeshPoints(false, line, PRO_data.grid_max_points);
    ReDrawItem();
  }
#endif

// Menu Creation and Drawing functions ======================================================

void ReturnToPreviousMenu() {
  #if ENABLED(CV_LASER_MODULE)
    if (PreviousMenu == LaserPrintMenu) return Draw_LaserPrint_Menu();
  #endif
  if (PreviousMenu == AdvancedSettings) return Draw_AdvancedSettings_Menu();
  if (PreviousMenu == FilSetMenu) return Draw_FilSet_Menu();
  if (PreviousMenu == TuneMenu) return Draw_Tune_Menu();
  if (PreviousMenu == FileMenu) return Draw_Print_File_Menu();
}

void Draw_Prepare_Menu() {
  checkkey = Menu;
  if (SET_MENU(PrepareMenu, MSG_PREPARE, 10 + PREHEAT_COUNT)) {
    BACK_ITEM(Goto_Main_Menu);
    MENU_ITEM(ICON_AxisC, MSG_MOVE_AXIS, onDrawSubMenu, Draw_Move_Menu);
    #if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
      MENU_ITEM(ICON_Homing, MSG_HOMING, onDrawSubMenu, Draw_Homing_Menu);
    #else
      MENU_ITEM(ICON_Homing, MSG_AUTO_HOME, onDrawMenuItem, AutoHome);
    #endif
    MENU_ITEM(ICON_CloseMotor, MSG_DISABLE_STEPPERS, onDrawMenuItem, DisableMotors);
    #if HAS_PREHEAT
      #define _ITEM_PREHEAT(N) MENU_ITEM(ICON_Preheat##N, MSG_PREHEAT_##N, onDrawMenuItem, DoPreheat##N);
      REPEAT_1(PREHEAT_COUNT, _ITEM_PREHEAT)
    #endif
    MENU_ITEM(ICON_Cool, MSG_COOLDOWN, onDrawMenuItem, DoCoolDown);
    #if HAS_ZOFFSET_ITEM
      MENU_ITEM(ICON_SetZOffset, MSG_PROBE_WIZARD, onDrawSubMenu, Draw_ZOffsetWiz_Menu);
    #endif
    MENU_ITEM(ICON_Tram, MSG_BED_TRAMMING, onDrawSubMenu, Draw_Tramming_Menu);
    MENU_ITEM(ICON_FilMan, MSG_FILAMENT_MAN, onDrawSubMenu, Draw_FilamentMan_Menu);
    #if ALL(PROUI_TUNING_GRAPH, PROUI_ITEM_PLOT)
      MENU_ITEM(ICON_PIDNozzle, MSG_HOTEND_TEMP_GRAPH, onDrawMenuItem, drawHPlot);
      MENU_ITEM(ICON_PIDBed, MSG_BED_TEMP_GRAPH, onDrawMenuItem, drawBPlot);
    #endif
  }
  ui.reset_status(true);
  UpdateMenu(PrepareMenu);
}

void Draw_Tramming_Menu() {
  checkkey = Menu;
  if (SET_MENU(TrammingMenu, MSG_BED_TRAMMING, 10)) {
    BACK_ITEM(Draw_Prepare_Menu);
    #if ENABLED(PROUI_ITEM_TRAM)
      #if HAS_BED_PROBE
        MENU_ITEM(ICON_Tram, MSG_TRAMMING_WIZARD, onDrawMenuItem, TramwizStart);
        EDIT_ITEM(ICON_Version, MSG_BED_TRAMMING_MANUAL, onDrawChkbMenu, SetManualTramming, &HMI_data.FullManualTramming);
        EDIT_ITEM(ICON_ResetEEPROM, MSG_TRAMWIZ_CALC, onDrawChkbMenu, SetCalcAvg, &HMI_data.CalcAvg);
      #else
        MENU_ITEM(ICON_MoveZ0, MSG_HOME_Z_AND_DISABLE, onDrawMenuItem, HomeZandDisable);
      #endif
    #endif
    MENU_ITEM(ICON_AxisBL, MSG_TRAM_FL, onDrawMenuItem, []{ (void)tram(0); });
    MENU_ITEM(ICON_AxisBR, MSG_TRAM_FR, onDrawMenuItem, []{ (void)tram(1); });
    MENU_ITEM(ICON_AxisTR, MSG_TRAM_BR, onDrawMenuItem, []{ (void)tram(2); });
    MENU_ITEM(ICON_AxisTL, MSG_TRAM_BL, onDrawMenuItem, []{ (void)tram(3); });
    MENU_ITEM(ICON_AxisC, MSG_TRAM_C, onDrawMenuItem, []{ (void)tram(4); });
    MENU_ITEM(ICON_HomeZ, MSG_AUTO_HOME_Z, onDrawMenuItem, HomeZ);
  }
  UpdateMenu(TrammingMenu);
}

void Draw_Control_Menu() {
  checkkey = Menu;
  if (SET_MENU(ControlMenu, MSG_CONTROL, 18)) {
    BACK_ITEM(Goto_Main_Menu);
    #if ENABLED(EEPROM_SETTINGS)
      MENU_ITEM(ICON_WriteEEPROM, MSG_STORE_EEPROM, onDrawMenuItem, WriteEeprom);
    #endif
    MENU_ITEM(ICON_Temperature, MSG_TEMPERATURE, onDrawSubMenu, Draw_Temperature_Menu);
    MENU_ITEM(ICON_Motion, MSG_MOTION, onDrawSubMenu, Draw_Motion_Menu);
    #if HAS_LCD_BRIGHTNESS
      MENU_ITEM(ICON_Box, MSG_BRIGHTNESS_OFF, onDrawMenuItem, TurnOffBacklight);
    #endif
    #if HAS_LOCKSCREEN
      MENU_ITEM(ICON_Checkbox, MSG_LOCKSCREEN, onDrawMenuItem, DWIN_LockScreen);
    #endif
    MENU_ITEM(ICON_Reboot, MSG_RESET_PRINTER, onDrawMenuItem, RebootPrinter);
    #if ENABLED(HOST_SHUTDOWN_MENU_ITEM) && defined(SHUTDOWN_ACTION)
      MENU_ITEM(ICON_Host, MSG_HOST_SHUTDOWN, onDrawMenuItem, HostShutDown);
    #endif
    #if HAS_ESDIAG
      MENU_ITEM(ICON_ESDiag, MSG_ENDSTOP_TEST, onDrawSubMenu, Draw_EndStopDiag);
    #endif
    #if PROUI_EX
      MENU_ITEM(ICON_PhySet, MSG_PHY_SET, onDrawSubMenu, Draw_PhySet_Menu);
    #endif
    #if HAS_HOME_OFFSET
      MENU_ITEM(ICON_HomeOffset, MSG_SET_HOME_OFFSETS, onDrawSubMenu, Draw_HomeOffset_Menu);
    #elif ALL(PROUI_EX, NOZZLE_PARK_FEATURE)
      MENU_ITEM(ICON_ParkPos, MSG_FILAMENT_PARK_ENABLED, onDrawSubMenu, Draw_ParkPos_Menu);
    #endif
    #if HAS_CUSTOM_COLORS
      MENU_ITEM(ICON_Scolor, MSG_COLORS_SELECT, onDrawSubMenu, Draw_SelectColors_Menu);
    #endif
    #if HAS_TOOLBAR
      MENU_ITEM(ICON_TBSetup, MSG_TOOLBAR_SETUP, onDrawSubMenu, Draw_TBSetup_Menu);
    #endif
    #if HAS_BED_PROBE || defined(MESH_BED_LEVELING)
      MENU_ITEM(ICON_Language, MSG_ADVANCED_SETTINGS, onDrawSubMenu, Draw_Advanced_Menu);
    #endif
    #if ENABLED(CASE_LIGHT_MENU)
      #if CASELIGHT_USES_BRIGHTNESS
        MENU_ITEM(ICON_CaseLight, MSG_CASE_LIGHT, onDrawSubMenu, Draw_CaseLight_Menu);
      #else
        EDIT_ITEM(ICON_CaseLight, MSG_CASE_LIGHT, onDrawChkbMenu, SetCaseLight, &caselight.on);
      #endif
    #endif
    #if ENABLED(LED_CONTROL_MENU)
      MENU_ITEM(ICON_LedControl, MSG_LED_CONTROL, onDrawSubMenu, Draw_LedControl_Menu);
    #endif
    #if ENABLED(PRINTCOUNTER)
      MENU_ITEM(ICON_PrintStats, MSG_INFO_STATS_MENU, onDrawSubMenu, gotoPrintStats);
    #endif
    MENU_ITEM(ICON_Info, MSG_INFO_SCREEN, onDrawSubMenu, Goto_Info_Menu);
  }
  ui.reset_status(true);
  UpdateMenu(ControlMenu);
}

void Draw_Move_Menu() {
  checkkey = Menu;
  if (SET_MENU(MoveMenu, MSG_MOVE_AXIS, 6)) {
    BACK_ITEM(Draw_Prepare_Menu);
    #if HAS_X_AXIS
      EDIT_ITEM(ICON_MoveX, MSG_MOVE_X, onDrawPFloatMenu, SetMoveX, &current_position.x);
    #endif
    #if HAS_Y_AXIS
      EDIT_ITEM(ICON_MoveY, MSG_MOVE_Y, onDrawPFloatMenu, SetMoveY, &current_position.y);
    #endif
    #if HAS_Z_AXIS
      EDIT_ITEM(ICON_MoveZ, MSG_MOVE_Z, onDrawPFloatMenu, SetMoveZ, &current_position.z);
    #endif
    #if HAS_HOTEND
      gcode.process_subcommands_now(F("G92E0"));  // reset extruder position
      EDIT_ITEM(ICON_Extruder, MSG_MOVE_E, onDrawPFloatMenu, SetMoveE, &current_position.e);
    #endif
    EDIT_ITEM(ICON_AxisC, MSG_LIVE_MOVE, onDrawChkbMenu, SetLiveMove, &EnableLiveMove);
  }
  UpdateMenu(MoveMenu);
  if (!all_axes_trusted()) { LCD_MESSAGE_F("..WARNING: Current position is unknown, Home axes."); }
}

#if HAS_HOME_OFFSET

  void Draw_HomeOffset_Menu() {
    checkkey = Menu;
    if (SET_MENU(HomeOffMenu, MSG_SET_HOME_OFFSETS, 6)) {
      BACK_ITEM(Draw_Control_Menu);
      #if ALL(PROUI_EX, NOZZLE_PARK_FEATURE)
        MENU_ITEM(ICON_ParkPos, MSG_FILAMENT_PARK_ENABLED, onDrawSubMenu, Draw_ParkPos_Menu);
      #endif
      #if HAS_X_AXIS
        EDIT_ITEM(ICON_HomeOffsetX, MSG_HOME_OFFSET_X, onDrawPFloatMenu, SetHomeOffsetX, &home_offset.x);
      #endif
      #if HAS_Y_AXIS
        EDIT_ITEM(ICON_HomeOffsetY, MSG_HOME_OFFSET_Y, onDrawPFloatMenu, SetHomeOffsetY, &home_offset.y);
      #endif
      #if HAS_Z_AXIS
        EDIT_ITEM(ICON_HomeOffsetZ, MSG_HOME_OFFSET_Z, onDrawPFloatMenu, SetHomeOffsetZ, &home_offset.z);
      #endif
      MENU_ITEM_F(ICON_SetHome, "Set as Home position: 0,0,0", onDrawMenuItem, SetHome);
    }
    UpdateMenu(HomeOffMenu);
  }

#endif

#if HAS_BED_PROBE

  void Draw_ProbeSet_Menu() {
    checkkey = Menu;
    if (SET_MENU(ProbeSetMenu, MSG_ZPROBE_SETTINGS, 11)) {
      BACK_ITEM(Draw_AdvancedSettings_Menu);
      #if HAS_X_AXIS
        EDIT_ITEM(ICON_ProbeOffsetX, MSG_ZPROBE_XOFFSET, onDrawPFloatMenu, SetProbeOffsetX, &probe.offset.x);
      #endif
      #if HAS_Y_AXIS
        EDIT_ITEM(ICON_ProbeOffsetY, MSG_ZPROBE_YOFFSET, onDrawPFloatMenu, SetProbeOffsetY, &probe.offset.y);
      #endif
      #if PROUI_EX
        EDIT_ITEM(ICON_ProbeZSpeed, MSG_Z_FEED_RATE, onDrawPIntMenu, SetProbeZSpeed, &PRO_data.zprobefeedslow);
        IF_DISABLED(BD_SENSOR, EDIT_ITEM(ICON_Cancel, MSG_ZPROBE_MULTIPLE, onDrawPInt8Menu, SetProbeMultiple, &PRO_data.multiple_probing);)
      #else
        EDIT_ITEM(ICON_ProbeZSpeed, MSG_Z_FEED_RATE, onDrawPIntMenu, SetProbeZSpeed, &HMI_data.zprobeFeed);
        IF_DISABLED(BD_SENSOR, EDIT_ITEM(ICON_Cancel, MSG_ZPROBE_MULTIPLE, onDrawPInt8Menu, SetProbeMultiple, &HMI_data.multiple_probing);)
      #endif
      #if ENABLED(BLTOUCH)
        MENU_ITEM(ICON_ProbeStow, MSG_MANUAL_STOW, onDrawMenuItem, ProbeStow);
        MENU_ITEM(ICON_ProbeDeploy, MSG_MANUAL_DEPLOY, onDrawMenuItem, ProbeDeploy);
        MENU_ITEM(ICON_BLtouchReset, MSG_MANUAL_RESET, onDrawMenuItem, bltouch._reset);
        MENU_ITEM(ICON_ProbeTest, MSG_M48_TEST, onDrawMenuItem, ProbeTest);
        #if ALL(HAS_BLTOUCH_HS_MODE, HS_MENU_ITEM)
          EDIT_ITEM(ICON_HSMode, MSG_ENABLE_HS_MODE, onDrawChkbMenu, SetHSMode, &bltouch.high_speed_mode);
        #endif
      #endif
    }
    UpdateMenu(ProbeSetMenu);
  }

#endif

void Draw_FilSet_Menu() {
  checkkey = Menu;
  if (SET_MENU(FilSetMenu, MSG_FILAMENT_SET, 8)) {
    BACK_ITEM(Draw_FilamentMan_Menu);
    #if HAS_FILAMENT_SENSOR
      EDIT_ITEM(ICON_Runout, MSG_RUNOUT_ENABLE, onDrawChkbMenu, SetRunoutEnable, &runout.enabled);
      #if PROUI_EX
        MENU_ITEM(ICON_Runout, MSG_RUNOUT_ACTIVE, onDrawRunoutActive, SetRunoutActive);
      #endif
    #endif
    #if ENABLED(CONFIGURE_FILAMENT_CHANGE)
      EDIT_ITEM(ICON_FilLoad, MSG_FILAMENT_LOAD, onDrawPFloatMenu, SetFilLoad, &fc_settings[EXT].load_length);
      EDIT_ITEM(ICON_FilUnload, MSG_FILAMENT_UNLOAD, onDrawPFloatMenu, SetFilUnload, &fc_settings[EXT].unload_length);
    #endif
    #if HAS_FILAMENT_RUNOUT_DISTANCE
      EDIT_ITEM(ICON_Runout, MSG_RUNOUT_DISTANCE_MM, onDrawPFloatMenu, SetRunoutDistance, &runout.runout_distance());
    #endif
    #if ALL(PROUI_EX, HAS_EXTRUDERS)
      EDIT_ITEM(ICON_InvertE0, MSG_INVERT_EXTRUDER, onDrawChkbMenu, SetInvertE0, &PRO_data.Invert_E0);
    #endif
    #if ENABLED(PREVENT_COLD_EXTRUSION)
      EDIT_ITEM(ICON_ExtrudeMinT, MSG_EXTRUDER_MIN_TEMP, onDrawPIntMenu, SetExtMinT, &HMI_data.ExtMinT);
    #endif
  }
  UpdateMenu(FilSetMenu);
}

#if PROUI_EX

  #if ENABLED(NOZZLE_PARK_FEATURE)
    void Draw_ParkPos_Menu() {
      checkkey = Menu;
      if (SET_MENU(ParkPosMenu, MSG_FILAMENT_PARK_ENABLED, 4)) {
        BACK_ITEM(TERN(HAS_HOME_OFFSET, Draw_HomeOffset_Menu, Draw_Control_Menu));
        EDIT_ITEM(ICON_ParkPosX, MSG_PARK_XPOSITION, onDrawPIntMenu, SetParkPosX, &PRO_data.Park_point.x);
        EDIT_ITEM(ICON_ParkPosY, MSG_PARK_YPOSITION, onDrawPIntMenu, SetParkPosY, &PRO_data.Park_point.y);
        EDIT_ITEM(ICON_ParkPosZ, MSG_PARK_ZRAISE, onDrawPIntMenu, SetParkZRaise, &PRO_data.Park_point.z);
      }
      UpdateMenu(ParkPosMenu);
    }
  #endif

  void Draw_PhySet_Menu() {
    checkkey = Menu;
    if (SET_MENU(PhySetMenu, MSG_PHY_SET, 8)) {
      BACK_ITEM(Draw_Control_Menu);
      EDIT_ITEM(ICON_BedSize, MSG_PHY_XBEDSIZE, onDrawPIntMenu, SetBedSizeX, &PRO_data.x_bed_size);
      EDIT_ITEM(ICON_BedSize, MSG_PHY_YBEDSIZE, onDrawPIntMenu, SetBedSizeY, &PRO_data.y_bed_size);
      EDIT_ITEM(ICON_MaxPosX, MSG_PHY_XMINPOS, onDrawPIntMenu, SetMinPosX, &PRO_data.x_min_pos);
      EDIT_ITEM(ICON_MaxPosY, MSG_PHY_YMINPOS, onDrawPIntMenu, SetMinPosY, &PRO_data.y_min_pos);
      EDIT_ITEM(ICON_MaxPosX, MSG_PHY_XMAXPOS, onDrawPIntMenu, SetMaxPosX, &PRO_data.x_max_pos);
      EDIT_ITEM(ICON_MaxPosY, MSG_PHY_YMAXPOS, onDrawPIntMenu, SetMaxPosY, &PRO_data.y_max_pos);
      EDIT_ITEM(ICON_MaxPosZ, MSG_PHY_ZMAXPOS, onDrawPIntMenu, SetMaxPosZ, &PRO_data.z_max_pos);
    }
    UpdateMenu(PhySetMenu);
  }

#endif

#if ALL(CASE_LIGHT_MENU, CASELIGHT_USES_BRIGHTNESS)

  void Draw_CaseLight_Menu() {
    checkkey = Menu;
    if (SET_MENU(CaseLightMenu, MSG_CASE_LIGHT, 3)) {
      BACK_ITEM(Draw_Control_Menu);
      EDIT_ITEM(ICON_CaseLight, MSG_CASE_LIGHT, onDrawChkbMenu, SetCaseLight, &caselight.on);
      EDIT_ITEM(ICON_Brightness, MSG_CASE_LIGHT_BRIGHTNESS, onDrawPInt8Menu, SetCaseLightBrightness, &caselight.brightness);
    }
    UpdateMenu(CaseLightMenu);
  }

#endif

#if ENABLED(LED_CONTROL_MENU)

  void Draw_LedControl_Menu() {
    checkkey = Menu;
    if (SET_MENU(LedControlMenu, MSG_LED_CONTROL, 10)) {
      BACK_ITEM((CurrentMenu == TuneMenu) ? Draw_Tune_Menu : Draw_Control_Menu);
      #if !ALL(CASE_LIGHT_MENU, CASE_LIGHT_USE_NEOPIXEL)
        EDIT_ITEM(ICON_LedControl, MSG_LEDS, onDrawChkbMenu, SetLedStatus, &leds.lights_on);
      #endif
      #if HAS_COLOR_LEDS
        #if ENABLED(LED_COLOR_PRESETS)
          MENU_ITEM(ICON_LedControl, MSG_SET_LEDS_WHITE, onDrawMenuItem,  leds.set_white);
          MENU_ITEM(ICON_LedControl, MSG_SET_LEDS_RED, onDrawMenuItem,    leds.set_red);
          MENU_ITEM(ICON_LedControl, MSG_SET_LEDS_ORANGE, onDrawMenuItem, leds.set_orange);
          MENU_ITEM(ICON_LedControl, MSG_SET_LEDS_YELLOW, onDrawMenuItem, leds.set_yellow);
          MENU_ITEM(ICON_LedControl, MSG_SET_LEDS_GREEN, onDrawMenuItem,  leds.set_green);
          MENU_ITEM(ICON_LedControl, MSG_SET_LEDS_BLUE, onDrawMenuItem,   leds.set_blue);
          MENU_ITEM(ICON_LedControl, MSG_SET_LEDS_INDIGO, onDrawMenuItem, leds.set_indigo);
          MENU_ITEM(ICON_LedControl, MSG_SET_LEDS_VIOLET, onDrawMenuItem, leds.set_violet);
        #else
          EDIT_ITEM(ICON_LedControl, MSG_COLORS_RED, onDrawPInt8Menu, SetLEDColorR, &leds.color.r);
          EDIT_ITEM(ICON_LedControl, MSG_COLORS_GREEN, onDrawPInt8Menu, SetLEDColorG, &leds.color.g);
          EDIT_ITEM(ICON_LedControl, MSG_COLORS_BLUE, onDrawPInt8Menu, SetLEDColorB, &leds.color.b);
          #if HAS_WHITE_LED
            EDIT_ITEM(ICON_LedControl, MSG_COLORS_WHITE, onDrawPInt8Menu, SetLEDColorW, &leds.color.w);
          #endif
        #endif
      #endif
    }
    UpdateMenu(LedControlMenu);
  }

#endif // LED_CONTROL_MENU

void Draw_Tune_Menu() {
  #if ENABLED(CV_LASER_MODULE)
    if (laser_device.is_laser_device()) return LCD_MESSAGE_F("Not available in laser mode");
  #endif
  checkkey = Menu;
  if (SET_MENU(TuneMenu, MSG_TUNE, 23)) {
    BACK_ITEM(Goto_PrintProcess);
    #if HAS_LCD_BRIGHTNESS
      MENU_ITEM(ICON_Box, MSG_BRIGHTNESS_OFF, onDrawMenuItem, TurnOffBacklight);
    #endif
    #if HAS_FEEDRATE_EDIT
      EDIT_ITEM(ICON_Speed, MSG_SPEED, onDrawPIntMenu, SetSpeed, &feedrate_percentage);
    #endif
    #if HAS_FLOW_EDIT
      EDIT_ITEM(ICON_Flow, MSG_FLOW, onDrawPIntMenu, SetFlow, &planner.flow_percentage[EXT]);
    #endif
    #if HAS_HOTEND
      HotendTargetItem = EDIT_ITEM(ICON_HotendTemp, MSG_UBL_SET_TEMP_HOTEND, onDrawPIntMenu, SetHotendTemp, &thermalManager.temp_hotend[EXT].target);
    #endif
    #if HAS_HEATED_BED
      BedTargetItem = EDIT_ITEM(ICON_BedTemp, MSG_UBL_SET_TEMP_BED, onDrawPIntMenu, SetBedTemp, &thermalManager.temp_bed.target);
    #endif
    #if HAS_FAN
      FanSpeedItem = EDIT_ITEM(ICON_FanSpeed, MSG_FAN_SPEED, onDrawPInt8Menu, SetFanSpeed, &thermalManager.fan_speed[EXT]);
    #endif
    #if HAS_ZOFFSET_ITEM && ANY(BABYSTEP_ZPROBE_OFFSET, JUST_BABYSTEP)
    EDIT_ITEM(ICON_Zoffset, MSG_ZPROBE_ZOFFSET, onDrawPFloat2Menu, SetZOffset, &BABY_Z_VAR);
    #endif
    #if ALL(PROUI_TUNING_GRAPH, PROUI_ITEM_PLOT)
      MENU_ITEM(ICON_PIDNozzle, MSG_HOTEND_TEMP_GRAPH, onDrawMenuItem, drawHPlot);
      MENU_ITEM(ICON_PIDBed, MSG_BED_TEMP_GRAPH, onDrawMenuItem, drawBPlot);
    #endif
    #if HAS_LOCKSCREEN
      MENU_ITEM(ICON_Lock, MSG_LOCKSCREEN, onDrawMenuItem, DWIN_LockScreen);
    #endif
    #if ENABLED(ADVANCED_PAUSE_FEATURE)
      MENU_ITEM(ICON_FilMan, MSG_FILAMENTCHANGE, onDrawMenuItem, ChangeFilament);
    #endif
    #if HAS_FILAMENT_SENSOR
      EDIT_ITEM(ICON_Runout, MSG_RUNOUT_ENABLE, onDrawChkbMenu, SetRunoutEnable, &runout.enabled);
    #endif
    #if ALL(PROUI_ITEM_PLR, POWER_LOSS_RECOVERY)
      EDIT_ITEM(ICON_Pwrlossr, MSG_OUTAGE_RECOVERY, onDrawChkbMenu, SetPwrLossr, &recovery.enabled);
    #endif
    #if ENABLED(SHOW_SPEED_IND)
      EDIT_ITEM(ICON_MaxSpeed, MSG_SPEED_IND, onDrawChkbMenu, SetSpdInd, &HMI_data.SpdInd);
    #endif
    #if ENABLED(FWRETRACT)
      MENU_ITEM(ICON_FWRetract, MSG_FWRETRACT, onDrawSubMenu, Draw_FWRetract_Menu);
    #endif
    #if ALL(PROUI_ITEM_JD, HAS_JUNCTION_DEVIATION)
      EDIT_ITEM(ICON_JDmm, MSG_JUNCTION_DEVIATION, onDrawPFloat3Menu, SetJDmm, &planner.junction_deviation_mm);
    #endif
    #if ALL(PROUI_ITEM_ADVK, LIN_ADVANCE)
      EDIT_ITEM(ICON_MaxAccelerated, MSG_ADVANCE_K, onDrawPFloat3Menu, SetLA_K, &planner.extruder_advance_K[EXT]);
    #endif
    #if ENABLED(EDITABLE_DISPLAY_TIMEOUT)
      EDIT_ITEM(ICON_RemainTime, MSG_SCREEN_TIMEOUT, onDrawPInt8Menu, SetTimer, &ui.backlight_timeout_minutes);
    #endif
    #if HAS_LCD_BRIGHTNESS
      EDIT_ITEM(ICON_Brightness, MSG_BRIGHTNESS, onDrawPInt8Menu, SetBrightness, &ui.brightness);
    #endif
    #if ENABLED(CASE_LIGHT_MENU)
      EDIT_ITEM(ICON_CaseLight, MSG_CASE_LIGHT, onDrawChkbMenu, SetCaseLight, &caselight.on);
      #if CASELIGHT_USES_BRIGHTNESS
        EDIT_ITEM(ICON_Brightness, MSG_CASE_LIGHT_BRIGHTNESS, onDrawPInt8Menu, SetCaseLightBrightness, &caselight.brightness);
      #endif
      #if ENABLED(LED_CONTROL_MENU)
        MENU_ITEM(ICON_LedControl, MSG_LED_CONTROL, onDrawSubMenu, Draw_LedControl_Menu);
      #endif
    #elif ENABLED(LED_CONTROL_MENU) && DISABLED(CASE_LIGHT_USE_NEOPIXEL)
      EDIT_ITEM(ICON_LedControl, MSG_LEDS, onDrawChkbMenu, SetLedStatus, &leds.lights_on);
    #endif
  }
  UpdateMenu(TuneMenu);
}

#if ENABLED(ADAPTIVE_STEP_SMOOTHING)
  void SetAdaptiveStepSmoothing() {
    Toggle_Chkb_Line(HMI_data.AdaptiveStepSmoothing);
  }
#endif

#if ENABLED(SHAPING_MENU)
  void ApplyShapingFreq() { stepper.set_shaping_frequency(HMI_value.axis, MenuData.Value * 0.01); }
  void ApplyShapingZeta() { stepper.set_shaping_damping_ratio(HMI_value.axis, MenuData.Value * 0.01); }

  #if ENABLED(INPUT_SHAPING_X)
    void onDrawShapingXFreq(MenuItemClass* menuitem, int8_t line) { onDrawFloatMenu(menuitem, line, 2, stepper.get_shaping_frequency(X_AXIS)); }
    void onDrawShapingXZeta(MenuItemClass* menuitem, int8_t line) { onDrawFloatMenu(menuitem, line, 2, stepper.get_shaping_damping_ratio(X_AXIS)); }
    void SetShapingXFreq() { HMI_value.axis = X_AXIS; SetFloatOnClick(0, 200, 2, stepper.get_shaping_frequency(X_AXIS), ApplyShapingFreq); }
    void SetShapingXZeta() { HMI_value.axis = X_AXIS; SetFloatOnClick(0, 1, 2, stepper.get_shaping_damping_ratio(X_AXIS), ApplyShapingZeta); }
  #endif

  #if ENABLED(INPUT_SHAPING_Y)
    void onDrawShapingYFreq(MenuItemClass* menuitem, int8_t line) { onDrawFloatMenu(menuitem, line, 2, stepper.get_shaping_frequency(Y_AXIS)); }
    void onDrawShapingYZeta(MenuItemClass* menuitem, int8_t line) { onDrawFloatMenu(menuitem, line, 2, stepper.get_shaping_damping_ratio(Y_AXIS)); }
    void SetShapingYFreq() { HMI_value.axis = Y_AXIS; SetFloatOnClick(0, 200, 2, stepper.get_shaping_frequency(Y_AXIS), ApplyShapingFreq); }
    void SetShapingYZeta() { HMI_value.axis = Y_AXIS; SetFloatOnClick(0, 1, 2, stepper.get_shaping_damping_ratio(Y_AXIS), ApplyShapingZeta); }
  #endif

  void Draw_InputShaping_Menu() {
    checkkey = Menu;
    if (SET_MENU(InputShapingMenu, MSG_INPUT_SHAPING, 5)) {
      BACK_ITEM(Draw_Motion_Menu);
      #if ENABLED(INPUT_SHAPING_X)
        MENU_ITEM(ICON_ShapingX, MSG_SHAPING_A_FREQ, onDrawShapingXFreq, SetShapingXFreq);
        MENU_ITEM(ICON_ShapingX, MSG_SHAPING_A_ZETA, onDrawShapingXZeta, SetShapingXZeta);
      #endif
      #if ENABLED(INPUT_SHAPING_Y)
        MENU_ITEM(ICON_ShapingY, MSG_SHAPING_B_FREQ, onDrawShapingYFreq, SetShapingYFreq);
        MENU_ITEM(ICON_ShapingY, MSG_SHAPING_B_ZETA, onDrawShapingYZeta, SetShapingYZeta);
      #endif
    }
    UpdateMenu(InputShapingMenu);
  }

#endif

#if HAS_TRINAMIC_CONFIG
  #if AXIS_IS_TMC(X)
    void SetXTMCCurrent() { SetPIntOnClick(MIN_TMC_CURRENT, MAX_TMC_CURRENT, []{ stepperX.refresh_stepper_current(); }); }
  #endif
  #if AXIS_IS_TMC(Y)
    void SetYTMCCurrent() { SetPIntOnClick(MIN_TMC_CURRENT, MAX_TMC_CURRENT, []{ stepperY.refresh_stepper_current(); }); }
  #endif
  #if AXIS_IS_TMC(Z)
    void SetZTMCCurrent() { SetPIntOnClick(MIN_TMC_CURRENT, MAX_TMC_CURRENT, []{ stepperZ.refresh_stepper_current(); }); }
  #endif
  #if AXIS_IS_TMC(E0)
    void SetETMCCurrent() { SetPIntOnClick(MIN_TMC_CURRENT, MAX_TMC_CURRENT, []{ stepperE0.refresh_stepper_current(); }); }
  #endif

  void Draw_TrinamicConfig_menu() {
    checkkey = Menu;
    if (SET_MENU(TrinamicConfigMenu, MSG_TMC_DRIVERS, 5)) {
      BACK_ITEM(Draw_AdvancedSettings_Menu);
      #if AXIS_IS_TMC(X)
        EDIT_ITEM(ICON_TMCXSet, MSG_TMC_ACURRENT, onDrawPIntMenu, SetXTMCCurrent, &stepperX.val_mA);
      #endif
      #if AXIS_IS_TMC(Y)
        EDIT_ITEM(ICON_TMCYSet, MSG_TMC_BCURRENT, onDrawPIntMenu, SetYTMCCurrent, &stepperY.val_mA);
      #endif
      #if AXIS_IS_TMC(Z)
        EDIT_ITEM(ICON_TMCZSet, MSG_TMC_CCURRENT, onDrawPIntMenu, SetZTMCCurrent, &stepperZ.val_mA);
      #endif
      #if AXIS_IS_TMC(E0)
        EDIT_ITEM(ICON_TMCESet, MSG_TMC_ECURRENT, onDrawPIntMenu, SetETMCCurrent, &stepperE0.val_mA);
      #endif
    }
    UpdateMenu(TrinamicConfigMenu);
  }

#endif

void Draw_Motion_Menu() {
  checkkey = Menu;
  if (SET_MENU(MotionMenu, MSG_MOTION, 8)) {
    BACK_ITEM(Draw_Control_Menu);
    MENU_ITEM(ICON_MaxSpeed, MSG_SPEED, onDrawSubMenu, Draw_MaxSpeed_Menu);
    MENU_ITEM(ICON_MaxAccelerated, MSG_ACCELERATION, onDrawSubMenu, Draw_MaxAccel_Menu);
    #if ENABLED(CLASSIC_JERK)
      MENU_ITEM(ICON_MaxJerk, MSG_JERK, onDrawSubMenu, Draw_MaxJerk_Menu);
    #elif HAS_JUNCTION_DEVIATION
      EDIT_ITEM(ICON_JDmm, MSG_JUNCTION_DEVIATION, onDrawPFloat3Menu, SetJDmm, &planner.junction_deviation_mm);
    #endif
    #if ENABLED(EDITABLE_STEPS_PER_UNIT)
      MENU_ITEM(ICON_Step, MSG_STEPS_PER_MM, onDrawSubMenu, Draw_Steps_Menu);
    #endif
    #if ENABLED(SHAPING_MENU)
      MENU_ITEM(ICON_InputShaping, MSG_INPUT_SHAPING, onDrawSubMenu, Draw_InputShaping_Menu);
    #endif
    #if ENABLED(LIN_ADVANCE)
      EDIT_ITEM(ICON_MaxAccelerated, MSG_ADVANCE_K, onDrawPFloat3Menu, SetLA_K, &planner.extruder_advance_K[EXT]);
    #endif
    #if ENABLED(ADAPTIVE_STEP_SMOOTHING)
      EDIT_ITEM(ICON_CloseMotor, MSG_STEP_SMOOTHING, onDrawChkbMenu, SetAdaptiveStepSmoothing, &HMI_data.AdaptiveStepSmoothing);
    #endif
  }
  UpdateMenu(MotionMenu);
}

void Draw_FilamentMan_Menu() {
  checkkey = Menu;
  if (SET_MENU(FilamentMenu, MSG_FILAMENT_MAN, 8)) {
    BACK_ITEM(Draw_Prepare_Menu);
    MENU_ITEM(ICON_FilSet, MSG_FILAMENT_SET, onDrawSubMenu, Draw_FilSet_Menu);
    #if ENABLED(FWRETRACT)
      MENU_ITEM(ICON_FWRetract, MSG_FWRETRACT, onDrawSubMenu, Draw_FWRetract_Menu);
    #endif
    #if HAS_FEEDRATE_EDIT
      EDIT_ITEM(ICON_Speed, MSG_SPEED, onDrawPIntMenu, SetSpeed, &feedrate_percentage);
    #endif
    #if HAS_FLOW_EDIT
      EDIT_ITEM(ICON_Flow, MSG_FLOW, onDrawPIntMenu, SetFlow, &planner.flow_percentage[EXT]);
    #endif
    #if ENABLED(ADVANCED_PAUSE_FEATURE)
      MENU_ITEM(ICON_FilMan, MSG_FILAMENTCHANGE, onDrawMenuItem, ChangeFilament);
    #endif
    #if ENABLED(FILAMENT_LOAD_UNLOAD_GCODES)
      MENU_ITEM(ICON_FilUnload, MSG_FILAMENTUNLOAD, onDrawMenuItem, UnloadFilament);
      MENU_ITEM(ICON_FilLoad, MSG_FILAMENTLOAD, onDrawMenuItem, LoadFilament);
    #endif
  }
  UpdateMenu(FilamentMenu);
}

#if HAS_PREHEAT

  void Draw_Preheat_Menu(bool NotCurrent) {
    checkkey = Menu;
    if (NotCurrent) {
      BACK_ITEM(Draw_Temperature_Menu);
      #if HAS_HOTEND
        EDIT_ITEM(ICON_HotendTemp, MSG_UBL_SET_TEMP_HOTEND, onDrawPIntMenu, SetPreheatEndTemp, &ui.material_preset[HMI_value.Select].hotend_temp);
      #endif
      #if HAS_HEATED_BED
        EDIT_ITEM(ICON_BedTemp, MSG_UBL_SET_TEMP_BED, onDrawPIntMenu, SetPreheatBedTemp, &ui.material_preset[HMI_value.Select].bed_temp);
      #endif
      #if HAS_FAN
        EDIT_ITEM(ICON_FanSpeed, MSG_FAN_SPEED, onDrawPIntMenu, SetPreheatFanSpeed, &ui.material_preset[HMI_value.Select].fan_speed);
      #endif
      #if ENABLED(EEPROM_SETTINGS)
        MENU_ITEM(ICON_WriteEEPROM, MSG_STORE_EEPROM, onDrawMenuItem, WriteEeprom);
      #endif
    }
    UpdateMenu(PreheatMenu);
  }

  #define _Preheat_Menu(N) \
    void Draw_Preheat## N ##_Menu() { \
      HMI_value.Select = (N) - 1; \
      Draw_Preheat_Menu(SET_MENU(PreheatMenu, MSG_PREHEAT_## N ##_SETTINGS, 5)); \
    }
  REPEAT_1(PREHEAT_COUNT, _Preheat_Menu)

#endif // HAS_PREHEAT

void Draw_Temperature_Menu() {
  checkkey = Menu;
  if (SET_MENU(TemperatureMenu, MSG_TEMPERATURE, 7 + PREHEAT_COUNT)) {
    BACK_ITEM(Draw_Control_Menu);
    #if HAS_HOTEND
      HotendTargetItem = EDIT_ITEM(ICON_HotendTemp, MSG_UBL_SET_TEMP_HOTEND, onDrawPIntMenu, SetHotendTemp, &thermalManager.temp_hotend[EXT].target);
    #endif
    #if HAS_HEATED_BED
      BedTargetItem = EDIT_ITEM(ICON_BedTemp, MSG_UBL_SET_TEMP_BED, onDrawPIntMenu, SetBedTemp, &thermalManager.temp_bed.target);
    #endif
    #if HAS_FAN
      FanSpeedItem = EDIT_ITEM(ICON_FanSpeed, MSG_FAN_SPEED, onDrawPInt8Menu, SetFanSpeed, &thermalManager.fan_speed[EXT]);
    #endif
    #if ENABLED(PIDTEMPBED) && ANY(PIDTEMP, MPCTEMP) // w/ Bed + Hotend (PID/MPC)
      MENU_ITEM(ICON_Temperature, MSG_PID_SETTINGS, onDrawSubMenu, Draw_PID_Menu);
    #elif ENABLED(PIDTEMP) && ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU) // w/o Bed, only Hotend (PID)
      MENU_ITEM(ICON_Temperature, MSG_HOTEND_PID_SETTINGS, onDrawSubMenu, Draw_HotendPID_Menu);
    #elif ENABLED(MPCTEMP) && ANY(MPC_EDIT_MENU, MPC_AUTOTUNE_MENU) // w/o Bed, only Hotend (MPC)
      MENU_ITEM(ICON_MPCNozzle, MSG_MPC_SETTINGS, onDrawSubMenu, Draw_HotendMPC_Menu);
    #elif ENABLED(PIDTEMPBED) && DISABLED(PIDTEMP) && ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU) // only Bed PID
      MENU_ITEM(ICON_BedTemp, MSG_BED_PID_SETTINGS, onDrawSubMenu, Draw_BedPID_Menu);
    #endif
    #if HAS_PREHEAT
      #define _ITEM_SETPREHEAT(N) MENU_ITEM(ICON_SetPreheat##N, MSG_PREHEAT_## N ##_SETTINGS, onDrawSubMenu, Draw_Preheat## N ##_Menu);
      REPEAT_1(PREHEAT_COUNT, _ITEM_SETPREHEAT)
    #endif
  }
  UpdateMenu(TemperatureMenu);
}

void Draw_MaxSpeed_Menu() {
  checkkey = Menu;
  if (SET_MENU(MaxSpeedMenu, MSG_MAX_SPEED, 5)) {
    BACK_ITEM(Draw_Motion_Menu);
    #if HAS_X_AXIS
      EDIT_ITEM(ICON_MaxSpeedX, MSG_VMAX_A, onDrawPFloatMenu, SetMaxSpeedX, &planner.settings.max_feedrate_mm_s[X_AXIS]);
    #endif
    #if HAS_Y_AXIS
      EDIT_ITEM(ICON_MaxSpeedY, MSG_VMAX_B, onDrawPFloatMenu, SetMaxSpeedY, &planner.settings.max_feedrate_mm_s[Y_AXIS]);
    #endif
    #if HAS_Z_AXIS
      EDIT_ITEM(ICON_MaxSpeedZ, MSG_VMAX_C, onDrawPFloatMenu, SetMaxSpeedZ, &planner.settings.max_feedrate_mm_s[Z_AXIS]);
    #endif
    #if HAS_HOTEND
      EDIT_ITEM(ICON_MaxSpeedE, MSG_VMAX_E, onDrawPFloatMenu, SetMaxSpeedE, &planner.settings.max_feedrate_mm_s[E_AXIS]);
    #endif
  }
  UpdateMenu(MaxSpeedMenu);
}

void Draw_MaxAccel_Menu() {
  checkkey = Menu;
  if (SET_MENU(MaxAccelMenu, MSG_ACC, 5)) {
    BACK_ITEM(Draw_Motion_Menu);
    #if HAS_X_AXIS
      EDIT_ITEM(ICON_MaxAccX, MSG_AMAX_A, onDrawPInt32Menu, SetMaxAccelX, &planner.settings.max_acceleration_mm_per_s2[X_AXIS]);
    #endif
    #if HAS_Y_AXIS
      EDIT_ITEM(ICON_MaxAccY, MSG_AMAX_B, onDrawPInt32Menu, SetMaxAccelY, &planner.settings.max_acceleration_mm_per_s2[Y_AXIS]);
    #endif
    #if HAS_Z_AXIS
      EDIT_ITEM(ICON_MaxAccZ, MSG_AMAX_C, onDrawPInt32Menu, SetMaxAccelZ, &planner.settings.max_acceleration_mm_per_s2[Z_AXIS]);
    #endif
    #if HAS_HOTEND
      EDIT_ITEM(ICON_MaxAccE, MSG_AMAX_E, onDrawPInt32Menu, SetMaxAccelE, &planner.settings.max_acceleration_mm_per_s2[E_AXIS]);
    #endif
  }
  UpdateMenu(MaxAccelMenu);
}

#if ENABLED(CLASSIC_JERK)

  void Draw_MaxJerk_Menu() {
    checkkey = Menu;
    if (SET_MENU(MaxJerkMenu, MSG_MAX_JERK, 5)) {
      BACK_ITEM(Draw_Motion_Menu);
      #if HAS_X_AXIS
        EDIT_ITEM(ICON_MaxSpeedJerkX, MSG_VA_JERK, onDrawPFloatMenu, SetMaxJerkX, &planner.max_jerk.x);
      #endif
      #if HAS_Y_AXIS
        EDIT_ITEM(ICON_MaxSpeedJerkY, MSG_VB_JERK, onDrawPFloatMenu, SetMaxJerkY, &planner.max_jerk.y);
      #endif
      #if HAS_Z_AXIS
        EDIT_ITEM(ICON_MaxSpeedJerkZ, MSG_VC_JERK, onDrawPFloatMenu, SetMaxJerkZ, &planner.max_jerk.z);
      #endif
      #if HAS_HOTEND
        EDIT_ITEM(ICON_MaxSpeedJerkE, MSG_VE_JERK, onDrawPFloatMenu, SetMaxJerkE, &planner.max_jerk.e);
      #endif
    }
    UpdateMenu(MaxJerkMenu);
  }

#endif // CLASSIC_JERK

#if ENABLED(EDITABLE_STEPS_PER_UNIT)

  void Draw_Steps_Menu() {
    checkkey = Menu;
    if (SET_MENU(StepsMenu, MSG_STEPS_PER_MM, 5)) {
      BACK_ITEM(Draw_Motion_Menu);
      #if HAS_X_AXIS
        EDIT_ITEM(ICON_StepX, MSG_A_STEPS, onDrawPFloat2Menu, SetStepsX, &planner.settings.axis_steps_per_mm[X_AXIS]);
      #endif
      #if HAS_Y_AXIS
        EDIT_ITEM(ICON_StepY, MSG_B_STEPS, onDrawPFloat2Menu, SetStepsY, &planner.settings.axis_steps_per_mm[Y_AXIS]);
      #endif
      #if HAS_Z_AXIS
        EDIT_ITEM(ICON_StepZ, MSG_C_STEPS, onDrawPFloat2Menu, SetStepsZ, &planner.settings.axis_steps_per_mm[Z_AXIS]);
      #endif
      #if HAS_HOTEND
        EDIT_ITEM(ICON_StepE, MSG_E_STEPS, onDrawPFloat2Menu, SetStepsE, &planner.settings.axis_steps_per_mm[E_AXIS]);
      #endif
    }
    UpdateMenu(StepsMenu);
  }

#endif

//=============================================================================
// UI editable custom colors
//=============================================================================

#if HAS_CUSTOM_COLORS

  void RestoreDefaultColors() {
    DWIN_SetColorDefaults();
    DWINUI::SetColors(HMI_data.Text_Color, HMI_data.Background_Color, HMI_data.TitleBg_Color);
    DWIN_RedrawScreen();
  }

  void SelColor() {
    MenuData.P_Int = (int16_t*)static_cast<MenuItemPtrClass*>(CurrentMenu->SelectedItem())->value;
    HMI_value.Color.r = GetRColor(*MenuData.P_Int);  // Red
    HMI_value.Color.g = GetGColor(*MenuData.P_Int);  // Green
    HMI_value.Color.b = GetBColor(*MenuData.P_Int);  // Blue
    Draw_GetColor_Menu();
  }

  void LiveRGBColor() {
      HMI_value.Color[CurrentMenu->line() - 2] = MenuData.Value;
      const uint16_t color = RGB(HMI_value.Color.r, HMI_value.Color.g, HMI_value.Color.b);
      DWIN_Draw_Rectangle(1, color, 20, 315, DWIN_WIDTH - 20, 335);
  }
  void SetRGBColor() {
    const uint8_t color = static_cast<MenuItemClass*>(CurrentMenu->SelectedItem())->icon;
    SetIntOnClick(0, (color == 1) ? 63 : 31, HMI_value.Color[color], nullptr, LiveRGBColor);
  }

  void DWIN_ApplyColor() {
    *MenuData.P_Int = RGB(HMI_value.Color.r, HMI_value.Color.g, HMI_value.Color.b);
    DWINUI::SetColors(HMI_data.Text_Color, HMI_data.Background_Color, HMI_data.TitleBg_Color);
    Draw_SelectColors_Menu();
    hash_changed = true;
    LCD_MESSAGE(MSG_COLORS_APPLIED);
    DWIN_Draw_Dashboard();
  }

  void DWIN_ApplyColor(const int8_t element, const bool ldef/*=false*/) {
    const uint16_t color = RGB(HMI_value.Color.r, HMI_value.Color.g, HMI_value.Color.b);
    switch (element) {
      case  2: HMI_data.Background_Color = ldef ? Def_Background_Color : color; DWINUI::SetBackgroundColor(HMI_data.Background_Color); break;
      case  3: HMI_data.Cursor_Color     = ldef ? Def_Cursor_Color     : color; break;
      case  4: HMI_data.TitleBg_Color    = ldef ? Def_TitleBg_Color    : color; DWINUI::SetButtonColor(HMI_data.TitleBg_Color); break;
      case  5: HMI_data.TitleTxt_Color   = ldef ? Def_TitleTxt_Color   : color; break;
      case  6: HMI_data.Text_Color       = ldef ? Def_Text_Color       : color; DWINUI::SetTextColor(HMI_data.Text_Color); break;
      case  7: HMI_data.Selected_Color   = ldef ? Def_Selected_Color   : color; break;
      case  8: HMI_data.SplitLine_Color  = ldef ? Def_SplitLine_Color  : color; break;
      case  9: HMI_data.Highlight_Color  = ldef ? Def_Highlight_Color  : color; break;
      case 10: HMI_data.StatusBg_Color   = ldef ? Def_StatusBg_Color   : color; break;
      case 11: HMI_data.StatusTxt_Color  = ldef ? Def_StatusTxt_Color  : color; break;
      case 12: HMI_data.PopupBg_Color    = ldef ? Def_PopupBg_Color    : color; break;
      case 13: HMI_data.PopupTxt_Color   = ldef ? Def_PopupTxt_Color   : color; break;
      case 14: HMI_data.AlertBg_Color    = ldef ? Def_AlertBg_Color    : color; break;
      case 15: HMI_data.AlertTxt_Color   = ldef ? Def_AlertTxt_Color   : color; break;
      case 16: HMI_data.PercentTxt_Color = ldef ? Def_PercentTxt_Color : color; break;
      case 17: HMI_data.Barfill_Color    = ldef ? Def_Barfill_Color    : color; break;
      case 18: HMI_data.Indicator_Color  = ldef ? Def_Indicator_Color  : color; break;
      case 19: HMI_data.Coordinate_Color = ldef ? Def_Coordinate_Color : color; break;
      case 20: HMI_data.Bottom_Color     = ldef ? Def_Bottom_Color     : color; break;
      default: break;
    }
  }

  void Draw_SelectColors_Menu() {
    checkkey = Menu;
    if (SET_MENU(SelectColorMenu, MSG_COLORS_SELECT, 21)) {
      BACK_ITEM(Draw_Control_Menu);
      MENU_ITEM(ICON_ResetEEPROM, MSG_RESTORE_DEFAULTS, onDrawMenuItem, RestoreDefaultColors);
      EDIT_ITEM_F(0, "Screen Background", onDrawSelColorItem, SelColor, &HMI_data.Background_Color);
      EDIT_ITEM_F(0, "Cursor", onDrawSelColorItem, SelColor, &HMI_data.Cursor_Color);
      EDIT_ITEM_F(0, "Title Background", onDrawSelColorItem, SelColor, &HMI_data.TitleBg_Color);
      EDIT_ITEM_F(0, "Title Text", onDrawSelColorItem, SelColor, &HMI_data.TitleTxt_Color);
      EDIT_ITEM_F(0, "Text", onDrawSelColorItem, SelColor, &HMI_data.Text_Color);
      EDIT_ITEM_F(0, "Selected", onDrawSelColorItem, SelColor, &HMI_data.Selected_Color);
      EDIT_ITEM_F(0, "Split Line", onDrawSelColorItem, SelColor, &HMI_data.SplitLine_Color);
      EDIT_ITEM_F(0, "Highlight", onDrawSelColorItem, SelColor, &HMI_data.Highlight_Color);
      EDIT_ITEM_F(0, "Status Background", onDrawSelColorItem, SelColor, &HMI_data.StatusBg_Color);
      EDIT_ITEM_F(0, "Status Text", onDrawSelColorItem, SelColor, &HMI_data.StatusTxt_Color);
      EDIT_ITEM_F(0, "Popup Background", onDrawSelColorItem, SelColor, &HMI_data.PopupBg_Color);
      EDIT_ITEM_F(0, "Popup Text", onDrawSelColorItem, SelColor, &HMI_data.PopupTxt_Color);
      EDIT_ITEM_F(0, "Alert Background", onDrawSelColorItem, SelColor, &HMI_data.AlertBg_Color);
      EDIT_ITEM_F(0, "Alert Text", onDrawSelColorItem, SelColor, &HMI_data.AlertTxt_Color);
      EDIT_ITEM_F(0, "Percent Text", onDrawSelColorItem, SelColor, &HMI_data.PercentTxt_Color);
      EDIT_ITEM_F(0, "Bar Fill", onDrawSelColorItem, SelColor, &HMI_data.Barfill_Color);
      EDIT_ITEM_F(0, "Indicator value", onDrawSelColorItem, SelColor, &HMI_data.Indicator_Color);
      EDIT_ITEM_F(0, "Coordinate value", onDrawSelColorItem, SelColor, &HMI_data.Coordinate_Color);
      EDIT_ITEM_F(0, "Bottom Line", onDrawSelColorItem, SelColor, &HMI_data.Bottom_Color);
    }
    UpdateMenu(SelectColorMenu);
  }

  void Draw_GetColor_Menu() {
    checkkey = Menu;
    if (SET_MENU(GetColorMenu, MSG_COLORS_GET, 5)) {
      BACK_ITEM(DWIN_ApplyColor);
      MENU_ITEM(ICON_Cancel, MSG_BUTTON_CANCEL, onDrawMenuItem, Draw_SelectColors_Menu);
      MENU_ITEM(0, MSG_COLORS_RED,   onDrawGetColorItem, SetRGBColor);
      MENU_ITEM(1, MSG_COLORS_GREEN, onDrawGetColorItem, SetRGBColor);
      MENU_ITEM(2, MSG_COLORS_BLUE,  onDrawGetColorItem, SetRGBColor);
    }
    UpdateMenu(GetColorMenu);
    DWIN_Draw_Rectangle(1, *MenuData.P_Int, 20, 315, DWIN_WIDTH - 20, 335);
  }

#endif

//=============================================================================
// Nozzle and Bed PID/MPC
//=============================================================================

#if ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU) || ANY(MPC_EDIT_MENU, MPC_AUTOTUNE_MENU)

  void Draw_PID_Menu() {
    checkkey = Menu;
    if (SET_MENU(PIDMenu, MSG_PID_SETTINGS, 3)) {
      BACK_ITEM(Draw_Temperature_Menu);
      #if ENABLED(PIDTEMP) && ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU)
        MENU_ITEM(ICON_PIDNozzle, MSG_HOTEND_PID_SETTINGS, onDrawSubMenu, Draw_HotendPID_Menu);
      #elif ENABLED(MPCTEMP) && ANY(MPC_EDIT_MENU, MPC_AUTOTUNE_MENU)
        MENU_ITEM(ICON_MPCNozzle, MSG_MPC_SETTINGS, onDrawSubMenu, Draw_HotendMPC_Menu);
      #endif
      #if ENABLED(PIDTEMPBED) && ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU)
        MENU_ITEM(ICON_PIDBed, MSG_BED_PID_SETTINGS, onDrawSubMenu, Draw_BedPID_Menu);
      #endif
    }
    UpdateMenu(PIDMenu);
  }

#endif

#if ANY(PIDTEMP, PIDTEMPBED, MPCTEMP)
  void Return_PID_Menu() { (PreviousMenu == PIDMenu) ? Draw_PID_Menu() : Draw_Temperature_Menu(); }
#endif

#if ANY(MPC_EDIT_MENU, MPC_AUTOTUNE_MENU)

  #if ENABLED(MPC_EDIT_MENU)
    void SetHeaterPower() { SetPFloatOnClick(1, 200, 1); }
    void SetBlkHeatCapacity() { SetPFloatOnClick(0, 40, 2); }
    void SetSensorResponse() { SetPFloatOnClick(0, 1, 4); }
    void SetAmbientXfer() { SetPFloatOnClick(0, 1, 4); }
    #if ENABLED(MPC_INCLUDE_FAN)
      void onDrawFanAdj(MenuItemClass* menuitem, int8_t line) { onDrawFloatMenu(menuitem, line, 4, thermalManager.temp_hotend[EXT].fanCoefficient()); }
      void ApplyFanAdj() { thermalManager.temp_hotend[EXT].applyFanAdjustment(MenuData.Value / POW(10, 4)); }
      void SetFanAdj() { SetFloatOnClick(0, 1, 4, thermalManager.temp_hotend[EXT].fanCoefficient(), ApplyFanAdj); }
    #endif
  #endif

  void Draw_HotendMPC_Menu() {
    checkkey = Menu;
    if (SET_MENU(HotendMPCMenu, MSG_MPC_SETTINGS, 7)) {
      MPC_t &mpc = thermalManager.temp_hotend[EXT].mpc;
      BACK_ITEM(Draw_Temperature_Menu);
      #if ENABLED(MPC_AUTOTUNE_MENU)
        MENU_ITEM(ICON_MPCNozzle, MSG_MPC_AUTOTUNE, onDrawMenuItem, []{ thermalManager.MPC_autotune(EXT, Temperature::MPCTuningType::AUTO); });
      #endif
      #if ENABLED(MPC_EDIT_MENU)
        EDIT_ITEM(ICON_MPCHeater, MSG_MPC_POWER, onDrawPFloatMenu, SetHeaterPower, &mpc.heater_power);
        EDIT_ITEM(ICON_MPCHeatCap, MSG_MPC_BLOCK_HEAT_CAPACITY, onDrawPFloat2Menu, SetBlkHeatCapacity, &mpc.block_heat_capacity);
        EDIT_ITEM(ICON_MPCValue, MSG_SENSOR_RESPONSIVENESS, onDrawPFloat4Menu, SetSensorResponse, &mpc.sensor_responsiveness);
        EDIT_ITEM(ICON_MPCValue, MSG_MPC_AMBIENT_XFER_COEFF, onDrawPFloat4Menu, SetAmbientXfer, &mpc.ambient_xfer_coeff_fan0);
        #if ENABLED(MPC_INCLUDE_FAN)
          EDIT_ITEM(ICON_MPCFan, MSG_MPC_AMBIENT_XFER_COEFF_FAN, onDrawFanAdj, SetFanAdj, &mpc.fan255_adjustment);
        #endif
      #endif
    }
    UpdateMenu(HotendMPCMenu);
  }

#endif // MPC_EDIT_MENU || MPC_AUTOTUNE_MENU

#if ALL(HAS_PID_HEATING, PID_AUTOTUNE_MENU)
  void SetPID(celsius_t t, heater_id_t h) {
    gcode.process_subcommands_now(
      TS(F("G28OXYR10\nG0Z10F300\nG0X"), X_CENTER, F("Y"), Y_CENTER, F("F5000\nM84\nM400"))
    );
    thermalManager.PID_autotune(t, h, HMI_data.PidCycles, true);
  }
  void SetPidCycles() { SetPIntOnClick(3, 50); }
#endif

#if ALL(HAS_PID_HEATING, PID_EDIT_MENU)
  void SetKp() { SetPFloatOnClick(0, 1000, 2); }
  void ApplyPIDi() {
    *MenuData.P_Float = scalePID_i(MenuData.Value / POW(10, 2));
    TERN_(PIDTEMP, thermalManager.updatePID();)
  }
  void ApplyPIDd() {
    *MenuData.P_Float = scalePID_d(MenuData.Value / POW(10, 2));
    TERN_(PIDTEMP, thermalManager.updatePID();)
  }
  void SetKi() {
    MenuData.P_Float = (float*)static_cast<MenuItemPtrClass*>(CurrentMenu->SelectedItem())->value;
    const float value = unscalePID_i(*MenuData.P_Float);
    SetFloatOnClick(0, 1000, 2, value, ApplyPIDi);
  }
  void SetKd() {
    MenuData.P_Float = (float*)static_cast<MenuItemPtrClass*>(CurrentMenu->SelectedItem())->value;
    const float value = unscalePID_d(*MenuData.P_Float);
    SetFloatOnClick(0, 1000, 2, value, ApplyPIDd);
  }
  void onDrawPIDi(MenuItemClass* menuitem, int8_t line) { onDrawFloatMenu(menuitem, line, 2, unscalePID_i(*(float*)static_cast<MenuItemPtrClass*>(menuitem)->value)); }
  void onDrawPIDd(MenuItemClass* menuitem, int8_t line) { onDrawFloatMenu(menuitem, line, 2, unscalePID_d(*(float*)static_cast<MenuItemPtrClass*>(menuitem)->value)); }
#endif

#if ENABLED(PIDTEMP) && ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU)

  #if ENABLED(PID_AUTOTUNE_MENU)
    void HotendPID() { SetPID(HMI_data.HotendPidT, H_E0); }
    void SetHotendPidT() { SetPIntOnClick(MIN_ETEMP, MAX_ETEMP); }
  #endif

  void Draw_HotendPID_Menu() {
    checkkey = Menu;
    if (SET_MENU(HotendPIDMenu, MSG_HOTEND_PID_SETTINGS, 7)) {
      BACK_ITEM(Return_PID_Menu);
      #if ENABLED(PID_AUTOTUNE_MENU)
        MENU_ITEM(ICON_PIDNozzle, MSG_HOTEND_TUNE, onDrawMenuItem, HotendPID);
        EDIT_ITEM(ICON_Temperature, MSG_TEMPERATURE, onDrawPIntMenu, SetHotendPidT, &HMI_data.HotendPidT);
        EDIT_ITEM(ICON_PIDCycles, MSG_PID_CYCLE, onDrawPIntMenu, SetPidCycles, &HMI_data.PidCycles);
      #endif
      #if ENABLED(PID_EDIT_MENU)
        EDIT_ITEM(ICON_PIDValue, MSG_PID_SET_KP, onDrawPFloat2Menu, SetKp, &thermalManager.temp_hotend[EXT].pid.Kp);
        EDIT_ITEM(ICON_PIDValue, MSG_PID_SET_KI, onDrawPIDi, SetKi, &thermalManager.temp_hotend[EXT].pid.Ki);
        EDIT_ITEM(ICON_PIDValue, MSG_PID_SET_KD, onDrawPIDd, SetKd, &thermalManager.temp_hotend[EXT].pid.Kd);
      #endif
    }
    UpdateMenu(HotendPIDMenu);
  }

#endif // PIDTEMP && (PID_AUTOTUNE_MENU || PID_EDIT_MENU)

#if ENABLED(PIDTEMPBED) && ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU)

  #if ENABLED(PID_AUTOTUNE_MENU)
    void BedPID() { SetPID(HMI_data.BedPidT, H_BED); }
    void SetBedPidT() { SetPIntOnClick(MIN_BEDTEMP, MAX_BEDTEMP); }
  #endif

  void Draw_BedPID_Menu() {
    checkkey = Menu;
    if (SET_MENU(BedPIDMenu, MSG_BED_PID_SETTINGS, 7)) {
      BACK_ITEM(Return_PID_Menu);
      #if ENABLED(PID_AUTOTUNE_MENU)
        MENU_ITEM(ICON_PIDBed, MSG_BED_TUNE, onDrawMenuItem, BedPID);
        EDIT_ITEM(ICON_Temperature, MSG_TEMPERATURE, onDrawPIntMenu, SetBedPidT, &HMI_data.BedPidT);
        EDIT_ITEM(ICON_PIDCycles, MSG_PID_CYCLE, onDrawPIntMenu, SetPidCycles, &HMI_data.PidCycles);
      #endif
      #if ENABLED(PID_EDIT_MENU)
        EDIT_ITEM(ICON_PIDValue, MSG_PID_SET_KP, onDrawPFloat2Menu, SetKp, &thermalManager.temp_bed.pid.Kp);
        EDIT_ITEM(ICON_PIDValue, MSG_PID_SET_KI, onDrawPIDi, SetKi, &thermalManager.temp_bed.pid.Ki);
        EDIT_ITEM(ICON_PIDValue, MSG_PID_SET_KD, onDrawPIDd, SetKd, &thermalManager.temp_bed.pid.Kd);
      #endif
    }
    UpdateMenu(BedPIDMenu);
  }

#endif // PIDTEMPBED

//=============================================================================
//
//=============================================================================

#if HAS_ZOFFSET_ITEM

  void Draw_ZOffsetWiz_Menu() {
    checkkey = Menu;
    if (SET_MENU(ZOffsetWizMenu, MSG_PROBE_WIZARD, 6)) {
      BACK_ITEM(Draw_Prepare_Menu);
      MENU_ITEM(ICON_Homing, MSG_AUTO_HOME, onDrawMenuItem, AutoHome);
      MENU_ITEM(ICON_AxisD, MSG_MOVE_NOZZLE_TO_BED, onDrawMenuItem, SetMoveZto0);
      EDIT_ITEM(ICON_Fade, MSG_XATC_UPDATE_Z_OFFSET, onDrawPFloat2Menu, SetZOffset, &BABY_Z_VAR);
      MENU_ITEM_F(ICON_HotendTemp,"For Best Results:\n", onDrawMenuItem);
      MENU_ITEM_F(ICON_Cancel, "Have Nozzle Touch Bed", onDrawMenuItem);
    }
    UpdateMenu(ZOffsetWizMenu);
    if (!axis_is_trusted(Z_AXIS)) { LCD_MESSAGE_F("..CAUTION: unknown Z position, Home Z axis."); }
    else { LCD_MESSAGE_F("..Center Nozzle - As Nozzle touches bed, save Z-Offset."); }
  }

#endif

#if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)

  void Draw_Homing_Menu() {
    checkkey = Menu;
    if (SET_MENU(HomingMenu, MSG_HOMING, 8)) {
      BACK_ITEM(Draw_Prepare_Menu);
      MENU_ITEM(ICON_Homing, MSG_AUTO_HOME, onDrawMenuItem, AutoHome);
      #if HAS_X_AXIS
        MENU_ITEM(ICON_HomeX, MSG_AUTO_HOME_X, onDrawMenuItem, HomeX);
      #endif
      #if HAS_Y_AXIS
        MENU_ITEM(ICON_HomeY, MSG_AUTO_HOME_Y, onDrawMenuItem, HomeY);
      #endif
      #if HAS_Z_AXIS
        MENU_ITEM(ICON_HomeZ, MSG_AUTO_HOME_Z, onDrawMenuItem, HomeZ);
      #endif
      #if ENABLED(NOZZLE_PARK_FEATURE)
        MENU_ITEM(ICON_Park, MSG_FILAMENT_PARK_ENABLED, onDrawMenuItem, ParkHead);
      #endif
      MENU_ITEM(ICON_MoveZ, MSG_TOOL_CHANGE_ZLIFT, onDrawMenuItem, RaiseHead);
      #if ENABLED(MESH_BED_LEVELING)
        EDIT_ITEM(ICON_ZAfterHome, MSG_Z_AFTER_HOME, onDrawPInt8Menu, SetZAfterHoming, &HMI_data.z_after_homing);
      #endif
    }
    UpdateMenu(HomingMenu);
  }

#endif

#if ENABLED(FWRETRACT)

  void Draw_FWRetract_Menu() {
    checkkey = Menu;
    if (SET_MENU(FWRetractMenu, MSG_FWRETRACT, 6)) {
      BACK_ITEM(Return_FWRetract_Menu);
      EDIT_ITEM(ICON_FWRetract, MSG_CONTROL_RETRACT, onDrawPFloatMenu, SetRetractLength, &fwretract.settings.retract_length);
      EDIT_ITEM(ICON_FWSpeed, MSG_SINGLENOZZLE_RETRACT_SPEED, onDrawPFloatMenu, SetRetractSpeed, &fwretract.settings.retract_feedrate_mm_s);
      EDIT_ITEM(ICON_FWZRaise, MSG_CONTROL_RETRACT_ZHOP, onDrawPFloat2Menu, SetZRaise, &fwretract.settings.retract_zraise);
      EDIT_ITEM(ICON_FWSpeed, MSG_SINGLENOZZLE_UNRETRACT_SPEED, onDrawPFloatMenu, SetRetractSpeed, &fwretract.settings.retract_recover_feedrate_mm_s);
      EDIT_ITEM(ICON_FWRetract, MSG_CONTROL_RETRACT_RECOVER, onDrawPFloatMenu, SetAddRecover, &fwretract.settings.retract_recover_extra);
    }
    UpdateMenu(FWRetractMenu);
  }

#endif

//=============================================================================
// Mesh Bed Leveling
//=============================================================================

#if HAS_MESH
    // void applyMeshPoints() { PRO_data.grid_max_points = MenuData.Value;
    //   set_bed_leveling_enabled(false);
    //   reset_bed_level();
    //   drawMeshPoints(false, CurrentMenu->line(), MenuData.Value);}


  #if PROUI_EX
    void ApplyMeshPoints() { ProEx.ApplyMeshPoints(); ReDrawMenu(); }
    void LiveMeshPoints() { ProEx.DrawMeshPoints(true, CurrentMenu->line(), MenuData.Value); }
    void SetMeshPoints() {
      SetOnClick(SetIntNoDraw, GRID_MIN, GRID_LIMIT, 0, PRO_data.grid_max_points, ApplyMeshPoints, LiveMeshPoints);
      ProEx.DrawMeshPoints(true, CurrentMenu->line(), PRO_data.grid_max_points);
    }
  #else
    void ApplyMeshPoints() { HMI_data.grid_max_points = MenuData.Value; }
    void SetMeshPoints() { SetPIntOnClick(GRID_MIN, GRID_LIMIT, ApplyMeshPoints); }
  #endif
    void ApplyMeshInset() { reset_bed_level(); ReDrawItem(); }
    void SetMeshInset()  { SetPFloatOnClick(MIN_MESH_INSET, MAX_MESH_INSET, UNITFDIGITS, ApplyMeshInset);  }
    void MaxMeshArea() {
      MESH_MIN_X = 0;
      MESH_MAX_X = X_BED_SIZE;
      MESH_MIN_Y = 0;
      MESH_MAX_Y = Y_BED_SIZE;
      reset_bed_level();
      ReDrawMenu();
    }
    void CenterMeshArea() {
      float max = (MESH_MIN_X + MESH_MIN_Y) * 0.5;
      if (max < X_BED_SIZE - MESH_MAX_X) { max = X_BED_SIZE - MESH_MAX_X; }
      if (max < MESH_MIN_Y) { max = MESH_MIN_Y; }
      if (max < Y_BED_SIZE - MESH_MAX_Y) { max = Y_BED_SIZE - MESH_MAX_Y; }
      MESH_MIN_X = max;
      MESH_MAX_X = X_BED_SIZE - max;
      MESH_MIN_Y = max;
      MESH_MAX_Y = Y_BED_SIZE - max;
      reset_bed_level();
      ReDrawMenu();
    }

  #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
    void ApplyMeshFadeHeight() { set_z_fade_height(planner.z_fade_height); }
    void SetMeshFadeHeight() { SetPFloatOnClick(0, 100, 1, ApplyMeshFadeHeight); }
  #endif

  #if ENABLED(ACTIVATE_MESH_ITEM)

    void SetMeshActive() {
      const bool val = planner.leveling_active;
      set_bed_leveling_enabled(!planner.leveling_active);
      #if ENABLED(AUTO_BED_LEVELING_UBL)
        if (!val) {
          if (planner.leveling_active && bedlevel.storage_slot >= 0)
            { ui.status_printf(0, GET_TEXT_F(MSG_MESH_ACTIVE), bedlevel.storage_slot); }
          else
            { ui.set_status(GET_TEXT_F(MSG_UBL_MESH_INVALID)); }
        }
        else { ui.reset_status(true); }
      #else
        UNUSED(val);
      #endif
      Show_Chkb_Line(planner.leveling_active);
    }

  #endif

  #if ALL(HAS_HEATED_BED, PREHEAT_BEFORE_LEVELING)
    void SetBedLevT() { SetPIntOnClick(MIN_BEDTEMP, MAX_BEDTEMP); }
  #endif

  #if ENABLED(PROUI_MESH_EDIT)
    bool AutoMovToMesh = false;
    void SetAutoMovToMesh() { Toggle_Chkb_Line(AutoMovToMesh); }

    void LiveEditMesh()  { ((MenuItemPtrClass*)EditZValueItem)->value = &bedlevel.z_values[HMI_value.Select ? bedLevelTools.mesh_x : MenuData.Value][HMI_value.Select ? MenuData.Value : bedLevelTools.mesh_y]; EditZValueItem->redraw(); }
    void LiveEditMeshZ() { *MenuData.P_Float = MenuData.Value / POW(10, 3); if (AutoMovToMesh) { bedLevelTools.MoveToZ(); } }
    void ApplyEditMeshX() { bedLevelTools.mesh_x = MenuData.Value; if (AutoMovToMesh) { bedLevelTools.MoveToXY(); } }
    void ApplyEditMeshY() { bedLevelTools.mesh_y = MenuData.Value; if (AutoMovToMesh) { bedLevelTools.MoveToXY(); } }
    void ZeroPoint() { bedLevelTools.manual_value_update(bedLevelTools.mesh_x, bedLevelTools.mesh_y, true); EditZValueItem->redraw(); LCD_MESSAGE(MSG_ZERO_MESH); }
    void ZeroMesh()  { bedLevelTools.mesh_reset(); LCD_MESSAGE(MSG_MESH_RESET); }
    void SetEditMeshX() { HMI_value.Select = 0; SetIntOnClick(0, GRID_MAX_POINTS_X - 1, bedLevelTools.mesh_x, ApplyEditMeshX, LiveEditMesh); }
    void SetEditMeshY() { HMI_value.Select = 1; SetIntOnClick(0, GRID_MAX_POINTS_Y - 1, bedLevelTools.mesh_y, ApplyEditMeshY, LiveEditMesh); }
    void SetEditZValue() { SetPFloatOnClick(Z_OFFSET_MIN, Z_OFFSET_MAX, 3, nullptr, LiveEditMeshZ); if (AutoMovToMesh) { bedLevelTools.MoveToXYZ(); } }

    // Zero or Reset Bed Mesh Values
    void Popup_ResetMesh() { DWIN_Popup_ConfirmCancel(ICON_Info_0, F("Reset Current Mesh?")); }
    void OnClick_ResetMesh() {
      if (HMI_flag.select_flag) {
        HMI_ReturnScreen();
        ZeroMesh();
        DONE_BUZZ(true);
      }
      else { HMI_ReturnScreen(); }
    }
    void ResetMesh() { Goto_Popup(Popup_ResetMesh, OnClick_ResetMesh); }
  #endif

#endif // HAS_MESH

#if HAS_MESH
  void Draw_MeshSet_Menu() {
    checkkey = Menu;
    if (SET_MENU(MeshMenu, MSG_MESH_LEVELING, 7)) {
      BACK_ITEM(Draw_AdvancedSettings_Menu);
      #if ENABLED(ACTIVATE_MESH_ITEM)
        EDIT_ITEM(ICON_UBLActive, MSG_ACTIVATE_MESH, onDrawChkbMenu, SetMeshActive, &planner.leveling_active);
      #endif
      #if PROUI_EX
        MENU_ITEM(ICON_MeshPoints, MSG_MESH_POINTS, onDrawMeshPoints, SetMeshPoints);
      #else
        EDIT_ITEM(ICON_MeshPoints, MSG_MESH_POINTS, onDrawPInt8Menu, SetMeshPoints, &HMI_data.grid_max_points);
      #endif
      MENU_ITEM(ICON_ProbeMargin, MSG_MESH_INSET, onDrawSubMenu, Draw_MeshInset_Menu);
      #if ALL(HAS_HEATED_BED, PREHEAT_BEFORE_LEVELING)
        EDIT_ITEM(ICON_Temperature, MSG_UBL_SET_TEMP_BED, onDrawPIntMenu, SetBedLevT, &HMI_data.BedLevT);
      #endif
      #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
        EDIT_ITEM(ICON_Fade, MSG_Z_FADE_HEIGHT, onDrawPFloatMenu, SetMeshFadeHeight, &planner.z_fade_height);
      #endif
      #if ENABLED(AUTO_BED_LEVELING_UBL)
        EDIT_ITEM(ICON_Tilt, MSG_UBL_TILTING_GRID, onDrawPInt8Menu, SetUBLTiltGrid, &bedLevelTools.tilt_grid);
      #endif
    }
    UpdateMenu(MeshMenu);
  }

  #if ENABLED(PROUI_MESH_EDIT)
    void Draw_EditMesh_Menu() {
      if (!leveling_is_valid()) { LCD_MESSAGE(MSG_UBL_MESH_INVALID); return; }
      TERN_(HAS_LEVELING, set_bed_leveling_enabled(false);)
      checkkey = Menu;
      if (SET_MENU(EditMeshMenu, MSG_MESH_EDITOR, 7)) {
        bedLevelTools.mesh_x = bedLevelTools.mesh_y = 0;
        BACK_ITEM(Draw_AdvancedSettings_Menu);
        EDIT_ITEM(ICON_SetHome, MSG_PROBE_WIZARD_MOVING, onDrawChkbMenu, SetAutoMovToMesh, &AutoMovToMesh);
        EDIT_ITEM(ICON_MeshEditX, MSG_MESH_X, onDrawPInt8Menu, SetEditMeshX, &bedLevelTools.mesh_x);
        EDIT_ITEM(ICON_MeshEditY, MSG_MESH_Y, onDrawPInt8Menu, SetEditMeshY, &bedLevelTools.mesh_y);
        EditZValueItem = EDIT_ITEM(ICON_MeshEditZ, MSG_MESH_EDIT_Z, onDrawPFloat3Menu, SetEditZValue, &bedlevel.z_values[bedLevelTools.mesh_x][bedLevelTools.mesh_y]);
        TERN_(HAS_BED_PROBE, MENU_ITEM(ICON_Probe, MSG_PROBE_WIZARD_PROBING, onDrawMenuItem, bedLevelTools.ProbeXY);)
        MENU_ITEM(ICON_SetZOffset, MSG_ZERO_MESH, onDrawMenuItem, ZeroPoint);
      }
      UpdateMenu(EditMeshMenu);
    }
  #endif

  void Draw_MeshInset_Menu() {
    checkkey = Menu;
    if (SET_MENU(MeshInsetMenu, MSG_MESH_INSET, 7)) {
      BACK_ITEM(Draw_MeshSet_Menu);
      EDIT_ITEM(ICON_Box, MSG_MESH_MIN_X, onDrawPFloatMenu, SetMeshInset, &ui.mesh_inset_min_x);
      EDIT_ITEM(ICON_ProbeMargin, MSG_MESH_MAX_X, onDrawPFloatMenu, SetMeshInset, &ui.mesh_inset_max_x);
      EDIT_ITEM(ICON_Box, MSG_MESH_MIN_Y, onDrawPFloatMenu, SetMeshInset, &ui.mesh_inset_min_y);
      EDIT_ITEM(ICON_ProbeMargin, MSG_MESH_MAX_Y, onDrawPFloatMenu, SetMeshInset, &ui.mesh_inset_max_y);
      MENU_ITEM(ICON_AxisC, MSG_MESH_AMAX, onDrawMenuItem, MaxMeshArea);
      MENU_ITEM(ICON_SetHome, MSG_MESH_CENTER, onDrawMenuItem, CenterMeshArea);
    }
    UpdateMenu(MeshInsetMenu);
    LCD_MESSAGE_F("..Center Area sets mesh equidistant by greatest inset from edge.");
  }

#endif  // HAS_MESH

//=============================================================================
// CV Laser Module support
//=============================================================================
#if ENABLED(CV_LASER_MODULE)

  #if HAS_HOME_OFFSET
    // Make the current position 0,0,0
    void SetHome() {
      laser_device.homepos += current_position;
      set_all_homed();
      gcode.process_subcommands_now(F("G92X0Y0Z0"));
      ReDrawMenu();
    }
  #endif

  void LaserOn(const bool turn_on) {
    laser_device.laser_set(turn_on);
    DWIN_Draw_Dashboard();
  }

  void LaserToggle() {
    LaserOn(!laser_device.is_laser_device());
    Show_Chkb_Line(laser_device.is_laser_device());
  }

  void LaserPrint() {
    if (!laser_device.is_laser_device()) return;
    thermalManager.disable_all_heaters(); // 关闭加热107011 -20211012
    print_job_timer.reset();  //107011 -20211009 清除前一次的打印时间
    laser_device.laser_power_open(); // 打开激光, 以最弱的激光输出。专业固件是最好的
    // queue.inject_P(PSTR("M999\nG92.9Z0")); // 107011 -20211013
    card.openAndPrintFile(card.filename);
  }

  void LaserRunRange() {
    if (!laser_device.is_laser_device()) return;
    if (!all_axes_trusted()) return LCD_MESSAGE_F("First set home");
    DWIN_Show_Popup(ICON_TempTooHigh, "LASER", "Run Range", BTN_Cancel);
    HMI_SaveProcessID(WaitResponse);
    laser_device.laser_range();
  }

  void Draw_LaserSettings_Menu() {
    EnableLiveMove = true;
    checkkey = Menu;
    if (SET_MENU(LaserSettings, MSG_LASER_MENU, 7)) {
      BACK_ITEM(ReturnToPreviousMenu);
      EDIT_ITEM(ICON_LaserToggle, MSG_LASER_TOGGLE, onDrawChkbMenu, LaserToggle, &laser_device.laser_enabled);
      MENU_ITEM(ICON_Homing, MSG_AUTO_HOME, onDrawMenuItem, AutoHome);
      EDIT_ITEM_F(ICON_LaserFocus, "Laser Focus", onDrawPFloatMenu, SetMoveZ, &current_position.z);
      EDIT_ITEM(ICON_MoveX, MSG_MOVE_X, onDrawPFloatMenu, SetMoveX, &current_position.x);
      EDIT_ITEM(ICON_MoveY, MSG_MOVE_Y, onDrawPFloatMenu, SetMoveY, &current_position.y);
      TERN_(HAS_HOME_OFFSET, MENU_ITEM_F(ICON_SetHome, "Set as Home position: 0,0,0", onDrawMenuItem, SetHome);)
    }
    UpdateMenu(LaserSettings);
  }

  void Draw_LaserPrint_Menu() {
    if (!laser_device.is_laser_device()) return Goto_Main_Menu();
    checkkey = Menu;
    if (SET_MENU(LaserPrintMenu, MSG_LASER_MENU, 4)) {
      BACK_ITEM(Draw_Print_File_Menu);
      MENU_ITEM(ICON_SetHome, MSG_CONFIGURATION, onDrawSubMenu, Draw_LaserSettings_Menu);
      MENU_ITEM_F(ICON_LaserPrint, "Engrave", onDrawMenuItem, LaserPrint);
      MENU_ITEM_F(ICON_LaserRunRange, "Run Range", onDrawMenuItem, LaserRunRange);
    }
    UpdateMenu(LaserPrintMenu);
    char buf[23], str_1[5], str_2[5];
    sprintf_P(buf, PSTR("XMIN: %s XMAX: %s"), dtostrf(LASER_XMIN, 1, 1, str_1), dtostrf(LASER_XMAX, 1, 1, str_2));
    DWINUI::Draw_String(LBLX, MBASE(4) + 10, buf);
    sprintf_P(buf, PSTR("YMIN: %s YMAX: %s"), dtostrf(LASER_YMIN, 1, 1, str_1), dtostrf(LASER_YMAX, 1, 1, str_2));
    DWINUI::Draw_String(LBLX, MBASE(5) - 10, buf);
  }

#endif // CV_LASER_MODULE

//=============================================================================
// ToolBar
//=============================================================================
#if HAS_TOOLBAR

  void Draw_TBSetup_Menu() {
    checkkey = Menu;
    if (SET_MENU(TBSetupMenu, MSG_TOOLBAR_SETUP, TBMaxOpt + 1)) {
      BACK_ITEM(Draw_Control_Menu);
      for (uint8_t i = 0; i < TBMaxOpt; ++i) EDIT_ITEM_F(0, "", onDrawTBSetupItem, SetTBSetupItem, &PRO_data.TBopt[i]);
    }
    UpdateMenu(TBSetupMenu);
  }

  void Exit_ToolBar() {
    select_page.set(PAGE_ADVANCE);
    ICON_AdvSettings();
    checkkey = MainMenu;
    ToolBar.draw();
    DWIN_UpdateLCD();
  }

  void Goto_ToolBar() {
    checkkey = Menu;
    ToolBar.draw();
  }

#endif  // HAS_TOOLBAR

//=============================================================================
// More Host support
//=============================================================================

#if ENABLED(HOST_SHUTDOWN_MENU_ITEM) && defined(SHUTDOWN_ACTION)

  void PopUp_HostShutDown() { DWIN_Popup_ConfirmCancel(ICON_Info_1, GET_TEXT_F(MSG_HOST_SHUTDOWN)); }
  void OnClick_HostShutDown() {
    if (HMI_flag.select_flag) { hostui.shutdown(); }
    HMI_ReturnScreen();
  }
  void HostShutDown() { Goto_Popup(PopUp_HostShutDown, OnClick_HostShutDown); }

#endif

//=============================================================================

#if DEBUG_DWIN
  void DWIN_Debug(const char *msg) {
    DEBUG_ECHOLNPGM_P(msg);
    DWIN_Show_Popup(ICON_Control_1, STR_DEBUG_PREFIX, msg, BTN_Continue);
    wait_for_user_response();
    Draw_Main_Area();
  }
#endif

#if ENABLED(AUTO_BED_LEVELING_UBL)
void Draw_AdvancedSettings_Menu() {
  checkkey = Menu;
  if (SET_MENU(AdvancedSettings, MSG_UBL_LEVELING, 14)) {
    BACK_ITEM(Goto_Main_Menu);
    #if ENABLED(EEPROM_SETTINGS)
      MENU_ITEM(ICON_WriteEEPROM, MSG_STORE_EEPROM, onDrawMenuItem, WriteEeprom);
    #endif
    #if HAS_BED_PROBE
      MENU_ITEM(ICON_Level, MSG_AUTO_MESH, onDrawMenuItem, AutoLevStart);
      MENU_ITEM(ICON_Tilt, MSG_UBL_TILT_MESH, onDrawMenuItem, UBLMeshTilt);
      MENU_ITEM(ICON_Probe, MSG_ZPROBE_SETTINGS, onDrawSubMenu, Draw_ProbeSet_Menu);
    #endif
    MENU_ITEM(ICON_PrintSize, MSG_MESH_LEVELING, onDrawSubMenu, Draw_MeshSet_Menu);
    MENU_ITEM(ICON_MeshViewer, MSG_MESH_VIEW, onDrawSubMenu, DWIN_MeshViewer);
    #if USE_GRID_MESHVIEWER
      EDIT_ITEM(ICON_PrintSize, MSG_CHANGE_MESH, onDrawChkbMenu, SetViewMesh, &bedLevelTools.view_mesh);
    #endif
    #if ENABLED(PROUI_MESH_EDIT)
      MENU_ITEM(ICON_MeshEdit, MSG_EDIT_MESH, onDrawSubMenu, Draw_EditMesh_Menu);
      MENU_ITEM(ICON_MeshReset, MSG_MESH_RESET, onDrawMenuItem, ResetMesh);
    #endif
    EDIT_ITEM(ICON_UBLSlot, MSG_UBL_STORAGE_SLOT, onDrawUBLSlot, SetUBLSlot, &bedlevel.storage_slot);
    MENU_ITEM(ICON_UBLSaveMesh, MSG_UBL_SAVE_MESH, onDrawMenuItem, SaveMesh);
    MENU_ITEM(ICON_UBLLoadMesh, MSG_UBL_LOAD_MESH, onDrawMenuItem, UBLMeshLoad);
    MENU_ITEM(ICON_UBLSmartFill, MSG_UBL_SMART_FILLIN, onDrawMenuItem, UBLSmartFillMesh);
  }
  ui.reset_status(true);
  UpdateMenu(AdvancedSettings);
}

#elif ENABLED(AUTO_BED_LEVELING_BILINEAR)
void Draw_AdvancedSettings_Menu() {
  checkkey = Menu;
  if (SET_MENU(AdvancedSettings, MSG_BILINEAR_LEVELING, 9)) {
    BACK_ITEM(Goto_Main_Menu);
    #if ENABLED(EEPROM_SETTINGS)
      MENU_ITEM(ICON_WriteEEPROM, MSG_STORE_EEPROM, onDrawMenuItem, SaveMesh);
    #endif
    #if HAS_BED_PROBE
      MENU_ITEM(ICON_Level, MSG_AUTO_MESH, onDrawMenuItem, AutoLevStart);
      MENU_ITEM(ICON_Probe, MSG_ZPROBE_SETTINGS, onDrawSubMenu, Draw_ProbeSet_Menu);
    #endif
    MENU_ITEM(ICON_PrintSize, MSG_MESH_LEVELING, onDrawSubMenu, Draw_MeshSet_Menu);
    MENU_ITEM(ICON_MeshViewer, MSG_MESH_VIEW, onDrawSubMenu, DWIN_MeshViewer);
    #if USE_GRID_MESHVIEWER
      EDIT_ITEM(ICON_PrintSize, MSG_CHANGE_MESH, onDrawChkbMenu, SetViewMesh, &bedLevelTools.view_mesh);
    #endif
    #if ENABLED(PROUI_MESH_EDIT)
      MENU_ITEM(ICON_MeshEdit, MSG_EDIT_MESH, onDrawSubMenu, Draw_EditMesh_Menu);
    #endif
    MENU_ITEM(ICON_MeshReset, MSG_MESH_RESET, onDrawMenuItem, ResetMesh);
  }
  ui.reset_status(true);
  UpdateMenu(AdvancedSettings);
}

#elif ENABLED(MESH_BED_LEVELING)
void Draw_AdvancedSettings_Menu() {
  checkkey = Menu;
  if (SET_MENU(AdvancedSettings, MSG_UBL_MESH_LEVELING, 10)) {
    BACK_ITEM(Goto_Main_Menu);
    MENU_ITEM(ICON_ManualMesh, MSG_UBL_CONTINUE_MESH, onDrawMenuItem, ManualMeshStart);
    MMeshMoveZItem = EDIT_ITEM(ICON_Zoffset, MSG_MESH_EDIT_Z, onDrawPFloat2Menu, SetMMeshMoveZ, &current_position.z);
    MENU_ITEM(ICON_AxisD, MSG_LEVEL_BED_NEXT_POINT, onDrawMenuItem, ManualMeshContinue);
    MENU_ITEM(ICON_PrintSize, MSG_MESH_LEVELING, onDrawSubMenu, Draw_MeshSet_Menu);
    MENU_ITEM(ICON_MeshViewer, MSG_MESH_VIEW, onDrawSubMenu, DWIN_MeshViewer);
    #if USE_GRID_MESHVIEWER
      EDIT_ITEM(ICON_PrintSize, MSG_CHANGE_MESH, onDrawChkbMenu, SetViewMesh, &bedLevelTools.view_mesh);
    #endif
    #if ENABLED(PROUI_MESH_EDIT)
      MENU_ITEM(ICON_MeshEdit, MSG_EDIT_MESH, onDrawSubMenu, Draw_EditMesh_Menu);
    #endif
    MENU_ITEM(ICON_MeshSave, MSG_UBL_SAVE_MESH, onDrawMenuItem, SaveMesh);
    MENU_ITEM(ICON_MeshReset, MSG_MESH_RESET, onDrawMenuItem, ResetMesh);
  }
  ui.reset_status(true);
  UpdateMenu(AdvancedSettings);
}

#else // Default-No Probe
void Draw_AdvancedSettings_Menu() {
  checkkey = Menu;
  if (SET_MENU(AdvancedSettings, MSG_ADVANCED_SETTINGS, 18)) {
    BACK_ITEM(Goto_Main_Menu);
    #if ENABLED(EEPROM_SETTINGS)
      MENU_ITEM(ICON_ReadEEPROM, MSG_LOAD_EEPROM, onDrawMenuItem, ReadEeprom);
      MENU_ITEM(ICON_ResetEEPROM, MSG_RESTORE_DEFAULTS, onDrawMenuItem, ResetEeprom);
    #endif
    #if HAS_LCD_BRIGHTNESS
      EDIT_ITEM(ICON_Brightness, MSG_BRIGHTNESS, onDrawPInt8Menu, SetBrightness, &ui.brightness);
    #endif
    #if ENABLED(EDITABLE_DISPLAY_TIMEOUT)
      EDIT_ITEM(ICON_RemainTime, MSG_SCREEN_TIMEOUT, onDrawPInt8Menu, SetTimer, &ui.backlight_timeout_minutes);
    #endif
    #if BED_SCREW_INSET
      EDIT_ITEM(ICON_ProbeMargin, MSG_SCREW_INSET, onDrawPFloatMenu, SetRetractSpeed, &ui.screw_pos);
    #endif
    #if ALL(PROUI_ITEM_PLR, POWER_LOSS_RECOVERY)
      EDIT_ITEM(ICON_Pwrlossr, MSG_OUTAGE_RECOVERY, onDrawChkbMenu, SetPwrLossr, &recovery.enabled);
    #endif
    #if ENABLED(SHOW_SPEED_IND)
      EDIT_ITEM(ICON_MaxSpeed, MSG_SPEED_IND, onDrawChkbMenu, SetSpdInd, &HMI_data.SpdInd);
    #endif
    #if ENABLED(SOUND_MENU_ITEM)
      EDIT_ITEM(ICON_Sound, MSG_TICK, onDrawChkbMenu, SetEnableTick, &ui.tick_on);
      EDIT_ITEM(ICON_Sound, MSG_SOUND, onDrawChkbMenu, SetEnableSound, &ui.sound_on);
    #endif
    #if HAS_GCODE_PREVIEW
      EDIT_ITEM(ICON_File, MSG_HAS_PREVIEW, onDrawChkbMenu, SetPreview, &HMI_data.EnablePreview);
    #endif
    #if ENABLED(BAUD_RATE_GCODE)
      EDIT_ITEM(ICON_SetBaudRate, MSG_250K_BAUD, onDrawChkbMenu, SetBaudRate, &HMI_data.Baud250K);
    #endif
    #if ENABLED(PROUI_MEDIASORT)
      EDIT_ITEM(ICON_File, MSG_MEDIA_SORT, onDrawChkbMenu, SetMediaSort, &HMI_data.MediaSort);
    #endif
    EDIT_ITEM(ICON_File, MSG_MEDIA_UPDATE, onDrawChkbMenu, SetMediaAutoMount, &HMI_data.MediaAutoMount);
    #if HAS_TRINAMIC_CONFIG
      MENU_ITEM(ICON_TMCSet, MSG_TMC_DRIVERS, onDrawSubMenu, Draw_TrinamicConfig_menu);
    #endif
    #if ENABLED(PRINTCOUNTER)
      MENU_ITEM(ICON_PrintStatsReset, MSG_INFO_PRINT_COUNT_RESET, onDrawSubMenu, printStatsReset);
    #endif
    #if ALL(ENCODER_RATE_MULTIPLIER, ENC_MENU_ITEM)
      EDIT_ITEM_F(ICON_Motion, "Enc steps/sec 100x", onDrawPIntMenu, SetEncRateA, &ui.enc_rateA);
      EDIT_ITEM_F(ICON_Motion, "Enc steps/sec 10x", onDrawPIntMenu, SetEncRateB, &ui.enc_rateB);
    #endif
  }
  ui.reset_status(true);
  UpdateMenu(AdvancedSettings);
}
#endif

void Draw_Advanced_Menu() { // From Control_Menu (Control) || Default-NP AdvancedSettings_Menu (Level)
  checkkey = Menu;
  if (SET_MENU(AdvancedMenu, MSG_ADVANCED_SETTINGS, 18)) {
    BACK_ITEM(Draw_Control_Menu);
    #if ENABLED(EEPROM_SETTINGS)
      MENU_ITEM(ICON_ReadEEPROM, MSG_LOAD_EEPROM, onDrawMenuItem, ReadEeprom);
      MENU_ITEM(ICON_ResetEEPROM, MSG_RESTORE_DEFAULTS, onDrawMenuItem, ResetEeprom);
    #endif
    #if HAS_LCD_BRIGHTNESS
      EDIT_ITEM(ICON_Brightness, MSG_BRIGHTNESS, onDrawPInt8Menu, SetBrightness, &ui.brightness);
    #endif
    #if ENABLED(EDITABLE_DISPLAY_TIMEOUT)
      EDIT_ITEM(ICON_RemainTime, MSG_SCREEN_TIMEOUT, onDrawPInt8Menu, SetTimer, &ui.backlight_timeout_minutes);
    #endif
    #if BED_SCREW_INSET
      EDIT_ITEM(ICON_ProbeMargin, MSG_SCREW_INSET, onDrawPFloatMenu, SetRetractSpeed, &ui.screw_pos);
    #endif
    #if ALL(PROUI_ITEM_PLR, POWER_LOSS_RECOVERY)
      EDIT_ITEM(ICON_Pwrlossr, MSG_OUTAGE_RECOVERY, onDrawChkbMenu, SetPwrLossr, &recovery.enabled);
    #endif
    #if ENABLED(SHOW_SPEED_IND)
      EDIT_ITEM(ICON_MaxSpeed, MSG_SPEED_IND, onDrawChkbMenu, SetSpdInd, &HMI_data.SpdInd);
    #endif
    #if ENABLED(SOUND_MENU_ITEM)
      EDIT_ITEM(ICON_Sound, MSG_TICK, onDrawChkbMenu, SetEnableTick, &ui.tick_on);
      EDIT_ITEM(ICON_Sound, MSG_SOUND, onDrawChkbMenu, SetEnableSound, &ui.sound_on);
    #endif
    #if HAS_GCODE_PREVIEW
      EDIT_ITEM(ICON_File, MSG_HAS_PREVIEW, onDrawChkbMenu, SetPreview, &HMI_data.EnablePreview);
    #endif
    #if ENABLED(BAUD_RATE_GCODE)
      EDIT_ITEM(ICON_SetBaudRate, MSG_250K_BAUD, onDrawChkbMenu, SetBaudRate, &HMI_data.Baud250K);
    #endif
    #if ENABLED(PROUI_MEDIASORT)
      EDIT_ITEM(ICON_File, MSG_MEDIA_SORT, onDrawChkbMenu, SetMediaSort, &HMI_data.MediaSort);
    #endif
    EDIT_ITEM(ICON_File, MSG_MEDIA_UPDATE, onDrawChkbMenu, SetMediaAutoMount, &HMI_data.MediaAutoMount);
    #if HAS_TRINAMIC_CONFIG
      MENU_ITEM(ICON_TMCSet, MSG_TMC_DRIVERS, onDrawSubMenu, Draw_TrinamicConfig_menu);
    #endif
    #if ENABLED(PRINTCOUNTER)
      MENU_ITEM(ICON_PrintStatsReset, MSG_INFO_PRINT_COUNT_RESET, onDrawSubMenu, printStatsReset);
    #endif
    #if ALL(ENCODER_RATE_MULTIPLIER, ENC_MENU_ITEM)
      EDIT_ITEM_F(ICON_Motion, "Enc steps/sec 100x", onDrawPIntMenu, SetEncRateA, &ui.enc_rateA);
      EDIT_ITEM_F(ICON_Motion, "Enc steps/sec 10x", onDrawPIntMenu, SetEncRateB, &ui.enc_rateB);
    #endif
  }
  ui.reset_status(true);
  UpdateMenu(AdvancedMenu);
}

#endif // DWIN_LCD_PROUI
