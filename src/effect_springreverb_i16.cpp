/*  Stereo spring reverb  for Teensy 4
 *
 *  Author: Piotr Zapart
 *          www.hexefx.com
 *
 * Copyright (c) 2024 by Piotr Zapart
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
 */

#include <Arduino.h>
#include "effect_springreverb_i16.h"

#define INP_ALLP_COEFF      (0.6f)
#define CHIRP_ALLP_COEFF    (-0.6f)

#define TREBLE_LOSS_FREQ    (0.55f)
#define BASS_LOSS_FREQ      (0.36f)

AudioEffectSpringReverb_i16::AudioEffectSpringReverb_i16() : AudioStream(2, inputQueueArray)
{
    inputGain = 0.5f;
	rv_time_k = 0.8f;
    in_allp_k = INP_ALLP_COEFF;
	bool memOK = true;
	if(!sp_lp_allp1a.init(&in_allp_k)) memOK = false;
	if(!sp_lp_allp1b.init(&in_allp_k)) memOK = false;
	if(!sp_lp_allp1c.init(&in_allp_k)) memOK = false;
	if(!sp_lp_allp1d.init(&in_allp_k)) memOK = false;

	if(!sp_lp_allp2a.init(&in_allp_k)) memOK = false;
	if(!sp_lp_allp2b.init(&in_allp_k)) memOK = false;
	if(!sp_lp_allp2c.init(&in_allp_k)) memOK = false;
	if(!sp_lp_allp2d.init(&in_allp_k)) memOK = false;	
	if(!lp_dly1.init(SPRVB_DLY1_LEN)) memOK = false;
	if(!lp_dly2.init(SPRVB_DLY2_LEN)) memOK = false;
	// chirp allpass chain
	sp_chrp_alp1_buf = (float *)malloc(SPRVB_CHIRP_AMNT*SPRVB_CHIRP1_LEN*sizeof(float));
	sp_chrp_alp2_buf = (float *)malloc(SPRVB_CHIRP_AMNT*SPRVB_CHIRP2_LEN*sizeof(float));
	sp_chrp_alp3_buf = (float *)malloc(SPRVB_CHIRP_AMNT*SPRVB_CHIRP3_LEN*sizeof(float));
	sp_chrp_alp4_buf = (float *)malloc(SPRVB_CHIRP_AMNT*SPRVB_CHIRP4_LEN*sizeof(float));
	if (!sp_chrp_alp1_buf) memOK = false;
	if (!sp_chrp_alp2_buf) memOK = false;
	if (!sp_chrp_alp3_buf) memOK = false;
	if (!sp_chrp_alp4_buf) memOK = false;
	memset(&sp_chrp_alp1_buf[0], 0, SPRVB_CHIRP_AMNT*SPRVB_CHIRP1_LEN*sizeof(float));
	memset(&sp_chrp_alp2_buf[0], 0, SPRVB_CHIRP_AMNT*SPRVB_CHIRP2_LEN*sizeof(float));
	memset(&sp_chrp_alp3_buf[0], 0, SPRVB_CHIRP_AMNT*SPRVB_CHIRP3_LEN*sizeof(float));
	memset(&sp_chrp_alp4_buf[0], 0, SPRVB_CHIRP_AMNT*SPRVB_CHIRP4_LEN*sizeof(float));
	in_BassCut_k = 0.0f;
	in_TrebleCut_k = 0.95f;
	lp_BassCut_k = 0.0f;
	lp_TrebleCut_k = 1.0f;
	flt_in.init(BASS_LOSS_FREQ, &in_BassCut_k, TREBLE_LOSS_FREQ, &in_TrebleCut_k);
	flt_lp1.init(BASS_LOSS_FREQ, &lp_BassCut_k, TREBLE_LOSS_FREQ, &lp_TrebleCut_k);
	flt_lp2.init(BASS_LOSS_FREQ, &lp_BassCut_k, TREBLE_LOSS_FREQ, &lp_TrebleCut_k);
	mix(0.5f);
	cleanup_done = true;
	if (memOK) initialized = true;
}

