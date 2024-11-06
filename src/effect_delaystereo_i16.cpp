/*  Stereo Ping Pong delay for Teensy 4
 *
 *  Author: Piotr Zapart
 *          www.hexefx.com
 *
 * Copyright (c) 2024 by Piotr Zapart
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "effect_delaystereo_i16.h"

#define TREBLE_LOSS_FREQ    (0.20f)
#define BASS_LOSS_FREQ      (0.05f)
#define BASS_FREQ      		(0.15f)
#define DLY_TIME_FAILSAFE	(500)

extern uint8_t external_psram_size;

AudioEffectDelayStereo_i16::AudioEffectDelayStereo_i16(uint32_t dly_range_ms, bool use_psram) : AudioStream(2, inputQueueArray)
{
	begin(dly_range_ms, use_psram);
}

void AudioEffectDelayStereo_i16::begin(uint32_t dly_range_ms, bool use_psram)
{
	initialized = false;
	// failsafe if psram is required but not found
	// limit the delay time to 500ms (88200 bytes at 44.1kHz)
	psram_mode = use_psram;
	#if ARDUINO_TEENSY41
	if (psram_mode && external_psram_size == 0)
	{
		psram_mode = false;
		if (dly_range_ms > DLY_TIME_FAILSAFE) dly_range_ms = DLY_TIME_FAILSAFE;
	}	
	#else
		psram_mode = false;
		if (dly_range_ms > DLY_TIME_FAILSAFE) dly_range_ms = DLY_TIME_FAILSAFE;	
	#endif
	bool memOk = true;
	dly_length = ((float32_t)(dly_range_ms)/1000.0f) * AUDIO_SAMPLE_RATE_EXACT;	
	if (!dly0a.init(dly_length, psram_mode)) memOk = false;
	if (!dly0b.init(dly_length, psram_mode)) memOk = false;
	if (!dly1a.init(dly_length, psram_mode)) memOk = false;
	if (!dly1b.init(dly_length, psram_mode)) memOk = false;
	flt0L.init(BASS_LOSS_FREQ, &bassCut_k, TREBLE_LOSS_FREQ, &trebleCut_k);
	flt1L.init(BASS_LOSS_FREQ, &bass_k, TREBLE_LOSS_FREQ, &treble_k);
	flt0R.init(BASS_LOSS_FREQ, &bassCut_k, TREBLE_LOSS_FREQ, &trebleCut_k);
	flt1R.init(BASS_LOSS_FREQ, &bass_k, TREBLE_LOSS_FREQ, &treble_k);
	mix(0.5f);
	feedback(0.5f);
	cleanup_done = true;
	if (memOk) initialized = true;
}

void AudioEffectDelayStereo_i16::update()
{
	if (!initialized) return;
	if (!memsetup_done)
	{
		memsetup_done = memCleanup();
		return;		
	}

	audio_block_t *blockL, *blockR;
	int i;
	float32_t acc1, acc2, inL, inR, outL, outR, mod_fr[4];
	uint32_t mod_int;
	static float32_t dly_time_flt = 0.0f;

	blockL = receiveWritable(0);
	blockR = receiveWritable(1);
	if (!bypass_process(&blockL, &blockR, bp_mode, bp))
		return;

    if (bp)
    {
		// mem cleanup not required in TRAILS mode
		if (!cleanup_done && bp_mode != BYPASS_MODE_TRAILS)
		{
			cleanup_done = memCleanup();
			tap_active = false;	// reset tap tempo
			tap_counter = 0;
		}
		if (infinite) freeze(false);
		if (bp_mode != BYPASS_MODE_TRAILS)
		{
			transmit(blockL, 0);
			transmit(blockR, 1);
			release(blockL);
			release(blockR);
			return;
		}
		else
		{
			inputGainSet = 0.0f;
			tap_active = false;	// reset tap tempo
			tap_counter = 0;		
		}
	}

	cleanup_done = false;

	for (i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
    {  
		inputGain += (inputGainSet - inputGain) * 0.25f;
		// tap tempo
		if (tap_active)
		{
			if (++tap_counter > tap_counter_max)
			{
				tap_active = false;
				tap_counter = 0;
			}
		}

		if (dly_time < dly_time_set)
		{
			dly_time += dly_time_step;
			if (dly_time > dly_time_set) dly_time = dly_time_set;
		}
		if (dly_time > dly_time_set)
		{
			dly_time -= dly_time_step;
			if (dly_time < dly_time_set) dly_time = dly_time_set;
		}
		// lowpass the delay time
		acc1 = dly_time - dly_time_flt;
		dly_time_flt += acc1 * 0.1f;
		dly_time = dly_time_flt;

		lfo.update();

		lfo.get(BASIC_LFO_PHASE_0, &mod_int, &mod_fr[0]);
		mod_fr[0] = (float32_t)mod_int + mod_fr[0];
		acc2 = (float32_t)dly_length - 1.0f - (dly_time + mod_fr[0]);
		if (acc2 < 0.0f) mod_fr[0] += acc2;

		lfo.get(BASIC_LFO_PHASE_60, &mod_int, &mod_fr[1]);
		mod_fr[1] = (float32_t)mod_int + mod_fr[1];
		acc2 = (float32_t)dly_length - 1.0f - (dly_time + mod_fr[1]);
		if (acc2 < 0.0f) mod_fr[1] += acc2;

		lfo.get(BASIC_LFO_PHASE_120, &mod_int, &mod_fr[2]);
		mod_fr[2] = (float32_t)mod_int + mod_fr[2];
		acc2 = (float32_t)dly_length - 1.0f - (dly_time + mod_fr[2]);
		if (acc2 < 0.0f) mod_fr[2] += acc2;	

		lfo.get(BASIC_LFO_PHASE_180, &mod_int, &mod_fr[3]);
		mod_fr[3] = (float32_t)mod_int + mod_fr[3];	
		acc2 = (float32_t)dly_length - 1.0f - (dly_time + mod_fr[3]);
		if (acc2 < 0.0f) mod_fr[3] += acc2;		

		inL = (float32_t)blockL->data[i] / 32768.0f;
		inR = (float32_t)blockR->data[i] / 32768.0f; 

		acc1 = dly0b.getTapHermite(dly_time+mod_fr[0]);
		outR = acc1 * 0.6f;
		acc1 = flt0R.process(acc1) * feedb;
		acc1 += inR * inputGain;
		acc1 = flt1R.process(acc1);
		acc2 = dly0a.getTapHermite(dly_time+mod_fr[1]);
		dly0b.write_toOffset(acc2, 0);
		outL = acc2 * 0.6f;
		dly0a.write_toOffset(acc1, 0);

		acc1 = dly1b.getTapHermite(dly_time+mod_fr[2]);
		outR += acc1 * 0.6f;
		acc1 = flt0L.process(acc1) * feedb;
		acc1 += inL * inputGain;
		acc1 = flt1L.process(acc1);
		acc2 = dly1a.getTapHermite(dly_time+mod_fr[3]);
		dly1b.write_toOffset(acc2, 0);
		outL += acc2 * 0.6f;
		dly1a.write_toOffset(acc1, 0);

		dly0a.updateIndex();
		dly0b.updateIndex();
		dly1a.updateIndex();
		dly1b.updateIndex();
		blockL->data[i] = (int16_t)((outL * wet_gain + inL * dry_gain) * 32767.0f);
		blockR->data[i] = (int16_t)((outR * wet_gain + inR * dry_gain) * 32767.0f);

	}
    transmit(blockL, 0);
	transmit(blockR, 1);
	release(blockL);
	release(blockR);
}
void AudioEffectDelayStereo_i16::freeze(bool state)
{
	if (infinite == state) return;
	infinite = state;
	if (state)
	{
		feedb_tmp = feedb;      // store the settings
		inputGain_tmp = inputGainSet;
		// bassCut_k_tmp = bassCut_k;
		// trebleCut_k_tmp = trebleCut_k;
		__disable_irq();
		flt0R.bypass_set(true);
		flt1R.bypass_set(true);
		flt0L.bypass_set(true);
		flt1L.bypass_set(true);
		feedb = 1.0f; // infinite echo                                    
		inputGainSet = freeze_ingain;
		__enable_irq();
	}
	else
	{
		__disable_irq();
		feedb = feedb_tmp;
		inputGainSet = inputGain_tmp;
		flt0R.bypass_set(false);
		flt1R.bypass_set(false);
		flt0L.bypass_set(false);
		flt1L.bypass_set(false);		
		__enable_irq();
	}
}

/**
 * @brief Partial memory clear
 * 	Clearing all the delay buffers at once, esp. if 
 * 	the PSRAM is used takes too long for the audio ISR.
 *  Hence the buffer clear is done in configurable portions
 *  spread over a few audio update routines.
 * 
 * @return true 	Memory clean is complete
 * @return false 	Memory clean still in progress
 */
