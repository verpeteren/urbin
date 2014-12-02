#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../config.h"
#include "core.h"

void Core_Boot( ) {
	fprintf( stdout, "Starting\n" );
	picoev_init( MAX_FDS );
}

void Core_Shutdown( ) {
	fprintf( stdout, "Shutdown\n" );
	picoev_deinit( );
}


struct Core * Core_New( ) {
	struct {unsigned int good:1;
		unsigned int loop:1;
		unsigned int core:1;} cleanUp;
	struct Core * core;

	memset( &cleanUp, 0, sizeof( cleanUp ) );

	cleanUp.good = ( ( core = malloc( sizeof( * core ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.core = 1;
	}
	if ( cleanUp.good )  {
		cleanUp.good = ( ( core->loop = picoev_create_loop( LOOP_TIMEOUT ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.loop = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.loop ) {
			picoev_destroy_loop( core->loop );
		}
		if ( cleanUp.core ) {
			free( core ); core = NULL;
		}
	}

	return core;
}

void Core_Loop( struct Core * core ) {
	core->keepOnRunning = 1;
	while ( core->keepOnRunning )  {
		picoev_loop_once( core->loop, 0 );
	}

}
void Core_Delete( struct Core * core ) {
	picoev_destroy_loop( core->loop );
	free( core );
}
