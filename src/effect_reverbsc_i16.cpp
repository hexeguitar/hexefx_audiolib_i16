#include "effect_reverbsc_i16.h"

#define DELAYPOS_SHIFT 28
#define DELAYPOS_SCALE 0x10000000
#define DELAYPOS_MASK 0x0FFFFFFF
#ifndef M_PI
	#define M_PI 3.14159265358979323846 /* pi */
#endif

/* kReverbParams[n][0] = delay time (in seconds)                     */
/* kReverbParams[n][1] = random variation in delay time (in seconds) */
/* kReverbParams[n][2] = random variation frequency (in 1/sec)       */
/* kReverbParams[n][3] = random seed (0 - 32767)                     */

static const float32_t kReverbParams[8][4] =
{
	{(2473.0f / AUDIO_SAMPLE_RATE_EXACT), 0.0010f, 3.100f, 1966.0f},
	{(2767.0f / AUDIO_SAMPLE_RATE_EXACT), 0.0011f, 3.500f, 29491.0f},
	{(3217.0f / AUDIO_SAMPLE_RATE_EXACT), 0.0017f, 1.110f, 22937.0f},
	{(3557.0f / AUDIO_SAMPLE_RATE_EXACT), 0.0006f, 3.973f, 9830.0f},
	{(3907.0f / AUDIO_SAMPLE_RATE_EXACT), 0.0010f, 2.341f, 20643.0f},
	{(4127.0f / AUDIO_SAMPLE_RATE_EXACT), 0.0011f, 1.897f, 22937.0f},
	{(2143.0f / AUDIO_SAMPLE_RATE_EXACT), 0.0017f, 0.891f, 29491.0f},
	{(1933.0f / AUDIO_SAMPLE_RATE_EXACT), 0.0006f, 3.221f, 14417.0f}
};
static int DelayLineMaxSamples(float32_t sr, float32_t i_pitch_mod, int n);
static int DelayLineBytesAlloc(float32_t sr, float32_t i_pitch_mod, int n);
static const float32_t kOutputGain = 0.35f;
static const float32_t kJpScale = 0.25f;

extern uint8_t external_psram_size;

AudioEffectReverbSC_i16::AudioEffectReverbSC_i16(bool use_psram) : AudioStream(2, inputQueueArray)
{
	sample_rate_ = AUDIO_SAMPLE_RATE_EXACT;
	feedback_ = 0.7f;
	lpfreq_ = 10000;
	i_pitch_mod_ = 1;
	damp_fact_ = 0.195847f; // ~16kHz
	flags.mem_fail = 0;
	flags.bypass = 0;
	flags.freeze = 0;
	flags.cleanup_done = 1;
	flags.memsetup_done = 0;
	bp_mode = BYPASS_MODE_PASS;
	int i, n_bytes = 0;
	n_bytes = 0;
	if (use_psram)	
	{
		#if ARDUINO_TEENSY41 
		// no PSRAM detected - enter the memoery failsafe mode = fixed bypass
		if (external_psram_size == 0) 
		{
			flags.mem_fail = 1;
			initialized = true;
			return;
		}
		aux_ = (float32_t *) extmem_malloc(aux_size_bytes);
		#else
			flags.mem_fail = 1;
			initialized = true;
			return;
		#endif
	}
	else			
	{
		aux_ = (float32_t *) malloc(aux_size_bytes);
	}
	if (!aux_) 
	{
		flags.mem_fail = 1;
		return;
	}

	for (i = 0; i < 8; i++)
	{
		if (n_bytes > REVERBSC_I16_DLYBUF_SIZE)
			return;
		delay_lines_[i].buf = (aux_) + n_bytes;
		InitDelayLine(&delay_lines_[i], i);
		n_bytes += DelayLineBytesAlloc(AUDIO_SAMPLE_RATE_EXACT, 1, i);
	}
	mix(0.5f);

	initialized = true;
}

static int DelayLineMaxSamples(float32_t sr, float32_t i_pitch_mod, int n)
{
	float32_t max_del;

	max_del = kReverbParams[n][0];
	max_del += (kReverbParams[n][1] * (float32_t)i_pitch_mod * 1.125);
	return (int)(max_del * sr + 16.5);
}

static int DelayLineBytesAlloc(float32_t sr, float32_t i_pitch_mod, int n)
{
	int n_bytes = 0;

	n_bytes += (DelayLineMaxSamples(sr, i_pitch_mod, n) * (int)sizeof(float32_t));
	return n_bytes;
}

