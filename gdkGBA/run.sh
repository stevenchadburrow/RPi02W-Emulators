#!/bin/bash

amixer set Master 50%

/home/username/gdkGBA/keyboard.o &
/home/username/gdkGBA/joystick.o &

stty -echo

sudo chmod -w /dev/stdin

echo "Changing /dev/stdin to Read-Only"

/home/username/gdkGBA/gdkGBA.o $1

sudo chmod +w /dev/stdin

echo "Changing /dev/stdin to Read-Write"

stty echo




