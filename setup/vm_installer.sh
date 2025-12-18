#!/bin/bash

sudo apt update

sudo apt install xorg
sudo apt install xfce4 xfce4-goodies
sudo apt install x11vnc
sudo apt install build-essential libgl1-mesa-dev
sudo apt install dbus-x11
sudo apt install mesa-utils

sudo mkdir -p /etc/X11/xorg.conf.d/

sudo tee /etc/X11/xorg.conf.d/10-nvidia-headless.conf > /dev/null <<EOF
Section "ServerLayout"
    Identifier     "Headless Layout"
    Screen      0  "Headless Screen" 0 0
    Option         "StandbyTime" "0"
    Option         "SuspendTime" "0"
    Option         "OffTime"     "0"
    Option         "BlankTime"   "0"
EndSection

Section "Monitor"
    Identifier     "Headless Monitor"
    VendorName     "Unknown"
    ModelName      "Unknown"
    HorizSync       28.0 - 80.0
    VertRefresh     48.0 - 75.0
    Option         "DPMS"
EndSection

Section "Device"
    Identifier     "NVIDIA GPU"
    Driver         "nvidia"
    
    # BUS ID for GPU #0 (Tesla P100)
    BusID          "PCI:3:0:0"
    
    Option         "AllowEmptyInitialConfiguration" "True"
    Option         "ConnectedMonitor" "DFP"
    Option         "TripleBuffer" "True"
EndSection

Section "Screen"
    Identifier     "Headless Screen"
    Device         "NVIDIA GPU"
    Monitor        "Headless Monitor"
    DefaultDepth    24
    Option         "UseDisplayDevice" "None"
    SubSection     "Display"
        Virtual     1920 1080
        Depth       24
    EndSubSection
EndSection
EOF
