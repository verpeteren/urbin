#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "core.h"
#include "utils.h"
#include "configuration.h"

#ifdef SYSLOG_NAMES
extern CODE prioritynames[];
#else
typedef struct _code {
	const char *			c_name;
	const int				c_val;
} CODE;

CODE prioritynames[] = {
	{ "emerg", LOG_EMERG },
	{ "alert", LOG_ALERT },
	{ "crit", LOG_CRIT },
	{ "err", LOG_ERR },
	{ "warning", LOG_WARNING },
	{ "notice", LOG_NOTICE },
	{ "info", LOG_INFO },
	{ "debug", LOG_DEBUG },
	{ NULL, -1 }
};
#endif
/*****************************************************************************/
/* Global things                                                             */
/*****************************************************************************/
void Boot( const int fds ) {
	int fdMax;

	fdMax = ( fds == 0 ) ? PR_CFG_LOOP_MAX_FDS : fds;
	fprintf( stdout, "Starting with %d slots\n", fdMax );
	picoev_init( fdMax );
}

void Shutdown( ) {
	fprintf( stdout, "Shutdown\n" );
	picoev_deinit( );
}

void SetupSocket( const int fd, const unsigned char tcp ) {
	int on, r;

	on = 1;
	if ( tcp ) {
		r = setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof( on ) );
		assert( r == 0 );
	}
	on = fcntl( fd, F_GETFL, 0 );
	r = fcntl( fd, F_SETFL, on | O_NONBLOCK );
	assert( r == 0 );
}

