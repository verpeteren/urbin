#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#include "core.h"

void Boot( ) {
	fprintf( stdout, "Starting\n" );
	picoev_init( MAX_FDS );
}

void Shutdown( ) {
	fprintf( stdout, "Shutdown\n" );
	picoev_deinit( );
}

void SetupSocket( int fd ) {
	int on, r;

	on = 1;
	r = setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof( on ) );
	assert( r == 0 );
	r = fcntl( fd, F_SETFL, O_NONBLOCK );
	assert( r == 0 );
}


struct core_t * Core_New( ) {
	struct {unsigned int good:1;
		unsigned int loop:1;
		unsigned int core:1;} cleanUp;
	struct core_t * core;

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

void Core_Loop( struct core_t * core ) {
	core->keepOnRunning = 1;
	while ( core->keepOnRunning )  {
		picoev_loop_once( core->loop, 0 );
	}

}
void Core_Delete( struct core_t * core ) {
	picoev_destroy_loop( core->loop );
	free( core );
}
