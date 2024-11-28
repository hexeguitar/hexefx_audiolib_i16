/*  Mono Phaser/Vibrato effect for Teensy Audio library
 *
 *  Author: Piotr Zapart
 *          www.hexefx.com
 *
 * Copyright (c) 2021 by Piotr Zapart
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

#include <Arduino.h>
#include "effect_phaserStereo_i16.h"

// ---------------------------- INTERNAL LFO -------------------------------------
#define LFO_LUT_BITS					8
#define LFO_MAX_F						(AUDIO_SAMPLE_RATE_EXACT / 2.0f)		
#define LFO_INTERP_INT_SHIFT			(32-LFO_LUT_BITS)
#define LFO_INTERP_FRACT_MASK			((1<<LFO_INTERP_INT_SHIFT)-1)
#define LFO_LUT_SIZE_MASK               ((1<<LFO_LUT_BITS)-1)

// parabollic/hypertriangular waveform used for the internal LFO
extern "C" {
extern const uint16_t AudioWaveformHyperTri[];
}
// ---------------------------- /INTERNAL LFO ------------------------------------

AudioEffectPhaserStereo_i16::AudioEffectPhaserStereo_i16() : AudioStream(3, inputQueueArray)
{
	memset(allpass_x, 0, PHASER_STEREO_STAGES * sizeof(float32_t) * 2);
	memset(allpass_y, 0, PHASER_STEREO_STAGES * sizeof(float32_t) * 2);
    bp = false;
    lfo_phase_acc = 0;
    lfo_add = 0;
    lfo_lrphase = 0.0f;
    lfo_lroffset = 0;
    feedb = 0.0f;
    mix_ratio = 0.5f;         // start with classic phaser sound 
    stg = PHASER_STEREO_STAGES;
}
AudioEffectPhaserStereo_i16::~AudioEffectPhaserStereo_i16()
{
}


void AudioEffectPhaserStereo_i16::update()
{
#if defined(__IMXRT1062__)
    audio_block_t *blockL, *blockR; 
    const audio_block_t *blockMod;    // inputs
    bool internalLFO = false;                    // use internal LFO of no modulation input
    uint16_t i = 0;
    float32_t modSigL, modSigR;
    uint32_t phaseAcc = lfo_phase_acc;
    uint32_t phaseAdd = lfo_add;
    float32_t _lfo_scaler = lfo_scaler;
    float32_t _lfo_bias = lfo_bias;
    uint32_t y0, y1, fract;
    uint64_t y;
    float32_t inSigL, drySigL, inSigR, drySigR;
    float32_t fdb = feedb;

    blockL = receiveWritable(0);       // audio data
    blockR = receiveWritable(1);       // audio data
 
 	if (!bypass_process(&blockL, &blockR, BYPASS_MODE_PASS, bp))
		return;
 
    blockMod = receiveReadOnly(2);      // bipolar/int16_t control input
    if (!blockMod)  internalLFO = true;         // no modulation input provided -> use internal LFO

	for (i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
    {
        if(internalLFO)
        {
            uint32_t LUTaddr = phaseAcc >> LFO_INTERP_INT_SHIFT;	//8 bit address
            fract = phaseAcc & LFO_INTERP_FRACT_MASK;				// fractional part mask
            y0 = AudioWaveformHyperTri[LUTaddr];
            y1 = AudioWaveformHyperTri[LUTaddr+1];
            y = ((int64_t) y0 * (LFO_INTERP_FRACT_MASK - fract));
            y += ((int64_t) y1 * (fract));
            modSigL = (float32_t)(y>>LFO_INTERP_INT_SHIFT) / 65535.0f;
            if (lfo_lroffset)
            {
                LUTaddr = (LUTaddr + lfo_lroffset) & LFO_LUT_SIZE_MASK;
                y0 = AudioWaveformHyperTri[LUTaddr];
                y1 = AudioWaveformHyperTri[LUTaddr+1];
                y = ((int64_t) y0 * (LFO_INTERP_FRACT_MASK - fract));
                y += ((int64_t) y1 * (fract));
                modSigR = (float32_t)(y>>LFO_INTERP_INT_SHIFT) / 65535.0f;               
            }
            else    modSigR = modSigL;

            phaseAcc += phaseAdd;
        }
        else    // external modulation signal does not use modulation offset between LR 
        {
            modSigL = ((float32_t)blockMod->data[i] + 32768.0f) / 65535.0f;    // mod signal is 0.0 to 1.0
            modSigR = modSigL;  
        }
        // apply scale/offset to the modulation wave
        modSigL = modSigL * _lfo_scaler + _lfo_bias;
        modSigR = modSigR * _lfo_scaler + _lfo_bias;




        drySigL = ((float32_t)blockL->data[i] / 32768.0f) * (1.0f - abs(fdb)*0.25f);  // attenuate the input if using feedback
        inSigL = drySigL + last_sampleL * fdb;
        drySigR = ((float32_t)blockR->data[i] / 32768.0f)  * (1.0f - abs(fdb)*0.25f);
        inSigR = drySigR + last_sampleR * fdb;

        y0 = stg;
        while (y0)  // process allpass filters in pairs
        {
            y0--;
		    allpass_y[0][y0] = modSigL * (allpass_y[0][y0] + inSigL) - allpass_x[0][y0];    // left channel
		    allpass_x[0][y0] = inSigL;
		    allpass_y[1][y0] = modSigR * (allpass_y[1][y0] + inSigR) - allpass_x[1][y0];    // right channel
		    allpass_x[1][y0] = inSigR;
            y0--;
		    allpass_y[0][y0] = modSigL * (allpass_y[0][y0] + allpass_y[0][y0+1]) - allpass_x[0][y0];
		    allpass_x[0][y0] = allpass_y[0][y0+1];
            inSigL = allpass_y[0][y0];
            allpass_y[1][y0] = modSigR * (allpass_y[1][y0] + allpass_y[1][y0+1]) - allpass_x[1][y0];
		    allpass_x[1][y0] = allpass_y[1][y0+1];
            inSigR = allpass_y[1][y0];
        }
		if (inSigL > 1.0f) 			inSigL = 1.0f;
		else if (inSigL < -1.0f) 	inSigL = -1.0f;
		if (inSigR > 1.0f)			inSigR = 1.0f;
		else if (inSigR < -1.0f) 	inSigR = -1.0f;

        last_sampleL = inSigL;
        last_sampleR = inSigR;
        blockL->data[i] = (int16_t)((drySigL * (1.0f - mix_ratio) + last_sampleL * mix_ratio) * 32767.0f) ;     // dry/wet mixer
        blockR->data[i] = (int16_t)((drySigR * (1.0f - mix_ratio) + last_sampleR * mix_ratio) * 32767.0f);     // dry/wet mixer

    }
    lfo_phase_acc = phaseAcc;
    transmit(blockL, 0);
    transmit(blockR, 1);
	release(blockL);
    release(blockR);
    if (blockMod) release((audio_block_t *)blockMod);
#endif


}


bool AudioEffectPhaserStereo_i16::bypass_process(audio_block_t** p_blockL, audio_block_t** p_blockR, bypass_mode_t mode, bool state)
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