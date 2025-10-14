#!/bin/sh

gcc -o /home/username/LakeSnes/keyboard.o /home/username/LakeSnes/keyboard.c
gcc -o /home/username/LakeSnes/joystick.o /home/username/LakeSnes/joystick.c
gcc -O3 -o /home/username/LakeSnes/LakeSnes.o /home/username/LakeSnes/main.c -lm

echo "Installed LakeSnes!"
echo "To manually play: sh ~/LakeSnes/run.sh ~/LakeSnes/ROMS/SuperMarioWorld.sfc"
echo "To automatically play: echo 'sh ~/LakeSnes/run.sh ~/LakeSnes/ROMS/SuperMarioWorld.sfc' >> ~/.bashrc"
echo "To reboot: sudo reboot"
