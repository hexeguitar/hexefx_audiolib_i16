/**
 * @file ReverbSC_Arduino.ino
 * @author Piotr Zapart
 * @brief Example project for the Stereo Reverb SC component
 * 			int16_t standard Teensy Audio library compatible
 * 		required libraries: 
 * 			HexeFX_audiolib_I16 https://github.com/hexeguitar/hexefx_audiolib_I16

 * 			
 * @version 0.1
 * @date 2024-11-106
 * 
 * @copyright Copyright www.hexefx.com (c) 2024
 * 
 */
#include <Audio.h>
#include <hexefx_audio_I16.h>

#ifndef DBG_SERIAL 
	#define DBG_SERIAL Serial
#endif

AudioControlSGTL5000			codec;
AudioInputI2S					i2s_in;
AudioEffectReverbSC_i16			reverb;
AudioOutputI2S	     			i2s_out; 

// reverb has it's own internal dry/wet mixer
AudioConnection     cable1(i2s_in, 0, reverb, 0);
AudioConnection     cable2(i2s_in, 1, reverb, 1);
AudioConnection     cable3(reverb, 0, i2s_out, 0);
AudioConnection     cable4(reverb, 1, i2s_out, 1);

enum 
{
	MIDI_CC_REVERB_TIME			= 50,
	MIDI_CC_REVERB_MIX			= 52,
	MIDI_CC_REVERB_TREBLE_CUT	= 54,
};

enum
{
	MIDI_NOTE_REVERB 				= 4,
	MIDI_NOTE_REVERB_FREEZE			= 5,
	// --- diverse ---
	MIDI_NOTE_MCU_RESET				= 127
};

// Callbacks for MIDI
void cb_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
void cb_NoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
void cb_ControlChange(uint8_t channel, uint8_t control, uint8_t value);


int8_t pitch=0, pitchShm=0;
uint32_t timeNow, timeLast;
const char PROGMEM *termPosHome = "\x1b[;H";

void printMemInfo(void);

void setup()
{
	DBG_SERIAL.begin(115200);

	AudioMemory(20);

	if (!codec.enable()) DBG_SERIAL.println("Codec init error!");
	codec.inputSelect(AUDIO_INPUT_LINEIN);
	codec.volume(0.8f);
	codec.lineInLevel(10, 10);
	codec.adcHighPassFilterDisable();

	DBG_SERIAL.println("Codec initialized.");

	// set callbacks for USB MIDI
	usbMIDI.setHandleNoteOn(cb_NoteOn);
	usbMIDI.setHandleNoteOff(cb_NoteOff);
	usbMIDI.setHandleControlChange(cb_ControlChange);

	reverb.bypass_set(false);
}

void loop()
{
	usbMIDI.read();
	timeNow = millis();
    if (timeNow - timeLast > 500)
    {
        printMemInfo();		
        timeLast = timeNow;
	}
}
/**
 * @brief USB MIDI NoteOn callback
 * 
 * @param channel 
 * @param note 
 * @param velocity 
 */
void cb_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
	switch(note)
	{
		case MIDI_NOTE_REVERB:
			reverb.bypass_set(false);
			break;
		case MIDI_NOTE_REVERB_FREEZE:
			reverb.freeze(true);
			break;				
		case MIDI_NOTE_MCU_RESET:
			SCB_AIRCR = 0x05FA0004; // MCU reset
			break;
		default:
			break;
	}
}

void cb_NoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
	switch(note)
	{
		case MIDI_NOTE_REVERB:
			reverb.bypass_set(true);	
			break;
		case MIDI_NOTE_REVERB_FREEZE:
			reverb.freeze(false);
			break;				
		case MIDI_NOTE_MCU_RESET:
		default:
			break;
	}
}

void cb_ControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
	float32_t valuef = (float32_t) value / 127.0f;
	switch(control)
	{
	
		// --- Reverb ---
		case MIDI_CC_REVERB_TIME:
			reverb.feedback(valuef);
			break;
		case MIDI_CC_REVERB_MIX:
			reverb.mix(valuef);
			break;
		case MIDI_CC_REVERB_TREBLE_CUT:
			reverb.lowpass(valuef);
			break;
		default:    break;
	}	
}

void printMemInfo(void)
{
    const char *on = "\x1b[32mon \x1b[0m";
    const char *off = "\x1b[31moff\x1b[0m";
    float load_rv = reverb.processorUsageMax();
    reverb.processorUsageMaxReset();

    float load = AudioProcessorUsageMax();
    AudioProcessorUsageMaxReset();

    DBG_SERIAL.printf("%sCPU usage: reverb = %2.2f%% max = %2.2f%%   \r\n", termPosHome, load_rv, load);

    DBG_SERIAL.printf("Reverb %s \tFreeze %s\r\n", 
                        reverb.bypass_get()?off:on,
                        reverb.freeze_get()?on:off);
}
