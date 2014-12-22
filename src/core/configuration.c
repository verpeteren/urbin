#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

#include "configuration.h"

void Usage( const char * prog_name, const int code ) {
	printf( "\nUsage: %s [options] [configurationFilename]\n"
				"\t option                             default\n"
				"\t configurationFileName   [FILE]     ( %s )\n"
				, prog_name,
				PR_MAINCONFIG_FILENAME );
	exit( code );
}

static const cfg_opt_t mainCfgOpts[] = {
	CFG_INT( (char *) "loop_max_fds",					PR_CFG_LOOP_MAX_FDS, CFGF_NONE ),
	CFG_INT( (char *) "loop_timeout_sec", 				PR_CFG_LOOP_TIMEOUT_SEC, CFGF_NONE ),
	CFG_INT( (char *) "loop_ticks_ms", 				PR_CFG_LOOP_TICKS_MS, CFGF_NONE ),
	CFG_INT( (char *) "loop_max_wait", 				PR_CFG_LOOP_MAX_WAIT, CFGF_NONE ),
	CFG_BOOL( (char *) "loop_daemon",					PR_CFG_LOOP_DAEMON, CFGF_NONE ),
	CFG_STR( (char *) "loop_log_level",	(char *)	PR_CFG_LOOP_LOG_LEVEL_TEXT, CFGF_NONE ),
	CFG_END( )
};

static const cfg_opt_t webserverCfgOpts[] = {
	CFG_STR( (char *) "documentroot", 		(char *)	PR_CFG_MODULES_WEBSERVER_ROOT, CFGF_NONE ),
	CFG_STR( (char *) "path", 				(char *)	PR_CFG_MODULES_WEBSERVER_PATH, CFGF_NONE ),
	CFG_STR( (char *) "ip", 				(char *)	PR_CFG_MODULES_WEBSERVER_IP, CFGF_NONE ),
	CFG_INT( (char *) "port", 							PR_CFG_MODULES_WEBSERVER_PORT, CFGF_NONE ),
	CFG_INT( (char *) "timeout_sec", 					PR_CFG_MODULES_WEBSERVER_TIMEOUT_SEC, CFGF_NONE ),
	CFG_INT( (char *) "listen_backlog", 				PR_CFG_MODULES_WEBSERVER_LISTEN_BACKLOG, CFGF_NONE ),
	CFG_END( )
};

static const cfg_opt_t mysqlclientCfgOpts[] = {
	CFG_STR( (char *) "database",	 		(char *)	PR_CFG_MODULES_MYSQLCLIENT_DATABASE, CFGF_NONE ),
	CFG_STR( (char *) "ip", 				(char *)	PR_CFG_MODULES_MYSQLCLIENT_IP, CFGF_NONE ),
	CFG_INT( (char *) "port", 							PR_CFG_MODULES_MYSQLCLIENT_PORT, CFGF_NONE ),
	CFG_INT( (char *) "timeout_sec", 					PR_CFG_MODULES_MYSQLCLIENT_TIMEOUT_SEC, CFGF_NONE ),
	CFG_END( )
};

static const cfg_opt_t pgsqlclientCfgOpts[] = {
	CFG_STR( (char *) "database",	 		(char *)	PR_CFG_MODULES_PGSQLCLIENT_DATABASE, CFGF_NONE ),
	CFG_STR( (char *) "ip", 				(char *)	PR_CFG_MODULES_PGSQLCLIENT_IP, CFGF_NONE ),
	CFG_INT( (char *) "port", 							PR_CFG_MODULES_PGSQLCLIENT_PORT, CFGF_NONE ),
	CFG_INT( (char *) "timeout_sec", 					PR_CFG_MODULES_PGSQLCLIENT_TIMEOUT_SEC, CFGF_NONE ),
	CFG_END( )
};

static const cfg_opt_t javascriptCfgOpts[] = {
	CFG_STR( (char *) "path", 				(char *)	PR_CFG_GLOT_PATH, CFGF_NONE ),
	CFG_STR( (char *) "main", 				(char *)	PR_CFG_GLOT_MAIN, CFGF_NONE ),
	CFG_END( )
};

static const cfg_opt_t modulesCfgOpts[] = {
	CFG_SEC( (char *) "webserver", 		(cfg_opt_t *) webserverCfgOpts, CFGF_MULTI | CFGF_TITLE),
	CFG_SEC( (char *) "mysqlclient", 		(cfg_opt_t *) mysqlclientCfgOpts, CFGF_MULTI | CFGF_TITLE),
	CFG_SEC( (char *) "postgresqlclient",	(cfg_opt_t *) pgsqlclientCfgOpts, CFGF_MULTI | CFGF_TITLE),
	CFG_END( )
};

static const cfg_opt_t glotCfgOpts[] = {
	CFG_SEC( (char *) "javascript", 		(cfg_opt_t *) javascriptCfgOpts, CFGF_MULTI | CFGF_TITLE),
	CFG_END( )
};

static const cfg_opt_t allCfgOpts[] = {
	CFG_SEC( (char *) "main", 				(cfg_opt_t *) mainCfgOpts, CFGF_MULTI | CFGF_TITLE),
	CFG_SEC( (char *) "modules",			(cfg_opt_t *) modulesCfgOpts, CFGF_MULTI | CFGF_TITLE),
	CFG_SEC( (char *) "glot",				(cfg_opt_t *) glotCfgOpts, CFGF_MULTI | CFGF_TITLE),
	CFG_END( )
};

cfg_t * ProcessCommandline( const int argc, const char ** argv ) {
	int i;
	cfg_t * config;
	char * arg;
	const char * progName, * fileName;
	struct {unsigned char good:1;
			unsigned char config:1; } cleanUp;

	progName = PR_NAME;
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	config = NULL;
	cleanUp.good = ( ( config = cfg_init( (cfg_opt_t *) allCfgOpts, 0 ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.config = 1;
		progName = basename( (char *) &argv[0][0] );
		fileName = PR_MAINCONFIG_FILENAME;
		for ( i = 1; i < argc && cleanUp.good; i++ ) {
			arg = (char *) argv[i];
			if ( strcmp( arg, "--version" ) == 0 ) {
				printf( "%s Version: %s\n", argv[0], PR_VERSION );
				exit( 0 );
			} else if ( strcmp( arg, "--help" ) == 0 ) {
				Usage( progName, 0 );
			} else if ( i == argc - 1 ) {
				fileName = arg;
			} else {
				cleanUp.good = 0;
			}
		}
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( cfg_parse( config, fileName ) == CFG_SUCCESS );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.config ) {
			cfg_free( config ); config = NULL;
		}
		Usage( progName, 1 );
	}

	return config;
}

