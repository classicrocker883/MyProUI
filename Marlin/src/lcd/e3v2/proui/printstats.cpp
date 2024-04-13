/**
 * Print Stats page for PRO UI
 * Author: Miguel A. Risco-Castillo (MRISCOC)
 * Version: 1.4.0
 * Date: 2022/12/03
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

#if ALL(DWIN_LCD_PROUI, PRINTCOUNTER)

#include "printstats.h"

#include "../../../MarlinCore.h"
#include "../../marlinui.h"
#include "../../../module/printcounter.h"
#include "dwin_popup.h"

PrintStats printStats;

void PrintStats::draw() {
  char str[30] = "";
  constexpr int8_t MRG = 30;

  Title.ShowCaption(GET_TEXT_F(MSG_INFO_STATS_MENU));
  DWINUI::ClearMainArea();
  Draw_Popup_Bkgd();
  DWINUI::Draw_Button(BTN_Continue, 86, 250, true);
  printStatistics ps = print_job_timer.getStats();

  DWINUI::Draw_String(MRG,  80, TS(GET_TEXT_F(MSG_INFO_PRINT_COUNT), F(": "), ps.totalPrints));
  DWINUI::Draw_String(MRG, 100, TS(GET_TEXT_F(MSG_INFO_COMPLETED_PRINTS), F(": "), ps.finishedPrints));
  duration_t(print_job_timer.getStats().printTime).toDigital(str, true);
  DWINUI::Draw_String(MRG, 120, TS(GET_TEXT_F(MSG_INFO_PRINT_TIME), F(": "), str));
  duration_t(print_job_timer.getStats().longestPrint).toDigital(str, true);
  DWINUI::Draw_String(MRG, 140, TS(GET_TEXT_F(MSG_INFO_PRINT_LONGEST), F(": "), str));
  DWINUI::Draw_String(MRG, 160, TS(GET_TEXT_F(MSG_INFO_PRINT_FILAMENT), F(": "), p_float_t(ps.filamentUsed / 1000, 2), F(" m")));
}

void PrintStats::reset() {
  print_job_timer.initStats();
  DONE_BUZZ(true);
}

void gotoPrintStats() {
  printStats.draw();
  HMI_SaveProcessID(WaitResponse);
}

//Print Stats Reset Popup
void Popup_ResetStats() { DWIN_Popup_ConfirmCancel(ICON_Info_1, GET_TEXT_F(MSG_RESET_STATS)); }
void OnClick_ResetStats() {
  if (HMI_flag.select_flag) { PrintStats::reset(); }
  HMI_ReturnScreen();
}
void printStatsReset() { Goto_Popup(Popup_ResetStats, OnClick_ResetStats); }

#endif // DWIN_LCD_PROUI && PRINTCOUNTER
