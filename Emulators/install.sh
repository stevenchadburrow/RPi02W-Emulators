#!/bin/bash

gcc -o keyboard.o keyboard.c
gcc -o joystick.o joystick.c
gcc -o buttons.o buttons.c -lpigpio
gcc -o menu.o menu.c

gcc -O3 -o PICnes/PICnes.o PICnes/PICnes.c
gcc -O3 -o PeanutGB/PeanutGB.o PeanutGB/PeanutGB.c
gcc -O3 -o gdkGBA/gdkGBA.o gdkGBA/gdkGBA.c

echo "Installed PICnes, PeanutGB, and gdkGBA!"
echo "To manually play: sh ~/Emulators/run.sh"
echo "To automatically play: echo 'sh ~/Emulators/run.sh' >> ~/.bashrc"
echo "To reboot: sudo reboot"





