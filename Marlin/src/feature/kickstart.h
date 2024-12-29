/**
 * Fan Kickstart settings
 * Author: wlkmanist
 * Date: 2024/04/03
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

#if ENABLED(FAN_KICKSTART_EDITABLE)

typedef struct {
  uint16_t  duration_ms;     // Duration in milliseconds for the fan to run at kickstart speed
  uint8_t   speed;           // 0-255 (fullspeed); Fans first start speed
  bool      enabled;
} kickstart_settings_t;

#ifndef FAN_KICKSTART_TIME
  #define FAN_KICKSTART_TIME  100
#endif
#ifndef FAN_KICKSTART_POWER
  #define FAN_KICKSTART_POWER 180
#endif

static constexpr kickstart_settings_t kickstart_defaults = {
  FAN_KICKSTART_TIME,
  FAN_KICKSTART_POWER,
  true
};

class Kickstart {
  public:
    static kickstart_settings_t settings;

    static void reset() { TERN_(FAN_KICKSTART_EDITABLE, settings = kickstart_defaults); }
    static void setup() { reset(); }
};

extern Kickstart kickstart;

#endif
