#ifndef _BASIC_LFO_H_
#define _BASIC_LFO_H_

#include <Arduino.h>

#define BASIC_LFO_PHASE_0	(0)
#define BASIC_LFO_PHASE_45	(32)
#define BASIC_LFO_PHASE_60	(43)
#define BASIC_LFO_PHASE_90	(64)
#define BASIC_LFO_PHASE_120	(85)
#define BASIC_LFO_PHASE_180	(128)

extern "C" {
extern const int16_t AudioWaveformSine[257];
}

/*
 * @brief Basic sin LFO with float output
 * 
 */
class AudioBasicLfo
{
public:
	AudioBasicLfo(float rateHz, uint32_t ampl)
	{
		acc = 0;
		if (!ampl) 
		{
			state = false;
			divider = 0x8000;
		}
		else divider = (0x7FFF + (ampl>>1)) / ampl;	
		state = true;
		adder = (uint32_t)(rateHz * rate_mult);
		
	}
	inline void update()
	{
		acc += adder; // update the phase acc
	}
	/**
	 * @brief LFO output split in two parts
	 * 
	 * @param phase8bit 0-360deg scaled to 8bit value - phase shift (ie 64=90deg)
	 * @param intOffset pointer top integer value used as address offset
	 * @param fractOffset pointer to fractional part used for interpolation
	 */
	inline void get(uint8_t phase8bit, uint32_t *intOffset, float *fractOffset)
	{
		if (!state) // lfo off
		{
			*intOffset = 0;
			*fractOffset = 0.0f;
			return;
		}
		uint32_t idx;
		uint32_t y0, y1;
		uint64_t y;
		float intOff;
		idx = ((acc >> 24) + phase8bit) & 0xFF;
        y0 =  AudioWaveformSine[idx] + 32767;
        y1 = AudioWaveformSine[idx+1] + 32767;
        idx = acc & 0x00FFFFFF;   // lower 24 bit = fractional part
        y = (int64_t)y0 * (0x00FFFFFF - idx);
        y += (int64_t)y1 * idx;
		y0 = (int32_t) (y >> 24); // 16bit output
		*fractOffset = modff((float)y0 / (float)divider, &intOff);
		*intOffset = (uint32_t)intOff;
	}
	inline void setRate(float rateHz)
	{
		adder = (uint32_t)(rateHz * rate_mult);
	}
	inline void setDepth(uint32_t ampl)
	{
		if (!ampl) 
		{
			state = false; 
			return; // do not update the divider
		}
		state = true;
		divider = (0x7FFF + (ampl>>1)) / ampl;	
	}
private:
	bool state = true; 
	uint32_t acc;
	uint32_t adder;
	int32_t divider = 1;
	const uint32_t rate_mult = 4294967295.0f / AUDIO_SAMPLE_RATE_EXACT;
};

#endif // _BASIC_LFO_H_
