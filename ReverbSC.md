# HexeFX Stereo Reverb SC for Teensy4.x  
Example PlatformIO project using the Stereo Reverb SC component from the `hexefx_audioLibrary_i16`, compatible with the standard Teensy Audio Library.  
This reverb uses an extensive **387k** of RAM2 (DMARAM) for the delay buffers. Use with caution when combining it with the rest of the project.
## Hardware  
Default hardware is either Teensy4.1 or Teensy4.0 with Audio Adapter Board Rev.D.
## Usage  
1. Open the project in the PlatformIO environment.
2. Build the project and upload it to the Teensy4 board.
3. Open the `ReverbSC_I16.html` file placed in the `html` folder in Chrome, Chromium or Edge browser (others do not implement WebMIDI and WebSerial).
4. Connect to the USB MIDI interface listed as Teensy.  
5. Click `Connect` button on the top of the page and choose Teensy Serial port.
6. Use the dials and buttons to control the effect.  
 
## Controls  
* **On/Off** button - delay bypass
* **Freeze** - button, Freeze on/off
* **Time** - reverb time
* **TrebleCut** - lowpass filter placed in the feedback loop, makes each repeat darker
* **Mix** - dry/wset mix control
* **MCU RST** - Teensy software reset
___
Copyright 11.2024 by Piotr Zapart  
www.hexefx.com  
