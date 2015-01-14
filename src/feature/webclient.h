#ifndef SRC_FEATURE_WEBCLIENT_H_
#define SRC_FEATURE_WEBCLIENT_H_

#include <Uri.h>

#include "../core/core.h"

#include "webcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

struct webpage_t;
struct headers_t;

typedef void 						(*	webclientHandler_cb_t)	( const struct webpage_t * webpage );

struct webpage_t{
	char *								url;
	enum requestMode_t					mode;
	UriUriA								uri;
	struct PRCListStr					mLink;
	webclientHandler_cb_t				handlerCb;
	void *								cbArgs;
	clearFunc_cb_t						clearFuncCb;
	struct {
		struct buffer_t *					topLine;
		struct buffer_t *					headers;
		struct buffer_t *					content;
								}		request;
	struct {
		enum httpCode_t 					httpCode;
		struct buffer_t *					headers;
		struct buffer_t *					content;
								} 		response;
};

struct webclient_t {
	const struct core_t *				core;
	unsigned char						timeoutSec;
	int									socketFd;
	enum connection_t					connection;
	char *								ip;
	uint16_t							port;
	struct webpage_t *					webpages;
	struct webpage_t *					currentWebpage;
};

struct webpage_t * 						Webclient_Queue				( struct webclient_t * webclient, const enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb );
struct webclient_t * 					Webclient_New				( const struct core_t * core, const enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb , const unsigned char timeoutSec );
void									Webclient_Delete			( struct webclient_t * webclient );

#ifdef __cplusplus
}
#endif


#endif  // SRC_FEATURE_WEBCLIENT_H_