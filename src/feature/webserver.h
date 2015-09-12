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

typedef struct _Webserverclient_t Webserverclient_t;

typedef void 				(* webserverHandler_cb_t)	( const Webserverclient_t * webserverclient );

enum routeType_t {
	ROUTETYPE_DOCUMENTROOT,
	ROUTETYPE_DYNAMIC
};

typedef struct _Route_t {
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
} Route_t;

typedef struct _MimeDetail_t {
	enum mimeType_t				mime;
	const char *				ext;
	const char *				applicationString;
} MimeDetail_t;

typedef struct _Webserverclientresponse_t {
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
			Buffer_t *			buffer;
					}			dynamic;
			}				content;
} Webserverclientresponse_t;

typedef struct _Webserver_t {
	Core_t *					core;
	Route_t *					routes;
	OnigOptionType				regexOptions;
	uint16_t					port;
	int							socketFd;
	uint8_t						timeoutSec;
	const char *				hostName;
	uint8_t		 				listenBacklog;
} Webserver_t;

typedef struct _Webserverclient_t {
	int									socketFd;
	ssize_t								wroteBytes;
	enum sending_t						sendingNow;
	enum requestMode_t					mode;
	enum connection_t					connection;
	Webserver_t *		 				webserver;
	Route_t *							route;
	OnigRegion *						region;
	struct {
		RequestHeader *	 					header;
		Buffer_t *							buffer;
							}			request;
	Webserverclientresponse_t			response;
} Webserverclient_t;

typedef struct _NamedRegex_t {
	size_t 						numGroups;
	char ** 					kvPairs;
	const Webserverclient_t * 	webserverclient;
} NamedRegex_t;

PRStatus 						Webserverclientresponse_SetContent	( Webserverclientresponse_t * response, const char * content );
unsigned char					Webserverclientresponse_SetCode		( Webserverclientresponse_t * response, const unsigned int code );
unsigned char					Webserverclientresponse_SetMime		( Webserverclientresponse_t * response, const char * mimeString );
const char *					Webserverclient_GetUrl				( const Webserverclient_t * webserverclient );
NamedRegex_t * 					Webserverclient_GetNamedGroups		( Webserverclient_t * webserverclient );
const char *					Webserverclient_GetIp				( const Webserverclient_t * webserverclient );
PRStatus						Webserver_DocumentRoot				( Webserver_t * webserver, const char * pattern, const char * documentRoot );
PRStatus						Webserver_DynamicHandler			( Webserver_t * webserver, const char * pattern, const webserverHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb );

void 							NamedRegex_Delete					( NamedRegex_t * namedRegex );

Webserver_t *					Webserver_New						( const Core_t * core, const char * ip, const uint16_t port, const uint8_t timeoutSec, const uint8_t listenBacklog );
void 							Webserver_JoinCore					( Webserver_t * webserver );
void							Webserver_Delete					( Webserver_t * webserver );

#ifdef __cplusplus
}
#endif

#endif  // SRC_FEATURE_WEBSERVER_H_

