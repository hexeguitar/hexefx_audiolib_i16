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
 */

#ifndef _SYNTH_WAVEFORM_EXT_I16_H_
#define _SYNTH_WAVEFORM_EXT_I16_H_

#include <Arduino.h>     // github.com/PaulStoffregen/cores/blob/master/teensy4/Arduino.h
#include <AudioStream.h> // github.com/PaulStoffregen/cores/blob/master/teensy4/AudioStream.h
#include <arm_math.h>    // github.com/PaulStoffregen/cores/blob/master/teensy4/arm_math.h
#include "synth_waveform.h" // extends the existing AudioSynthWaveform


#define WAVEFORM_BANDLIMIT_PULSE_SEQ	16
// P.Z. 2024: i have tried longer PWM step chans, but <4 steps seem to deliver the most optimal sound pallete 
#define WAVEFORM_PULSE_SEQ_LEN_MAX		4

typedef struct 
{
	uint8_t length = WAVEFORM_PULSE_SEQ_LEN_MAX;
	float steps[WAVEFORM_PULSE_SEQ_LEN_MAX];
	int8_t oct[WAVEFORM_PULSE_SEQ_LEN_MAX];
}waveform_pulse_seq_data_t;


class AudioSynthWaveform_ext_i16 : public AudioStream
{
public:
	AudioSynthWaveform_ext_i16(void) : AudioStream(0,NULL),
		phase_accumulator(0), phase_increment(0), phase_offset(0),
		magnitude(0), pulse_width(0x40000000),
		arbdata(NULL), sample(0), tone_type(WAVEFORM_SINE),
		tone_offset(0) {
			// initialize the pulse sequence table
			// Default setting is classic POKEY Atari bass sound
			pulse_seq_table[0] = 0.75f * 4294967296.0f;
			pulse_seq_table[1] = 0.5f * 4294967296.0f;
			pulse_seq_table[2] = 0.5f * 4294967296.0f; 
			pulse_seq_table[3] = 0.5f * 4294967296.0f; 
			pulse_seq_oct[0] = 0;
			pulse_seq_oct[1] = 0;
			pulse_seq_oct[2] = 1;
			pulse_seq_oct[3] = 1;
			pulse_seq_idx = 0;
			pulse_seq_len = WAVEFORM_PULSE_SEQ_LEN_MAX;
	}

