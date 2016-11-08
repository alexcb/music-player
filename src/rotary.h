#ifndef _ROTARY_H
#define _ROTARY_H

typedef struct RotaryState
{
	volatile long value;
	volatile int last_encoded;
	volatile long last_dedent;
} RotaryState;

void initRotaryState(RotaryState *state, int pin1, int pin2);
int updateRotary(RotaryState *state, int pin1, int pin2);

#endif //_ROTARY_H
