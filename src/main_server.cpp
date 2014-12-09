#include <stdio.h>
#include <string.h>

#include "./core/core.h"
#include "./feature/webserver.h"
#include "./feature/sqlclient.h"
#include "./glot/javascript.h"

extern struct module_t gWebserverModule;
extern struct module_t gJavascriptModule;

struct core_t * core;

static void SignalHandler( int sign ) {
	fprintf( stdout, "Shutting down...\n" );
	core->keepOnRunning = 0;
}

int main( int argc, const char ** argv ) {
	struct sqlclient_t * pgSqlclient, *mySqlclient;
	cfg_t * config;
	const int modulesCount = 1;
	struct module_t modules[modulesCount];
	struct {
		unsigned int good:1;
		unsigned int core:1;
		unsigned int webserver:1;
		unsigned int pgSqlclient:1;
		unsigned int mySqlclient:1;
		} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	core = NULL;
	pgSqlclient = NULL;
	mySqlclient = NULL;
	modules[0] = gWebserverModule;
	//modules[0] = gJavascriptModule;
	Boot( );
	cleanUp.good = ( ( config = ProcessCommandline( argc, argv ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( core = Core_New( modules, modulesCount, config  ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.core = 1;
		cleanUp.good = ( ( pgSqlclient = Postgresql_New( core, "localhost", "127.0.0.1", 5432, "apedev", "vedepa", "apedev", 10 ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.pgSqlclient = 1;
		cleanUp.good = ( ( mySqlclient = Mysql_New( core, "localhost", "127.0.0.1", 3305, "apedev", "vedepa", "apedev", 10 ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.mySqlclient = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Core_PrepareDaemon( core, SignalHandler ) == 1 );
	}
	if ( cleanUp.good ) {
		Core_Loop( core );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.pgSqlclient) {
			Sqlclient_Delete( pgSqlclient );
		}
		if ( cleanUp.mySqlclient) {
			Sqlclient_Delete( mySqlclient );
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

