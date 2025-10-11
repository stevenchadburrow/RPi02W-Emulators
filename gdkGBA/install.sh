#!/bin/bash

gcc -o /home/username/gdkGBA/keyboard.o /home/username/gdkGBA/keyboard.c
gcc -o /home/username/gdkGBA/joystick.o /home/username/gdkGBA/joystick.c
gcc -O3 -o /home/username/gdkGBA/gdkGBA.o /home/username/gdkGBA/gdkGBA.c

echo "Installed gdkGBA!"
echo "To manually play: sh ~/gdkGBA/run.sh ~/gdkGBA/ROMS/SonicAdvance.gba"
echo "To automatically play: echo 'sh ~/gdkGBA/run.sh ~/gdkGBA/ROMS/SonicAdvance.gba' >> ~/.bashrc"
echo "To reboot: sudo reboot"





