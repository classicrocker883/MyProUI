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
#pragma once

#include "../../../inc/MarlinConfig.h"

#include "dwinui.h"
#include "../common/encoder.h"
#include "../common/limits.h"
#include "../../../MarlinCore.h"
#if ENABLED(LED_CONTROL_MENU)
  #include "../../../feature/leds/leds.h"
#endif

#if HAS_CGCODE
  #include "custom_gcodes.h"
#endif

#if HAS_MEDIA
  #include "../../../sd/cardreader.h"
#endif

#if ENABLED(CV_LASER_MODULE)
  #include "cv_laser_module.h"
#endif

extern char DateTime[16+1];

enum processID : uint8_t {
  // Process ID
  MainMenu,
  Menu,
  SetInt,
  SetFloat,
  SetPInt,
  SetPFloat,
  SetIntNoDraw,
  PrintProcess,
  Popup,
  Leveling,
  Locked,
  PrintDone,
  ESDiagProcess,
  PlotProcess,
  WaitResponse,
  Homing,
  PIDProcess,
  MPCProcess,
  NothingToDo
};

#if ANY(HAS_PID_HEATING, MPCTEMP)
  enum tempcontrol_t : uint8_t {
    AUTOTUNE_DONE,
    #if HAS_PID_HEATING
      OPTITEM(PIDTEMP, PID_EXTR_START)
      OPTITEM(PIDTEMPBED, PID_BED_START)
      OPTITEM(PIDTEMPCHAMBER, PID_CHAMBER_START)
      PID_BAD_HEATER_ID,
      PID_TEMP_TOO_HIGH,
      PID_TUNING_TIMEOUT,
    #endif
    #if ENABLED(MPCTEMP)
      MPCTEMP_START,
      MPC_TEMP_ERROR,
      MPC_INTERRUPTED,
    #endif
  };
#endif

typedef struct {
  int8_t r, g, b;
  void set(int8_t _r, int8_t _g, int8_t _b) { r = _r; g = _g; b = _b; }
  int8_t& operator[](const int i) {
    switch (i) {
      default:
      case 0: return r;
      case 1: return g;
      case 2: return b;
    }
  }
} rgb_t;

typedef struct {
  rgb_t Color;                        // Color
  #if ANY(HAS_PID_HEATING, MPCTEMP)
    tempcontrol_t tempControl = AUTOTUNE_DONE;
  #endif
  uint8_t Select = 0;                 // Auxiliary selector variable
  AxisEnum axis = X_AXIS;             // Axis Select
} HMI_value_t;

extern HMI_value_t HMI_value;

typedef struct {
  bool printing_flag:1; // sd or host printing
  bool abort_flag:1;    // sd or host was aborted
  bool pause_flag:1;    // printing is paused
  bool select_flag:1;   // Popup button selected
  bool cancel_lev:1; // cancel current abl
} HMI_flag_t;

extern HMI_flag_t HMI_flag;
extern uint8_t checkkey;

inline bool Printing()      { return printingIsActive() || printingIsPaused(); }
inline bool SD_Printing()   { return Printing() && IS_SD_FILE_OPEN(); }
inline bool Host_Printing() { return Printing() && !IS_SD_FILE_OPEN(); }

// Popups
#if HAS_HOTEND || HAS_HEATED_BED
  void DWIN_Popup_Temperature(const int_fast8_t heater_id, const uint8_t state);
#endif
#if ENABLED(POWER_LOSS_RECOVERY)
  void Popup_PowerLossRecovery();
#endif

// Tool Functions
uint32_t GetHash(char * str);
#if ENABLED(EEPROM_SETTINGS)
  void WriteEeprom();
  void ReadEeprom();
  void ResetEeprom();
  #if HAS_MESH
    void ManualMeshSave();
    void SaveMesh();
  #endif
#endif
#if ALL(PROUI_TUNING_GRAPH, PROUI_ITEM_PLOT)
  void dwinDrawPlot(tempcontrol_t result);
  void drawHPlot();
  void drawBPlot();
  void drawCPlot();
#endif
#if ENABLED(ENC_MENU_ITEM)
  void SetEncRateA();
  void SetEncRateB();
#endif
#if ENABLED(PROUI_ITEM_ENC)
  void SetRevRate();
#endif
#if ALL(HAS_BLTOUCH_HS_MODE, HS_MENU_ITEM)
  void SetHSMode();
