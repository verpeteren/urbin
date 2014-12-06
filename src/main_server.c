#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "./core/core.h"
#include "./feature/webserver.h"
#include "./feature/sqlclient.h"

struct core_t * core;

static void SignalHandler( int sign ) {
	fprintf( stdout, "Shutting down...\n" );
	core->keepOnRunning = 0;
}


int main( int argc, const char ** argv ) {
	struct webserver_t * webserver;
	struct sqlclient_t * pgSqlclient, *mySqlclient;
	struct {
		unsigned int good:1;
		unsigned int core:1;
		unsigned int webserver:1;
		unsigned int pgSqlclient:1;
		unsigned int mySqlclient:1;
		} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	core = NULL;
	webserver = NULL;
	pgSqlclient = NULL;
	mySqlclient = NULL;

	Boot( );
	cleanUp.good = ( ( core = Core_New( ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.core = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( webserver = Webserver_New( core, "127.0.0.1", 8080, 7 ) )  != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( pgSqlclient = Postgresql_New( core, "localhost", "127.0.0.1", 5432, "apedev", "vedepa", "apedev" ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( mySqlclient = Mysql_New( core, "localhost", "127.0.0.1", 3305, "apedev", "vedepa", "apedev" ) ) != NULL );
	}
	if ( cleanUp.good ) {
		Webserver_DocumentRoot( webserver, "/static/(.*)" , "/var/www" );
	}
	if ( cleanUp.good ) {
		signal( SIGUSR2, &SignalHandler );
		cleanUp.webserver = 1;
		Webserver_JoinCore( webserver );
		Core_Loop( core );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.pgSqlclient) {
			Sqlclient_Delete( pgSqlclient );
		}
		if ( cleanUp.mySqlclient) {
			Sqlclient_Delete( mySqlclient );
		}
		if ( cleanUp.webserver ) {
			Webserver_Delete( webserver );
		}
		if ( cleanUp.core ) {
			Core_Delete( core ) ;
		}
	} else {
		Webserver_Delete( webserver );
		Core_Delete( core ) ;
	}

	Shutdown( );
	return 0;
}

