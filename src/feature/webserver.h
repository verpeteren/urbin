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
	HTTPCODE_NONE				= 0,
	HTTPCODE_OK					= 200,
	HTTPCODE_FORBIDDEN			= 403,
	HTTPCODE_NOTFOUND			= 404,
	HTTPCODE_ERROR				= 500,
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

struct webclient_t;

typedef void				(* dynamicHandler_cb_t)	( struct webclient_t * webclient );

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
	struct PRCListStr			mLink;
};

struct mimeDetail_t {
	enum mimeType_t				mime;
	const char *				ext;
	const char *				applicationString;
};

struct webserver_t {
	struct core_t *				core;
	struct route_t *			routes;
	OnigRegion *				region;
	OnigOptionType regexOptions;
	uint16_t					port;
	int							socketFd;
	unsigned char				timeoutSec;
	const char *				ip;

};

struct webclient_t{
	int							socketFd;
	enum requestMode_t			mode;
	enum connection_t			connection;
	struct webserver_t * 		webserver;
	struct route_t *			route;
	RequestHeader *	 			header;
	char						buffer[HTTP_BUFF_LENGTH];
	struct {
			unsigned int			headersSent:1;
			unsigned int			contentSent:1;
			int						contentLength;
			enum httpCode_t			httpCode;
			enum mimeType_t			mimeType;
			enum contentType_t		contentType;
			char *	 				content;
			time_t					start;
			time_t					end;
							}	response;
};

int 							Webserver_DocumentRoot	( struct webserver_t * webserver, const char * pattern, const char * documentRoot );
int 							Webserver_DynamicHandler( struct webserver_t * webserver, const char * pattern, dynamicHandler_cb_t handlerCb );

struct webserver_t *			Webserver_New			( struct core_t * core, const char * ip, const uint16_t port, const unsigned char timeoutSec );
void 							Webserver_JoinCore		( struct webserver_t * webserver );
void							Webserver_Delete		( struct webserver_t * webserver );


#ifdef __cplusplus
}
#endif

#endif  // SRC_FEATURE_WEBSERVER_H_

