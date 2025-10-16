#!/bin/bash

gcc -o /home/username/PeanutGB/keyboard.o /home/username/PeanutGB/keyboard.c
gcc -o /home/username/PeanutGB/joystick.o /home/username/PeanutGB/joystick.c
gcc -o /home/username/PeanutGB/menu.o /home/username/PeanutGB/menu.c
gcc -O3 -o /home/username/PeanutGB/PeanutGB.o /home/username/PeanutGB/PeanutGB.c

echo "Installed PeanutGB!"
echo "To manually play: sh ~/PeanutGB/run.sh"
echo "To automatically play: echo 'sh ~/PeanutGB/run.sh' >> ~/.bashrc"
echo "To reboot: sudo reboot"





