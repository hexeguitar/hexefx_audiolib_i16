# HexeFX Stereo Spring Reverb for Teensy4.x  
Example PlatformIO project using the Stereo Spring Reverb component from the `hexefx_audioLibrary_i16`, compatible with the standard Teensy Audio Library.  
## Hardware  
Default hardware is either Teensy4.1 or Teensy4.0 with Audio Adapter Board Rev.D.
## Usage  
1. Open the project in the PlatformIO environment.
2. Build the project and upload it to the Teensy4 board.
3. Open the `SpringReverb_I16.html` file placed in the `html` folder in Chrome, Chromium or Edge browser (others do not implement WebMIDI and WebSerial).
4. Connect to the USB MIDI interface listed as Teensy.  
5. Click `Connect` button on the top of the page and choose Teensy Serial port.
6. Use the dials and buttons to control the effect.  
 
## Controls  
* **On/Off** button - reverb bypass  
* **Time** - reverb time  
* **TrebleCut** - lowpass filter placed in the feedback loop, makes the faiding out reverb tail darker  
* **BassCut** - highpass filter placed in the feedback loop, makes the faiding reverb tail loose the low end
* **Mix** - dry/wset mix control  
___
Copyright 11.2024 by Piotr Zapart  
www.hexefx.com  
