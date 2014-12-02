#include <stdio.h>
#include <string.h>

#include "core.h"


int main( int argc, const char ** argv ) {
	struct Core * core;
	struct {
		unsigned int good:1;
		unsigned int core:1;
		} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	Core_Boot( );
	cleanUp.good = ( ( core = Core_New( ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.core = 1;
	}
	if ( cleanUp.good ) {
		
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.core ) {
			Core_Delete( core ) ;
		}
	} else {
		Core_Delete( core ) ;
	}

	Core_Shutdown( );
	return 0;
}
