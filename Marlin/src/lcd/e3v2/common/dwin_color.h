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
#define RGB(R,G,B)  (R << 11) | (G << 5) | (B) // R: 0..31, G: 0..63, B: 0..31
#define GetRColor(color) ((color >> 11) & 0x1F)
#define GetGColor(color) ((color >>  5) & 0x3F)
#define GetBColor(color) ((color >>  0) & 0x1F)

// RGB565 colors: https://rgbcolorpicker.com/565
#define Color_Voxelab_Red   0xA804 //0xF00F
#define Color_Cyan          0x07FF
#define Color_White         0xFFFF
#define Color_Yellow        0xFF0F //RGB(0x1F,0x3F,0x00)
#define Color_Red           0xA804 //RGB(0x1F,0x00,0x00)
#define Color_Error_Red     0xC000 // Error!
#define Color_Bg_Red        Color_Voxelab_Red // Red background color
#define Color_Bg_Window     0x31E8 // Popup background color
#define Color_Bg_Blue       0x1125 // Dark blue background color
#define Color_Bg_Black      0x0841 // Black background color
#define Color_IconBlue      0x45FA // Lighter blue that matches icons/accents
#define Color_Silver        0xC618 // Greyish line seperator
#define Popup_Text_Color    0xD6BA // Popup font background color
#define Line_Color          Color_White // Split line color 0x3A6A
#define Rectangle_Color     Color_Voxelab_Red // Blue square cursor color 0xEE2F
#define Percent_Color       Color_White // Percentage color 0xFE29
#define BarFill_Color       0x10E4 // Fill color of progress bar
#define Select_Color        0x8430 // Selected color 0x33BB
