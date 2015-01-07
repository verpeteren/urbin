#include <stdio.h>
#include <string.h>

#include "../core/core.h"
#include "../feature/webserver.h"
#include "../feature/sqlclient.h"
#include "../glot/javascript.h"


static struct core_t * core;

static void SignalHandler( const int signal ) {
	fprintf( stdout, "Shutting down...\n" );
	core->keepOnRunning = 0;
}

int main( int argc, const char ** argv ) {
	cfg_t * config, * mainSection;
	int fds;
	struct module_t * javascriptModule;
	struct {
		unsigned int good:1;
		unsigned int core:1;
		unsigned int javascript:1;
		} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	core = NULL;
	javascriptModule = NULL;
	cleanUp.good = ( ( config = ProcessCommandline( argc, argv ) ) != NULL );
	fds = PR_CFG_CORE_MAX_FDS;
	if ( cleanUp.good ) {
		mainSection = cfg_getnsec( config, "main", 0 );
		fds = cfg_getint( mainSection, "max_fds" );
	}
	Boot( fds );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( core = Core_New( config  ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.core = 1;
		cleanUp.good = ( ( javascriptModule = Module_New( "javascript", JavascriptModule_Load, JavascriptModule_Ready, JavascriptModule_Unload, NULL, NULL ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Core_AddModule( core, javascriptModule ) ) ? 1 : 0;
	}
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
		cleanUp.good = ( Core_PrepareDaemon( core, SignalHandler ) == 1 );
	}
	if ( cleanUp.good ) {
		Core_Loop( core );
		Core_Delete( core );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.javascript ) {
			Core_DelModule( core, javascriptModule );
		}
		if ( cleanUp.core ) {
			Core_Delete( core );
		}
	}

	Shutdown( );

	return 0;
}

