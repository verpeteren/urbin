#ifndef SRC_FEATURE_WEBSERVER_H_
#define SRC_FEATURE_WEBSERVER_H_


#include <time.h>
#include <stdint.h>

#include <h3.h>
#include <oniguruma.h>

#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_BUFF_LENGTH 1024

enum requestMode_t {
	MODE_GET,
	MODE_POST
};

enum contentType_t{
	CONTENTTYPE_BUFFER,
	CONTENTTYPE_FILE,
};

enum connection_t{
	CONNECTION_CLOSE,
	CONNECTION_KEEPALIVE
};

enum httpCode_t {
	HTTPCODE_NONE				 = 0,
	HTTPCODE_OK					 = 200,
	HTTPCODE_FORBIDDEN			 = 403,
	HTTPCODE_NOTFOUND			 = 404,
	HTTPCODE_ERROR				 = 500,
	__HTTPCODE_LAST				 = 999,
};

enum mimeType_t {
	MIMETYPE_HTML = 0,
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

typedef void 				(* dynamicHandler_cb_t)	( const struct webserverclient_t * webserverclient );
typedef void				(* payloadClear_cb_t)	( void * );

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
		dynamicHandler_cb_t			handlerCb;
							}	details;
	void *						cbArgs;
	payloadClear_cb_t			clearFunc_cb;
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
	OnigRegion *				region;
	OnigOptionType				regexOptions;
	uint16_t					port;
	int							socketFd;
	unsigned char				timeoutSec;
	const char *				ip;

};

struct webserverclientresponse_t {
	time_t					start;
	time_t					end;
	int						contentLength;
	enum contentType_t		contentType;
	enum httpCode_t			httpCode;
	enum mimeType_t			mimeType;
	unsigned char			headersSent:1;
	unsigned char			contentSent:1;
	char *	 				content;
};


struct webserverclient_t{
	int									socketFd;
	enum requestMode_t					mode;
	enum connection_t					connection;
	struct webserver_t *		 		webserver;
	struct route_t *					route;
	RequestHeader *	 					header;
	char								buffer[HTTP_BUFF_LENGTH];
	struct webserverclientresponse_t	response;
	};

unsigned char					Webserverclientresponse_SetContent	( struct webserverclientresponse_t * response, const char * content );
unsigned char					Webserverclientresponse_SetCode		( struct webserverclientresponse_t * response, const unsigned int code );
unsigned char					Webserverclientresponse_SetMime		( struct webserverclientresponse_t * response, const char * mimeString );
const char *					Webserverclient_GetUrl				( const struct webserverclient_t * webserverclient );
const char *					Webserverclient_GetIp				( const struct webserverclient_t * webserverclient );

int 							Webserver_DocumentRoot				( struct webserver_t * webserver, const char * pattern, const char * documentRoot );
int 							Webserver_DynamicHandler			( struct webserver_t * webserver, const char * pattern, const dynamicHandler_cb_t handlerCb, void * cbArgs, payloadClear_cb_t clearFunc_cb );

struct webserver_t *			Webserver_New						( const struct core_t * core, const char * ip, const uint16_t port, const unsigned char timeoutSec );
void 							Webserver_JoinCore					( struct webserver_t * webserver );
void							Webserver_Delete					( struct webserver_t * webserver );


#ifdef __cplusplus
}
#endif

#endif  // SRC_FEATURE_WEBSERVER_H_

