/**
 * DWIN Single var plot
 * Author: Miguel A. Risco-Castillo
 * Version: 2.2.3
 * Date: 2023/01/29
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
 * For commercial applications additional licenses can be requested
 */

#include "../../../inc/MarlinConfig.h"

#if ALL(DWIN_LCD_PROUI, PROUI_TUNING_GRAPH)

#include "../../marlinui.h"
#include "dwin.h"
#include "plot.h"

#define Plot_Bg_Color RGB( 1, 12,  8)

Plot plot;

uint16_t graphpoints, r, x2, y2 = 0;
frame_rect_t graphframe = {0};
float scale = 0;

void Plot::draw(const frame_rect_t &frame, const_celsius_float_t max, const_float_t ref/*=0*/) {
  graphframe = frame;
  graphpoints = 0;
  scale = frame.h / max;
  x2 = frame.x + frame.w - 1;
  y2 = frame.y + frame.h - 1;
  r = round((y2) - ref * scale);
  DWINUI::Draw_Box(1, Plot_Bg_Color, frame);
  for (uint8_t i = 1; i < 4; i++) if (i * 60 < frame.w) DWIN_Draw_VLine(Line_Color, i * 60 + frame.x, frame.y, frame.h);
  DWINUI::Draw_Box(0, Color_White, DWINUI::ExtendFrame(frame, 1));
  DWIN_Draw_HLine(Color_Red, frame.x, r, frame.w);
}

void Plot::update(const_float_t value) {
  if (!scale) { return; }
  const uint16_t y = round((y2) - value * scale);
  if (graphpoints < graphframe.w) {
    DWIN_Draw_Point(Color_Yellow, 1, 1, graphpoints + graphframe.x, y);
  }
  else {
    DWIN_Frame_AreaMove(1, 0, 1, Plot_Bg_Color, graphframe.x, graphframe.y, x2, y2);
    if ((graphpoints % 60) == 0) DWIN_Draw_VLine(Line_Color, x2 - 1, graphframe.y + 1, graphframe.h - 2);
    DWIN_Draw_Point(Color_Red, 1, 1, x2 - 1, r);
    DWIN_Draw_Point(Color_Yellow, 1, 1, x2 - 1, y);
  }
  graphpoints++;
  #if LCD_BACKLIGHT_TIMEOUT_MINS
    ui.refresh_backlight_timeout();
  #endif
}

#endif // DWIN_LCD_PROUI && PROUI_TUNING_GRAPH
