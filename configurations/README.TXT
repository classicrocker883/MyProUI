More configuration files at: https://github.com/mriscoc/Special_Configurations

##**EXPERIMENTAL**
Redefine the configuration settings (Perhaps more easily?)

_Defines_Configuration
_Defines_Configuration_adv.h
_Undef_Configuration.h
_Undef_Configuration_adv.h

To use:
Within Marlin/src/inc/MarlinConfigPre.h, look for the `#include` of these files above and uncomment them.

In the _Undef_ file, look for the setting you wish to change and uncomment it.
(i.e. // #undef MOTHERBOARD => #undef MOTHERBOARD)

Then, go to _Defines_ file, uncomment the setting which you just `#undef` and change to whatever.
(i.e. // #define MOTHERBOARD => #define MOTHERBOARD BOARD_CREALITY_V4)