void AudioEffectSpringReverb_i16::update()
{   
#if defined(__IMXRT1062__)
	audio_block_t *blockL, *blockR;
	int i, j;
	float32_t inL, inR, dryL, dryR;
	float32_t acc;
    float32_t lp_out1, lp_out2, mono_in, dry_in;
    float32_t rv_time;
	uint32_t allp_idx;
	uint32_t offset;
	float lfo_fr;	
    if (!initialized) return;

	blockL = receiveWritable(0);
	blockR = receiveWritable(1);
	if (!bypass_process(&blockL, &blockR, bp_mode, bp))
		return;

    if (bp)
    {
		if (!cleanup_done && bp_mode != BYPASS_MODE_TRAILS)
		{
			sp_lp_allp1a.reset();
			sp_lp_allp1b.reset();
			sp_lp_allp1c.reset();
			sp_lp_allp1d.reset();
			sp_lp_allp2a.reset();
			sp_lp_allp2b.reset();
			sp_lp_allp2c.reset();
			sp_lp_allp2d.reset();
			lp_dly1.reset();
			lp_dly2.reset();
			memset(&sp_chrp_alp1_buf[0], 0, SPRVB_CHIRP_AMNT*SPRVB_CHIRP1_LEN*sizeof(float));
			memset(&sp_chrp_alp2_buf[0], 0, SPRVB_CHIRP_AMNT*SPRVB_CHIRP2_LEN*sizeof(float));
			memset(&sp_chrp_alp3_buf[0], 0, SPRVB_CHIRP_AMNT*SPRVB_CHIRP3_LEN*sizeof(float));
			memset(&sp_chrp_alp4_buf[0], 0, SPRVB_CHIRP_AMNT*SPRVB_CHIRP4_LEN*sizeof(float));			
			cleanup_done = true;
		}
		if (bp_mode != BYPASS_MODE_TRAILS)
		{
			transmit(blockL, 0);
			transmit(blockR, 1);
			release(blockL);
			release(blockR);
			return;
		}
    }
	
	cleanup_done = false;
    rv_time = rv_time_k;
	for (i=0; i < AUDIO_BLOCK_SAMPLES; i++) 
    {  
		lfo.update();
		inputGain += (inputGainSet - inputGain) * 0.25f;
		dryL = (float32_t)blockL->data[i] / 32780.0f;
		dryR = (float32_t)blockR->data[i] / 32768.0f;
		dry_in = (dryL + dryR) * inputGain;

		mono_in = flt_in.process(dry_in)* (1.0f + in_BassCut_k*-2.5f);
		acc = lp_dly1.getTap(0) * rv_time;
		lp_out1 = flt_lp1.process(acc);

		acc = sp_lp_allp1a.process(lp_out1); 
		acc = sp_lp_allp1b.process(acc);
		acc = sp_lp_allp1c.process(acc);
		acc = sp_lp_allp1d.process(acc);
		acc = lp_dly2.process(acc + mono_in) * rv_time;
		lp_out2 = flt_lp2.process(acc);

		acc = sp_lp_allp2a.process(lp_out2); 
		acc = sp_lp_allp2b.process(acc);
		acc = sp_lp_allp2c.process(acc);
		acc = sp_lp_allp2d.process(acc);

		lp_dly1.write_toOffset(acc + mono_in, 0);
		lp_dly1.updateIndex();
		
		inL = inR = (lp_out1 + lp_out2);

		j = 0;
        while(j < SPRVB_CHIRP_AMNT)
        {
			// 1st 4 are left channel
			allp_idx = j*SPRVB_CHIRP1_LEN + chrp_alp1_idx[j];
			acc = sp_chrp_alp1_buf[allp_idx] + inL * chrp_allp_k[0];
			sp_chrp_alp1_buf[allp_idx] = inL - chrp_allp_k[0] * acc;
            inL = acc;
            if (++chrp_alp1_idx[j] >= SPRVB_CHIRP1_LEN) chrp_alp1_idx[j] = 0;   
            
			allp_idx = (j+1)*SPRVB_CHIRP1_LEN + chrp_alp1_idx[j+1];
            acc = sp_chrp_alp1_buf[allp_idx]  + inL * chrp_allp_k[1];  
            sp_chrp_alp1_buf[allp_idx] = inL - chrp_allp_k[1] * acc;			
            inL = acc;
            if (++chrp_alp1_idx[j+1] >= SPRVB_CHIRP1_LEN) chrp_alp1_idx[j+1] = 0; 
            
			allp_idx = (j+2)*SPRVB_CHIRP1_LEN + chrp_alp1_idx[j+2];
			acc = sp_chrp_alp1_buf[allp_idx]  + inL * chrp_allp_k[2];  
            sp_chrp_alp1_buf[allp_idx] = inL - chrp_allp_k[2] * acc;
            inL = acc;
            if (++chrp_alp1_idx[j+2] >= SPRVB_CHIRP1_LEN) chrp_alp1_idx[j+2] = 0; 

			allp_idx = (j+3)*SPRVB_CHIRP1_LEN + chrp_alp1_idx[j+3];
            acc = sp_chrp_alp1_buf[allp_idx]  + inL * chrp_allp_k[3];  
            sp_chrp_alp1_buf[allp_idx] = inL - chrp_allp_k[3] * acc;
            inL = acc;
            if (++chrp_alp1_idx[j+3] >= SPRVB_CHIRP1_LEN) chrp_alp1_idx[j+3] = 0; 
			// channel R
			allp_idx = (j+4)*SPRVB_CHIRP1_LEN + chrp_alp1_idx[j+4];
            acc = sp_chrp_alp1_buf[allp_idx]  + inR * chrp_allp_k[3];  
            sp_chrp_alp1_buf[allp_idx] = inR - chrp_allp_k[3] * acc;
            inR = acc;
            if (++chrp_alp1_idx[j+4] >= SPRVB_CHIRP1_LEN) chrp_alp1_idx[j+4] = 0; 

			allp_idx = (j+5)*SPRVB_CHIRP1_LEN + chrp_alp1_idx[j+5];
            acc = sp_chrp_alp1_buf[allp_idx]  + inR * chrp_allp_k[2];  
            sp_chrp_alp1_buf[allp_idx] = inR - chrp_allp_k[2] * acc;
            inR = acc;
            if (++chrp_alp1_idx[j+5] >= SPRVB_CHIRP1_LEN) chrp_alp1_idx[j+5] = 0; 

			allp_idx = (j+6)*SPRVB_CHIRP1_LEN + chrp_alp1_idx[j+6];
            acc = sp_chrp_alp1_buf[allp_idx]  + inR * chrp_allp_k[1];  
            sp_chrp_alp1_buf[allp_idx] = inR - chrp_allp_k[1] * acc;
            inR = acc;
            if (++chrp_alp1_idx[j+6] >= SPRVB_CHIRP1_LEN) chrp_alp1_idx[j+6] = 0; 

			allp_idx = (j+7)*SPRVB_CHIRP1_LEN + chrp_alp1_idx[j+7];
			acc = sp_chrp_alp1_buf[allp_idx]  + inR * chrp_allp_k[0];  
            sp_chrp_alp1_buf[allp_idx] = inR - chrp_allp_k[0] * acc;			
            inR = acc;
            if (++chrp_alp1_idx[j+7] >= SPRVB_CHIRP1_LEN) chrp_alp1_idx[j+7] = 0; 

            j = j + 8;
        }
        j = 0;
        while(j < SPRVB_CHIRP_AMNT)
        {	// channel L
			allp_idx = j*SPRVB_CHIRP2_LEN + chrp_alp2_idx[j];
            acc = sp_chrp_alp2_buf[allp_idx]  + inL * chrp_allp_k[0];  
            sp_chrp_alp2_buf[allp_idx] = inL - chrp_allp_k[0] * acc;
            inL = acc;
            if (++chrp_alp2_idx[j] >= SPRVB_CHIRP2_LEN) chrp_alp2_idx[j] = 0;   
            
			allp_idx = (j+1)*SPRVB_CHIRP2_LEN + chrp_alp2_idx[j+1];
            acc = sp_chrp_alp2_buf[allp_idx]  + inL * chrp_allp_k[1];  
            sp_chrp_alp2_buf[allp_idx] = inL - chrp_allp_k[1] * acc;			
            inL = acc;
            if (++chrp_alp2_idx[j+1] >= SPRVB_CHIRP2_LEN) chrp_alp2_idx[j+1] = 0; 
            
			allp_idx = (j+2)*SPRVB_CHIRP2_LEN + chrp_alp2_idx[j+2];
            acc = sp_chrp_alp2_buf[allp_idx]  + inL * chrp_allp_k[2];  
            sp_chrp_alp2_buf[allp_idx] = inL - chrp_allp_k[2] * acc;			
            inL = acc;
            if (++chrp_alp2_idx[j+2] >= SPRVB_CHIRP2_LEN) chrp_alp2_idx[j+2] = 0; 
            
			allp_idx = (j+3)*SPRVB_CHIRP2_LEN + chrp_alp2_idx[j+3];
            acc = sp_chrp_alp2_buf[allp_idx]  + inL * chrp_allp_k[3];  
            sp_chrp_alp2_buf[allp_idx] = inL - chrp_allp_k[3] * acc;			
            inL = acc;
            if (++chrp_alp2_idx[j+3] >= SPRVB_CHIRP2_LEN) chrp_alp2_idx[j+3] = 0; 
			// channel R
			allp_idx = (j+4)*SPRVB_CHIRP2_LEN + chrp_alp2_idx[j+4];
			acc = sp_chrp_alp2_buf[allp_idx]  + inR * chrp_allp_k[3];  
            sp_chrp_alp2_buf[allp_idx] = inR - chrp_allp_k[3] * acc;
            inR = acc;
            if (++chrp_alp2_idx[j+4] >= SPRVB_CHIRP2_LEN) chrp_alp2_idx[j+4] = 0; 
    
			allp_idx = (j+5)*SPRVB_CHIRP2_LEN + chrp_alp2_idx[j+5];
			acc = sp_chrp_alp2_buf[allp_idx]  + inR * chrp_allp_k[2];  
            sp_chrp_alp2_buf[allp_idx] = inR - chrp_allp_k[2] * acc;
            inR = acc;
            if (++chrp_alp2_idx[j+5] >= SPRVB_CHIRP2_LEN) chrp_alp2_idx[j+5] = 0; 

			allp_idx = (j+6)*SPRVB_CHIRP2_LEN + chrp_alp2_idx[j+6];
			acc = sp_chrp_alp2_buf[allp_idx]  + inR * chrp_allp_k[1];  
            sp_chrp_alp2_buf[allp_idx] = inR - chrp_allp_k[1] * acc;
            inR = acc;
            if (++chrp_alp2_idx[j+6] >= SPRVB_CHIRP2_LEN) chrp_alp2_idx[j+6] = 0; 

			allp_idx = (j+7)*SPRVB_CHIRP2_LEN + chrp_alp2_idx[j+7];
			acc = sp_chrp_alp2_buf[allp_idx]  + inR * chrp_allp_k[0];  
            sp_chrp_alp2_buf[allp_idx] = inR - chrp_allp_k[0] * acc;
            inR = acc;
            if (++chrp_alp2_idx[j+7] >= SPRVB_CHIRP2_LEN) chrp_alp2_idx[j+7] = 0; 
            j = j + 8;
        }
        j = 0;
        while(j < SPRVB_CHIRP_AMNT)
        {	// channel L
			allp_idx = j*SPRVB_CHIRP3_LEN + chrp_alp3_idx[j];
            acc = sp_chrp_alp3_buf[allp_idx]  + inL * chrp_allp_k[0];  
            sp_chrp_alp3_buf[allp_idx] = inL - chrp_allp_k[0] * acc;
            inL = acc;
            if (++chrp_alp3_idx[j] >= SPRVB_CHIRP3_LEN) chrp_alp3_idx[j] = 0;   
  
			allp_idx = (j+1)*SPRVB_CHIRP3_LEN + chrp_alp3_idx[j+1];
			acc = sp_chrp_alp3_buf[allp_idx]  + inL * chrp_allp_k[1];  
            sp_chrp_alp3_buf[allp_idx] = inL - chrp_allp_k[1] * acc;    
            inL = acc;
            if (++chrp_alp3_idx[j+1] >= SPRVB_CHIRP3_LEN) chrp_alp3_idx[j+1] = 0; 
            
			allp_idx = (j+2)*SPRVB_CHIRP3_LEN + chrp_alp3_idx[j+2];
            acc = sp_chrp_alp3_buf[allp_idx]  + inL * chrp_allp_k[2];  
            sp_chrp_alp3_buf[allp_idx] = inL - chrp_allp_k[2] * acc;			
            inL = acc;
            if (++chrp_alp3_idx[j+2] >= SPRVB_CHIRP3_LEN) chrp_alp3_idx[j+2] = 0; 

			allp_idx = (j+3)*SPRVB_CHIRP3_LEN + chrp_alp3_idx[j+3];
            acc = sp_chrp_alp3_buf[allp_idx]  + inL * chrp_allp_k[3];  
            sp_chrp_alp3_buf[allp_idx] = inL - chrp_allp_k[3] * acc;			
            inL = acc;
            if (++chrp_alp3_idx[j+3] >= SPRVB_CHIRP3_LEN) chrp_alp3_idx[j+3] = 0; 
			// channel R
			allp_idx = (j+4)*SPRVB_CHIRP3_LEN + chrp_alp3_idx[j+4];
            acc = sp_chrp_alp3_buf[allp_idx]  + inR * chrp_allp_k[3];  
            sp_chrp_alp3_buf[allp_idx] = inR - chrp_allp_k[3] * acc;			
            inR = acc;
            if (++chrp_alp3_idx[j+4] >= SPRVB_CHIRP3_LEN) chrp_alp3_idx[j+4] = 0; 
    
			allp_idx = (j+5)*SPRVB_CHIRP3_LEN + chrp_alp3_idx[j+5];
            acc = sp_chrp_alp3_buf[allp_idx]  + inR * chrp_allp_k[2];  
            sp_chrp_alp3_buf[allp_idx] = inR - chrp_allp_k[2] * acc;			
            inR = acc;
            if (++chrp_alp3_idx[j+5] >= SPRVB_CHIRP3_LEN) chrp_alp3_idx[j+5] = 0; 

			allp_idx = (j+6)*SPRVB_CHIRP3_LEN + chrp_alp3_idx[j+6];
            acc = sp_chrp_alp3_buf[allp_idx]  + inR * chrp_allp_k[1];  
            sp_chrp_alp3_buf[allp_idx] = inR - chrp_allp_k[1] * acc;			
            inR = acc;
            if (++chrp_alp3_idx[j+6] >= SPRVB_CHIRP3_LEN) chrp_alp3_idx[j+6] = 0; 

			allp_idx = (j+7)*SPRVB_CHIRP3_LEN + chrp_alp3_idx[j+7];
            acc = sp_chrp_alp3_buf[allp_idx]  + inR * chrp_allp_k[0];  
            sp_chrp_alp3_buf[allp_idx] = inR - chrp_allp_k[0] * acc;			
            inR = acc;
            if (++chrp_alp3_idx[j+7] >= SPRVB_CHIRP3_LEN) chrp_alp3_idx[j+7] = 0; 
            j = j + 8;
        }
		j = 0;
        while(j < SPRVB_CHIRP_AMNT)
        {	// channel L
			allp_idx = j*SPRVB_CHIRP4_LEN + chrp_alp4_idx[j];
            acc = sp_chrp_alp4_buf[allp_idx]  + inL * chrp_allp_k[0];  
            sp_chrp_alp4_buf[allp_idx] = inL - chrp_allp_k[0] * acc;
            inL = acc;
            if (++chrp_alp4_idx[j] >= SPRVB_CHIRP4_LEN) chrp_alp4_idx[j] = 0;   
            
			allp_idx = (j+1)*SPRVB_CHIRP4_LEN + chrp_alp4_idx[j+1];
            acc = sp_chrp_alp4_buf[allp_idx]  + inL * chrp_allp_k[1];  
            sp_chrp_alp4_buf[allp_idx] = inL - chrp_allp_k[1] * acc;			
            inL = acc;
            if (++chrp_alp4_idx[j+1] >= SPRVB_CHIRP4_LEN) chrp_alp4_idx[j+1] = 0; 

	        allp_idx = (j+2)*SPRVB_CHIRP4_LEN + chrp_alp4_idx[j+2];
            acc = sp_chrp_alp4_buf[allp_idx]  + inL * chrp_allp_k[1];  
            sp_chrp_alp4_buf[allp_idx] = inL - chrp_allp_k[1] * acc;			
            inL = acc;
            if (++chrp_alp4_idx[j+2] >= SPRVB_CHIRP4_LEN) chrp_alp4_idx[j+2] = 0; 

			allp_idx = (j+3)*SPRVB_CHIRP4_LEN + chrp_alp4_idx[j+3];
            acc = sp_chrp_alp4_buf[allp_idx]  + inL * chrp_allp_k[3];  
            sp_chrp_alp4_buf[allp_idx] = inL - chrp_allp_k[3] * acc;			
            inL = acc;
            if (++chrp_alp4_idx[j+3] >= SPRVB_CHIRP4_LEN) chrp_alp4_idx[j+3] = 0; 
			// channel R
			allp_idx = (j+4)*SPRVB_CHIRP4_LEN + chrp_alp4_idx[j+4];
            acc = sp_chrp_alp4_buf[allp_idx]  + inR * chrp_allp_k[3];  
            sp_chrp_alp4_buf[allp_idx] = inR - chrp_allp_k[3] * acc;			
            inR = acc;
            if (++chrp_alp4_idx[j+4] >= SPRVB_CHIRP4_LEN) chrp_alp4_idx[j+4] = 0; 

			allp_idx = (j+5)*SPRVB_CHIRP4_LEN + chrp_alp4_idx[j+5];
            acc = sp_chrp_alp4_buf[allp_idx]  + inR * chrp_allp_k[2];  
            sp_chrp_alp4_buf[allp_idx] = inR - chrp_allp_k[2] * acc;			
            inR = acc;
            if (++chrp_alp4_idx[j+5] >= SPRVB_CHIRP4_LEN) chrp_alp4_idx[j+5] = 0; 

			allp_idx = (j+6)*SPRVB_CHIRP4_LEN + chrp_alp4_idx[j+6];
            acc = sp_chrp_alp4_buf[allp_idx]  + inR * chrp_allp_k[1];  
            sp_chrp_alp4_buf[allp_idx] = inR - chrp_allp_k[1] * acc;			
            inR = acc;
            if (++chrp_alp4_idx[j+6] >= SPRVB_CHIRP4_LEN) chrp_alp4_idx[j+6] = 0; 

			allp_idx = (j+7)*SPRVB_CHIRP4_LEN + chrp_alp4_idx[j+7];
            acc = sp_chrp_alp4_buf[allp_idx]  + inR * chrp_allp_k[0];  
            sp_chrp_alp4_buf[allp_idx] = inR - chrp_allp_k[0] * acc;			
            inR = acc;
            if (++chrp_alp4_idx[j+7] >= SPRVB_CHIRP4_LEN) chrp_alp4_idx[j+7] = 0; 
            j = j + 8;
        }

		// modulate the allpass filters
		lfo.get(BASIC_LFO_PHASE_0, &offset, &lfo_fr); 
		acc = sp_lp_allp1d.getTap(offset+1, lfo_fr);
		sp_lp_allp1d.write_toOffset(acc, (lfo_ampl<<1)+1);
		lfo.get(BASIC_LFO_PHASE_90, &offset, &lfo_fr); 
		acc = sp_lp_allp2d.getTap(offset+1, lfo_fr);
		sp_lp_allp2d.write_toOffset(acc, (lfo_ampl<<1)+1);

        blockL->data[i] = (int16_t)((inL * wet_gain + dryL * dry_gain) * 32767.0f); 
		blockR->data[i] = (int16_t)((inR * wet_gain + dryR * dry_gain) * 32767.0f);
	}
    transmit(blockL, 0);
	transmit(blockR, 1);
	release(blockL);
	release(blockR);
#endif
}

bool AudioEffectSpringReverb_i16::bypass_process(audio_block_t** p_blockL, audio_block_t** p_blockR, bypass_mode_t mode, bool state)
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