# Updating the Display

To update the graphics and icons on the display:

- [Direct Download Link](https://minhaskamal.github.io/DownGit/#/home?url=https://github.com/classicrocker883/MriscocProUI/tree/2023-November/display%20assets/Aquila%20Display%20firmware/Firmware%20Sets) to just the Firmware Sets folder.
- Copy one of the `DWIN_SET`s from the Firmware Sets folder to an SD card (Formatted to FAT32 4096 or 4k allocation size). Remove the identifier so the folder is just named `DWIN_SET`. Remove the back cover of the display and insert the card into the microSD slot on the back of the display unit's PCB.
- Power on the machine and wait for the screen to change from blue to orange.
- Power off the machine.
- Remove the SD card from the back of the display.
- Power on to confirm a successful flash.

# Revert to the original font
You can revert back to the original font simply by downloading the folder "DWIN Font (Original)" (renamed to DWIN_SET) to the SD card and flash to your display.

## Troubleshooting
If flashing is not working (e.g. the blue screen only flashes for a second before turning to a red/orange screen):
- Reformat the SD card in FAT32 with 4k/4096 allocation size
- Leave only the folder `DWIN_SET`
- Use a storage size SD card 8GB or less
