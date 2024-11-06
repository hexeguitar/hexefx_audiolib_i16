/**
 * @file PlateReverbStereo_Arduino.ino
 * @author Piotr Zapart
 * @brief Example project for the Stereo Plate Reverb component
 * 			int16_t standard Teensy Audio library compatible
 * 		required libraries: 
 * 			HexeFX_audiolib_I16 https://github.com/hexeguitar/hexefx_audiolib_I16
 * 
 * 	MCU: Teensy4.0 + Audio Adaptor Rev.D
 *  USB: MIDI + SERIAL
 * 			
 * @version 0.1
 * @date 2024-11-06
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
AudioEffectPlateReverb_i16		reverb;
AudioOutputI2S	     			i2s_out; 

AudioConnection     cable1(i2s_in, 0, reverb, 0);
AudioConnection     cable2(i2s_in, 1, reverb, 1);
AudioConnection     cable3(reverb, 0, i2s_out, 0);
AudioConnection     cable4(reverb, 1, i2s_out, 1);

enum 
{
	MIDI_CC_REVERBPL_TIME			= 50,
	MIDI_CC_REVERBPL_DIFF			= 51,
	MIDI_CC_REVERBPL_MIX			= 52,
	MIDI_CC_REVERBPL_BASS_CUT		= 53,
	MIDI_CC_REVERBPL_TREBLE_CUT		= 54,
	MIDI_CC_REVERBPL_BASS			= 55,
	MIDI_CC_REVERBPL_TREBLE			= 56,
	MIDI_CC_REVERBPL_BLEEDIN		= 57,
	MIDI_CC_REVERBPL_PITCH			= 58,
	MIDI_CC_REVERBPL_PITCHMIX		= 59,
	MIDI_CC_REVERBPL_SHIMMER		= 60,
	MIDI_CC_REVERBPL_SHIMM_PITCH	= 61
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

void printMemInfo(void);

void setup()
{
	DBG_SERIAL.begin(115200);
	DBG_SERIAL.println("T41GFX - Stereo Plate Reverb");
	DBG_SERIAL.println("01.2024 www.hexefx.com");
	
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

		case MIDI_NOTE_MCU_RESET:
			break;
		case MIDI_NOTE_REVERB:
			reverb.bypass_set(true);	
			break;
		case MIDI_NOTE_REVERB_FREEZE:
			reverb.freeze(false);
			break;			
		default:
			break;
	}
}

void cb_ControlChange(uint8_t channel, uint8_t control, uint8_t value)
{
	float32_t tmp_f;
	float32_t valuef = (float32_t) value / 127.0f;
	switch(control)
	{
	
		// --- Reverb ---
		case MIDI_CC_REVERBPL_TIME:
			reverb.size(valuef);
			break;
		case MIDI_CC_REVERBPL_DIFF:
			reverb.diffusion(valuef);
			break;
		case MIDI_CC_REVERBPL_MIX:
			reverb.mix(valuef);
			break;
		case MIDI_CC_REVERBPL_BASS_CUT:
			reverb.lodamp(valuef);
			break;
		case MIDI_CC_REVERBPL_TREBLE_CUT:
			reverb.hidamp(valuef);
			break;
		case MIDI_CC_REVERBPL_BASS:
			tmp_f = 1.0f - valuef;
			reverb.hipass(tmp_f);
			break;
		case MIDI_CC_REVERBPL_TREBLE:
			tmp_f = 1.0f - valuef;
			reverb.lowpass(tmp_f);
			break;
		case MIDI_CC_REVERBPL_BLEEDIN:
			reverb.freezeBleedIn(valuef);
			break;
		case MIDI_CC_REVERBPL_PITCH:
			reverb.pitchNormalized(valuef);
			break;
		case  MIDI_CC_REVERBPL_PITCHMIX:
			reverb.pitchMix(valuef);
			break;
		case MIDI_CC_REVERBPL_SHIMMER:
			reverb.shimmer(valuef);
			break;
		case MIDI_CC_REVERBPL_SHIMM_PITCH:
			reverb.shimmerPitchNormalized(valuef);
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

    DBG_SERIAL.printf("CPU usage: reverb = %2.2f%% max = %2.2f%%   \r\n", load_rv, load);

    DBG_SERIAL.printf("Reverb %s \tFreeze %s\r\n", 
                        reverb.bypass_get()?off:on,
                        reverb.freeze_get()?on:off);
    DBG_SERIAL.printf("Reverb pitch \t%d semitones          \r\n", pitch);
	DBG_SERIAL.printf("Shimmer pitch \t%d semitones          \r\n", pitchShm);			

}
