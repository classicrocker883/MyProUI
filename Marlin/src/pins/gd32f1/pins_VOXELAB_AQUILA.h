/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2023 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
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

/**
 * FFP0173_Aquila_Main_Board_V1.0.1 (GD32F103RC / N32G455RE)
 *
 * Uses CREALITY V4 (STM32F103RE / STM32F103RC) board pin assignments
 */

#ifndef BOARD_INFO_NAME
  #define BOARD_INFO_NAME      "Aquila v1.0.1"
#endif
#ifndef DEFAULT_MACHINE_NAME
  #define DEFAULT_MACHINE_NAME "Aquila"
#endif

#define NO_MAPLE_WARNING              // Disable warning when compiling with Maple env

//
// Power Loss Detection
//
//#ifndef POWER_LOSS_PIN
//  #define POWER_LOSS_PIN      PA15  // PWRDET
//#endif

//#define LED                   PA3

/**
 *   RET6 12864 LCD (Board)        DWIN LCD Display
 *          ------                      ------
 *    PC6  | 1  2 | PB2             NC | 1  2 | NC
 *    PB10 | 3  4 | PB11             B | 3  4 | A
 *    PB14   5  6 | PB13            EN   5  6 | BEEP
 *    PB12 | 7  8 | PB15            RX | 7  8 | TX
 *     GND | 9 10 | +5V            GND | 9 10 | VCC (+5V)
 *          ------                      ------
 * 
 *
 *         BL-Touch                   Debug port
 *          -----                       -----
 *          | 1 | PB1                   | 1 | VCC
 *            2 | GND                   | 2 | PA13
 *          | 3 | PB0                   | 3 | PA14
 *            4 | +5V                   | 4 | GND
 *          | 5 | GND                   -----
 *          -----
 */

#include "../stm32f1/pins_CREALITY_V4.h"
