#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
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
/* Modules                                                                   */
/*****************************************************************************/
struct module_t * Module_New ( const char *name, moduleHandler_cb_t onLoad,  moduleHandler_cb_t onReady, moduleHandler_cb_t onUnload, void * cbArg ) {
	struct module_t * module;
	struct {unsigned int good:1;
			unsigned int name:1;
			unsigned int module:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( module = malloc( sizeof( *module ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.module = 1;
		module->onLoad = onLoad;
		module->onReady = onReady;
		module->onUnload = onUnload;
		module->cbArg = cbArg;
		PR_INIT_CLIST( &module->mLink );
		cleanUp.name = ( ( module->name = strdup( name ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.name = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.name ) {
			free( (char * ) module->name ); module->name = NULL;
		}
		if ( cleanUp.module ) {
			free( module ); module = NULL;
		}
	}
	return module;
}

void Module_Delete ( struct module_t * module ) {
	free( (char * ) module->name ); module->name = NULL;
	free( module ); module = NULL;
}

/*****************************************************************************/
/* Timings                                                                    */
/*****************************************************************************/
static struct timing_t *			Timing_New 					( unsigned int ms, uint32_t identifier, unsigned int repeat, timerHandler_cb_t timerHandler_cb, void * cbArg );
static void 						Timer_CalculateDue			( struct timing_t * timing, PRUint32 nowOrHorizon );
static void 						Timing_Delete				( struct timing_t * timing );

static struct timing_t * Timing_New ( unsigned int ms, uint32_t identifier, unsigned int repeat, timerHandler_cb_t timerHandler_cb, void * cbArg ) {
	struct timing_t * timing;
	PRUint32 horizon;
	PRIntervalTime now;
	struct {unsigned int good:1;
			unsigned timing:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) ) ;
	cleanUp.good = ( ( timing = malloc( sizeof(* timing ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.timing = 1;
		timing->ms = ms;
		timing->repeat = ( repeat == 1 ) ? 1: 0;
		timing->identifier = identifier;
		timing->timerHandler_cb = timerHandler_cb;
		timing->cbArg = cbArg;
		now = PR_IntervalNow( );
		horizon = PR_IntervalToMicroseconds( now );
		Timer_CalculateDue( timing, horizon );
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
static void Timer_CalculateDue ( struct timing_t * timing, PRUint32 nowOrHorizon ) {
	timing->due = ( nowOrHorizon + timing->ms );  //  @FIXME, this might overflow if ms is in the far future (+6 hours)
}

static void Timing_Delete( struct timing_t * timing ) {
	if ( timing->clearFunc_cb != NULL ) {
		timing->clearFunc_cb( timing->cbArg);
	}
	timing->ms = 0;
	timing->repeat = 0;
	timing->identifier = 0;
	timing->timerHandler_cb = NULL;
	timing->clearFunc_cb= NULL;
	timing->cbArg = NULL;
	free( timing ); timing = NULL;
}

/*****************************************************************************/
/* Core                                                                      */
/*****************************************************************************/
struct core_t * Core_New( cfg_t * config ) {
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
		core->maxIdentifier = 1;
		// set up the linked lists
		PR_INIT_CLIST( (&core->timings->mLink) );
		PR_INIT_CLIST( (&core->modules->mLink) );
		//  Set up the ticks
		mainSection = cfg_getnsec( core->config, "main", 0 );
		timeoutSec = cfg_getint( mainSection, "loop_timeout_sec" );
		if ( timeoutSec == 0 ) {
			timeoutSec = PR_CFG_LOOP_TIMEOUT_SEC;
		}
		core->processTicksMs = (unsigned char) cfg_getint( mainSection, "loop_ticks_ms" );
		if ( core->processTicksMs == 0 ) {
			core->processTicksMs = PR_CFG_LOOP_TICKS_MS;
		}
		// Set up the logging
#if DEBUG
		core->logger.logLevel = LOG_DEBUG;
#else
		core->logger.logLevel = PR_LOG_LOG_LEVEL;
#endif
		if ( cfg_getbool( mainSection, "daemon" ) ) {
			 get_syslog_logger( & core->logger.logFun, 0, &core->logger.logMask);
		} else {
			//  we log to the stderr in interactive mode
			get_stderr_logger( &core->logger.logFun, 0, &core->logger.logMask );
		}
		core->logger.logMask( LOG_UPTO( core->logger.logLevel ) );
		cleanUp.good = ( ( core->loop = picoev_create_loop( timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.loop = 1;
		if ( config != NULL ) {
			core->config = config;
		} else {
			cleanUp.good = ( ( core->config = cfg_init( allCfgOpts, 0 ) ) != NULL );
		}
	}
	if ( cleanUp.good ) {
		cleanUp.config = 1;
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
/*
inline void Core_Log( struct core_t * core, const int logLevel, const char * message ) {
	core->logger.logFun( logLevel, message );
}*/
inline void Core_Log( struct core_t * core, int logLevel, const char * fmt, ... ) {
	char *message;
	va_list val;

	va_start( val, fmt );
	vasprintf( &message, fmt, val );
	va_end( val );
	core->logger.logFun( logLevel, "%", message );

	free( message );
}

int Core_PrepareDaemon( struct core_t * core , signalAction_cb_t signalHandler ) {
	struct rlimit limit;
	cfg_t * mainSection;
	cfg_bool_t daemonize;
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
		daemonize = cfg_getbool( mainSection, "daemon" );
		if ( daemonize )  {
			if (0 != fork( ) ) {
				exit( 0 );
			}
			if ( -1 == setsid( ) ) {
				exit( 0 );
			}
			signal( SIGHUP, SIG_IGN );
			if ( 0 != fork( ) ) {
				exit( 0 );
			}
		}
	}

	return cleanUp.good;
}

static void Core_ProcessTick( struct core_t * core ) {
	struct timing_t * timing;
	PRCList * next;
	PRUint32 horizon;
	PRIntervalTime now;
	int needToFire;

	now = PR_IntervalNow( );
	horizon = PR_IntervalToMicroseconds( now ) + core->processTicksMs;
	next = core->timings->mLink.next;
	if ( PR_CLIST_IS_EMPTY( next ) != 0 ) {
		do {
			timing = FROM_NEXT_TO_ITEM( struct timing_t );
			next = timing->mLink.next;
			needToFire = ( timing->due < horizon && timing->due != 0 );
			if ( needToFire ) {
				if ( timing->repeat ) {
					Timer_CalculateDue( timing, horizon );
				} else {
					Core_DelTiming( timing );
				}
			}
		} while( next != NULL );
	}
}

void Core_Loop( struct core_t * core ) {
	struct module_t * module;
	PRCList * next;

	next = core->modules->mLink.next;
	if ( PR_CLIST_IS_EMPTY( next ) != 0 ) {
		do {
			module = FROM_NEXT_TO_ITEM( struct module_t );
			next = module->mLink.next;
			if ( module->onReady != NULL )  {
					module->onReady ( module->cbArg );
			}
		} while( next != NULL );
	}
	core->keepOnRunning = 1;
	while ( core->keepOnRunning )  {
		Core_ProcessTick( core );
		picoev_loop_once( core->loop, 0 );
	}
}

void Core_AddModule( struct core_t * core, struct module_t * module ) {
	if ( module != NULL ) {
		PR_APPEND_LINK( &core->modules->mLink, &module->mLink );
		if ( module->onLoad != NULL ) {
			module->onLoad( module->cbArg );
		}
	}
}

void Core_DelModule( struct core_t * core, struct module_t * module ) {
	if ( module != NULL ) {
		PR_REMOVE_AND_INIT_LINK( &module->mLink );
		if ( module->onUnload != NULL ) {
			module->onUnload( module->cbArg );
		}
	}
}

struct timing_t *  Core_AddTiming ( struct core_t * core , unsigned int ms, unsigned int repeat, timerHandler_cb_t timerHandler_cb, void * cbArg ) {
	struct timing_t * timing;
	struct {unsigned int good:1;
			unsigned timing:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) ) ;
	core->maxIdentifier++;
	cleanUp.good = ( (  timing = Timing_New( ms, core->maxIdentifier, repeat, timerHandler_cb, cbArg ) ) != NULL );
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
	struct module_t * module;
	PRCList * next;
	//  cleanup the timers
	next = core->timings->mLink.next;
	if ( PR_CLIST_IS_EMPTY( next ) != 0 ) {
		do {
			timing = FROM_NEXT_TO_ITEM( struct timing_t );
			next = timing->mLink.next;
			Core_DelTiming( timing );
		} while( next != NULL );
	}
	//  cleanup the modules
	next = core->modules->mLink.next;
	if ( PR_CLIST_IS_EMPTY( next ) != 0 ) {
		do {
			module = FROM_NEXT_TO_ITEM( struct module_t );
			next = module->mLink.next;
			Core_DelModule( core, module );
		} while( next != NULL );
	}
	//  cleanup the rest
	core->processTicksMs = 0;
	picoev_destroy_loop( core->loop );
	core->modules = NULL;
	core->modules = 0;
	cfg_free( core->config );
	free( core );
}

