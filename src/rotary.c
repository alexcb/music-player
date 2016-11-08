#include "rotary.h"

#define ROTART_STATE_INIT_PHASE 0b10000

int updateRotary(RotaryState *state, int pin1, int pin2)
{
	int encoded = (pin1 << 1) | pin2;
	int sum = (state->last_encoded << 2) | encoded;

	if( state->last_encoded == ROTART_STATE_INIT_PHASE ) {
		if( encoded == 0b11 ) {
			state->last_encoded = encoded;
		}
		return 0;
	}

	if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) state->value++;
	if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) state->value--;

	state->last_encoded = encoded;

	if( !(state->value % 4) ){
		int res = state->last_dedent - state->value;
		state->last_dedent = state->value;
		return res;
	}
	return 0;
}

void initRotaryState(RotaryState *state, int pin1, int pin2)
{
	state->value = 0;
	state->last_dedent = 0;
	state->last_encoded = ROTART_STATE_INIT_PHASE;
	updateRotary(state, pin1, pin2);
}
