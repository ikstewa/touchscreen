#!/bin/bash

#TouchMouse
rmmod touchmouse
cd touchmouse
make clean
cd ..

# TUIOD
for X in `ps -e | grep tuiod | awk {'print $1'}`; do
   kill $X;
done
cd tuiod
make clean
cd ..

# TUIODriver
rmmod tuio
cd tuiodriver
make clean
cd ..
