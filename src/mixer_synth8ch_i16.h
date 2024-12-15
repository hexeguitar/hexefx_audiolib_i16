/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ---------------------------------------------
 * 8 mono input version with stereo output and gain/pan control per channel
 * useful for polyphonic synth voice mixers 
 * -----> Teensy4.x only <-----
 * 
 * 12.2024 Piotr Zapart (www.hexefx.com)
 */
#ifndef _MIXERSYNTH8_I16_H_
#define _MIXERSYNTH8_I16_H_

#include <Arduino.h>	 // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include "basic_DSPutils.h" // part of hexefx_audiolib_i16, used for constant power pan control
#include "utility/dspinst.h"

#define VOICEMIXER_CHANNELS	8

enum
{
	VOICEMIXER_MOD_OFF = 0,
	VOICEMIXER_MOD_VOL, 
	VOICEMIXER_MOD_PAN,
	VOICEMIXER_MOD_VOL_PAN  
};

class AudioMixerSynth8ch_i16 : public AudioStream
{
#if defined(__ARM_ARCH_7EM__)
public:
	AudioMixerSynth8ch_i16(void) : AudioStream(VOICEMIXER_CHANNELS+2, inputQueueArray)
	{
		for (int i = 0; i < VOICEMIXER_CHANNELS; i++)
		{
			gain(i, 1.0f, 0.0f);
			pan(i, 0.5f);
			panMod(i, 0.0f);
			modEnable(i, VOICEMIXER_MOD_VOL_PAN);
		}		
	}
	virtual void update(void);

	// ------------------- VOLUME/GAIN -------------------------------
	/**
	 * @brief setts the gain for one channel
	 * 
	 * @param channel 	channel number
	 * @param gain 		gain in range 0.0 to 1.0
	 * @param modMult 	moodulation multiplier in range -1.0 (inv phase) to 1.0
	 * @return 			none
	 */
	FLASHMEM void gain(uint8_t channel, float gain, float modMult)
	{
		if (channel >= VOICEMIXER_CHANNELS)
			return;
		gain = constrain(gain, 0.0f, 1.0f);
		modMult = constrain(modMult, -1.0f, 1.0f);
		chnGainSet[channel] = gain * 32767.0f;
		chnGainModMultSet[channel] = modMult * 16383.0f;
		mixGain(channel);
		calc_gain(channel);
	}

	/**
	 * @brief Sets the gain for one channel without modyfing 
	 * 		the modulation multiplier
	 * 
	 * @param channel 	channel number
	 * @param g 		channel gain in range 0.0 to 1.0
	 * @return 			none
	 */
	FLASHMEM void gain(uint8_t channel, float g)
	{
		if (channel >= VOICEMIXER_CHANNELS)
			return;
		g = constrain(g, 0.0f, 1.0f);
		chnGainSet[channel] = g * 32767.0f;
		mixGain(channel);	
		calc_gain(channel);
	}
	/**
	 * @brief Sets the gain pnly for all channels
	 * 
	 * @param g 	gain value in range 0.0 to 1.0
	 * @return 		none 
	 */
	FLASHMEM void gainAll(float g)
	{
		for (int i=0; i<VOICEMIXER_CHANNELS; i++)
		{
			gain(i, g);
		}
	}

	/**
	 * @brief Sets the gain (volume) modulation multiplier
	 * 		for all channels
	 * 
	 * @param modMult 	multiplier value -1.0 (inv phase) to 1.0
	 * @return 			none 
	 */
	FLASHMEM void gainModAll(float modMult)
	{
		for (int i=0; i<VOICEMIXER_CHANNELS; i++)
		{
			gainMod(i, modMult);
		}
	}
	
	/**
	 * @brief Sets the gain/volume modulation multiplier for one channel
	 * 
	 * @param channel 	channel number
	 * @param modMult 	multiplier value -1.0 (inv phase) to 1.0
	 * @return 			none 
	 */
	FLASHMEM void gainMod(uint8_t channel, float modMult) 
	{ 
		modMult = constrain(modMult, -1.0f, 1.0f);
		chnGainModMultSet[channel] = modMult * 16383.0f;
		mixGain(channel);
		calc_gain(channel);
	}

	// ------------------- PANORAMA-------------------------------
	/**
	 * @brief Sets the panorama value for one channel.
	 * 
	 * @param channel 	channel number
	 * @param p 		panorama value (0.0 = left, 1.0 = right, 0.5 = center)
	 * @return 			none
	 */
	FLASHMEM void pan(uint8_t channel, float p)
	{
		if (channel >= VOICEMIXER_CHANNELS)
			return;
		p = constrain(p, 0.0f, 1.0f);
		chnPanSet[channel] =  p * 32767.0f;
		mixPan(channel);
		calc_gain(channel);
	}
	/**
	 * @brief Sets the panorama value for all channels.
	 * 
	 * @param p 	panorama value (0.0 = left, 1.0 = right, 0.5 = center)
	 * @return 		none 
	 */
	FLASHMEM void panAll(float p)
	{
		for (int i=0; i<VOICEMIXER_CHANNELS; i++)
		{
			pan(i, p);
		}
	}
	/**
	 * @brief Sets the panorama modulation multiplier for one channel
	 * 
	 * @param channel 		channel number
	 * @param modMult 		multiplier value -1.0 (inv phase) to 1.0
	 * @return 				none 
	 */
	FLASHMEM void panMod(uint8_t channel, float modMult) 
	{ 
		modMult = constrain(modMult, -1.0f, 1.0f);
		chnPanModMultSet[channel] = modMult * 16383.0f;
		mixPan(channel);
		calc_gain(channel);
	}
	/**
	 * @brief Sets the panorama modulation multiplier for all channels
	 * 
	 * @param modMult 	multiplier value -1.0 (inv phase) to 1.0
	 * @return 			none 
	 */
	FLASHMEM void panModAll(float modMult)
	{
		for (int i=0; i<VOICEMIXER_CHANNELS; i++)
		{
			panMod(i, modMult);
		}
	}