void AudioEffectReverbSC_i16::NextRandomLineseg(ReverbScDl_t *lp, int n)
{
	float32_t prv_del, nxt_del, phs_inc_val;

	/* update random seed */
	if (lp->seed_val < 0)
		lp->seed_val += 0x10000;
	lp->seed_val = (lp->seed_val * 15625 + 1) & 0xFFFF;
	if (lp->seed_val >= 0x8000)
		lp->seed_val -= 0x10000;
	/* length of next segment in samples */
	lp->rand_line_cnt = (int)((sample_rate_ / kReverbParams[n][2]) + 0.5f);
	prv_del = (float32_t)lp->write_pos;
	prv_del -= ((float32_t)lp->read_pos + ((float32_t)lp->read_pos_frac / (float32_t)DELAYPOS_SCALE));
	while (prv_del < 0.0)
		prv_del += lp->buffer_size;
	prv_del = prv_del / sample_rate_; /* previous delay time in seconds */
	nxt_del = (float32_t)lp->seed_val * kReverbParams[n][1] / 32768.0f;
	/* next delay time in seconds */
	nxt_del = kReverbParams[n][0] + (nxt_del * (float32_t)i_pitch_mod_);
	/* calculate phase increment per sample */
	phs_inc_val = (prv_del - nxt_del) / (float32_t)lp->rand_line_cnt;
	phs_inc_val = phs_inc_val * sample_rate_ + 1.0;
	lp->read_pos_frac_inc = (int)(phs_inc_val * DELAYPOS_SCALE + 0.5f);
}

void AudioEffectReverbSC_i16::InitDelayLine(ReverbScDl_t *lp, int n)
{
	float32_t read_pos;

	/* calculate length of delay line */
	lp->buffer_size = DelayLineMaxSamples(sample_rate_, 1, n);
	lp->dummy = 0;
	lp->write_pos = 0;
	/* set random seed */
	lp->seed_val = (int)(kReverbParams[n][3] + 0.5f);
	/* set initial delay time */
	read_pos = (float32_t)lp->seed_val * kReverbParams[n][1] / 32768.0f;
	read_pos = kReverbParams[n][0] + (read_pos * (float32_t)i_pitch_mod_);
	read_pos = (float32_t)lp->buffer_size - (read_pos * sample_rate_);
	lp->read_pos = (int)read_pos;
	read_pos = (read_pos - (float32_t)lp->read_pos) * (float32_t)DELAYPOS_SCALE;
	lp->read_pos_frac = (int)(read_pos + 0.5);
	/* initialise first random line segment */
	NextRandomLineseg(lp, n);
	/* clear delay line to zero */
	lp->filter_state = 0.0f;
	for (int i = 0; i < lp->buffer_size; i++)
	{
		lp->buf[i] = 0;
	}
}

