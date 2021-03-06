#ifndef SRC_FEATURE_WEBCLIENT_H_
#define SRC_FEATURE_WEBCLIENT_H_

#include <Uri.h>

#include "../core/core.h"

#include "webcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Webpage_t Webpage_t;

typedef void 						(*	webclientHandler_cb_t)	( const Webpage_t * webpage );

struct _Webpage_t{
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
		Buffer_t *						topLine;
		Buffer_t *						headers;
		Buffer_t *						content;
								}		request;
	struct {
		RequestHeader *	 					header;
		enum httpCode_t 					httpCode;
		Buffer_t *							buffer;
								} 		response;
};

typedef struct _Webclient_t {
	Core_t *							core;
	Webpage_t *							webpages;
	Webpage_t *							currentWebpage;
	int									socketFd;
	uint16_t							port;
	enum connection_t					connection;
	uint8_t								timeoutSec;
	char *								hostName;
} Webclient_t;

Webpage_t * 							Webclient_Queue				( Webclient_t * webclient, const enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb );
Webclient_t * 							Webclient_New				( const Core_t * core, const enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb , const uint8_t timeoutSec );
void									Webclient_Delete			( Webclient_t * webclient );

#ifdef __cplusplus
}
#endif


#endif  // SRC_FEATURE_WEBCLIENT_H_
