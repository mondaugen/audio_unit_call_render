#!/bin/bash
mkdir -p bin/
cc player_call_render.c -o bin/player_call_render -framework CoreAudio \
    -framework AudioToolbox -framework CoreFoundation -g
