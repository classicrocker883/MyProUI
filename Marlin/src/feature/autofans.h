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

#pragma once

#include "../inc/MarlinConfigPre.h"

#if HAS_AUTO_FAN

typedef struct {
  uint8_t extruder_temp;           // Auto fans temperature thresholds
  uint8_t chamber_temp;
  uint8_t cooler_temp;
} autofans_settings_t;

#ifndef EXTRUDER_AUTO_FAN_TEMPERATURE
  #define EXTRUDER_AUTO_FAN_TEMPERATURE 50
#endif
#ifndef CHAMBER_AUTO_FAN_TEMPERATURE
  #define CHAMBER_AUTO_FAN_TEMPERATURE 30
#endif
#ifndef COOLER_AUTO_FAN_TEMPERATURE
  #define COOLER_AUTO_FAN_TEMPERATURE 18
#endif

static constexpr autofans_settings_t autofans_defaults = {
  EXTRUDER_AUTO_FAN_TEMPERATURE,
  CHAMBER_AUTO_FAN_TEMPERATURE,
  COOLER_AUTO_FAN_TEMPERATURE
};

  #if ENABLED(AUTO_FAN_EDITABLE)

  class Autofans {
    public:
      static autofans_settings_t settings;

      static void reset() { settings = autofans_defaults; }
      static void setup() { reset(); }
  };

  extern Autofans autofans;

  #undef  EXTRUDER_AUTO_FAN_TEMPERATURE
  #define EXTRUDER_AUTO_FAN_TEMPERATURE (autofans.settings.extruder_temp)
  #undef  CHAMBER_AUTO_FAN_TEMPERATURE
  #define CHAMBER_AUTO_FAN_TEMPERATURE  (autofans.settings.chamber_temp)
  #undef  COOLER_AUTO_FAN_TEMPERATURE
  #define COOLER_AUTO_FAN_TEMPERATURE   (autofans.settings.cooler_temp)

  #endif
#endif