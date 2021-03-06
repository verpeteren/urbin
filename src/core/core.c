#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>

#include <oniguruma.h>

#include "core.h"
#include "utils.h"

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

	fdMax = ( fds == 0 ) ? PR_CFG_CORE_MAX_FDS : fds;
	fprintf( stdout, "Starting with %d slots\n", fdMax );
	picoev_init( fdMax );
}

void Shutdown( ) {
	fprintf( stdout, "Shutdown\n" );
	picoev_deinit( );
	onig_end( );
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

int GetPriorityFromName( const char * name ) {
	int logLevel = PR_LOG_LEVEL_VALUE;
	CODE * priority;

	priority = &prioritynames[0];
	while ( priority->c_name != NULL ) {
		if ( strcmp( priority->c_name, name ) == 0 ) {
			logLevel = priority->c_val;
			break;
		}
		priority++;
	}

	return logLevel;
}

#ifdef DEBUG
void ShowLink( const PRCList * start, const char * label, const size_t count ) {
	PRCList * current;
	size_t i;
	current = (PRCList *) start;
	printf( "------------------------------------------------%s----------------------\n", label );
	for ( i = 0; i < count; i++ ) {
		printf( "%d\t%u\t%u\t%u\n", i, (unsigned int) current, (unsigned int) current->prev, (unsigned int) current->next );
		current = current->next;
	}
}
#endif
/*****************************************************************************/
/* Buffer                                                                   */
/*****************************************************************************/
Buffer_t * Buffer_New( size_t initialSize ) {
	Buffer_t * buffer;
	struct {unsigned char good:1;
			unsigned char buffer:1;
			unsigned char bytes:1;} cleanUp;
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( buffer = malloc( sizeof( *buffer ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.bytes = 1;
		buffer->used = 0;
		buffer->size = initialSize;
		cleanUp.good = ( ( buffer->bytes = calloc( initialSize, 1 ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.bytes = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.bytes ) {
			free( buffer->bytes ); buffer->bytes = NULL;
		}
		if ( cleanUp.buffer ) {
			buffer->used = 0;
			buffer->size = 0;
			free( buffer ); buffer = NULL;
		}
	}

	return buffer;
}

Buffer_t * Buffer_NewText( const char * text ) {
	Buffer_t * buffer;
	size_t initialSize = strlen( text );
	struct {unsigned char good:1;
			unsigned char buffer:1;
			unsigned char bytes:1;} cleanUp;
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( buffer = malloc( sizeof( *buffer ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.bytes = 1;
		buffer->used = 0;
		buffer->size = initialSize;
		cleanUp.good = ( ( buffer->bytes = Xstrdup( text ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.bytes = 1;
		buffer->used = strlen( text );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.bytes ) {
			free( buffer->bytes ); buffer->bytes = NULL;
		}
		if ( cleanUp.buffer ) {
			buffer->used = 0;
			buffer->size = 0;
			free( buffer ); buffer = NULL;
		}
	}

	return buffer;
}

#if 0
PRStatus Buffer_Split( Buffer_t * orgBuffer, buffer_t * otherBuffer, size_t orgBufferSplitPos ) {
	size_t rest;
	struct {unsigned char good:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	assert( orgBufferSplitPos < orgBuffer->size );
	assert( orgBufferSplitPos < orgBuffer->used );
	rest = orgBuffer->used - orgBufferSplitPos;
	if ( otherBuffer == NULL ) {
		//  there is no spoon
		cleanUp.good = ( ( Buffer_NewText( orgBuffer->bytes + orgBufferSplitPos ) ) != NULL );
	} else if ( rest > otherBuffer->size ) {
		//  it does not fit
		cleanUp.good = ( Buffer_Increase( otherBuffer, otherBuffer->size - rest ) == PR_SUCCESS );
		if ( cleanUp.good ) {
			otherBuffer->used = 0;
			Buffer_Append( otherBuffer, orgBuffer->bytes + orgBufferSplitPos, rest );
		}
	} else {
		// it does fit
		cleanUp.good = 1;
		otherBuffer->used = 0;
		Buffer_Append( otherBuffer, orgBuffer->bytes + orgBufferSplitPos, rest );
	}
	if ( cleanUp.good ) {
		memset( orgBuffer->bytes + orgBufferSplitPos, '\0', rest );
		orgBuffer->used = orgBufferSplitPos;
	}
	return ( cleanUp.good ) ? PR_SUCCESS: PR_FAILURE;
}
#endif

PRStatus Buffer_Append( Buffer_t * buffer, const char * bytes, size_t bytesLen ) {
	size_t i, newSize;
	char * pos, *newBytes;
	int fits;
	struct {unsigned char good:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	newSize = buffer->used + bytesLen;
	fits = ( buffer->size >= newSize + 1 );
	if ( fits ) {
		pos = &buffer->bytes[buffer->used];
		for ( i = buffer->used; i < newSize; i++ ) {
			*pos = *bytes;
			pos++;
			bytes++;
		}
		buffer->used = newSize;
		cleanUp.good = 1;
	} else {
		cleanUp.good = ( ( newBytes = realloc( buffer->bytes, newSize + 1 ) ) != NULL );
		if ( cleanUp.good ) {
			memcpy( newBytes + buffer->used, bytes, bytesLen );
			buffer->size = newSize;
			buffer->used += bytesLen;
			buffer->bytes = newBytes;
		} else {
			Buffer_Delete( buffer ); buffer = NULL;
		}
	}

	return ( cleanUp.good ) ? PR_SUCCESS: PR_FAILURE;
}

PRStatus Buffer_Increase( Buffer_t * buffer, size_t extraBytes ) {
	size_t newSize;
	char * newBytes;
	struct {unsigned char good:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	newSize = buffer->size + extraBytes;
	cleanUp.good = ( ( newBytes = realloc( buffer->bytes, newSize ) ) != NULL );
	if ( cleanUp.good ) {
		memset( newBytes + buffer->size,'\0', extraBytes );
		buffer->size = newSize;
		buffer->bytes = newBytes;
	} else {
		Buffer_Delete( buffer ); buffer = NULL;
	}

	return ( cleanUp.good ) ? PR_SUCCESS: PR_FAILURE;
}

PRStatus Buffer_Reset( Buffer_t * buffer, size_t minLen ) {
	struct {unsigned char good:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	if ( buffer->size <= minLen ) {
		memset( buffer->bytes, '\0', buffer->used );
		buffer->used = 0;
		cleanUp.good = 1;
	} else {
		free( buffer->bytes ); buffer->bytes = NULL;
		cleanUp.good = ( ( buffer->bytes = calloc( minLen, 1 ) ) != NULL );
	}
	if ( ! cleanUp.good ) {
		Buffer_Delete( buffer ); buffer = NULL;
	}

	return ( cleanUp.good ) ? PR_SUCCESS: PR_FAILURE;
}

void Buffer_Delete( Buffer_t * buffer ) {
	if ( buffer->bytes != NULL ) {
		free( buffer->bytes ); buffer->bytes = NULL;
	}
	buffer->used = 0;
	buffer->size = 0;
	free( buffer ); buffer = NULL;
}

/*****************************************************************************/
/* Modules                                                                   */
/*****************************************************************************/
Module_t * Module_New( const char * name, const moduleHandler_cb_t onLoad, const moduleHandler_cb_t onReady, const moduleHandler_cb_t onUnload, void * cbArgs, const clearFunc_cb_t clearFunc_cb ) {
	Module_t * module;
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
		module->clearFunc_cb = clearFunc_cb;
		PR_INIT_CLIST( &module->mLink );
		cleanUp.name = ( ( module->name = Xstrdup( name ) ) != NULL );
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
			if ( module->clearFunc_cb != NULL && module->cbArgs != NULL ) {
				module->clearFunc_cb( module->cbArgs );
			}
			module->clearFunc_cb = NULL;
			module->cbArgs = NULL;
			free( module ); module = NULL;
		}
	}

	return module;
}

void Module_Delete( Module_t * module ) {
	free( (char * ) module->name ); module->name = NULL;
	if ( module->clearFunc_cb != NULL && module->cbArgs != NULL ) {
		module->clearFunc_cb( module->cbArgs );
	}
	module->clearFunc_cb = NULL;
	module->cbArgs = NULL;
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
static Timing_t *			Timing_New 					( const unsigned int ms, const uint32_t identifier, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs, const clearFunc_cb_t clearFunc_cb );
static void 						Timer_CalculateDue			( Timing_t * timing, const PRUint32 nowOrHorizon );
static void 						Timing_Delete				( Timing_t * timing );

static Timing_t * Timing_New ( const unsigned int ms, const uint32_t identifier, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs, const clearFunc_cb_t clearFunc_cb ) {
	Timing_t * timing;
	PRUint32 horizon;
	PRIntervalTime now;
	struct {unsigned char good:1;
			unsigned char timing:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( timing = malloc( sizeof( *timing ) ) ) != NULL );
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
		timing->clearFunc_cb = clearFunc_cb;
		PR_INIT_CLIST( &timing->mLink );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.timing ) {
			if ( timing->clearFunc_cb != NULL && timing->cbArgs != NULL ) {
				timing->clearFunc_cb( timing->cbArgs );
			}
			timing->clearFunc_cb = NULL;
			timing->cbArgs = NULL;
			free( timing ); timing = NULL;
		}
	}

	return timing;
}
static void Timer_CalculateDue( Timing_t * timing, const PRUint32 nowOrHorizon ) {
	timing->due = ( nowOrHorizon + ( 1000 * timing->ms ) - 1 );  //  @FIXME:  this might overflow if ms is in the far future ( +6 hours )
}

static void Timing_Delete( Timing_t * timing ) {
	if ( timing->clearFunc_cb != NULL && timing->cbArgs != NULL ) {
		timing->clearFunc_cb( timing->cbArgs );
	}
	timing->clearFunc_cb = NULL;
	timing->cbArgs = NULL;
	timing->ms = 0;
	timing->repeat = 0;
	timing->identifier = 0;
	timing->timerHandler_cb = NULL;
	timing->clearFunc_cb = NULL;
	timing->cbArgs = NULL;
	PR_INIT_CLIST( &timing->mLink );
	free( timing ); timing = NULL;
}

/*****************************************************************************/
/* Core                                                                      */
/*****************************************************************************/
Core_t * Core_New( const PRBool isDaemon ) {
	Core_t * core;
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
		core->isDaemon = isDaemon;
		core->loop = NULL;
		core->timings = NULL;
		core->modules = NULL;
		core->dns.dns = NULL;
		core->dns.actives = 0;
		core->logger.logLevel = PR_LOG_LEVEL_VALUE;
		timeoutSec = PR_CFG_CORE_TIMEOUT_SEC;
		core->processTicksMs = PR_CFG_CORE_TICKS_MS;
		
		//  Set up the logging
		if ( isDaemon ) {
			 get_syslog_logger( &core->logger.logFun, 0, &core->logger.logMask );
		} else {
			//  we log to the stderr in interactive mode
			get_stderr_logger( &core->logger.logFun, 0, &core->logger.logMask );
		}
		core->logger.logLevel = PR_LOG_LEVEL_VALUE;
		core->logger.logMask( LOG_UPTO( core->logger.logLevel ) );
		cleanUp.good = ( ( core->loop = picoev_create_loop( timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.loop = 1;
		cleanUp.good = ( ( core->dns.dns = dns_init( ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.dns = 1;
		SetupSocket( dns_get_fd( core->dns.dns ), 0 );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.dns ) {
			dns_fini( core->dns.dns ); core->dns.dns = NULL;
			core->dns.actives = 0;
		}
		if ( cleanUp.loop ) {
			picoev_destroy_loop( core->loop );
			core->loop = NULL;
		}
		if ( cleanUp.core ) {
			core->modules = NULL;
			core->timings = NULL;
			core->isDaemon = PR_FALSE;
			free( core ); core = NULL;
		}
	}

	return core;
}

void Core_Log( const Core_t * core, const int logLevel, const char * fileName, const unsigned int lineNr, const char * message ) {
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

extern int setgroups( size_t __n, __const gid_t *__groups );
extern int initgroups( const char * user, gid_t group );

static PRStatus Core_SwitchToUser( const Core_t * core, const char* runAsUser, const char * runAsGroup ) {
	struct group *grp;
	struct passwd *pwd;
	struct {unsigned char good:1;
			unsigned char pwd:1;
			unsigned char grp:1;} cleanUp;
  //  @TODO:  windows
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	grp = NULL;
	pwd = NULL;
	/* Get the user information */
	if  ( strcmp( runAsUser, "none" ) != 0 ) {
		cleanUp.good = ( ( pwd = getpwnam( runAsUser ) ) != NULL );
		if ( cleanUp.good ) {
			cleanUp.good = ( pwd->pw_uid != 0 );
		}
		if ( cleanUp.good ) {
			cleanUp.good = ( setuid( pwd->pw_uid ) != -1 );
		}
		if ( cleanUp.good ) {
			cleanUp.pwd = 1;
			Core_Log( core, LOG_INFO, __FILE__ , __LINE__, "Switched user" );
		} else {
			Core_Log( core, LOG_WARNING, __FILE__ , __LINE__, "Could not switch user" );
		}
	}
	cleanUp.good = 0;

	/* Get the group information */
	if  ( strcmp( runAsGroup, "none" ) != 0 ) {
		cleanUp.good = ( ( grp = getgrnam( runAsGroup ) ) != NULL );
		if ( cleanUp.good ) {
			cleanUp.good = ( grp->gr_gid != 0 );
		}
		if ( cleanUp.good ) {
			cleanUp.good = ( setgid( grp->gr_gid ) != 1 );
		}
		if ( cleanUp.good ) {
			cleanUp.grp = 1;
			Core_Log( core, LOG_INFO, __FILE__ , __LINE__, "Switched group" );
		} else {
			Core_Log( core, LOG_WARNING, __FILE__ , __LINE__, "Could not switch group" );
		}
	}
	cleanUp.good = 0;
	/* set other information */
	cleanUp.good = ( setgroups( 0, NULL ) != -1 );
	if ( cleanUp.pwd && cleanUp.grp ) {
		cleanUp.good = ( initgroups( runAsUser, grp->gr_gid ) == 0 );
	}
	if ( getuid( ) == 0 ) {
		Core_Log( core, LOG_CRIT, __FILE__ , __LINE__, "Running as root!" );

		return PR_FAILURE;
	}

	return PR_SUCCESS;
}

PRStatus Core_PrepareDaemon( const Core_t * core , const int fds, const signalAction_cb_t signalHandler, const char * runAsUser, const char * runAsGroup ) {
	struct rlimit limit;
	struct {unsigned char good:1;
			unsigned char fds:1;
			unsigned char signal:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	limit.rlim_cur = ( rlim_t ) fds;
	limit.rlim_max = ( rlim_t ) fds;
	cleanUp.good = ( setrlimit( RLIMIT_NOFILE, &limit) != -1 );
	if ( cleanUp.good ) {
		cleanUp.fds = 1;
		cleanUp.signal = 1;
		signal( SIGUSR2, signalHandler );
		if ( core->isDaemon )  {
			if ( 0 != fork( ) ) {
				exit( 0 );
			}
			if ( -1 == setsid( ) ) {
				exit( 0 );
			}
			signal( SIGHUP, SIG_IGN );
			if ( 0 != fork( ) ) {
				exit( 0 );
			}
 			signal( SIGPIPE, SIG_IGN );
		}
	}
	if ( cleanUp.good ) {
		if ( getuid( ) == 0 ) {
			cleanUp.good = ( Core_SwitchToUser( core, runAsUser, runAsGroup ) == PR_SUCCESS ) ? 1 : 0;
		}
	}

	return ( cleanUp.good ) ? PR_SUCCESS: PR_FAILURE;
}

static void Core_ProcessTick( Core_t * core ) {
	Timing_t * timing, *firstTiming;
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
				timing->timerHandler_cb( timing->cbArgs );
				if ( timing->repeat ) {
					Timer_CalculateDue( timing, horizon );
				} else {
					Core_DelTiming( core, timing );
				}
			}
			timing = FROM_NEXT_TO_ITEM( Timing_t );
		} while ( timing != firstTiming );
	}
}

static void Dns_ReadWrite_cb( picoev_loop * loop, int fd, int events, void * cbArgs ) {
	Core_t * core;

	core = ( Core_t *) cbArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout, stop and retry again..... */
		core->dns.actives--;
	} else {
		//  sending and receiving shit
		picoev_set_timeout( loop, fd, DNS_QUERY_TIMEOUT );
		dns_poll( core->dns.dns );
		//  @TODO:  somehow every callbackHandler must decrement core->dns.actives, else the cpu load will rise through the roof.
	}
	if ( core->dns.actives < 1 ) {
		picoev_del( loop, fd );
	}
}

char * DnsData_ToString( const struct dns_cb_data * dnsData ) {
	char *cIp;
	char pos[4];
	size_t i, len;
	struct {unsigned char good:1;
			unsigned char ip:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cIp = NULL;
	i = 0;
	len = dnsData->addr_len;
	if ( dnsData->addr_len != 0 ) {
		for ( i = 0; i < dnsData->addr_len; i++ ) {
			len += STRING_LENGTH_OF_INT( dnsData->addr[i] );
		}
	 	cleanUp.good = ( ( cIp = (char *) calloc( len, 1 ) ) != NULL );
	}
	if ( cleanUp.good ) {
		if ( dnsData->addr_len == 4 ) {
			snprintf( cIp, len, "%u.%u.%u.%u", dnsData->addr[0], dnsData->addr[1], dnsData->addr[2], dnsData->addr[3] );
		} else {
			//  @TODO:  ipv6
			for ( i = 0; i < dnsData->addr_len; i++ ) {
				snprintf( &pos[0], 4, "%u", (unsigned int ) dnsData->addr[i] );
				strcat( cIp, &pos[0] );
				strcat( cIp, "." );
			};
			cIp[len] = '\0';
		}
	}

	return cIp;
}

void Core_GetHostByName( Core_t * core, const char * hostName, dns_callback_t onSuccess_cb, void * queryCbArgs ) {
	enum dns_query_type queryType;
	int dnsSocketFd;

	dnsSocketFd = dns_get_fd( core->dns.dns );
	queryType = DNS_A_RECORD;
	if ( ! picoev_is_active( core->loop, dnsSocketFd ) ) {
		picoev_add( core->loop, dnsSocketFd, PICOEV_READWRITE, DNS_QUERY_TIMEOUT, Dns_ReadWrite_cb, (void *) core );
	}
	core->dns.actives++;
	// even if it is a valid address, we still will give it to tadns
	dns_queue( core->dns.dns, queryCbArgs, hostName, queryType, onSuccess_cb );
}

PRStatus Core_Loop( Core_t * core, const int maxWait ) {
	Module_t * module, * firstModule;
	PRCList * next;
	int dnsSocketFd;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	dnsSocketFd = dns_get_fd( core->dns.dns );
	cleanUp.good = 1;
	firstModule = module = core->modules;
	if ( firstModule != NULL ) {
		do {
			next = PR_NEXT_LINK( &module->mLink );
			if ( module->onReady != NULL )  {
					cleanUp.good = ( module->onReady( core, module, module->cbArgs ) == PR_SUCCESS ) ? 1 : 0;
			}
			module = FROM_NEXT_TO_ITEM( Module_t );
		} while ( cleanUp.good && module != firstModule );
	}
	if ( cleanUp.good ) {
		Core_Log( core, LOG_INFO, __FILE__ , __LINE__, "Starting loop" );
		core->keepOnRunning = 1;
		while ( core->keepOnRunning == 1 )  {
			Core_ProcessTick( core );
			//printf( "%d\n", maxWait );
			picoev_loop_once( core->loop, maxWait );
		}
	}
	if ( picoev_is_active( core->loop, dnsSocketFd ) ) {
		picoev_del( core->loop, dnsSocketFd );
	}

	return ( cleanUp.good ) ? PR_SUCCESS: PR_FAILURE;
}

PRStatus Core_AddModule( Core_t * core, Module_t * module ) {
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = 1;
	if ( module != NULL ) {
		if ( core->modules == NULL ) {
			core->modules = module;
		} else {
			PR_INSERT_BEFORE( &module->mLink, &core->modules->mLink );
		}
		if ( module->onLoad != NULL ) {
			cleanUp.good = ( module->onLoad( core, module, module->cbArgs ) == PR_SUCCESS) ? 1 : 0;
		}
		Core_Log( core, LOG_INFO, __FILE__ , __LINE__, "New Module allocated" );
	}

	return ( cleanUp.good ) ? PR_SUCCESS: PR_FAILURE;
}

PRStatus Core_DelModule( Core_t * core, Module_t * module ) {
	Module_t * moduleNext;
	PRCList * next;
	struct {unsigned char good:1;}cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = 1;
	if ( PR_CLIST_IS_EMPTY( &module->mLink ) ) {
		core->modules = NULL;
	} else {
		next = PR_NEXT_LINK( &module->mLink );
		moduleNext = FROM_NEXT_TO_ITEM( Module_t );
		core->modules = moduleNext;
	}
	PR_REMOVE_AND_INIT_LINK( &module->mLink );
	if ( module->onUnload != NULL ) {
		cleanUp.good = ( module->onUnload( core, module, module->cbArgs ) == PR_SUCCESS ) ? 1 : 0;
	}
	Module_Delete( module ); module = NULL;
	Core_Log( core, LOG_INFO, __FILE__ , __LINE__, "Delete Module free-ed" );

	return ( cleanUp.good ) ? PR_SUCCESS: PR_FAILURE;
}

Timing_t * Core_AddTiming( Core_t * core , const unsigned int ms, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs, const clearFunc_cb_t clearFunc_cb ) {
	Timing_t * timing;
	struct {unsigned char good:1;
			unsigned char timing:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	timing = NULL;
	if ( ms > 0 ) {
		core->maxIdentifier++;
		cleanUp.good = ( (  timing = Timing_New( ms, core->maxIdentifier, repeat, timerHandler_cb, cbArgs, clearFunc_cb ) ) != NULL );
		if ( cleanUp.good ) {
			cleanUp.timing = 1;
			if ( core->timings == NULL ) {
				core->timings = timing;
			} else {
				PR_INSERT_BEFORE( &timing->mLink, &core->timings->mLink );
			}
			Core_Log( core, LOG_INFO, __FILE__ , __LINE__, "New Timing allocated" );
		}
		if ( ! cleanUp.good ) {
			if ( cleanUp.timing ) {
					Timing_Delete( timing ); timing = NULL;
			}
		}
	}

	return timing;
}

void Core_DelTiming( Core_t * core, Timing_t * timing ) {
	Timing_t * timingNext;
	PRCList * next;

	if ( PR_CLIST_IS_EMPTY( &timing->mLink ) ) {
		core->timings = NULL;
	} else {
		next = PR_NEXT_LINK( &timing->mLink );
		timingNext = FROM_NEXT_TO_ITEM( Timing_t );
		core->timings = timingNext;
	}
	PR_REMOVE_AND_INIT_LINK( &timing->mLink );
	Timing_Delete( timing ); timing = NULL;
}

void Core_DelTimingId( Core_t * core , uint32_t id ) {
	Timing_t * timing, *firstTiming;
	PRCList * next;

	firstTiming = timing = core->timings;
	if ( firstTiming != NULL ) {
		do {
			next = PR_NEXT_LINK( &timing->mLink );
			if ( timing->identifier == id ) {
				Core_DelTiming( core, timing );
				break;
			}
			timing = FROM_NEXT_TO_ITEM( Timing_t );
		} while ( timing != firstTiming );
	}
}

void Core_Delete( Core_t * core ) {
	Timing_t * firstTiming;
	Module_t * firstModule;
	int dnsSocketFd;

	//  cleanup the modules
	firstModule = core->modules;
	while ( firstModule != NULL ) {
		Core_DelModule( core, firstModule );
		firstModule = core->modules;
	}
	firstTiming = core->timings;
	while ( firstTiming != NULL ) {
		Core_DelTiming( core, firstTiming );
		firstTiming = core->timings;
	}
	//  cleanup dns
	dnsSocketFd = dns_get_fd( core->dns.dns );
	if ( picoev_is_active( core->loop, dnsSocketFd ) ) {
		picoev_del( core->loop, dnsSocketFd );
	}
	dns_fini( core->dns.dns ); core->dns.dns = NULL;
	core->dns.actives = 0;
	//  cleanup the rest
	core->isDaemon = PR_FALSE;
	core->processTicksMs = 0;
	picoev_destroy_loop( core->loop );
	free( core ); core = NULL;
}

