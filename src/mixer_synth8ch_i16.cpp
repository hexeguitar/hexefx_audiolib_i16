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
 *
 * ---------------------------------------------
 * 8 mono input version with stereo output and gain/pan control per channel
 * useful for polyphonic synth voice mixers 
 * -----> Teensy4.x only <-----
 * 
 * 12.2024 Piotr Zapart (www.hexefx.com)
 */


#include <Arduino.h>
#include "mixer_synth8ch_i16.h"
#include "basic_DSPutils.h"


#if defined(__ARM_ARCH_7EM__)
#define VOICEMIXER_MULTI_UNITYGAIN 32767


/**
 * @brief multiply an audio data block by another block (ie volume modulation)
 * 		This version uses 15bit multiplier, as the modulation input accepts int16_t
 * 		data.
 * 
 * @param data 			pointer to the audio data block
 * @param mult_block 	pointer to the gain/multiplier block
 */
static void applyGain(int16_t *data, int16_t *mult_block)
{
	uint32_t *p = (uint32_t *)data;
	const uint32_t *end = (uint32_t *)(data + AUDIO_BLOCK_SAMPLES);
	uint32_t *p_g = (uint32_t *)mult_block;

	do
	{
		uint32_t tmp32 = *p; // read 2 samples from *data
		uint32_t gain = *p_g++;
		int32_t val2 = multiply_16tx16t(gain, tmp32);// mult[31_16] * data[31:16]
		int32_t val1 = multiply_16bx16b(gain, tmp32); // mult[15:0] * data[15:0]	
		val1 = signed_saturate_rshift(val1, 16, 15);
		val2 = signed_saturate_rshift(val2, 16, 15);
		*p++ = pack_16b_16b(val2, val1);
	} while (p < end);
}


/**
 * @brief multiply audio data block by a 16bit value
 * 
 * @param data pointer to the input data block
 * @param mult 16bit multiplier
 */
static void applyGain(int16_t *data, int32_t mult)
{
	uint32_t *p = (uint32_t *)data;
	const uint32_t *end = (uint32_t *)(data + AUDIO_BLOCK_SAMPLES);

	do
	{
		uint32_t tmp32 = *p; // read 2 samples from *data
		int32_t val1 = signed_multiply_32x16b(mult, tmp32);
		int32_t val2 = signed_multiply_32x16t(mult, tmp32);
		val1 = signed_saturate_rshift(val1, 15, 0);
		val2 = signed_saturate_rshift(val2, 15, 0);
		*p++ = pack_16b_16b(val2, val1);
	} while (p < end);
}

/**
 * @brief Takes the "in" data block, multiplies by "mult" and 
 * 		adds to the "data" block
 * 
 * @param data 
 * @param in 
 * @param mult 
 */
static void applyGainThenAdd(int16_t *data, const int16_t *in, int32_t mult)
{
	uint32_t *dst = (uint32_t *)data;
	const uint32_t *src = (uint32_t *)in;
	const uint32_t *end = (uint32_t *)(data + AUDIO_BLOCK_SAMPLES);

	if (mult == VOICEMIXER_MULTI_UNITYGAIN)
	{
		do
		{
			uint32_t tmp32 = *dst;
			*dst++ = signed_add_16_and_16(tmp32, *src++);
			tmp32 = *dst;
			*dst++ = signed_add_16_and_16(tmp32, *src++);
		} while (dst < end);
	}
	else
	{
		do
		{
			uint32_t tmp32 = *src++; // read 2 samples from *data
			int32_t val1 = signed_multiply_32x16b(mult, tmp32);
			int32_t val2 = signed_multiply_32x16t(mult, tmp32);
			val1 = signed_saturate_rshift(val1, 15, 0);
			val2 = signed_saturate_rshift(val2, 15, 0);
			tmp32 = pack_16b_16b(val2, val1);
			uint32_t tmp32b = *dst;
			*dst++ = signed_add_16_and_16(tmp32, tmp32b);
		} while (dst < end);
	}
}

/**
 * @brief Takes the "in" data block, multiplies by "mult" and 
 * 		adds to the "data" block
 * 
 * @param data 
 * @param in 
 * @param mult 
 */
