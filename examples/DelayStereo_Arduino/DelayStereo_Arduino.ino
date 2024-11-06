/**
 * @file DelayStereo_Arduino.ino
 * @author Piotr Zapart
 * @brief Example project for the Stereo Ping-Pong Modulated Delay component
			int16_t standard Teensy Audio library compatible 
 * 		required libraries: 
 * 			HexeFX_audiolib_I16 https://github.com/hexeguitar/hexefx_audiolib_I16
 * 			
 * @version 0.1
 * @date 2024-11-11
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
AudioEffectDelayStereo_i16		echo;
AudioOutputI2S	     			i2s_out; 

AudioConnection     cable1(i2s_in, 0, echo, 0);
AudioConnection     cable2(i2s_in, 1, echo, 1);
AudioConnection     cable3(echo, 0, i2s_out, 0);
AudioConnection     cable4(echo, 1, i2s_out, 1);

enum 
{
	// Delay controls
	MIDI_CC_DELAY_TIME				= 1,	
	MIDI_CC_DELAY_INERTIA			= 2,
	MIDI_CC_DELAY_FEEDBACK			= 3,
	MIDI_CC_DELAY_MIX				= 4,
	MIDI_CC_DELAY_MOD_RATE			= 5,
	MIDI_CC_DELAY_MOD_DEPTH			= 6,
	MIDI_CC_DELAY_TREBLE_CUT		= 7,
	MIDI_CC_DELAY_BASS_CUT			= 8,
	MIDI_CC_DELAY_TREBLE			= 9,
	MIDI_CC_DELAY_BASS				= 10
};

enum
{
	MIDI_NOTE_DELAY					= 33,
	MIDI_NOTE_DELAY_TAP				= 36,
	MIDI_NOTE_DELAY_FREEZE			= 37,
	// --- diverse ---
	MIDI_NOTE_MCU_RESET				= 127
};

// Callbacks for MIDI
void cb_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
void cb_NoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
void cb_ControlChange(uint8_t channel, uint8_t control, uint8_t value);

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

	echo.bypass_set(false);
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
		case MIDI_NOTE_DELAY:
			echo.bypass_set(false);
			break;
		case MIDI_NOTE_DELAY_FREEZE:
			echo.freeze(true);
			break;
		case MIDI_NOTE_DELAY_TAP:
			echo.tap_tempo();
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

		case MIDI_NOTE_MCU_RESET:
			break;
		case MIDI_NOTE_DELAY:
			echo.bypass_set(true);	
			break;
		case MIDI_NOTE_DELAY_FREEZE:
			echo.freeze(false);
			break;			
		default:
			break;
	}
}

void cb_ControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
	float32_t valuef = (float32_t) value / 127.0f;
	switch(control)
	{
	
		// --- Delay---
		case MIDI_CC_DELAY_TIME:
			echo.time(valuef);
			break;
		case MIDI_CC_DELAY_INERTIA:
			echo.inertia(valuef);
			break;
		case MIDI_CC_DELAY_FEEDBACK:
			echo.feedback(valuef);
			break;			
		case MIDI_CC_DELAY_MIX:
			echo.mix(valuef);
			break;
		case MIDI_CC_DELAY_MOD_RATE:
			echo.mod_rate(valuef);
			break;
		case MIDI_CC_DELAY_MOD_DEPTH:
			echo.mod_depth(valuef);
			break;			
		case MIDI_CC_DELAY_BASS_CUT:
			echo.bass_cut(valuef);
			break;
		case MIDI_CC_DELAY_TREBLE_CUT:
			echo.treble_cut(valuef);
			break;
		case MIDI_CC_DELAY_BASS:
			echo.bass(valuef);
			break;
		case MIDI_CC_DELAY_TREBLE:
			echo.treble(valuef);
			break;


		default:    break;
	}	
}

void printMemInfo(void)
{
    static const char *on = "\x1b[32mon \x1b[0m";
    static const char *off = "\x1b[31moff\x1b[0m";
    float load_dl = echo.processorUsageMax();
    echo.processorUsageMaxReset();

    float load = AudioProcessorUsageMax();
    AudioProcessorUsageMaxReset();

    DBG_SERIAL.printf("%sCPU usage: delay = %2.2f%% max = %2.2f%%   \r\n", termPosHome, load_dl, load);

    DBG_SERIAL.printf("Delay %s \tFreeze %s\r\n", 
                        echo.bypass_get()?off:on,
                        echo.freeze_get()?on:off);	
}

