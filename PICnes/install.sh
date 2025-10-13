#!/bin/bash

gcc -o /home/username/PICnes/keyboard.o /home/username/PICnes/keyboard.c
gcc -o /home/username/PICnes/joystick.o /home/username/PICnes/joystick.c
gcc -O3 -o /home/username/PICnes/PICnes.o /home/username/PICnes/PICnes.c

echo "Installed PICnes!"
echo "To manually play: sh ~/PICnes/run.sh ~/PICnes/ROMS/SMARIOB.nes"
echo "To automatically play: echo 'sh ~/PICnes/run.sh ~/PICnes/ROMS/SMARIOB.nes' >> ~/.bashrc"
echo "To reboot: sudo reboot"





