#ifndef SRC_CORE_CORE_H_
#define SRC_CORE_CORE_H_

#include <signal.h>
#include <stdint.h>
#include <time.h>

#include <picoev.h>
#include <tadns.h>
#include <prclist.h>
#include <prtypes.h>
#include <prinrval.h>

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

enum sending_t{
	SENDING_NONE 				 = 0,
	SENDING_TOPLINE,
	SENDING_HEADER,
	SENDING_FILE,
	SENDING_CONTENT
};

typedef struct _Core_t Core_t;
typedef struct _Module_t Module_t;

typedef void 				( * signalAction_cb_t )		( const int signal );
typedef PRStatus			( * timerHandler_cb_t )		( void * cbArgs );
typedef void 				( * clearFunc_cb_t )		( void * cbArgs );
typedef PRStatus			( * moduleHandler_cb_t )	( const Core_t * core, Module_t * module, void * cbArgs );

#define FROM_NEXT_TO_ITEM( type ) ( (type *) ( ( (char * ) next ) - offsetof( type, mLink.next ) ) )

typedef struct _Buffer_t{
	char * 								bytes;
	size_t								used;
	size_t								size;
} Buffer_t;

typedef struct _Timing_t {
	unsigned int				ms;
	uint32_t					identifier;
	timerHandler_cb_t 			timerHandler_cb;
	clearFunc_cb_t				clearFunc_cb;
	void *						cbArgs;
	PRUint32					due;
	unsigned char				repeat:1;
	PRCList						mLink;
} Timing_t;

struct _Module_t {
	const char * 				name;
	moduleHandler_cb_t			onLoad;
	moduleHandler_cb_t			onReady;
	moduleHandler_cb_t			onUnload;
	void *						cbArgs;
	void *						instance;
	clearFunc_cb_t				clearFunc_cb;
	PRCList						mLink;
};

struct _Core_t {
	picoev_loop *				loop;
	struct {
		struct dns *				dns;
		unsigned int				actives;
							}	dns;
	Module_t *			modules;
	Timing_t *			timings;
	struct {
				int					logLevel;
				setlogmask_fun		logMask;
				syslog_fun			logFun;
							}	logger;
	uint32_t					maxIdentifier; 
	unsigned char				processTicksMs;
	unsigned char				keepOnRunning:1;
	PRBool						isDaemon;
};


Buffer_t *						Buffer_New				( size_t initialSize );
#if 0
PRStatus						Buffer_Split			( Buffer_t * orgBuffer, Buffer_t * otherBuffer, size_t orgBufferSplitPos );
#endif
PRStatus						Buffer_Increase			( Buffer_t * buffer, size_t extraBytes );
Buffer_t * 						Buffer_NewText			( const char * text );
PRStatus						Buffer_Append			( Buffer_t * buffer, const char * bytes, size_t bytesLen );
PRStatus	 					Buffer_Reset			( Buffer_t * buffer, size_t minLen );
void 							Buffer_Delete			( Buffer_t * buffer );

void							Boot					( const int maxFds );
void							Shutdown				( );
void							SetupSocket				( const int fd, const unsigned char tcp );
int 							GetPriorityFromName		( const char * name );

char * 							DnsData_ToString		( const struct dns_cb_data * dnsData );

Module_t *						Module_New				( const char * name, moduleHandler_cb_t onLoad, const moduleHandler_cb_t onReady, const moduleHandler_cb_t onUnload, void * data, const clearFunc_cb_t clearFunc );
void 							Module_Delete			( Module_t * module );

Core_t *						Core_New				( const PRBool isDaemon );
void 							Core_Log				( const Core_t * core, const int logLevel, const char * fileName, const unsigned int lineNr, const char * message );
PRStatus						Core_PrepareDaemon		( const Core_t * core, const int maxFds, const signalAction_cb_t signalHandler, const char * runAsUser, const char * runAsGroup );

void 							Core_GetHostByName		( Core_t * core, const char * hostName, dns_callback_t onSuccess_cb, void * queryCbArgs );
PRStatus						Core_AddModule			( Core_t * core, Module_t * module );
PRStatus						Core_Loop				( Core_t * core, const int maxWait );
PRStatus						Core_DelModule			( Core_t * core, Module_t * module );

Timing_t *						Core_AddTiming 			( Core_t * core, const unsigned int ms, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs, const clearFunc_cb_t clearFunc_cb );
void 							Core_DelTimingId		( Core_t * core, uint32_t id );
void 							Core_DelTiming 			( Core_t * core, Timing_t * timing );
void							Core_Delete				( Core_t * core );

#ifdef DEBUG
void ShowLink( const PRCList * start, const char * label, const size_t count );
#endif

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_CORE_H_

