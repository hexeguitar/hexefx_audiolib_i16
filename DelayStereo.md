# HexeFX Stereo Modulated Ping-Pong Delay for Teensy4.x  
Example PlatformIO project using the Stereo Ping-Pong Delay component from the `hexefx_audioLibrary_i16`, compatible with the standard Teensy Audio Library.  
## Hardware  
Default hardware is either Teensy4.1 or Teensy4.0 with Audio Adapter Board Rev.D.
## Usage  
1. Open the project in the PlatformIO environment.
2. Build the project and upload it to the Teensy4 board.
3. Open the `DelayStereo_I16.html` file placed in the `html` folder in Chrome, Chromium or Edge browser (others do not implement WebMIDI and WebSerial).
4. Connect to the USB MIDI interface listed as Teensy.  
5. Click `Connect` button on the top of the page and choose Teensy Serial port.
6. Use the dials and buttons to control the effect.  
 
## Controls  
* **On/Off** button - delay bypass
* **Time** - delay time
* **Inertia** - delay time update speed resuling is pitch up/down effects
* **Repeats** - delay feeedback control
* **Mix** - dry/wset mix control
* **HiCut** - lowpass filter placed in the feedback loop, makes each repeat darker
* **LowCut** - highpass filter placed in the feedback loop, makes each repat loose low end
* **Treble** - wet signal output treble control
* **Bass** - wet signal output bass control
* **ModRate** - modulation rate
* **ModDepth** - modulation depth
* **Freeze** - button, Freeze on/off
* **Tap** - button, delay time tap tempo  
___
Copyright 11.2024 by Piotr Zapart  
www.hexefx.com  
