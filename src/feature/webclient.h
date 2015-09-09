#ifndef SRC_FEATURE_WEBCLIENT_H_
#define SRC_FEATURE_WEBCLIENT_H_

#include <Uri.h>

#include "../core/core.h"

#include "webcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

struct webpage_t;

typedef void 						(*	webclientHandler_cb_t)	( const struct webpage_t * webpage );

struct webpage_t{
	UriUriA								uri;
	webclientHandler_cb_t				handlerCb;
	clearFunc_cb_t						clearFuncCb;
	void *								cbArgs;
	ssize_t								wroteBytes;
	enum requestMode_t					mode;
	enum sending_t						sendingNow;
	char *								url;
	struct PRCListStr					mLink;
	struct {
		struct buffer_t *					topLine;
		struct buffer_t *					headers;
		struct buffer_t *					content;
								}		request;
	struct {
		RequestHeader *	 					header;
		enum httpCode_t 					httpCode;
		struct buffer_t *					buffer;
								} 		response;
};

struct webclient_t {
	struct core_t *						core;
	struct webpage_t *					webpages;
	struct webpage_t *					currentWebpage;
	int									socketFd;
	uint16_t							port;
	enum connection_t					connection;
	uint8_t								timeoutSec;
	char *								hostName;
};

struct webpage_t * 						Webclient_Queue				( struct webclient_t * webclient, const enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb );
struct webclient_t * 					Webclient_New				( const struct core_t * core, const enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb , const uint8_t timeoutSec );
void									Webclient_Delete			( struct webclient_t * webclient );

#ifdef __cplusplus
}
#endif


#endif  // SRC_FEATURE_WEBCLIENT_H_
