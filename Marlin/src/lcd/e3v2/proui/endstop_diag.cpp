/**
 * DWIN End Stops diagnostic page for PRO UI
 * Author: Miguel A. Risco-Castillo (MRISCOC)
 * Version: 1.4.3
 * Date: 2023/05/10
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

#if ALL(DWIN_LCD_PROUI, HAS_ESDIAG)

#include "endstop_diag.h"

#include "../../../core/types.h"
#include "../../marlinui.h"
#include "dwin.h"
#include "dwin_popup.h"

#if HAS_FILAMENT_SENSOR
  #include "../../../feature/runout.h"
#endif

#if HAS_BED_PROBE
  #include "../../../module/probe.h"
#endif

ESDiag esDiag;

void draw_es_label(FSTR_P const flabel=nullptr) {
  DWINUI::cursor.x = 40;
  if (flabel) DWINUI::Draw_String(F(flabel));
  DWINUI::Draw_String(F(": "));
  DWINUI::MoveBy(0, 25);
}

void draw_es_state(const bool is_hit, const bool is_fil) {
  const uint8_t LM = 130;
  DWINUI::cursor.x = LM;
  DWIN_Draw_Rectangle(1, HMI_data.PopupBg_Color, LM, DWINUI::cursor.y, LM + 100, DWINUI::cursor.y + 20);
  is_fil ? (is_hit ? DWINUI::Draw_String(RGB(16,63,16), F("PRESENT")) : DWINUI::Draw_String(RGB(31,31,16), F("Runout Detected"))) :
  (is_hit ? DWINUI::Draw_String(RGB(31,31,16), F(STR_ENDSTOP_HIT)) : DWINUI::Draw_String(RGB(16,63,16), F(STR_ENDSTOP_OPEN)));
  DWINUI::MoveBy(0, 25);
}

void ESDiag::draw() {
  Title.ShowCaption(GET_TEXT_F(MSG_ENDSTOP_TEST));
  DWINUI::ClearMainArea();
  Draw_Popup_Bkgd();
  DWINUI::Draw_Button(BTN_Continue, 86, 250, true);
  DWINUI::cursor.y = 80;
  #define ES_LABEL(S) draw_es_label(F(STR_##S))
/*
  #if USE_X_MIN
    ES_LABEL(X_MIN);
  #endif
  TERN_(USE_X_MAX, ES_LABEL(X_MAX));
  #if USE_Y_MIN
    ES_LABEL(Y_MIN);
  #endif
  TERN_(USE_Y_MAX, ES_LABEL(Y_MAX));
  #if !USE_Z_MIN_PROBE //HAS_Z_MIN_PIN
    #if HAS_Z_MIN_PIN
      ES_LABEL(Z_MIN);
    #endif
    TERN_(USE_Z_MAX, ES_LABEL(Z_MAX));
  #endif
  #if HAS_FILAMENT_SENSOR
    draw_es_label(F(STR_FILAMENT));
  #endif
  #if USE_Z_MIN_PROBE
    draw_es_label(F(STR_Z_PROBE));
  #endif
  //TERN_(USE_Z_MIN_PROBE, ES_LABEL(Z_PROBE));
/*/
  TERN_(USE_X_MIN, ES_LABEL(X_MIN);) TERN_(USE_X_MAX, ES_LABEL(X_MAX);)
  TERN_(USE_Y_MIN, ES_LABEL(Y_MIN);) TERN_(USE_Y_MAX, ES_LABEL(Y_MAX);)
  IF_DISABLED(USE_Z_MIN_PROBE, TERN_(USE_Z_MIN, ES_LABEL(Z_MIN);) TERN_(USE_Z_MAX, ES_LABEL(Z_MAX);))
  TERN_(HAS_FILAMENT_SENSOR, ES_LABEL(FILAMENT);) //draw_es_label(F(STR_FILAMENT)));
  TERN_(USE_Z_MIN_PROBE, ES_LABEL(Z_PROBE);) //draw_es_label(F(STR_Z_PROBE));
//*/
  update();
}

void ESDiag::update() {
  DWINUI::cursor.y = 80;
  #define ES_REPORT(S) draw_es_state(READ(S##_PIN) == S##_ENDSTOP_HIT_STATE, false)
/*
  #if USE_X_MIN
    ES_REPORT(X_MIN);
  #endif
  TERN_(USE_X_MAX, ES_REPORT(X_MAX);)
  #if USE_Y_MIN
    ES_REPORT(Y_MIN);
  #endif
  TERN_(USE_Y_MAX, ES_REPORT(Y_MAX);)
  #if !USE_Z_MIN_PROBE
    #if USE_Z_MIN
      ES_REPORT(Z_MIN);
    #endif
    TERN_(USE_Z_MAX, ES_REPORT(Z_MAX);)
  #endif
  #if HAS_FILAMENT_SENSOR
    #if PROUI_EX
      draw_es_state(!FilamentSensorDevice::poll_runout_state(0));
    #else
      //draw_es_state(!FilamentSensorSwitch::poll_runout_states());
      draw_es_state(READ(FIL_RUNOUT1_PIN) != FIL_RUNOUT1_STATE);
    #endif
  #endif
  #if USE_Z_MIN_PROBE
    draw_es_state(READ(Z_MIN_PROBE_PIN) != Z_MIN_PROBE_ENDSTOP_HIT_STATE);
  #endif
  //TERN_(USE_Z_MIN_PROBE, ES_REPORT(Z_MIN_PROBE));
/*/
  TERN_(USE_X_MIN, ES_REPORT(X_MIN);) TERN_(USE_X_MAX, ES_REPORT(X_MAX);)
  TERN_(USE_Y_MIN, ES_REPORT(Y_MIN);) TERN_(USE_Y_MAX, ES_REPORT(Y_MAX);)
  IF_DISABLED(USE_Z_MIN_PROBE, TERN_(USE_Z_MIN, ES_REPORT(Z_MIN);) TERN_(USE_Z_MAX, ES_REPORT(Z_MAX);))
  #if HAS_FILAMENT_SENSOR
    #if PROUI_EX
      draw_es_state(!FilamentSensorDevice::poll_runout_state(0), true);
    #else
      //draw_es_state(!FilamentSensorSwitch::poll_runout_states());
      draw_es_state(READ(FIL_RUNOUT1_PIN) != FIL_RUNOUT1_STATE, true);
    #endif
  #endif
  //TERN_(USE_Z_MIN_PROBE, draw_es_state(!Z_MIN_PROBE_ENDSTOP_HIT_STATE);
  //TERN_(USE_Z_MIN_PROBE, draw_es_state(READ(Z_MIN_PROBE_PIN) != Z_MIN_PROBE_ENDSTOP_HIT_STATE));
  TERN_(USE_Z_MIN_PROBE, ES_REPORT(Z_MIN_PROBE);)
//*/
  DWIN_UpdateLCD();
}

#endif // DWIN_LCD_PROUI && HAS_ESDIAG