#endif
#if PROUI_EX
  void SetMeshPoints();
  void SetXMeshInset();
  void SetYMeshInset();
  void MaxMeshArea();
  void CenterMeshArea();
#endif
#if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
  void SetMeshFadeHeight();
#endif
#if HAS_BED_PROBE
  void AutoLev();
  void AutoLevStart();
  void PopUp_StartAutoLev();
  void OnClick_StartAutoLev();
#else
  void HomeZandDisable();
#endif
void RebootPrinter();
void DisableMotors();
void AutoHome();
void SetRetractSpeed();
#if HAS_PREHEAT
  #define _DOPREHEAT(N) void DoPreheat##N();
  REPEAT_1(PREHEAT_COUNT, _DOPREHEAT)
#endif
void DoCoolDown();
#if ENABLED(BAUD_RATE_GCODE)
  void SetBaud115K();
  void SetBaud250K();
#endif
#if HAS_LCD_BRIGHTNESS
  void TurnOffBacklight();
#endif
void ApplyExtMinT();
void ParkHead();
void RaiseHead();
TERN(HAS_BED_PROBE, float, void) tram(uint8_t point OPTARG(HAS_BED_PROBE, bool stow_probe=true));
#if HAS_BED_PROBE && ENABLED(PROUI_ITEM_TRAM)
  void Trammingwizard();
  void TramwizStart();
  void OnClick_StartTramwiz();
  void PopUp_StartTramwiz();
#endif
#if ALL(LED_CONTROL_MENU, HAS_COLOR_LEDS)
  void ApplyLEDColor();
#endif
#if ENABLED(AUTO_BED_LEVELING_UBL)
  void UBLMeshTilt();
  void UBLMeshSave();
  void UBLMeshLoad();
#endif
#if ENABLED(HOST_SHUTDOWN_MENU_ITEM) && defined(SHUTDOWN_ACTION)
  void HostShutDown();
#endif

// Other
void Goto_PrintProcess();
void Goto_Main_Menu();
void Goto_Info_Menu();
void Goto_PowerLossRecovery();
void Goto_ConfirmToPrint();
void Draw_Main_Area();      // Redraw main area
void DWIN_Draw_Dashboard(); // Status Area
void DWIN_DrawStatusLine(PGM_P text); // Draw simple status text
inline void DWIN_DrawStatusLine(FSTR_P fstr) { DWIN_DrawStatusLine(FTOP(fstr)); }
void DWIN_RedrawDash();     // Redraw Dash and Status line
void DWIN_RedrawScreen();   // Redraw all screen elements
void HMI_MainMenu();        // Main process screen
void HMI_Printing();        // Print page
void HMI_ReturnScreen();    // Return to previous screen before popups
void HMI_WaitForUser();
void HMI_SaveProcessID(const uint8_t id);
void HMI_SDCardUpdate();
void EachMomentUpdate();
void update_variable();
void Init();
void DWIN_InitScreen();
void DWIN_HandleScreen();
void DWIN_CheckStatusMessage();
void DWIN_HomingStart();
void DWIN_HomingDone();
#if HAS_MESH
  void DWIN_MeshUpdate(const int8_t cpos, const int8_t tpos, const_float_t zval);
#endif
#if HAS_LEVELING
  void DWIN_LevelingStart();
  void DWIN_LevelingDone();
#endif
void DWIN_Print_Started();
void DWIN_Print_Pause();
void DWIN_Print_Resume();
void DWIN_Print_Finished();
void DWIN_Print_Aborted();
void DWIN_Print_Header(PGM_P const cstr=nullptr);
void DWIN_SetColorDefaults();
void DWIN_CopySettingsTo(char * const buff);
void DWIN_CopySettingsFrom(PGM_P const buff);
void DWIN_SetDataDefaults();
void DWIN_RebootScreen();

#if ENABLED(ADVANCED_PAUSE_FEATURE)
  void DWIN_Popup_Pause(FSTR_P const fmsg, uint8_t button=0);
  void Draw_Popup_FilamentPurge();
  void Goto_FilamentPurge();
#endif

// Utility and extensions
#if HAS_LOCKSCREEN
  void DWIN_LockScreen();
  void DWIN_UnLockScreen();
  void HMI_LockScreen();
#endif
#if HAS_MESH
  void DWIN_MeshViewer();
  #if USE_GRID_MESHVIEWER
    void SetViewMesh();
  #endif
#endif
#if HAS_ESDIAG
  void Draw_EndStopDiag();
