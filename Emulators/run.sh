#!/bin/bash

amixer set Master 75%

./keyboard.o &
./joystick.o &
sudo ./buttons.o &

stty -echo

sudo chmod -w /dev/stdin

echo "Changing /dev/stdin to Read-Only"

./menu.o

sh game.sh

sudo chmod +w /dev/stdin

echo "Changing /dev/stdin to Read-Write"

stty echo

sudo pkill keyboard.o
sudo pkill joystick.o
sudo pkill buttons.o






