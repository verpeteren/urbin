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

struct core_t;
struct module_t;

typedef void 				( * signalAction_cb_t )		( const int signal );
typedef PRStatus			( * timerHandler_cb_t )		( void * cbArgs );
typedef void 				( * clearFunc_cb_t )		( void * cbArgs );
typedef PRStatus			( * moduleHandler_cb_t )	( const struct core_t * core, struct module_t * module, void * cbArgs );

#define FROM_NEXT_TO_ITEM( type ) ( (type *) ( ( (char * ) next ) - offsetof( type, mLink.next ) ) )

struct buffer_t{
	char * 								bytes;
	size_t								used;
	size_t								size;
};

struct timing_t;
struct timing_t {
	unsigned int				ms;
	uint32_t					identifier;
	timerHandler_cb_t 			timerHandler_cb;
	clearFunc_cb_t				clearFunc_cb;
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
	void *						instance;
	clearFunc_cb_t				clearFunc_cb;
	PRCList						mLink;
};

struct core_t {
	picoev_loop *				loop;
	struct {
		struct dns *				dns;
		unsigned int				actives;
							}	dns;
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
	PRBool						isDaemon;
};


struct buffer_t *				Buffer_New				( size_t initialSize );
#if 0
PRStatus						Buffer_Split			( struct buffer_t * orgBuffer, struct buffer_t * otherBuffer, size_t orgBufferSplitPos );
#endif
PRStatus						Buffer_Increase			( struct buffer_t * buffer, size_t extraBytes );
struct buffer_t * 				Buffer_NewText			( const char * text );
PRStatus						Buffer_Append			( struct buffer_t * buffer, const char * bytes, size_t bytesLen );
PRStatus	 					Buffer_Reset			( struct buffer_t * buffer, size_t minLen );
void 							Buffer_Delete			( struct buffer_t * buffer );

void							Boot					( const int maxFds );
void							Shutdown				( );
void							SetupSocket				( const int fd, const unsigned char tcp );
int 							GetPriorityFromName		( const char * name );

char * 							DnsData_ToString		( const struct dns_cb_data * dnsData );

struct module_t *				Module_New				( const char * name, moduleHandler_cb_t onLoad, const moduleHandler_cb_t onReady, const moduleHandler_cb_t onUnload, void * data, const clearFunc_cb_t clearFunc );
void 							Module_Delete			( struct module_t * module );

struct core_t *					Core_New				( const PRBool isDaemon );
void 							Core_Log				( const struct core_t * core, const int logLevel, const char * fileName, const unsigned int lineNr, const char * message );
PRStatus						Core_PrepareDaemon		( const struct core_t * core, const int maxFds, const signalAction_cb_t signalHandler, const char * runAsUser, const char * runAsGroup );

void 							Core_GetHostByName		( struct core_t * core, const char * hostName, dns_callback_t onSuccess_cb, void * queryCbArgs );
PRStatus						Core_AddModule			( struct core_t * core, struct module_t * module );
PRStatus						Core_Loop				( struct core_t * core, const int maxWait );
PRStatus						Core_DelModule			( struct core_t * core, struct module_t * module );

struct timing_t *				Core_AddTiming 			( struct core_t * core, const unsigned int ms, const unsigned int repeat, const timerHandler_cb_t timerHandler_cb, void * cbArgs, const clearFunc_cb_t clearFunc_cb );
void 							Core_DelTimingId		( struct core_t * core, uint32_t id );
void 							Core_DelTiming 			( struct core_t * core, struct timing_t * timing );
void							Core_Delete				( struct core_t * core );

#ifdef DEBUG
void ShowLink( const PRCList * start, const char * label, const size_t count );
#endif

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_CORE_H_

