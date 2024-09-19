/**
 * DWIN Enhanced implementation for PRO UI
 * Based on the original work of: Miguel A. Risco-Castillo (MRISCOC)
 * Version: 3.11.1
 * Date: 2022/02/28
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

#include "dwin.h"

typedef void (*popupDrawFunc_t)();
typedef void (*popupClickFunc_t)();
extern popupDrawFunc_t Draw_Popup;

void Draw_Select_Highlight(const bool sel, const uint16_t ypos);
inline void Draw_Select_Highlight(const bool sel) { Draw_Select_Highlight(sel, 280); }
void DWIN_Popup_ConfirmCancel(const uint8_t icon, FSTR_P const fmsg2);
void Goto_Popup(const popupDrawFunc_t fnDraw, const popupClickFunc_t fnClick=nullptr);
void HMI_Popup();
#if ENABLED(ADVANCED_PAUSE_FEATURE)
  void DWIN_Popup_Pause(FSTR_P const fmsg, uint8_t button=0);
  void Draw_Popup_FilamentPurge();
#endif

inline void Draw_Popup_Bkgd() {
  DWIN_Draw_Rectangle(1, HMI_data.PopupBg_Color, 14, 60, 258, 330);
  DWIN_Draw_Rectangle(0, HMI_data.Highlight_Color, 14, 60, 258, 330);
}

template<typename T, typename U>
void DWIN_Draw_Popup(const uint8_t icon, T amsg1=nullptr, U amsg2=nullptr, uint8_t button=0) {
  xy_uint8_t pos;
  switch (icon) {
    default: pos.set(81, 90); break;          // Icon#:1-8,90-91,93-94; W:110px|H:100px
    case 17 ... 24: pos.set(96, 90); break;   // Icon#:17-24;           W: 80px|H:100px
    case 78 ... 81: pos.set(100, 107); break; // Icon#:78-81;           W: 73px|H: 66px
  }
  DWINUI::ClearMainArea();
  Draw_Popup_Bkgd();
  if (icon)   DWINUI::Draw_Icon(icon, pos.x, pos.y);
  if (amsg1)  DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 210, amsg1);
  if (amsg2)  DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 240, amsg2);
  if (button) DWINUI::Draw_Button(button, 86, 280, true);
  HMI_SaveProcessID(button ? Popup : NothingToDo);
  DWIN_UpdateLCD();
}

template<typename T, typename U>
inline void DWIN_Popup_Confirm(const uint8_t icon, T amsg1, U amsg2) {
  DWIN_Draw_Popup(icon, amsg1, amsg2, BTN_Confirm); // Button Confirm
}

template<typename T, typename U>
inline void DWIN_Popup_Continue(const uint8_t icon, T amsg1, U amsg2) {
  DWIN_Draw_Popup(icon, amsg1, amsg2, BTN_Continue); // Button Continue
}

#if DEBUG_DWIN
  template<typename T, typename U, typename V, typename W>
  void DWIN_Debug_Popup(T amsg1=nullptr, U amsg2=nullptr, V amsg3=nullptr, W amsg4=nullptr) {
    HMI_SaveProcessID(WaitResponse);
    DWINUI::ClearMainArea();
    Draw_Popup_Bkgd();
    DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 160, STR_DEBUG_PREFIX);
    if (amsg1) DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 180, amsg1);
    if (amsg2) DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 210, amsg2);
    if (amsg3) DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 240, amsg3);
    if (amsg4) DWINUI::Draw_CenteredString(HMI_data.PopupTxt_Color, 270, amsg4);
    DWINUI::Draw_Button(BTN_Confirm, 86, 300, true);
    DWIN_UpdateLCD();
  }
#endif
