#!/bin/bash

amixer set Master 50%

/home/username/PeanutGB/keyboard.o &
/home/username/PeanutGB/joystick.o &

stty -echo

sudo chmod -w /dev/stdin

echo "Changing /dev/stdin to Read-Only"

/home/username/PeanutGB/menu.o
/home/username/PeanutGB/PeanutGB.o

sudo chmod +w /dev/stdin

echo "Changing /dev/stdin to Read-Write"

stty echo




