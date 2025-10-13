#!/bin/bash

amixer set Master 50%

/home/username/PICnes/keyboard.o &
/home/username/PICnes/joystick.o &

stty -echo

sudo chmod -w /dev/stdin

echo "Changing /dev/stdin to Read-Only"

/home/username/PICnes/PICnes.o $1

sudo chmod +w /dev/stdin

echo "Changing /dev/stdin to Read-Write"

stty echo




