#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

#include "configuration.h"
#include "../common.h"

void usage( const char * prog_name, int code ) {
	printf( "\nUsage: %s [options] [configurationFilename]\n"
				"\t  option                             default\n"
				"\t  configurationFileName   [FILE]     ( %s )\n"
				, prog_name,
				PR_MAINCONFIG_FILENAME
		);
	exit( code );
}

cfg_opt_t main_cfg_opts[] = {
	CFG_INT( ( char * ) "max_file_descriptors",		PR_CFG_MAX_FDS, CFGF_NONE ),
	CFG_END()
};

cfg_opt_t Webserver_cfg_opts[] = {
	CFG_STR( (char * ) "documentroot", 	(char *)	PR_CFG_MODULES_WEBSERVER_ROOT, CFGF_NONE ),
	CFG_STR( (char * ) "path", 			(char *)	PR_CFG_MODULES_WEBSERVER_PATH, CFGF_NONE ),
	CFG_STR( (char * ) "ip", 			(char *)	PR_CFG_MODULES_WEBSERVER_IP, CFGF_NONE ),
	CFG_INT( (char * ) "port", 						PR_CFG_MODULES_WEBSERVER_PORT, CFGF_NONE ),
	CFG_INT( (char * ) "timeout_sec", 				PR_CFG_MODULES_WEBSERVER_TIMEOUT_SEC, CFGF_NONE ),
	CFG_END()
};

cfg_opt_t interpreter_cfg_opts[] = {
	CFG_STR( (char * ) "path", 			(char *)	PR_CFG_GLOT_PATH, CFGF_NONE ),
	CFG_STR( (char * ) "main", 			(char *)	PR_CFG_GLOT_MAIN, CFGF_NONE ),
	CFG_END()
};

cfg_opt_t modules_cfg_opts[] = {
		CFG_SEC( (char * ) "webserver", Webserver_cfg_opts, CFGF_MULTI | CFGF_TITLE),
	CFG_END()
};

cfg_opt_t glot_cfg_opts[] = {
	CFG_SEC( (char * ) "javascript", interpreter_cfg_opts, CFGF_MULTI | CFGF_TITLE),
	CFG_END()
};

cfg_opt_t all_cfg_opts[] = {
	CFG_SEC( (char * ) "main", 		main_cfg_opts, CFGF_MULTI | CFGF_TITLE),
	CFG_SEC( (char * ) "modules",	modules_cfg_opts, CFGF_MULTI | CFGF_TITLE),
	CFG_SEC( (char * ) "glot",		glot_cfg_opts, CFGF_MULTI | CFGF_TITLE),
	CFG_END()
};

cfg_t * process_commandline( int argc, const char ** argv ) {
	int i;
	cfg_t * config;
	char * arg;
	const char * prog_name, * filename;
	struct {unsigned int good:1;
			unsigned int config:1; } cleanUp;

	prog_name = PR_NAME;
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	config = NULL;
	cleanUp.good = ( ( config = cfg_init( all_cfg_opts, 0 ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.config = 1;
		prog_name = basename( (char *) &argv[0][0] );
		filename = PR_MAINCONFIG_FILENAME;
		for ( i = 1; i < argc && cleanUp.good ; i++ ) {
		 	arg = (char *) argv[i];
			if ( strcmp( arg, "--version" ) == 0 ) {
				printf( "%s Version: %s\n", argv[0], PR_VERSION );
				exit( 0 );
			} else if ( strcmp( arg, "--help" ) == 0 ) {
				usage( prog_name, 0 );
			} else if ( i == argc - 1) {
				filename = arg;
			} else {
				cleanUp.good = 0;
			}
		 }
	}
	if ( cleanUp.good) {
		cleanUp.good = ( cfg_parse( config, filename ) == CFG_SUCCESS );
	 }
	 if ( ! cleanUp.good ) {
		if ( cleanUp.config ) {
			cfg_free( config ); config = NULL;
		}
	 	usage( prog_name, 1 );
	 }

	return config;
}