static int GetPriorityFromName( const char * name ) {
	int logLevel = PR_LOG_LEVEL_VALUE;
	CODE * priority;

	priority = &prioritynames[0];
	while( priority->c_name != NULL ) {
		if ( strcmp( priority->c_name, name) == 0 ) {
			logLevel = priority->c_val;
			break;
		}
		priority++;
	}

	return logLevel;
}
/*****************************************************************************/
/* Modules                                                                   */
/*****************************************************************************/
struct module_t * Module_New ( const char * name, const moduleHandler_cb_t onLoad, const moduleHandler_cb_t onReady, const moduleHandler_cb_t onUnload, void * cbArgs ) {
	struct module_t * module;
	struct {unsigned char good:1;
			unsigned char name:1;
			unsigned char module:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( module = malloc( sizeof( *module ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.module = 1;
		module->onLoad = onLoad;
		module->onReady = onReady;
		module->onUnload = onUnload;
		module->cbArgs = cbArgs;
		module->instance = NULL;
		PR_INIT_CLIST( &module->mLink );
		cleanUp.name = ( ( module->name =  Xstrdup( name ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.name = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.name ) {
			free( (char * ) module->name ); module->name = NULL;
		}
		module->cbArgs = NULL,
		module->onLoad = NULL;
		module->onReady = NULL;
		module->onUnload = NULL;
		module->instance = NULL;
		PR_INIT_CLIST( &module->mLink );
		if ( cleanUp.module ) {
			free( module ); module = NULL;
		}
	}
	return module;
}

void Module_Delete ( struct module_t * module ) {
	free( (char * ) module->name ); module->name = NULL;
	module->cbArgs = NULL,
	module->onLoad = NULL;
	module->onReady = NULL;
	module->onUnload = NULL;
	module->instance = NULL;
	PR_INIT_CLIST( &module->mLink );
	free( module ); module = NULL;
}

/*****************************************************************************/
/* Timings                                                                    */
/*****************************************************************************/
static struct timing_t *			Timing_New 					( const unsigned int ms, const uint32_t identifier, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs );
static void 						Timer_CalculateDue			( struct timing_t * timing, const PRUint32 nowOrHorizon );
static void 						Timing_Delete				( struct timing_t * timing );

static struct timing_t * Timing_New ( const unsigned int ms, const uint32_t identifier, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs ) {
	struct timing_t * timing;
	PRUint32 horizon;
	PRIntervalTime now;
	struct {unsigned char good:1;
			unsigned char timing:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) ) ;
	cleanUp.good = ( ( timing = malloc( sizeof(* timing ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.timing = 1;
		timing->ms = ms;
		timing->repeat = ( repeat == 1 ) ? 1: 0;
		timing->identifier = identifier;
		timing->timerHandler_cb = timerHandler_cb;
		timing->cbArgs = cbArgs;
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
static void Timer_CalculateDue ( struct timing_t * timing, const PRUint32 nowOrHorizon ) {
	timing->due = ( nowOrHorizon + timing->ms );  //  @FIXME, this might overflow if ms is in the far future (+6 hours)
}

static void Timing_Delete( struct timing_t * timing ) {
	if ( timing->clearFunc_cb != NULL ) {
		timing->clearFunc_cb( timing->cbArgs);
	}
	timing->ms = 0;
	timing->repeat = 0;
	timing->identifier = 0;
	timing->timerHandler_cb = NULL;
	timing->clearFunc_cb= NULL;
	timing->cbArgs = NULL;
	free( timing ); timing = NULL;
}

/*****************************************************************************/
/* Core                                                                      */
/*****************************************************************************/
struct core_t * Core_New( const cfg_t * config ) {
	struct core_t * core;
	cfg_t * mainSection;
	int timeoutSec;
	struct {unsigned char good:1;
			unsigned char loop:1;
			unsigned char config:1;
			unsigned char dns:1;
			unsigned char core:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( core = malloc( sizeof( * core ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.core = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.config = 1;
		core->maxIdentifier = 1;
		core->config =  config;
		core->timings = NULL;
		core->modules = NULL;
		core->dns = NULL;
		core->logger.logLevel = PR_LOG_LEVEL_VALUE;
		mainSection = cfg_getnsec( (cfg_t *) core->config, "main", 0 );
		timeoutSec = cfg_getint( mainSection, "loop_timeout_sec" );
		if ( timeoutSec == 0 ) {
			timeoutSec = PR_CFG_LOOP_TIMEOUT_SEC;
		}
		core->processTicksMs = (unsigned char) cfg_getint( mainSection, "loop_ticks_ms" );
		if ( core->processTicksMs == 0 ) {
			core->processTicksMs = PR_CFG_LOOP_TICKS_MS;
		}
		//  Set up the logging
		if ( cfg_getbool( mainSection, "loop_daemon" ) == cfg_true ) {
			 get_syslog_logger( & core->logger.logFun, 0, &core->logger.logMask);
		} else {
			//  we log to the stderr in interactive mode
			get_stderr_logger( &core->logger.logFun, 0, &core->logger.logMask );
		}
		core->logger.logLevel = GetPriorityFromName( cfg_getstr( mainSection, "loop_log_level" ) );
		core->logger.logMask( LOG_UPTO( core->logger.logLevel ) );
		cleanUp.good = ( ( core->loop = picoev_create_loop( timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.loop = 1;
		cleanUp.good = ( ( core->dns = dns_init( ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.dns = 1;
		SetupSocket( dns_get_fd( core->dns ), 0 );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.dns ) {
			dns_fini( core->dns ); core->dns = NULL;
		}
		if ( cleanUp.loop ) {
			picoev_destroy_loop( core->loop );
		}
		if ( cleanUp.config ) {
			cfg_free( (cfg_t *) core->config ); core->config = NULL;
		}
		if ( cleanUp.core ) {
			core->modules = NULL;
			core->timings = NULL;
			free( core ); core = NULL;
		}
	}

	return core;
}

void Core_Log( const struct core_t * core, const int logLevel, const char * fileName, const unsigned int lineNr, const char * message ) {
	char * line;
	size_t len;
	struct {unsigned char good:1;
			unsigned char line:1;} cleanUp;

	len = strlen( fileName ) + strlen( message ) + 4 +5 + 1;
	cleanUp.good = ( ( line = malloc( len ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.line = 1;
		snprintf( line, len, "[%s:%5d] %s", fileName, lineNr, message );
		core->logger.logFun( logLevel, "%s", line );
	}
	if ( cleanUp.line ) {
		free( line ); line = NULL;
	}
}

int Core_PrepareDaemon( const struct core_t * core , const signalAction_cb_t signalHandler ) {
	struct rlimit limit;
	cfg_t * mainSection;
	cfg_bool_t daemonize;
	int fds;
	struct {unsigned char good:1;
			unsigned char fds:1;
			unsigned char signal:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	mainSection = cfg_getnsec( (cfg_t *) core->config, "main", 0 );
	fds = cfg_getint( mainSection, "loop_max_fds" );
	if ( fds == 0 ) {
		fds = PR_CFG_LOOP_MAX_FDS;
	}
	limit.rlim_cur = ( rlim_t ) fds;
	limit.rlim_max = ( rlim_t ) fds;
	cleanUp.good = ( setrlimit( RLIMIT_NOFILE, &limit) != -1 );
	if ( cleanUp.good ) {
		cleanUp.fds = 1;
		cleanUp.signal = 1;
		signal( SIGUSR2, signalHandler );
		daemonize = cfg_getbool( mainSection, "loop_daemon" );
		if ( daemonize == cfg_true )  {
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
	struct timing_t * timing, *firstTiming;
	PRCList * next;
	PRUint32 horizon;
	PRIntervalTime now;
	int needToFire;

	firstTiming = timing = core->timings;
	if ( firstTiming != NULL ) {
		now = PR_IntervalNow( );
		horizon = PR_IntervalToMicroseconds( now ) + core->processTicksMs;
		do {
			next = PR_NEXT_LINK( &timing->mLink );
			needToFire = ( timing->due < horizon && timing->due != 0 );
			if ( needToFire ) {
				if ( timing->repeat ) {
					Timer_CalculateDue( timing, horizon );
				} else {
					Core_DelTiming( core, timing );
				}
			}
			timing = FROM_NEXT_TO_ITEM( struct timing_t );
		} while ( timing != firstTiming );
	}
}
#if 0
static void Dns_Found_cb( struct dns_cb_data * dnsData ) {
	if ( dnsData->addr_len == 0 ) {
		fprintf(stderr, "No idea about [%s]\n", dnsData->name);
	} else {
		printf("%u.%u.%u.%u\n", dnsData->addr[0], dnsData->addr[1], dnsData->addr[2], dnsData->addr[3]);
	}
}
#endif

static void Dns_ReadWrite_cb( picoev_loop * loop, int fd, int events, void * cbArgs ) {
	struct core_t * core;

	core = ( struct core_t *) cbArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout, stop and retry again..... */
		picoev_del( loop, fd );
		picoev_add( loop, fd, PICOEV_READWRITE, DNS_QUERY_TIMEOUT, Dns_ReadWrite_cb, cbArgs );
	} else {
		//  sending and receiving shit
		picoev_set_timeout( loop, fd, DNS_QUERY_TIMEOUT );
		dns_poll( core->dns );
	}
}

void Core_GetHostByName ( const struct core_t * core, const char * hostName, dns_callback_t onSuccess_cb ) {
	struct {unsigned char good:1; } cleanUp;
	enum dns_query_type queryType;
	int dnsSocketFd;
	void * cbArgs;

	cbArgs = NULL;
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	dnsSocketFd = dns_get_fd( core->dns );
	queryType = DNS_A_RECORD;
	if ( ! picoev_is_active( core->loop, dnsSocketFd ) ) {
		picoev_add( core->loop, dnsSocketFd, PICOEV_READWRITE, DNS_QUERY_TIMEOUT, Dns_ReadWrite_cb, (void *) core );
	}
	if ( INADDR_NONE == inet_addr( hostName ) ) {
		dns_queue( core->dns, cbArgs, hostName, queryType, onSuccess_cb );
	} else {
		// it is a valid ip address
		struct dns_cb_data data;
		data.context = cbArgs;
		data.query_type = queryType;
		data.name = hostName;
		data.addr = (unsigned char *) hostName;
		data.addr_len = sizeof( hostName );
		onSuccess_cb( &data );
	}
}

int Core_Loop( struct core_t * core ) {
	struct module_t * module, * firstModule;
	cfg_t * mainSection;
	PRCList * next;
	int maxWait, dnsSocketFd;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	dnsSocketFd = dns_get_fd( core->dns );
	maxWait = PR_CFG_LOOP_MAX_FDS;
	mainSection = cfg_getnsec( (cfg_t *) core->config, "main", 0 );
	maxWait = cfg_getint( mainSection, "loop_max_wait" );
	cleanUp.good = 1;
	if ( ! picoev_is_active( core->loop, dnsSocketFd ) ) {
		picoev_add( core->loop, dnsSocketFd, PICOEV_READWRITE, DNS_QUERY_TIMEOUT, Dns_ReadWrite_cb, (void *) core );
	}
	firstModule = module = core->modules;
	if ( firstModule != NULL ) {
		do {
			next = PR_NEXT_LINK( &module->mLink );
			if ( module->onReady != NULL )  {
					cleanUp.good = ( module->onReady ( core, module, module->cbArgs ) ) ? 1 : 0 ;
			}
			module = FROM_NEXT_TO_ITEM( struct module_t );
		} while ( cleanUp.good && module != firstModule );
	}
	if ( cleanUp.good ) {
		core->keepOnRunning = 1;
		while ( core->keepOnRunning == 1 )  {
			Core_ProcessTick( core );
			picoev_loop_once( core->loop, maxWait );
		}
	}
	if ( picoev_is_active( core->loop, dnsSocketFd ) ) {
		picoev_del( core->loop, dnsSocketFd );
	}
	return (cleanUp.good)? 1: 0;
}

int Core_AddModule( struct core_t * core, struct module_t * module ) {
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = 1;
	if ( module != NULL ) {
		if ( core->modules == NULL ) {
			core->modules = module;
		} else {
			PR_APPEND_LINK( &module->mLink, &core->modules->mLink );
		}
		if ( module->onLoad != NULL ) {
			cleanUp.good = ( module->onLoad( core, module, module->cbArgs ) ) ? 1 : 0;
		}
	}
	return ( cleanUp.good ) ? 1 : 0;
}

int Core_DelModule( struct core_t * core, struct module_t * module ) {
	struct module_t * moduleFirst, * moduleNext;
	PRCList * next;
	struct {unsigned char good:1;}cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = 1;
	moduleFirst = core->modules;
	if ( module == moduleFirst ){
		next = PR_NEXT_LINK( &module->mLink );
		if ( next  == &module->mLink ) {
			core->modules = NULL;
		} else {
			moduleNext = FROM_NEXT_TO_ITEM( struct module_t );
			core->modules = moduleNext;
		}
	}
	PR_REMOVE_AND_INIT_LINK( &module->mLink );
	if ( module->onUnload != NULL ) {
		cleanUp.good = ( module->onUnload( core, module, module->cbArgs ) ) ? 1 : 0;
	}
	Module_Delete( module ); module = NULL;
	return ( cleanUp.good ) ? 1 : 0;
}

struct timing_t * Core_AddTiming ( struct core_t * core , const unsigned int ms, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs ) {
	struct timing_t * timing;
	struct {unsigned char good:1;
			unsigned char timing:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) ) ;
	core->maxIdentifier++;
	cleanUp.good = ( (  timing = Timing_New( ms, core->maxIdentifier, repeat, timerHandler_cb, cbArgs ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.timing = 1;
		if ( core->timings == NULL ) {
			core->timings = timing;
		} else {
			PR_APPEND_LINK( &timing->mLink, &core->timings->mLink );
		}
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.timing ) {
				Timing_Delete( timing ); timing = NULL;
		}
	}

	return timing;
}

void Core_DelTiming( struct core_t * core, struct timing_t * timing ) {
	struct timing_t * timingFirst;
	PRCList * next;

	timingFirst = core->timings;
	if ( timing == timingFirst ){
		next = PR_NEXT_LINK( &timing->mLink );
		if ( next  == &timing->mLink ) {
			core->timings = NULL;
		} else {
			timingFirst = FROM_NEXT_TO_ITEM( struct timing_t );
			core->timings = timingFirst;
		}
	}
	PR_REMOVE_AND_INIT_LINK( &timing->mLink );
	Timing_Delete( timing ); timing = NULL;
}

void Core_DelTimingId ( struct core_t * core , uint32_t id ) {
	struct timing_t * timing, *firstTiming;
	PRCList * next;

	firstTiming = timing = core->timings;
	if ( firstTiming != NULL ) {
		do {
			next = PR_NEXT_LINK( &timing->mLink );
			if ( timing->identifier == id ) {
				Core_DelTiming( core, timing );
				break;
			}
			timing = FROM_NEXT_TO_ITEM( struct timing_t );
		} while ( timing != firstTiming );
	}
}

void Core_Delete( struct core_t * core ) {
	struct timing_t * firstTiming;
	struct module_t * firstModule;
	int dnsSocketFd;

	//  cleanup the modules
	firstModule = core->modules;
	while ( firstModule != NULL) {
		Core_DelModule( core, firstModule);
		firstModule = core->modules;
	}
	firstTiming = core->timings;
	while ( firstTiming != NULL ) {
		Core_DelTiming( core, firstTiming );
		firstTiming = core->timings;
	}
	//  cleanup dns
	dnsSocketFd = dns_get_fd( core->dns );
	if (picoev_is_active( core->loop, dnsSocketFd ) ) {
		picoev_del( core->loop, dnsSocketFd );
	}
	dns_fini( core->dns ); core->dns = NULL;
	//  cleanup the rest
	core->processTicksMs = 0;
	picoev_destroy_loop( core->loop );
	cfg_free( (cfg_t *) core->config ); core->config = NULL;
	free( core ); core = NULL;
}