bool AudioEffectDelayStereo_i16::memCleanup()
{

	static uint8_t dlyIdx = 0;
	bool result = false;
	if (dlyIdx == 0) // value 0 is used to reset the addr
	{
		memCleanupStart = 0;
		memCleanupEnd = memCleanupStep;
		flt0L.reset();
		flt0R.reset();
		flt1L.reset();
		flt1R.reset();
		dlyIdx = 1;
	}
	if (memCleanupEnd > dly_length) 		// last segment
	{
		memCleanupEnd = dly_length;
		result = true;
	}
	switch(dlyIdx)
	{
		case 1:
			dly0a.reset(memCleanupStart, memCleanupEnd);
			memCleanupStart = memCleanupEnd;
			memCleanupEnd += memCleanupStep;
			if (result) // if done, reset the mem addr
			{
				memCleanupStart = 0;
				memCleanupEnd = memCleanupStep;
				dlyIdx = 2;
				result = false;
			}
			break;
		case 2:
			dly0b.reset(memCleanupStart, memCleanupEnd);
			memCleanupStart = memCleanupEnd;
			memCleanupEnd += memCleanupStep;			
			if (result) // if done, reset the mem addr
			{
				memCleanupStart = 0;
				memCleanupEnd = memCleanupStep;
				dlyIdx = 3;
				result = false;
			}		
			break;
		case 3:
			dly1a.reset(memCleanupStart, memCleanupEnd);
			memCleanupStart = memCleanupEnd;
			memCleanupEnd += memCleanupStep;			
			if (result) // if done, reset the mem addr
			{
				memCleanupStart = 0;
				memCleanupEnd = memCleanupStep;
				dlyIdx = 4;
				result = false;
			}	
			break;
		case 4:
			dly1b.reset(memCleanupStart, memCleanupEnd);
			memCleanupStart = memCleanupEnd;
			memCleanupEnd += memCleanupStep;			
			if (result) // if done, reset the mem addr
			{
				dlyIdx = 0;
				result = true;
			}				
			break;
		default:
			dlyIdx = 0; // cleanup done, reset the dly line idx
			result = false;
			break;
	}	
	return result;
}

