#!/bin/bash

GAME_PATH="./built.x86_64"
WIDTH=960
HEIGHT=540

# Make LD_PRELOAD configurable
PRELOAD="/home/cc/repo/src/wrapper/libhook.so"

# Define the common flags once
FLAGS="-screen-width $WIDTH -screen-height $HEIGHT -screen-fullscreen 0 -popupwindow"

# Loop four times (0, 1, 2, 3)
for i in {0..3}; do
  # Calculate X position (0 or 640)
  X=$(( (i % 2) * WIDTH ))
  # Calculate Y position (0 or 360)
  Y=$(( (i / 2) * HEIGHT ))

  # Launch the game in the background
  LD_PRELOAD="$PRELOAD" $GAME_PATH $FLAGS &

  # Wait for the game window to become active
  sleep 2

  # Move and resize the active window
  # Note: This relies on the launched game being the active window after sleep
  wmctrl -r :ACTIVE: -e 0,$X,$Y,$WIDTH,$HEIGHT
done

echo "All games launched and positioned!"
