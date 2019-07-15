#include <picoapi.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "log.h"

static pico_System sys;
static pico_Engine engine;

#define MEM_SIZE 2500000

int init_pico() {
	char ta_resource_name[1024];
	char sg_resource_name[1024];
	
	pico_Resource ta_resource;
	pico_Resource sg_resource;

	const char *voice_name = "English";
	const char *ta_path = "/usr/share/pico/lang/en-US_ta.bin";
	const char *sg_path = "/usr/share/pico/lang/en-US_lh0_sg.bin";

	void *mem_space = (void*)malloc(MEM_SIZE);
	assert( pico_initialize( mem_space, MEM_SIZE, &sys ) == PICO_OK );
	assert( pico_loadResource( sys, (const pico_Char *) ta_path, &ta_resource) == PICO_OK );
	assert( pico_loadResource( sys, (const pico_Char *) sg_path, &sg_resource) == PICO_OK );
	assert( pico_createVoiceDefinition( sys, (const pico_Char*) voice_name ) == PICO_OK );
	assert( pico_getResourceName( sys, ta_resource, (char *)ta_resource_name ) == PICO_OK );
	assert( pico_getResourceName( sys, sg_resource, (char *)sg_resource_name ) == PICO_OK );
	assert( pico_addResourceToVoiceDefinition( sys, (const pico_Char*) voice_name, (const pico_Char *) &ta_resource_name) == PICO_OK );
	assert( pico_addResourceToVoiceDefinition( sys, (const pico_Char*) voice_name, (const pico_Char *) &sg_resource_name) == PICO_OK );
	assert( pico_newEngine( sys, (const pico_Char*) voice_name, &engine) == PICO_OK );
	return 0;
}

size_t synth_text_( const char *s, char *buf, int max_samples )
{
	assert( buf );
	assert( max_samples > 1024*1024 );
	pico_Status status;
	pico_Int16 textLeft = strlen(s) + 1;
	pico_Int16 textRead = 0;
	pico_Int16 sum = 0;

	while(textLeft != sum){
		pico_putTextUtf8( engine, (const pico_Char*)s, textLeft, &textRead );
		sum += textRead;
	}


max_samples = 160000;

	size_t total_bytes = 0;
	pico_Int16 n;
	pico_Int16 type;
	do {
		n = 0;
		type = 0;
		status = pico_getData( engine, (void*)buf, max_samples/2, &n, &type);
		assert( status >= 0 );
		buf += n;
		total_bytes += n;
	} while( status == PICO_STEP_BUSY );

	return total_bytes;
}

// convert 16bit mono 16000rate to 16bit stereo 44100rate
size_t convert_format(const char *buf, size_t num_samples, char *out)
{
	int multiplier = 3;
	for( int i = 0; i < num_samples; i++ ) {
		uint16_t x = *(uint16_t*)(buf+i*2);
		for( int j = 0; j < multiplier*2; j++ ) {
			*(uint16_t*)(out + i*2*2*multiplier + j*2) = x;
		}
	}
	return num_samples*6;
}

char *mono_buf = NULL;
size_t synth_text( const char *s, char *buf, int max_samples )
{
	if( !mono_buf ) {
		mono_buf = malloc(max_samples);
	}

	size_t n = synth_text_( s, mono_buf, max_samples );
	return convert_format( mono_buf, n, buf );
}
