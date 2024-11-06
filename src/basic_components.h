#ifndef _BASIC_COMPONENTS_H_
#define _BASIC_COMPONENTS_H_

#include "basic_allpass.h"
#include "basic_delay.h"
#include "basic_lfo.h"
#include "basic_shelvFilter.h"
#include "basic_pitch.h"
#include "basic_DSPutils.h"

// bypass modes used in various components
typedef enum
{
	BYPASS_MODE_PASS,
	BYPASS_MODE_OFF,
	BYPASS_MODE_TRAILS
}bypass_mode_t;

#endif // _BASIC_COMPONENTS_H_
