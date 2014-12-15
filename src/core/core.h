#ifndef SRC_CORE_CORE_H_
#define SRC_CORE_CORE_H_

#include <signal.h>
#include <stdint.h>
#include <time.h>

#include <picoev.h>
#include <tadns.h>
#include <prclist.h>
#include <prinrval.h>

#include "../common.h"
#include "configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void 				( * signalAction_cb_t )		( const int signal );
typedef int 				( * timerHandler_cb_t )		( void * cbArgs );
typedef void 				( * moduleHandler_cb_t )	( void * cbArgs );

#define LOG( core, level, ... ) do { Core_Log( (core), level, __VA_ARGS__ ); } while ( 0 );

#define FROM_NEXT_TO_ITEM( type )( (type *) ( ( (char * ) next ) - offsetof( type, mLink.next ) ) )

#define FEATURE_JOINCORE( Feat, feature ) \
void Feat##_JoinCore( struct feature##_t * feature ) { \
	picoev_add( feature->core->loop, feature->socketFd, PICOEV_READ, 0, Feat##_HandleAccept_cb, (void * ) feature ); \
}

struct timing_t;
struct timing_t {
	unsigned int				ms;
	uint32_t					identifier;
	timerHandler_cb_t 			timerHandler_cb;
	timerHandler_cb_t			clearFunc_cb;
	void *						cbArgs;
	PRUint32					due;
	unsigned char				repeat:1;
	PRCList						mLink;
};

struct module_t {
	const char * 				name;
	moduleHandler_cb_t			onLoad;
	moduleHandler_cb_t			onReady;
	moduleHandler_cb_t			onUnload;
	void *						cbArgs;
	PRCList						mLink;
};

struct core_t {
	picoev_loop *				loop;
	const cfg_t *				config;
	struct dns *				dns;
	struct module_t *			modules;
	struct timing_t *			timings;
	struct {
				int					logLevel;
				setlogmask_fun		logMask;
				syslog_fun			logFun;
							}	logger;
	uint32_t					maxIdentifier;
	unsigned char				processTicksMs;
	unsigned char				keepOnRunning:1;
};

void							Boot					( const int maxFds );
void							Shutdown				( );
void							SetupSocket				( const int fd );

struct module_t *				Module_New				( const char * name, moduleHandler_cb_t onLoad, const moduleHandler_cb_t onReady, const moduleHandler_cb_t onUnload, void * data ) ;
void 							Module_Delete			( struct module_t * module );
struct core_t *					Core_New				( const cfg_t * config );
void 							Core_Log				( const struct core_t * core, int logLevel, const char * fmt, ... );
void							Core_Loop				( struct core_t * core );
int 							Core_PrepareDaemon		( const struct core_t * core, const signalAction_cb_t signalHandler );
void							Core_AddModule			( struct core_t * core, struct module_t * module );
void							Core_DelModule			( struct core_t * core, struct module_t * module );
struct timing_t *				Core_AddTiming 			( struct core_t * core, const unsigned int ms, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs );
void 							Core_DelTimingId		( struct core_t * core, uint32_t id );
void 							Core_DelTiming 			( struct core_t * core, struct timing_t * timing );
void							Core_Delete				( struct core_t * core );

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_CORE_H_

