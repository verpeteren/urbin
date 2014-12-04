#ifndef SRC_WEBSERVER_H_
#define SRC_WEBSERVER_H_


#include <time.h>
#include <stdint.h>

#include <h3.h>

#include "../core/core.h"

#define HTTP_BUFF_LENGTH 1024
#define WEBSERVER_TIMEOUT_SEC 10

enum mode_t{
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

struct mimeDetail_t {
	enum mimeType_t				mime;
	const char *				ext;
	const char *				applicationString;
};

struct webserver_t {
	struct core_t *				core;
	uint16_t					port;
	int							socketFd;
	int							timeout_sec;
	const char *				ip;

};

struct webclient{
	int							socketFd;
	enum mode_t					mode;
	enum connection_t			connection;
	struct webserver_t * 		webserver;
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
								} response;
};

struct webclient * 				Webclient_New			( struct webserver_t * webserver, int socketFd);
void 							Webclient_Reset			( struct webclient * webclient );
void 							Webclient_Route			( struct webclient * webclient );
void 							Webclient_Delete		( struct webclient * webclient );

void							SetupSocket				( int fd );

struct webserver_t *			Webserver_New			( struct core_t * core, const char * ip, const uint16_t port, const int timeout_sec );
void 							Webserver_JoinCore		( struct webserver_t * webserver );
void							Webserver_Delete		( struct webserver_t * webserver );

#endif  // SRC_WEBSERVER_H_