bool AudioEffectDelayStereo_i16::bypass_process(audio_block_t** p_blockL, audio_block_t** p_blockR, bypass_mode_t mode, bool state)
{
	bool result = false;

	/**
	 * @brief bypass mode PASS can be used to validate the incoming audio blocks, create silence blocks if the input ones 
	 * 			are not available (NULL). 
	 */
	if (!state) mode = BYPASS_MODE_PASS;
	
	switch(mode)
	{
		/**
		 * @brief TRAILS mode is the same as OFF with the different the component "update" function does not return
		 * 			and processes the incoming data, which is silence. Used in reverb/delays to let them naturally 
		 * 			fade out
		 */
		case BYPASS_MODE_TRAILS:
		/**
		 * @brief bypass mode OFF sends silence on both outputs. Used with components placed in an effect loop with separate
		 * 			dry path. Classic example is a parallel effect loop used for delays and reverbs: 
		 * 			input ------------>	|0		|---> output
		 * 				|--->reverb	-->	|1 mixer|
		 */
		case BYPASS_MODE_OFF:
			if (*p_blockL) release(*p_blockL);								// discard both input blocks
			if (*p_blockR) release(*p_blockR);
			*p_blockL = NULL;
			*p_blockR = NULL;
			// no break - let it run through the next switch case, with input blocks as NULL it will emit silence on both channels

		/**
		 * @brief PASS mode connects the input signal directly to the output
		 * 		in case one of the blocks is not available, it tries to allocate a new block and make it silent
		 * 		returns false if allocation is not possbile due to not enoufg audio memory (increase AudioMemory(xx);)
		 * 		Used in components connected in series.
		 */
		case BYPASS_MODE_PASS:
			if(!*p_blockL)																	// no channel left available
			{
				*p_blockL = allocate();								// try to allocate a new block
				if( !*p_blockL)
				{
					if (*p_blockR) release(*p_blockR);						// if the Rch is present, release/discard it
					result = false;
					break;																	// operation failed due to no audio memory left
				}
				memset(&(*p_blockL)->data[0], 0, AUDIO_BLOCK_SAMPLES*sizeof(int16_t));	// zero the data block to make it silent
			}
			if(!*p_blockR)																	// no channel right available
			{
				*p_blockR = allocate();
				if( !*p_blockR) 															// no memory for a new block, but we might have L channel zeroed
				{
					if (*p_blockL) 	release(*p_blockL);					// blockL is available and contains audio. Discard it
					result = false;
					break;																	// and sigbnal failed operation
				}
				memset(&(*p_blockR)->data[0], 0, AUDIO_BLOCK_SAMPLES*sizeof(int16_t));	
			}
			result = true;																	// audio data on L and R is avaialble
			break;
		
		default:
			break;
	}
	return result;
}