/**
 * ToolBar for PRO UI
 * Author: Miguel A. Risco-Castillo (MRISCOC)
 * version: 1.4.1
 * Date: 2023/04/28
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

#include "dwin.h"
#include "toolbar.h"

#define TB_ITEM(I,L,V...) {I, GET_TEXT_F(L), V}

const TBItem_t TBItemA[] = {
  TB_ITEM(0, MSG_OPTION_DISABLED, nullptr),
  TB_ITEM(ICON_Homing, MSG_AUTO_HOME, AutoHome),
  #if HAS_BED_PROBE
    OPTITEM(PROUI_ITEM_TRAM, TB_ITEM(ICON_Tram, MSG_TRAMMING_WIZARD, TramwizStart))
    TB_ITEM(ICON_SetZOffset, MSG_PROBE_WIZARD, Draw_ZOffsetWiz_Menu),
    TB_ITEM(ICON_Level, MSG_AUTO_MESH, AutoLevStart),
  #else
    TB_ITEM(ICON_MoveZ0, MSG_HOME_Z_AND_DISABLE, HomeZandDisable),
  #endif
  TB_ITEM(ICON_CloseMotor, MSG_DISABLE_STEPPERS, DisableMotors),
  TB_ITEM(ICON_Cool, MSG_COOLDOWN, DoCoolDown),
  #if HAS_PREHEAT
    #define _TBPREHEAT(N) TB_ITEM(ICON_Preheat##N, MSG_PREHEAT_##N, DoPreheat##N),
    REPEAT_1(PREHEAT_COUNT, _TBPREHEAT)
  #endif
  TB_ITEM(ICON_Box, MSG_BRIGHTNESS_OFF, TurnOffBacklight),
  TB_ITEM(ICON_Reboot, MSG_RESET_PRINTER, RebootPrinter),
  TB_ITEM(ICON_WriteEEPROM, MSG_STORE_EEPROM, WriteEeprom),
  TB_ITEM(ICON_MoveZ, MSG_TOOL_CHANGE_ZLIFT, RaiseHead)
  OPTARG(NOZZLE_PARK_FEATURE, TB_ITEM(ICON_Park, MSG_FILAMENT_PARK_ENABLED, ParkHead))
};
