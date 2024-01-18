/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2021 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
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
#pragma once

// Extended and default UI Colors
#define RGB(R,G,B) (R << 11) | (G << 5) | (B) // R: 0..31, G: 0..63, B: 0..31
#define GetRColor(color) ((color >> 11) & 0x1F)
#define GetGColor(color) ((color >>  5) & 0x3F)
#define GetBColor(color) ((color >>  0) & 0x1F)

// RGB565 colors: https://rgbcolorpicker.com/565
#define Color_Voxelab_Red   0xA804 //0xF00F
#define COLOR_CYAN          0x07FF
#define COLOR_WHITE         0xFFFF
#define COLOR_YELLOW        0xFF0F //RGB(0x1F,0x3F,0x00)
#define COLOR_RED           0xA804 //RGB(0x1F,0x00,0x00)
#define COLOR_ERROR_RED     0xC000 // Error!
#define COLOR_BG_RED        Color_Voxelab_Red // Red background color
#define COLOR_BG_WINDOW     0x31E8 // Popup background color
#define COLOR_BG_BLUE       0x1125 // Dark blue background color
#define COLOR_BG_BLACK      0x0841 // Black background color
#define COLOR_ICONBLUE      0x45FA // Lighter blue that matches icons/accentsts
//#define Color_Silver         // 0xC618 // Greyish line seperator
#define COLOR_POPUP_TEXT    0xD6BA // Popup font background color
#define COLOR_LINE          COLOR_WHITE // Split line color 0x3A6A
#define COLOR_RECTANGLE     Color_Voxelab_Red // Blue square cursor color 0xEE2F
#define COLOR_PERCENT       COLOR_WHITE // Percentage color 0xFE29
#define COLOR_BARFILL       0x10E4 // Fill color of progress bar
#define COLOR_SELECT        0x8430 // Selected color 0x33BB

#if ENABLED(DWIN_CREALITY_LCD_JYERSUI)
#define Custom_Colors 10
#define COLOR_CHECKBOX      0x4E5C  // Check-box check color
#define COLOR_CONFIRM       0x34B9
#define COLOR_CANCEL        0x3186
#endif

// Extended and default UI Colors
#define COLOR_BLACK           0x0000
#define COLOR_GREEN           RGB(0, 63, 0)
#define COLOR_BLUE            0x015F  //RGB(0, 0, 31)
#define COLOR_LIGHT_WHITE     0xBDD7
#define COLOR_LIGHT_GREEN     0x3460
#define COLOR_LIGHT_CYAN      0x04F3
#define COLOR_LIGHT_BLUE      0x3A6A
#define COLOR_MAGENTA         0xF81F
#define COLOR_LIGHT_MAGENTA   0x9813
#define COLOR_LIGHT_RED       0x8800
#define COLOR_ORANGE          0xFC00
#define COLOR_LIGHT_ORANGE    0xFD20
#define COLOR_LIGHT_YELLOW    0xFF69
#define COLOR_BROWN           0xCC27
#define COLOR_LIGHT_BROWN     0x6204
#define COLOR_GREY            0xC618