	void frequency(float freq) {
		if (freq < 0.0f) {
			freq = 0.0;
		} else if (freq > AUDIO_SAMPLE_RATE_EXACT / 2.0f) {
			freq = AUDIO_SAMPLE_RATE_EXACT / 2.0f;
		}
		phase_increment = freq * (4294967296.0f / AUDIO_SAMPLE_RATE_EXACT);
		if (phase_increment > 0x7FFE0000u) phase_increment = 0x7FFE0000;
	}
	void phase(float angle) {
		if (angle < 0.0f) {
			angle = 0.0;
		} else if (angle > 360.0f) {
			angle = angle - 360.0f;
			if (angle >= 360.0f) return;
		}
		phase_offset = angle * (float)(4294967296.0 / 360.0);
	}
	void amplitude(float n) {	// 0 to 1.0
		if (n < 0) {
			n = 0;
		} else if (n > 1.0f) {
			n = 1.0;
		}
		magnitude = n * 65536.0f;
	}
	void offset(float n) {
		if (n < -1.0f) {
			n = -1.0f;
		} else if (n > 1.0f) {
			n = 1.0f;
		}
		tone_offset = n * 32767.0f;
	}
	void pulseWidth(float n) {	// 0.0 to 1.0
		if (n < 0) {
			n = 0;
		} else if (n > 1.0f) {
			n = 1.0f;
		}
		pulse_width = n * 4294967296.0f;
	}
	void begin(short t_type) {
		phase_offset = 0;
		tone_type = t_type;
		if (t_type == WAVEFORM_BANDLIMIT_SQUARE)
		  band_limit_waveform.init_square (phase_increment) ;
		else if (t_type == WAVEFORM_BANDLIMIT_PULSE)
		  band_limit_waveform.init_pulse (phase_increment, pulse_width) ;
		else if (t_type == WAVEFORM_BANDLIMIT_SAWTOOTH || t_type == WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE)
		  band_limit_waveform.init_sawtooth (phase_increment) ;
		else if (t_type == WAVEFORM_BANDLIMIT_PULSE_SEQ)
		{
			band_limit_waveform.init_pulse (phase_increment, pulse_seq_table[0]);
		}		  
	}
	/**
	 * @brief Set the parameters for one PWM step
	 * 
	 * @param step step number, must be less than pulse_seq_len
	 * @param pwm pwm duty setting in range 0.0 to 1.0
	 * @param oct octave setting in range -2 to 2
	 */
	void pulseWidthSeq_step(uint8_t step, float pwm, int8_t oct)
	{
		if (step >= pulse_seq_len) return;
		pulse_seq_table[step] = constrain(pwm, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_oct[step] = constrain(oct, -2, 2);
	}
	/**
	 * @brief Set the PWM duty for one step 
	 * 
	 * @param step step number
	 * @param pwm pwm duty in range 0.0 - 1.0 
	 */
	void pulseWidthSeq_step(uint8_t step, float pwm)
	{
		if (step >= pulse_seq_len) return;
		pulse_seq_table[step] = constrain(pwm, 0.0f, 1.0f) * 4294967296.0f;
	}
	/**
	 * @brief Set the octave of one step
	 * 
	 * @param step step number
	 * @param oct octave setting (-2...2)
	 */
	void pulseWidthSeq_oct(uint8_t step, int8_t oct)
	{
		if (step >= pulse_seq_len) return;
		pulse_seq_oct[step] = constrain(oct, -2, 2);
	}

	/**
	 * @brief set the length of the PWM sequence, max is 4
	 * 
	 * @param len pulse sequence length in steps
	 */
	void pulseWidthSeq_len(uint8_t len)
	{
		pulse_seq_len = constrain(len, 0, WAVEFORM_PULSE_SEQ_LEN_MAX);
	}
	/**
	 * @brief set the PWM duty steps and octaves using an external data structure
	 * 	might be useful for sound presets and import/export
	 * 
	 * @param data pointer to the wavefrom step data
	 */
	void pulseWidthSeq(waveform_pulse_seq_data_t* data)
	{
		uint8_t len = constrain(data->length, 0, WAVEFORM_PULSE_SEQ_LEN_MAX);
		for (int i=0; i<len; i++)
		{
			pulse_seq_table[i] = data->steps[i];
			pulse_seq_oct[i] = data->oct[i];
		}
	}
	/**
	 * @brief Set all PWM steps at once
	 * 
	 * @param step0 PWM duty for step 0
	 * @param oct0  octave setting for step 0
	 * @param step1 PWM duty for step 1
	 * @param oct1 octave setting for step 1
	 * @param step2 PWM duty for step 2
	 * @param oct2 octave setting for step 2
	 * @param step3 PWM duty for step 3
	 * @param oct3 octave setting for step 3
	 */
	void pulseWidthSeq(	float step0, int8_t oct0,
						float step1, int8_t oct1,
						float step2, int8_t oct2,
						float step3, int8_t oct3) 
	{	
		pulse_seq_table[0] = constrain(step0, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_table[1] = constrain(step1, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_table[2] = constrain(step2, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_table[3] = constrain(step3, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_oct[0] = constrain(oct0, -2, 2);
		pulse_seq_oct[1] = constrain(oct1, -2, 2);
		pulse_seq_oct[2] = constrain(oct2, -2, 2);
		pulse_seq_oct[3] = constrain(oct3, -2, 2);
	}


	void begin(float t_amp, float t_freq, short t_type) {
		amplitude(t_amp);
		frequency(t_freq);
		phase_offset = 0;
		begin (t_type);
	}
	void arbitraryWaveform(const int16_t *data, float maxFreq) {
		arbdata = data;
	}
	virtual void update(void);

private:
	uint32_t phase_accumulator;
	uint32_t phase_increment;
	uint32_t phase_offset;
	int32_t  magnitude;
	uint32_t pulse_width;
	const int16_t *arbdata;
	int16_t  sample; // for WAVEFORM_SAMPLE_HOLD
	short    tone_type;
	int16_t  tone_offset;
    BandLimitedWaveform band_limit_waveform ;
	// pulse sequenced mode
	uint32_t pulse_seq_table[WAVEFORM_PULSE_SEQ_LEN_MAX];
	int8_t pulse_seq_oct[WAVEFORM_PULSE_SEQ_LEN_MAX];
	uint8_t pulse_seq_idx;
	uint8_t pulse_seq_len;
};


class AudioSynthWaveformModulated_ext_i16 : public AudioStream
{
public:
	AudioSynthWaveformModulated_ext_i16(void) : AudioStream(2, inputQueueArray),
		phase_accumulator(0), phase_increment(0), modulation_factor(32768),
		magnitude(0), arbdata(NULL), sample(0), tone_offset(0),
		tone_type(WAVEFORM_SINE), modulation_type(0) {
		// initialize the pulse sequence table
		// Default setting is classic POKEY Atari bass sound
		pulse_seq_table[0] = 0.75f * 4294967296.0f;
		pulse_seq_table[1] = 0.5f * 4294967296.0f;
		pulse_seq_table[2] = 0.5f * 4294967296.0f; 
		pulse_seq_table[3] = 0.5f * 4294967296.0f; 
		pulse_seq_oct[0] = 0;
		pulse_seq_oct[1] = 0;
		pulse_seq_oct[2] = 1;
		pulse_seq_oct[3] = 1;
		pulse_seq_idx = 0;
		pulse_seq_len = WAVEFORM_PULSE_SEQ_LEN_MAX;			
	}

	void frequency(float freq) {
		if (freq < 0.0f) {
			freq = 0.0;
		} else if (freq > AUDIO_SAMPLE_RATE_EXACT / 2.0f) {
			freq = AUDIO_SAMPLE_RATE_EXACT / 2.0f;
		}
		phase_increment = freq * (4294967296.0f / AUDIO_SAMPLE_RATE_EXACT);
		if (phase_increment > 0x7FFE0000u) phase_increment = 0x7FFE0000;
	}
	void amplitude(float n) {	// 0 to 1.0
		if (n < 0) {
			n = 0;
		} else if (n > 1.0f) {
			n = 1.0f;
		}
		magnitude = n * 65536.0f;
	}
	void offset(float n) {
		if (n < -1.0f) {
			n = -1.0f;
		} else if (n > 1.0f) {
			n = 1.0f;
		}
		tone_offset = n * 32767.0f;
	}
	void begin(short t_type) {
		tone_type = t_type;
		if (t_type == WAVEFORM_BANDLIMIT_SQUARE)
		  band_limit_waveform.init_square (phase_increment) ;
		else if (t_type == WAVEFORM_BANDLIMIT_PULSE)
		  band_limit_waveform.init_pulse (phase_increment, 0x80000000u) ;
		else if (t_type == WAVEFORM_BANDLIMIT_SAWTOOTH || t_type == WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE)
		  band_limit_waveform.init_sawtooth (phase_increment) ;
		else if (t_type == WAVEFORM_BANDLIMIT_PULSE_SEQ)
		{
			band_limit_waveform.init_pulse (phase_increment, pulse_seq_table[0]) ;
		}		  
	}
	void begin(float t_amp, float t_freq, short t_type) {
		amplitude(t_amp);
		frequency(t_freq);
		begin (t_type) ;
	}
	void arbitraryWaveform(const int16_t *data, float maxFreq) {
		arbdata = data;
	}
	void frequencyModulation(float octaves) {
		if (octaves > 12.0f) {
			octaves = 12.0f;
		} else if (octaves < 0.1f) {
			octaves = 0.1f;
		}
		modulation_factor = octaves * 4096.0f;
		modulation_type = 0;
	}
	void phaseModulation(float degrees) {
		if (degrees > 9000.0f) {
			degrees = 9000.0f;
		} else if (degrees < 30.0f) {
			degrees = 30.0f;
		}
		modulation_factor = degrees * (float)(65536.0 / 180.0);
		modulation_type = 1;
	}
	/**
	 * @brief Set the parameters for one PWM step
	 * 
	 * @param step step number, must be less than pulse_seq_len
	 * @param pwm pwm duty setting in range 0.0 to 1.0
	 * @param oct octave setting in range -2 to 2
	 */
	void pulseWidthSeq_step(uint8_t step, float pwm, int8_t oct)
	{
		if (step >= pulse_seq_len) return;
		pulse_seq_table[step] = constrain(pwm, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_oct[step] = constrain(oct, -2, 2);
	}
	/**
	 * @brief Set the PWM duty for one step 
	 * 
	 * @param step step number
	 * @param pwm pwm duty in range 0.0 - 1.0 
	 */
	void pulseWidthSeq_step(uint8_t step, float pwm)
	{
		if (step >= pulse_seq_len) return;
		pulse_seq_table[step] = constrain(pwm, 0.0f, 1.0f) * 4294967296.0f;
	}
	/**
	 * @brief Set the octave of one step
	 * 
	 * @param step step number
	 * @param oct octave setting (-2...2)
	 */
	void pulseWidthSeq_oct(uint8_t step, int8_t oct)
	{
		if (step >= pulse_seq_len) return;
		pulse_seq_oct[step] = constrain(oct, -2, 2);
	}

	/**
	 * @brief set the length of the PWM sequence, max is 4
	 * 
	 * @param len pulse sequence length in steps
	 */
	void pulseWidthSeq_len(uint8_t len)
	{
		pulse_seq_len = constrain(len, 0, WAVEFORM_PULSE_SEQ_LEN_MAX);
	}
	/**
	 * @brief set the PWM duty steps and octaves using an external data structure
	 * 	might be useful for sound presets and import/export
	 * 
	 * @param data pointer to the wavefrom step data
	 */
	void pulseWidthSeq(waveform_pulse_seq_data_t* data)
	{
		uint8_t len = constrain(data->length, 0, WAVEFORM_PULSE_SEQ_LEN_MAX);
		for (int i=0; i<len; i++)
		{
			pulse_seq_table[i] = data->steps[i];
			pulse_seq_oct[i] = data->oct[i];
		}
	}
	/**
	 * @brief Set all PWM steps at once
	 * 
	 * @param step0 PWM duty for step 0
	 * @param oct0  octave setting for step 0
	 * @param step1 PWM duty for step 1
	 * @param oct1 octave setting for step 1
	 * @param step2 PWM duty for step 2
	 * @param oct2 octave setting for step 2
	 * @param step3 PWM duty for step 3
	 * @param oct3 octave setting for step 3
	 */
	void pulseWidthSeq(	float step0, int8_t oct0,
						float step1, int8_t oct1,
						float step2, int8_t oct2,
						float step3, int8_t oct3) 
	{	
		pulse_seq_table[0] = constrain(step0, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_table[1] = constrain(step1, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_table[2] = constrain(step2, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_table[3] = constrain(step3, 0.0f, 1.0f) * 4294967296.0f;
		pulse_seq_oct[0] = constrain(oct0, -2, 2);
		pulse_seq_oct[1] = constrain(oct1, -2, 2);
		pulse_seq_oct[2] = constrain(oct2, -2, 2);
		pulse_seq_oct[3] = constrain(oct3, -2, 2);
	}

	virtual void update(void);

private:
	audio_block_t *inputQueueArray[2];
	uint32_t phase_accumulator;
	uint32_t phase_increment;
	uint32_t modulation_factor;
	int32_t  magnitude;
	const int16_t *arbdata;
	uint32_t phasedata[AUDIO_BLOCK_SAMPLES];
	int16_t  sample; // for WAVEFORM_SAMPLE_HOLD
	int16_t  tone_offset;
	uint8_t  tone_type;
	uint8_t  modulation_type;
        BandLimitedWaveform band_limit_waveform ;
	// pulse sequenced mode
	uint32_t pulse_seq_table[WAVEFORM_PULSE_SEQ_LEN_MAX];
	int8_t pulse_seq_oct[WAVEFORM_PULSE_SEQ_LEN_MAX];
	uint8_t pulse_seq_idx;
	uint8_t pulse_seq_len;
};


#endif // _SYNTH_WAVEFORM_EXT_I16_H_
