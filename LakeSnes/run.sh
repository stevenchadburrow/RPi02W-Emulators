#!/bin/sh

amixer set Master 50%

/home/username/LakeSnes/keyboard.o &
/home/username/LakeSnes/joystick.o &

stty -echo

sudo chmod -w /dev/stdin

echo "Changing /dev/stdin to Read-Only"

/home/username/LakeSnes/LakeSnes.o $1

sudo chmod +w /dev/stdin

echo "Changing /dev/stdin to Read-Write"

stty echo