	/**
	 * @brief Enable/disable modulation for a single channel
	 * 
	 * @param channel input channel number 0-(VOICEMIXER_CHANNELS-1)
	 * @param mask 	VOICEMIXER_MOD_OFF = 0,
	 * 				VOICEMIXER_MOD_VOL, 
	 * 				VOICEMIXER_MOD_PAN,
	 * 				VOICEMIXER_MOD_VOL_PAN  
	 */
	void modEnable(uint8_t channel, uint8_t mask)
	{
		if (channel < VOICEMIXER_CHANNELS)	
		{
			modMask[channel] = mask;
			gain(channel, chnGainSet[channel]);
			pan(channel, chnPanSet[channel]);
			gainMod(channel, chnGainModMultSet[channel]);
			panMod(channel, chnPanModMultSet[channel]);			
		}
	}
	
	/**
	 * @brief Enable/disable modulation for all channels
	 * 
	 * @param mask 	VOICEMIXER_MOD_OFF = 0,
	 * 				VOICEMIXER_MOD_VOL, 
	 * 				VOICEMIXER_MOD_PAN,
	 * 				VOICEMIXER_MOD_VOL_PAN  
	 */
	void modEnableAll(uint8_t mask)
	{
		__disable_irq();
		for (int i=0; i<VOICEMIXER_CHANNELS; i++)
		{
			modEnable(i, mask);
		}
		__enable_irq();
	}
	float getGain(uint8_t channel) { return (channel < VOICEMIXER_CHANNELS ? chnGain[channel]/32767.0f : 0);}
	float getPan(uint8_t channel) { return (channel < VOICEMIXER_CHANNELS ? chnPan[channel]/32767.0f : 0);}

private:
	audio_block_t *inputQueueArray[VOICEMIXER_CHANNELS+2];
	int16_t chnGainSet[VOICEMIXER_CHANNELS];
	int16_t chnGainModMultSet[VOICEMIXER_CHANNELS];
	int16_t chnGainModMult[VOICEMIXER_CHANNELS];
	int16_t chnGain[VOICEMIXER_CHANNELS];


	int16_t chnPanSet[VOICEMIXER_CHANNELS];
	int16_t chnPanModMultSet[VOICEMIXER_CHANNELS];
	int16_t chnPanModMult[VOICEMIXER_CHANNELS];
	int16_t chnPan[VOICEMIXER_CHANNELS];
	
	
	int32_t multL[VOICEMIXER_CHANNELS];
	int32_t multR[VOICEMIXER_CHANNELS];
	uint8_t modMask[VOICEMIXER_CHANNELS];


	void mixGain(uint8_t channel)
	{
		int sign = chnGainModMultSet[channel] < 0 ? -1 : 1;
		int32_t mod_amp = modMask[channel] != VOICEMIXER_MOD_OFF ? abs(chnGainModMultSet[channel]) : 0;

		chnGain[channel] = chnGainSet[channel];
		if ((int32_t)chnGainSet[channel] + mod_amp >= 32767)
		{
			chnGain[channel] = 32767 - mod_amp;
		}
		chnGainModMult[channel] = chnGainModMultSet[channel];
		if ((int32_t)chnGainSet[channel] - mod_amp < 0)
		{
			chnGainModMult[channel] = chnGainSet[channel] * sign;
		}
	}

	void mixPan(uint8_t channel)
	{
		int32_t mod_amp = modMask[channel] != VOICEMIXER_MOD_OFF ? abs(chnPanModMultSet[channel]) : 0;

		chnPan[channel] = chnPanSet[channel];
		if ((int32_t)chnPanSet[channel] + mod_amp >= 32767)
		{
			chnPan[channel] = 32767 - mod_amp;
		}
		chnPanModMult[channel] = chnPanModMultSet[channel];
		if ((int32_t)chnPanSet[channel] - mod_amp < 0)
		{
			chnPan[channel] = mod_amp;

		}
	}


	/**
	 * @brief recalculate gains for L&R channels
	 * 		based on the volume & panorama setting
	 * 
	 * @param channel input channel number
	 */
	inline void calc_gain(uint8_t channel)
	{
		int32_t gL, gR;
		mix_const_ampl_i16(chnPan[channel], (int16_t*)&gR, (int16_t*)&gL);
		gL = signed_saturate_rshift(multiply_16bx16b(gL, chnGain[channel]), 16, 15);
		gR = signed_saturate_rshift(multiply_16bx16b(gR, chnGain[channel]), 16, 15);
		
		__disable_irq()
		multL[channel] = gL;
		multR[channel] = gR;
		__enable_irq();
	}

	/**
	 * @brief calculate gains for L&R channels based on provided volume and pan setting
	 * 
	 * @param gain channel gain
	 * @param pan channel panorama
	 * @param gainL pointer to the left channel gain 
	 * @param gainR pointer to the right channel gain
	 */
	inline void calc_gain(int16_t gain, int16_t pan, int16_t* gainL, int16_t* gainR)
	{
		int32_t gL, gR;
		mix_const_ampl_i16(pan, (int16_t*)&gR, (int16_t*)&gL);
		gL = signed_saturate_rshift(multiply_16bx16b(gL, gain), 16, 15);
		gR = signed_saturate_rshift(multiply_16bx16b(gR, gain), 16, 15);
		*gainL = gL;
		*gainR = gR;
	}

#endif
};

#endif // _MIXERSYNTH8_H_
