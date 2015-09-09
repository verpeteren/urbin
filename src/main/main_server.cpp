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
	int fds, maxWait;
	PRBool isDaemon;
	struct module_t * javascriptModule;
	char * runAsUser, * runAsGroup;
	struct {
		unsigned int good:1;
		unsigned int core:1;
		unsigned int javascript:1;
		} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	isDaemon = PR_CFG_CORE_DAEMON;
	core = NULL;
	javascriptModule = NULL;
	fds = PR_CFG_CORE_MAX_FDS;
	runAsUser = strdup( PR_CFG_CORE_RUN_AS_USER );
	runAsGroup = strdup( PR_CFG_CORE_RUN_AS_GROUP );
	maxWait = PR_CFG_CORE_MAX_FDS;
	Boot( fds );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( core = Core_New( isDaemon ) ) != NULL );
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
		cleanUp.good = ( Core_PrepareDaemon( core, PR_CFG_CORE_MAX_FDS, SignalHandler, runAsUser, runAsGroup ) == 1 );
	}
	if ( cleanUp.good ) {
		Core_Loop( core, maxWait );
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
	free( runAsUser ); runAsUser = NULL;
	free( runAsGroup ); runAsGroup = NULL;
	

	return 0;
}

