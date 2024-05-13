More configuration files at: https://github.com/mriscoc/Special_Configurations

##**EXPERIMENTAL**
Redefine the configuration settings (Perhaps more easily?)

_Define_Configuration
_Define_Configuration_adv.h
_Undef_Configuration.h
_Undef_Configuration_adv.h

To use:
Within Marlin/src/inc/MarlinConfigPre.h, look for the `#include` of these files above and uncomment them.

In the _Undef_ file, look for the setting you wish to change and uncomment it.
(i.e. // #undef MOTHERBOARD => #undef MOTHERBOARD)

Then, go to _Defines_ file, uncomment the setting which you just `#undef` and change to whatever.
(i.e. // #define MOTHERBOARD => #define MOTHERBOARD BOARD_CREALITY_V4)

###**NOTE:**
This is the process to which I used creating these files.
I went into a "bash" terminal and entered these commands:

Step 1: Create the _Define_ file
```
grep -o -P '#define.*?(?=\s*//|$)' input_file.h > output_file.h
```
- Take the input_file (e.g. Configuration.h) and output_file (e.g. _Define_Configuration.h)

Step 2: Create the _Undef_ file
```
grep -o -P '#define\s+\w+' output_file.h > output_file_undef.h
```
- Takes the output_file (e.g. _Define_Configuration.h) of the first command, and removes all the defined values (e.g. _Undef_Configuration.h)

Step 2a: Rename all "#define" to "#undef"

Step 3: Add the comment '//' to the beginning of the line. Do this to both files
 - Select All 'Ctrl+A'
 - Toggle Line Comment 'Ctrl+/' or 'Ctrl+K Ctrl+C'
