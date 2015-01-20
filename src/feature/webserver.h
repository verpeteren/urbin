#ifndef SRC_FEATURE_WEBSERVER_H_
#define SRC_FEATURE_WEBSERVER_H_

#include <time.h>
#include <stdint.h>

#include <oniguruma.h>

#include "../core/core.h"
#include "webcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

enum contentType_t{
	CONTENTTYPE_BUFFER,
	CONTENTTYPE_FILE,
};

//  @TODO:  use /etc/mime.types for this
enum mimeType_t {
	MIMETYPE_HTML 				 = 0,
	MIMETYPE_TXT,
	MIMETYPE_CSS,
	MIMETYPE_HTM,
	MIMETYPE_JS,
	MIMETYPE_GIF,
	MIMETYPE_JPG,
	MIMETYPE_JPEG,
	MIMETYPE_PNG,
	MIMETYPE_ICO,
	MIMETYPE_ZIP,
	MIMETYPE_GZ,
	MIMETYPE_TAR,
	MIMETYPE_XML,
	MIMETYPE_SVG,
	MIMETYPE_JSON,
	MIMETYPE_CSV,
	__MIMETYPE_LAST
};

struct webserverclient_t;

typedef void 				(* webserverHandler_cb_t)	( const struct webserverclient_t * webserverclient );

enum routeType_t {
	ROUTETYPE_DOCUMENTROOT,
	ROUTETYPE_DYNAMIC
};

struct route_t {
	enum routeType_t			routeType;
	const char * 				orgPattern;
	regex_t *					urlRegex;
	union {
		const char * 				documentRoot;
		webserverHandler_cb_t			handlerCb;
							}	details;
	void *						cbArgs;
	clearFunc_cb_t				clearFuncCb;
	struct PRCListStr			mLink;
};

struct mimeDetail_t {
	enum mimeType_t				mime;
	const char *				ext;
	const char *				applicationString;
};

struct webserver_t {
	const struct core_t *		core;
	struct route_t *			routes;
	OnigOptionType				regexOptions;
	uint16_t					port;
	int							socketFd;
	unsigned char				timeoutSec;
	const char *				hostName;

};

struct webserverclientresponse_t {
	time_t					start;
	time_t					end;
	enum httpCode_t			httpCode;
	enum mimeType_t			mimeType;
	enum contentType_t		contentType;
	union{
		struct {
			size_t					contentLength;
			const char *			fileName;
				}				file;
		struct {
			struct buffer_t *		buffer;
					}			dynamic;
			}				content;
};

struct webserverclient_t{
	int									socketFd;
	ssize_t								wroteBytes;
	enum sending_t						sendingNow;
	enum requestMode_t					mode;
	enum connection_t					connection;
	struct webserver_t *		 		webserver;
	struct route_t *					route;
	OnigRegion *						region;
	RequestHeader *	 					header;
	struct buffer_t *					buffer;
	struct webserverclientresponse_t	response;
	};

struct namedRegex_t {
	size_t 								numGroups;
	char ** 							kvPairs;
	const struct webserverclient_t * 	webserverclient;
};



unsigned char					Webserverclientresponse_SetContent	( struct webserverclientresponse_t * response, const char * content );
unsigned char					Webserverclientresponse_SetCode		( struct webserverclientresponse_t * response, const unsigned int code );
unsigned char					Webserverclientresponse_SetMime		( struct webserverclientresponse_t * response, const char * mimeString );
const char *					Webserverclient_GetUrl				( const struct webserverclient_t * webserverclient );
struct namedRegex_t * 			Webserverclient_GetNamedGroups		( struct webserverclient_t * webserverclient );
const char *					Webserverclient_GetIp				( const struct webserverclient_t * webserverclient );
int 							Webserver_DocumentRoot				( struct webserver_t * webserver, const char * pattern, const char * documentRoot );
int 							Webserver_DynamicHandler			( struct webserver_t * webserver, const char * pattern, const webserverHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb );

void 							NamedRegex_Delete					( struct namedRegex_t * namedRegex );

struct webserver_t *			Webserver_New						( const struct core_t * core, const char * ip, const uint16_t port, const unsigned char timeoutSec );
void 							Webserver_JoinCore					( struct webserver_t * webserver );
void							Webserver_Delete					( struct webserver_t * webserver );

#ifdef __cplusplus
}
#endif

#endif  // SRC_FEATURE_WEBSERVER_H_