static void applyGainThenAdd(int16_t *data, const int16_t *in, int16_t *mult)
{
	uint32_t *dst = (uint32_t *)data;
	const uint32_t *src = (uint32_t *)in;
	const uint32_t *end = (uint32_t *)(data + AUDIO_BLOCK_SAMPLES);
	uint32_t *gain = (uint32_t *)mult;
	do
	{
		uint32_t tmp32 = *src++; // read 2 samples from *data
		uint32_t g = *gain++;
		int32_t val1 = multiply_16bx16b(g, tmp32); // mult[15:0] * data[15:0]
		int32_t val2 = multiply_16tx16t(g, tmp32);// mult[31_16] * data[31:16]
		val1 = signed_saturate_rshift(val1, 16, 15);
		val2 = signed_saturate_rshift(val2, 16, 15);
		tmp32 = pack_16b_16b(val2, val1);
		uint32_t tmp32b = *dst;
		*dst++ = signed_add_16_and_16(tmp32, tmp32b);
	} while (dst < end);
}


void AudioMixerSynth8ch_i16::update(void)
{
	audio_block_t *in, *outL = NULL, *outR = NULL, *modVol, *modPan;
	audio_block_t *gainL=NULL, *gainR=NULL;
	unsigned int channel;
	uint8_t mod = 0; 											// 0=mod off, 1=modVol, 2=modPan, 3=modVol+modfPan
	uint8_t i;
	int32_t p, v;

	outR = allocate();
	if (!outR) return;
	modVol = receiveReadOnly(VOICEMIXER_CHANNELS); 				// volume modulation input
	modPan = receiveReadOnly(VOICEMIXER_CHANNELS+1);			// panorama modulation input
	// LR gain blocks will be required if any of thes inputs are present
	// and modulation is globally enabled
	if ((modVol || modPan) && modMask)			
	{
		gainL = allocate();
		gainR = allocate();
		if (gainR && gainL)
		{
			if (modVol) mod = 0b01;
			if (modPan)	mod |= 0b10;
		} 
		else 
		{	// not possible to allocate gain blocks, ignore the modulation inputs
			if (gainL) release(gainL);
			if (gainR) release(gainR);
			if (modVol) release(modVol);
			if (modPan) release(modPan);
			mod = 0;
		}
	}
	for (channel = 0; channel < VOICEMIXER_CHANNELS; channel++)
	{
		mod &= modMask[channel]; // apply the global setting 

		int16_t _ch_g = chnGain[channel];
		int16_t _ch_p = chnPan[channel];
		int16_t _ch_gm = mod & 0b01 ?  chnGainModMult[channel] : 0;
		int16_t _ch_pm = mod & 0b10 ? chnPanModMult[channel] : 0;

		// calculate gain blocks depending on the mod setting
		switch (mod)
		{
			case 0: break;
			case 1:	// only volume modulation, use fixed pan setting
				for (i=0; i<AUDIO_BLOCK_SAMPLES; i=i+4)
				{
					// final volume = bias + modulation
					v = signed_saturate_rshift(multiply_16bx16b(modVol->data[i], _ch_gm), 16, 15) + _ch_g;
					calc_gain(v, _ch_p, &gainL->data[i], &gainR->data[i]);

					v = signed_saturate_rshift(multiply_16bx16b(modVol->data[i+1], _ch_gm), 16, 15) + _ch_g;					
					calc_gain(v, _ch_p, &gainL->data[i+1], &gainR->data[i+1]);

					v = signed_saturate_rshift(multiply_16bx16b(modVol->data[i+2], _ch_gm), 16, 15) + _ch_g;
					calc_gain(v, _ch_p, &gainL->data[i+2], &gainR->data[i+2]);

					v = signed_saturate_rshift(multiply_16bx16b(modVol->data[i+3], _ch_gm), 16, 15) + _ch_g;
					calc_gain(v, _ch_p, &gainL->data[i+3], &gainR->data[i+3]);
				}
				break;
			case 2:	// only panorama modulation, use fixed volume setting
				for (i=0; i<AUDIO_BLOCK_SAMPLES; i=i+4)
				{
					p = signed_saturate_rshift(multiply_16bx16b(modPan->data[i], _ch_pm), 16, 15) + _ch_p;
					calc_gain(_ch_g, p, &gainL->data[i], &gainR->data[i]);

					p = signed_saturate_rshift(multiply_16bx16b(modPan->data[i+1], _ch_pm), 16, 15) + _ch_p;
					calc_gain(_ch_g, p, &gainL->data[i+1], &gainR->data[i+1]);

					p = signed_saturate_rshift(multiply_16bx16b(modPan->data[i+2], _ch_pm), 16, 15) + _ch_p;
					calc_gain(_ch_g, p, &gainL->data[i+2], &gainR->data[i+2]);

					p = signed_saturate_rshift(multiply_16bx16b(modPan->data[i+3], _ch_pm), 16, 15) + _ch_p;
					calc_gain(_ch_g, p, &gainL->data[i+3], &gainR->data[i+3]);
				}	
				break;
			case 3:	
				// volume + panorama are moodulated
				for (i=0; i<AUDIO_BLOCK_SAMPLES; i=i+4)
				{
					v = signed_saturate_rshift(multiply_16bx16b(modVol->data[i], _ch_gm), 16, 15) + _ch_g;
					p = signed_saturate_rshift(multiply_16bx16b(modPan->data[i], _ch_pm), 16, 15) + _ch_p;
					calc_gain(v, p, &gainL->data[i], &gainR->data[i]);

					v = signed_saturate_rshift(multiply_16bx16b(modVol->data[i+1], _ch_gm), 16, 15) + _ch_g;
					p = signed_saturate_rshift(multiply_16bx16b(modPan->data[i+1], _ch_pm), 16, 15) + _ch_p;
					calc_gain(v, p, &gainL->data[i+1], &gainR->data[i+1]);

					v = signed_saturate_rshift(multiply_16bx16b(modVol->data[i+2], _ch_gm), 16, 15) + _ch_g;
					p = signed_saturate_rshift(multiply_16bx16b(modPan->data[i+2], _ch_pm), 16, 15) + _ch_p;
					calc_gain(v, p, &gainL->data[i+2], &gainR->data[i+2]);

					v = signed_saturate_rshift(multiply_16bx16b(modVol->data[i+3], _ch_gm), 16, 15) + _ch_g;
					p = signed_saturate_rshift(multiply_16bx16b(modPan->data[i+3], _ch_pm), 16, 15) + _ch_p;
					calc_gain(v, p, &gainL->data[i+3], &gainR->data[i+3]);										
				}					
				break;
			default: break;

		}
		if (!outL)
		{
			outL = receiveWritable(channel);		
			if (outL)
			{
				int32_t mult = multR[channel];
				arm_copy_q15(outL->data, outR->data, AUDIO_BLOCK_SAMPLES);
					if (mod)	applyGain(outR->data, gainR->data); // modulation input
					else 		applyGain(outR->data, mult);
				mult = multL[channel];
					if (mod)	applyGain(outL->data, gainL->data);
					else 		applyGain(outL->data, mult);
			}
			else
			{
				arm_fill_q15(0, outR->data, AUDIO_BLOCK_SAMPLES);
			}
		}
		else
		{
			in = receiveReadOnly(channel);
			if (in)
			{
				if (mod) 	applyGainThenAdd(outL->data, in->data, gainL->data);
				else 		applyGainThenAdd(outL->data, in->data, multL[channel]);
				if (mod) 	applyGainThenAdd(outR->data, in->data, gainR->data);
				else 		applyGainThenAdd(outR->data, in->data, multR[channel]);				
				release(in);
			}
		}
	}
	if (outL)
	{
		transmit(outL, 0);
		release(outL);
	}
	if (outR)
	{
		transmit(outR, 1);
		release(outR);
	}
	if (gainL) release(gainL);
	if (gainR) release(gainR);
	if (modVol) release(modVol);
	if (modPan) release(modPan);
}

#endif