#endif

// Menu drawing functions
void Draw_Print_File_Menu();
void Draw_Control_Menu();
void Draw_Advanced_Menu();
void Draw_AdvancedSettings_Menu();
void Draw_Prepare_Menu();
void Draw_Move_Menu();
void Draw_Tramming_Menu();
#if HAS_HOME_OFFSET
  void Draw_HomeOffset_Menu();
#endif
#if HAS_BED_PROBE
  void Draw_ProbeSet_Menu();
#endif
void Draw_FilSet_Menu();
#if ENABLED(NOZZLE_PARK_FEATURE)
  void Draw_ParkPos_Menu();
#endif
void Draw_PhySet_Menu();
#if ALL(CASE_LIGHT_MENU, CASELIGHT_USES_BRIGHTNESS)
  void Draw_CaseLight_Menu();
#endif
#if ENABLED(LED_CONTROL_MENU)
  void Draw_LedControl_Menu();
#endif
void Draw_Tune_Menu();
void Draw_Motion_Menu();
#if ENABLED(ADVANCED_PAUSE_FEATURE)
  void ChangeFilament();
#endif
void Draw_FilamentMan_Menu();
void Draw_Temperature_Menu();
void Draw_MaxSpeed_Menu();
void Draw_MaxAccel_Menu();
#if ENABLED(CLASSIC_JERK)
  void Draw_MaxJerk_Menu();
#endif
#if ENABLED(EDITABLE_STEPS_PER_UNIT)
  void Draw_Steps_Menu();
#endif
#if ANY(HAS_BED_PROBE, BABYSTEPPING)
  void Draw_ZOffsetWiz_Menu();
#endif
#if ENABLED(INDIVIDUAL_AXIS_HOMING_SUBMENU)
  void Draw_Homing_Menu();
#endif
#if ENABLED(FWRETRACT)
  void Draw_FWRetract_Menu();
#endif
#if HAS_MESH
  void Draw_MeshSet_Menu();
  void Draw_MeshInset_Menu();
  #if ENABLED(PROUI_MESH_EDIT)
    void Draw_EditMesh_Menu();
  #endif
#endif
#if HAS_TRINAMIC_CONFIG
  void Draw_TrinamicConfig_menu();
#endif

// Custom colors editing
#if HAS_CUSTOM_COLORS
  void DWIN_ApplyColor();
  void DWIN_ApplyColor(const int8_t element, const bool ldef=false);
  void Draw_SelectColors_Menu();
  void Draw_GetColor_Menu();
#endif

// PID
#if HAS_PID_HEATING
  #include "../../../module/temperature.h"
  void DWIN_M303(const int c, const heater_id_t hid, const celsius_t temp);
  void DWIN_PIDTuning(tempcontrol_t result);
  void Draw_PID_Menu();
  #if ENABLED(PIDTEMP)
    #if ENABLED(PID_AUTOTUNE_MENU)
      void HotendPID();
    #endif
    #if ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU)
      void Draw_HotendPID_Menu();
    #endif
  #endif
  #if ENABLED(PIDTEMPBED)
    #if ENABLED(PID_AUTOTUNE_MENU)
      void BedPID();
    #endif
    #if ANY(PID_AUTOTUNE_MENU, PID_EDIT_MENU)
      void Draw_BedPID_Menu();
    #endif
  #endif
  #if ENABLED(PIDTEMPCHAMBER)
    #if ENABLED(PID_AUTOTUNE_MENU)
      void ChamberPID();
    #endif
    void Draw_ChamberPID_Menu();
  #endif
#endif

// MPC
#if ANY(MPC_EDIT_MENU, MPC_AUTOTUNE_MENU)
  void Draw_HotendMPC_Menu();
#endif
#if ENABLED(MPC_AUTOTUNE)
  void DWIN_MPCTuning(tempcontrol_t result);
#endif

// CV Laser Module
#if ENABLED(CV_LASER_MODULE)
  void LaserOn(const bool turn_on);
  void Draw_LaserSettings_Menu();
  void Draw_LaserPrint_Menu();
  #if HAS_HOME_OFFSET
    void SetHome();
  #endif
#endif

// ToolBar
#if HAS_TOOLBAR
  void Draw_TBSetup_Menu();
  void Goto_ToolBar();
  void Exit_ToolBar();
#endif

#if DEBUG_DWIN
  void DWIN_Debug(PGM_P msg);
#endif
