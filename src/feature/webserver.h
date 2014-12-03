#ifndef SRC_WEBSERVER_H_
#define SRC_WEBSERVER_H_

#include <time.h>

#include <h3.h>

#include "../core/core.h"

#define HTTP_BUFF_LENGTH 1024
#define WEBSERVER_TIMEOUT_SEC 10

enum Mode{
	MODE_Get,
	MODE_Post
};
enum ContentType{
	CONTENTTYPE_Buffer,
	CONTENTTYPE_File,
};

enum Connection{
	CONNECTION_Close,
	CONNECTION_KeepAlive
};

enum HttpCode {
	HTTPCODE_None				= 0,
	HTTPCODE_Ok					= 200,
	HTTPCODE_Forbidden			= 403,
	HTTPCODE_NotFound			= 404,
	HTTPCODE_Error				= 500,
};

enum MimeType {
	MIMETYPE_Html = 0,
	MIMETYPE_Txt,
	MIMETYPE_Css,
	MIMETYPE_Htm,
	MIMETYPE_Js,
	MIMETYPE_Gif,
	MIMETYPE_Jpg,
	MIMETYPE_Jpeg,
	MIMETYPE_Png,
	MIMETYPE_Ico,
	MIMETYPE_Zip,
	MIMETYPE_Gz,
	MIMETYPE_Tar,
	MIMETYPE_Xml,
	MIMETYPE_Svg,
	MIMETYPE_Json,
	MIMETYPE_Csv,
	__MIMETYPE_Last
};

struct Mime {
	enum MimeType				mime;
	const char *				ext;
	const char *				applicationString;
};

struct webserver {
	struct Core *				core;
	uint16_t					port;
	int							socketFd;
	int							timeout_sec;
	const char *				ip;

};

struct webclient{
	int							socketFd;
	enum Mode					mode;
	enum Connection				connection;
	struct webserver * 			webserver;
	RequestHeader *	 			header;
	char						buffer[HTTP_BUFF_LENGTH];
	struct {
			unsigned int			headersSent:1;
			unsigned int			contentSent:1;
			int						contentLength;
			enum HttpCode			httpCode;
			enum MimeType			mimeType;
			enum ContentType		contentType;
			char *	 				content;
			time_t					start;
			time_t					end;
								} response;
};

struct webclient * 				Webclient_New			( struct webserver * webserver, int socketFd);
void 							Webclient_Reset			( struct webclient * webclient );
void 							Webclient_Route			( struct webclient * webclient );
void 							Webclient_Delete		( struct webclient * webclient );

void							SetupSocket				( int fd );

struct webserver *				Webserver_New			( struct Core * core, const char * ip, const uint16_t port, const int timeout_sec );
void 							Webserver_JoinCore		( struct webserver * webserver );
void							Webserver_Delete		( struct webserver * webserver );

#endif  // SRC_WEBSERVER_H_

