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
	struct module_t * webserverModule, *javascriptModule, *pgSqlclientModule;
#if HAVE_MYSQL == 1
	struct module_t * mySqlclientModule;
#endif
	struct {
		unsigned int good:1;
		unsigned int core:1;
		unsigned int webserver:1;
		unsigned int pgSqlclient:1;
#if HAVE_MYSQL == 1
		unsigned int mySqlclient:1;
#endif
		unsigned int javascript:1;
		} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	core = NULL;
	webserverModule = NULL;
	javascriptModule = NULL;
	pgSqlclientModule = NULL;
#if HAVE_MYSQL == 1
	mySqlclientModule = NULL;
#endif
	cleanUp.good = ( ( config = ProcessCommandline( argc, argv ) ) != NULL );
	fds = PR_CFG_LOOP_MAX_FDS;
	if ( cleanUp.good ) {
		mainSection = cfg_getnsec( config, "main", 0 );
		fds = cfg_getint( mainSection, "loop_max_fds" );
	}
	Boot( fds );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( core = Core_New( config  ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.core = 1;
		cleanUp.good = ( ( webserverModule = Module_New( "webserver", NULL, NULL, NULL, NULL ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Core_AddModule( core, webserverModule ) ) ? 1 : 0;
	}
	if ( cleanUp.good ) {
		cleanUp.webserver = 1;
		cleanUp.good = ( ( pgSqlclientModule = Module_New( "postgresql", NULL, NULL, NULL, NULL ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Core_AddModule( core, pgSqlclientModule ) ) ? 1 : 0 ;
	}
	if ( cleanUp.good ) {
		cleanUp.pgSqlclient = 1;
#if HAVE_MYSQL == 1
		cleanUp.good = ( ( mySqlclientModule = Module_New( "mysqlclient", NULL, NULL, NULL, NULL ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Core_AddModule( core, mySqlclientModule ) ) ? 1 : 0;
	}
	if ( cleanUp.good ) {
		cleanUp.mySqlclient = 1;
#endif
		cleanUp.good = ( ( javascriptModule = Module_New( "javascript", JavascriptModule_Load, JavascriptModule_Ready, JavascriptModule_Unload, NULL ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Core_AddModule( core, javascriptModule ) ) ? 1 : 0 ;
	}
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
		cleanUp.good = ( Core_PrepareDaemon( core, SignalHandler ) == 1 );
	}
	if ( cleanUp.good ) {
		Core_Loop( core );
		Core_Delete( core ) ;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.webserver) {
			Core_DelModule( core, webserverModule );
		}
		if ( cleanUp.pgSqlclient) {
			Core_DelModule( core, pgSqlclientModule );
		}
#if HAVE_MYSQL == 1
		if ( cleanUp.mySqlclient) {
			Core_DelModule( core, mySqlclientModule );
		}
#endif
		if ( cleanUp.javascript ) {
			Core_DelModule( core, javascriptModule );
		}
		if ( cleanUp.core ) {
			Core_Delete( core ) ;
		}
	}

	Shutdown( );

	return 0;
}

