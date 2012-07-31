#!/bin/bash

# TUIODriver
cd tuiodriver
make
insmod tuio.ko
chmod 666 /dev/tuio
cd ..

# TUIOD
cd tuiod
make
./tuiod 3333 /dev/tuio
cd ..

#TouchMouse
cd touchmouse
make
insmod touchmouse.ko
cd ..

