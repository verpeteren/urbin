#ifndef SRC_CORE_CORE_H_
#define SRC_CORE_CORE_H_

#include <signal.h>
#include <stdint.h>

#include <picoev.h>

#include "../common.h"
#include "configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void 				( * signalAction_cb_t )		( int );
typedef int 				( * timerHandler_cb_t )		( void * cbArgs );

#define FEATURE_JOINCORE( Feat, feature ) \
void Feat##_JoinCore( struct feature##_t * feature ) { \
	picoev_add( feature->core->loop, feature->socketFd, PICOEV_READ, 0, Feat##_HandleAccept_cb, (void * ) feature ); \
}
enum moduleEvent_t {
	MODULEEVENT_LOAD,
	MODULEEVENT_READY,
	MODULEEVENT_UNLOAD
};

struct timing_t {
	int							ms;
	uint32_t					identifier;
	timerHandler_cb_t 			timerHandler_cb;
	timerHandler_cb_t			clearFunc;
	void *						cbArg;
};

struct core_t {
	picoev_loop *				loop;
	cfg_t *						config;
	struct module_t *			modules;
	int							modulesCount;
	struct timing_t *			timers;
	unsigned int				keepOnRunning:1;
};

struct module_t {
	void *						( * module_load )		( struct core_t * core );
	void						( * module_ready )		( struct core_t * core, void * arg );
	void						( * module_unload )		( struct core_t * core, void * arg );
	void *						data;
};

void							Boot					( );
void							Shutdown				( );
void							SetupSocket				( int fd );

struct core_t *					Core_New				( struct module_t * modules, const int modulesCount, cfg_t * config);
void							Core_Loop				( struct core_t * core );
void							Core_FireEvent			( struct core_t * core, enum moduleEvent_t event );
int 							Core_PrepareDaemon		( struct core_t * core , signalAction_cb_t signalHandler );
struct timing_t *				Core_AddTimer 			( struct core_t * core , int ms, timerHandler_cb_t timerHandler_cb, void * cb_arg );
void 							Core_DelTimerId			( struct core_t * core , uint32_t id );
void 							Core_DelTimer 			( struct core_t * core , struct timing_t * timing );
void							Core_Delete				( struct core_t * core );

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_CORE_H_

