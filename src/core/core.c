#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/resource.h>

#include "core.h"

#include "configuration.h"
/*****************************************************************************/
/* Global things                                                             */
/*****************************************************************************/

extern cfg_opt_t allCfgOpts[];

void Boot( int fds) {
	if ( fds == 0 ) {
		fds = PR_CFG_LOOP_MAX_FDS;
	}
	fprintf( stdout, "Starting\n" );
	picoev_init( fds );
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


/*****************************************************************************/
/* Timings                                                                    */
/*****************************************************************************/
struct timing_t *			Timing_New 					( int ms, timerHandler_cb_t timerHandler_cb, void * cbArg );
void 						Timing_Delete				( struct timing_t * timing );


struct timing_t * Timing_New ( int ms, timerHandler_cb_t timerHandler_cb, void * cbArg ) {
	struct timing_t * timing;
	struct {unsigned int good:1;
			unsigned timing:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) ) ;
	cleanUp.good = ( ( timing = malloc( sizeof(* timing ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.timing = 1;
		timing->ms = ms;
		timing->identifier = 1;  //  @FIXME:  generate a unqiue nr
		timing->timerHandler_cb = timerHandler_cb;
		timing->cbArg = cbArg;
		timing->clearFunc_cb = NULL;
		PR_INIT_CLIST( &timing->mLink );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.timing ) {
				free( timing ); timing = NULL;
		}
	}
	return timing;
}

void Timing_Delete( struct timing_t * timing ) {
	if ( timing->clearFunc_cb != NULL ) {
		timing->clearFunc_cb( timing->cbArg);
	}
	timing->ms = 0;
	timing->identifier = 0;
	timing->timerHandler_cb = NULL;
	timing->clearFunc_cb= NULL;
	timing->cbArg = NULL;
	free( timing ); timing = NULL;
}

/*****************************************************************************/
/* Core                                                                      */
/*****************************************************************************/
struct core_t * Core_New( struct module_t * modules, const int modulesCount, cfg_t * config ) {
	struct core_t * core;
	cfg_t * mainSection;
	int timeoutSec;
	struct {unsigned int good:1;
		unsigned int loop:1;
		unsigned int config:1;
		unsigned int core:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( core = malloc( sizeof( * core ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.core = 1;
		PR_INIT_CLIST( (&core->timings->mLink) );
		mainSection = cfg_getnsec( core->config, "main", 0 );
		timeoutSec = cfg_getint( mainSection, "loop_timeout_sec" );
		if ( timeoutSec == 0 ) {
			timeoutSec = PR_CFG_LOOP_TIMEOUT_SEC;
		}
		cleanUp.good = ( ( core->loop = picoev_create_loop( timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.loop = 1;
		if (modulesCount < 1 || modules == NULL ) {
			core->modules = NULL;
			core->modules = 0;
		} else {
			core->modules = modules;
		}
		core->modulesCount = modulesCount;
	}
	if ( cleanUp.good ) {
		if ( config != NULL ) {
			core->config = config;
		} else {
			cleanUp.good = ( ( core->config = cfg_init( allCfgOpts, 0 ) ) != NULL );
		}
	}
	if ( cleanUp.good ) {
		cleanUp.config = 1;
	}
	if ( cleanUp.good ) {
		Core_FireEvent( core, MODULEEVENT_LOAD );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.loop ) {
			picoev_destroy_loop( core->loop );
		}
		if ( cleanUp.config ) {
			cfg_free( core->config );
		}
		if ( cleanUp.core ) {
			core->modules = NULL;
			core->modules = 0;
			free( core ); core = NULL;
		}
	}

	return core;
}

int Core_PrepareDaemon( struct core_t * core , signalAction_cb_t signalHandler ) {
	struct rlimit limit;
	cfg_t * mainSection;
	int fds;
	struct {unsigned int good:1;
			unsigned int fds:1;
			unsigned int signal:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	mainSection = cfg_getnsec( core->config, "main", 0 );
	fds = cfg_getint( mainSection, "max_fds" );
	if ( fds == 0 ) {
		fds = PR_CFG_LOOP_MAX_FDS;
	}
	limit.rlim_cur = ( rlim_t ) fds;
	limit.rlim_max = ( rlim_t ) fds;
	cleanUp.good = ( setrlimit( RLIMIT_NOFILE, &limit) != -1 );
	if ( cleanUp.good ) {
		cleanUp.fds = 1;
		cleanUp.signal = 1;
		signal( SIGINT, signalHandler );
		signal( SIGTERM, signalHandler );
	}

	return cleanUp.good;
}

void Core_Loop( struct core_t * core ) {
	Core_FireEvent( core, MODULEEVENT_READY );
	core->keepOnRunning = 1;
	while ( core->keepOnRunning )  {
		  //  @TODO:  processTick( core->timing );
		picoev_loop_once( core->loop, 0 );
	}
}

void Core_FireEvent( struct core_t *core, enum moduleEvent_t event ) {
	struct module_t * module;
	void * instance;
	int i;

	switch ( event ) {
	case MODULEEVENT_LOAD:
		for ( i = 0; i < core->modulesCount; i++ ) {
			module = &core->modules[i];
			if (module->module_load != NULL ) {
				instance = module->module_load( core );
				if ( instance == NULL ) {
					  //  @TODO:  better logging on errors
				}
				module->data = instance;  //  @FIXME:  handle multiple instances
			}
		}
		break;
	case MODULEEVENT_READY:
		for ( i = 0; i < core->modulesCount; i++ ) {
			module = &core->modules[i];
			if (module->module_ready != NULL && module->data != NULL ) {
		//		module->module_ready( core, module->data );
			}
		}
		break;
	case MODULEEVENT_UNLOAD:
		for ( i = core->modulesCount; i > 0; i-- ) {
			module = &core->modules[i - 1 ];
			if (module->module_unload != NULL && module->data != NULL ) {
				module->module_unload( core, module->data );
			}
		}
		break;
	default:
		break;

	}
}

struct timing_t *  Core_AddTiming ( struct core_t * core , int ms, timerHandler_cb_t timerHandler_cb, void * cbArg ) {
	struct timing_t * timing;
	struct {unsigned int good:1;
			unsigned timing:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) ) ;
	cleanUp.good = ( (  timing = Timing_New( ms, timerHandler_cb, cbArg ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.timing = 1;
		PR_APPEND_LINK( &core->timings->mLink, &timing->mLink );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.timing ) {
				Timing_Delete( timing ); timing = NULL;
		}
	}
	return timing;
}
void Core_DelTimingId ( struct core_t * core , uint32_t id ) {
	struct timing_t * timing;
	PRCList * next;

	next = core->timings->mLink.next;
	if ( PR_CLIST_IS_EMPTY( next ) != 0 ) {
		do {
			timing = FROM_NEXT_TO_ITEM( struct timing_t );
			if ( timing->identifier == id ) {
				Core_DelTiming( timing );
				break;
			}
			next = timing->mLink.next;
		} while ( next != NULL );
	}
}

void Core_DelTiming( struct timing_t * timing ) {
	PR_REMOVE_AND_INIT_LINK( &timing->mLink );
	Timing_Delete( timing );
}

void Core_Delete( struct core_t * core ) {
	struct timing_t * timing;
	PRCList * next;

	next = core->timings->mLink.next;
	if ( PR_CLIST_IS_EMPTY( next ) != 0 ) {
		do {
			timing = FROM_NEXT_TO_ITEM( struct timing_t );
			next = timing->mLink.next;
			Core_DelTiming( timing );
		} while( next != NULL );
	}

	picoev_destroy_loop( core->loop );
	Core_FireEvent( core, MODULEEVENT_UNLOAD );
	core->modules = NULL;
	core->modules = 0;
	cfg_free( core->config );
	free( core );
}

