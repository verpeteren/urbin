#include <stdio.h>
#include <string.h>

#include "./core/core.h"
#include "./feature/webserver.h"
#include "./feature/sqlclient.h"
#include "./glot/javascript.h"


struct core_t * core;

static void SignalHandler( int sign ) {
	fprintf( stdout, "Shutting down...\n" );
	core->keepOnRunning = 0;
}

int main( int argc, const char ** argv ) {
	cfg_t * config, * mainSection;
	int fds;
	struct module_t * webserverModule, * pgSqlclientModule, * mySqlclientModule, *javascriptModule;
	struct {
		unsigned int good:1;
		unsigned int core:1;
		unsigned int webserver:1;
		unsigned int pgSqlclient:1;
		unsigned int javascript:1;
		unsigned int mySqlclient:1;
		} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	core = NULL;
	cleanUp.good = ( ( config = ProcessCommandline( argc, argv ) ) != NULL );
	if ( cleanUp.good ) {
		mainSection = cfg_getnsec( core->config, "main", 0 );
		fds = cfg_getint( mainSection, "max_fds" );
	}
	Boot( fds );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( core = Core_New( config  ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.core = 1;
		cleanUp.good = ( ( webserverModule = Module_New( "webserver", NULL, NULL, NULL, NULL ) ) != NULL );
		Core_AddModule( core, webserverModule );

	}
	if ( cleanUp.good ) {
		cleanUp.webserver = 1;
		cleanUp.good = ( ( pgSqlclientModule = Module_New( "postgresql", NULL, NULL, NULL, NULL ) ) != NULL );
		Core_AddModule( core, pgSqlclientModule );
	}
	if ( cleanUp.good ) {
		cleanUp.pgSqlclient = 1;
		cleanUp.good = ( ( mySqlclientModule = Module_New( "mysqlclient", NULL, NULL, NULL, NULL ) ) != NULL );
		Core_AddModule( core, mySqlclientModule );
	}
	if ( cleanUp.good ) {
		cleanUp.mySqlclient = 1;
		cleanUp.good = ( ( javascriptModule = Module_New( "javascript", NULL, NULL, NULL, NULL ) ) != NULL );
		Core_AddModule( core, javascriptModule );
	}
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Core_PrepareDaemon( core, SignalHandler ) == 1 );
	}
	if ( cleanUp.good ) {
		Core_Loop( core );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.webserver) {
			Core_DelModule( core, webserverModule );
		}
		if ( cleanUp.pgSqlclient) {
			Core_DelModule( core, pgSqlclientModule );
		}
		if ( cleanUp.pgSqlclient) {
			Core_DelModule( core, mySqlclientModule );
		}
		if ( cleanUp.javascript ) {
			Core_DelModule( core, javascriptModule );
		}
		if ( cleanUp.core ) {
			Core_Delete( core ) ;
		}
	} else {
		Core_Delete( core ) ;
	}

	Shutdown( );
	return 0;
}

