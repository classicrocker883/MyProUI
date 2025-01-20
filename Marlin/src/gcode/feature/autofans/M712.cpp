/**
 * Auto Fans settings (Extruder Cooling Fans)
 * Author: wlkmanist
 * Date: 2024/04/08
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

#if ENABLED(AUTO_FAN_EDITABLE)

#include "../../gcode.h"
#include "../../../feature/autofans.h"

/**
 * M712: Set Extruder Auto Fans settings
 *
 *  R         : Reset to defaults
 *  E         : Set Extruder fans temperature threshold
 *  H         : Set Chamber fan temperature threshold
 *  C         : Set Cooler fan temperature threshold
 *
 * Examples:
 *   M712                   ; Report current Settings
 *   M712 R                 ; Reset EHC to defaults
 *   M712 E60               ; Set Extruder temp to 60
 *   M712 E50 H30 C18       ; Set Extruder temp to 50, Chamber temp to 30, Cooler temp to 18
 */
void GcodeSuite::M712() {

  const bool seenR = parser.seen('R');
  if (seenR) autofans.reset();

  const bool seenE = parser.seenval('E');
  if (seenE) autofans.settings.extruder_temp = parser.value_byte();

  const bool seenH = parser.seenval('H');
  if (seenH) autofans.settings.chamber_temp = parser.value_byte();

  const bool seenC = parser.seenval('C');
  if (seenC) autofans.settings.cooler_temp = parser.value_byte();

  if (!(seenR || seenE || seenH || seenC))
    M712_report();
}

void GcodeSuite::M712_report(const bool forReplay/*=true*/) {
  TERN_(MARLIN_SMALL_BUILD, return);

  report_heading_etc(forReplay, F(STR_AUTOFANS));
  SERIAL_ECHOLNPGM("  M712"
    " E", int(autofans.settings.extruder_temp),
    " H", int(autofans.settings.chamber_temp),
    " C", int(autofans.settings.cooler_temp)
  );
}

#endif // AUTO_FAN_EDITABLE