void AudioEffectReverbSC_i16::update()
{
#if defined(__IMXRT1062__)
	audio_block_t *blockL, *blockR;
	int16_t i;
	float32_t a_in_l, a_in_r, a_out_l, a_out_r, dryL, dryR;
	float32_t vm1, v0, v1, v2, am1, a0, a1, a2, frac;
	ReverbScDl_t *lp;
	int read_pos;
	uint32_t n;
	int buffer_size; /* Local copy */
	float32_t damp_fact = damp_fact_;
	
	if (!initialized) return;
	// special case if memory allocation failed, pass the input signal directly to the output
	if (flags.mem_fail) bp_mode = BYPASS_MODE_PASS;
	
	blockL = receiveWritable(0);
	blockR = receiveWritable(1);
	
	if (!bypass_process(&blockL, &blockR, bp_mode, (bool)flags.bypass))
		return;

	if (flags.bypass || !flags.memsetup_done)
	{
		if ( !flags.memsetup_done)	flags.memsetup_done  = memCleanup();
		transmit(blockL, 0);
		transmit(blockR, 1);
		release(blockL);
		release(blockR);
		return;
	}

	flags.cleanup_done = 0;
	for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
	{
		input_gain += (input_gain_set - input_gain) * 0.25f;
		/* calculate "resultant junction pressure" and mix to input signals */
		a_in_l = a_out_l = a_out_r = 0.0f;
		dryL = ((float32_t)blockL->data[i] / 32768.0f) * input_gain;
		dryR = ((float32_t)blockR->data[i] / 32768.0f) * input_gain;

		for (n = 0; n < 8; n++)
		{
			a_in_l += delay_lines_[n].filter_state;
		}
		a_in_l *= kJpScale;
		a_in_r = a_in_l + dryR;
		a_in_l = a_in_l + dryL;

		/* loop through all delay lines */
		for (n = 0; n < 8; n++)
		{
			lp = &delay_lines_[n];
			buffer_size = lp->buffer_size;

			/* send input signal and feedback to delay line */
			lp->buf[lp->write_pos] = (float32_t)((n & 1 ? a_in_r : a_in_l) - lp->filter_state);
			if (++lp->write_pos >= buffer_size) 	lp->write_pos -= buffer_size;

			/* read from delay line with cubic interpolation */
			if (lp->read_pos_frac >= DELAYPOS_SCALE)
			{
				lp->read_pos += (lp->read_pos_frac >> DELAYPOS_SHIFT);
				lp->read_pos_frac &= DELAYPOS_MASK;
			}
			if (lp->read_pos >= buffer_size)
				lp->read_pos -= buffer_size;
			read_pos = lp->read_pos;
			frac = (float32_t)lp->read_pos_frac * (1.0f / (float32_t)DELAYPOS_SCALE);

			/* calculate interpolation coefficients */
			a2 = frac * frac;
			a2 *= (1.0f / 6.0f);
			a1 = frac;
			a1 += 1.0f;
			a1 *= 0.5f;
			am1 = a1 - 1.0f;
			a0 = 3.0f * a2;
			a1 -= a0;
			am1 -= a2;
			a0 -= frac;

			/* read four samples for interpolation */
			if (read_pos > 0 && read_pos < (buffer_size - 2))
			{
				vm1 = (float32_t)(lp->buf[read_pos - 1]);
				v0 = (float32_t)(lp->buf[read_pos]);
				v1 = (float32_t)(lp->buf[read_pos + 1]);
				v2 = (float32_t)(lp->buf[read_pos + 2]);
			}
			else
			{
				/* at buffer wrap-around, need to check index */
				if (--read_pos < 0)	read_pos += buffer_size;
				vm1 = (float32_t)lp->buf[read_pos];
				if (++read_pos >= buffer_size) read_pos -= buffer_size;
				v0 = (float32_t)lp->buf[read_pos];
				if (++read_pos >= buffer_size) read_pos -= buffer_size;
				v1 = (float32_t)lp->buf[read_pos];
				if (++read_pos >= buffer_size) read_pos -= buffer_size;
				v2 = (float32_t)lp->buf[read_pos];
			}
			v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;

			/* update buffer read position */
			lp->read_pos_frac += lp->read_pos_frac_inc;

			// apply filter			
			v0 = (lp->filter_state - v0) * damp_fact + v0;

			/* mix to output */
			if (n & 1) 	a_out_r += v0;
			else		a_out_l += v0;

			v0 *= (float32_t)feedback_; // apply feedback
			lp->filter_state = v0;	// save filter - this will make the reverb volume constant

			/* start next random line segment if current one has reached endpoint */
			if (--(lp->rand_line_cnt) <= 0)
			{
				NextRandomLineseg(lp, n);
			}
		}

		blockL->data[i] = (int16_t)((a_out_l * wet_gain + dryL * dry_gain) * 32767.0f);
		blockR->data[i] = (int16_t)((a_out_r * wet_gain + dryR * dry_gain) * 32767.0f);
	} // end block processing
    AudioStream::transmit(blockL, 0);
	AudioStream::transmit(blockR, 1);
	AudioStream::release(blockL);
	AudioStream::release(blockR);	
#endif	
}

void AudioEffectReverbSC_i16::freeze(bool state)
{
	if (flags.freeze == state) return;
	flags.freeze = state;
	if (state)
	{
		feedback_tmp = feedback_;      // store the settings
		damp_fact_tmp = damp_fact_;
		input_gain_tmp = input_gain_set;
		__disable_irq();
		feedback_ = 1.0f; // infinite reverb                                      
		input_gain_set = freeze_ingain;
		__enable_irq();
	}
	else
	{
		__disable_irq();
		feedback_ = feedback_tmp;
		damp_fact_ = damp_fact_tmp;
		if (!flags.bypass)
		{
			input_gain_set = input_gain_tmp;
		}
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
bool AudioEffectReverbSC_i16::memCleanup()
{
	bool result = false;
	if (memCleanupEnd > REVERBSC_I16_DLYBUF_SIZE) // last segment
	{
		memCleanupEnd = REVERBSC_I16_DLYBUF_SIZE;
		result = true;	
	}
	uint32_t l = (memCleanupEnd - memCleanupStart) * sizeof(float32_t);
	uint8_t* memPtr = (uint8_t *)&aux_[0]+(memCleanupStart*sizeof(float32_t));
	memset(memPtr, 0, l);
	arm_dcache_flush_delete(memPtr, l);

	memCleanupStart = memCleanupEnd;
	memCleanupEnd += memCleanupStep;
	
	return result;
}


bool AudioEffectReverbSC_i16::bypass_process(audio_block_t** p_blockL, audio_block_t** p_blockR, bypass_mode_t mode, bool state)
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