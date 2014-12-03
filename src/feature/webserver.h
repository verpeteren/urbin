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

enum Connection{
	CONNECTION_Close,
	CONNECTION_KeepAlive
};

typedef void 			     (* event_cb_t)			( picoev_loop* loop, int fd, int events, void* cb_arg );
struct webserver {
	struct Core *				core;
	int							port;
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
			int						httpCode;
			int						contentLength;
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

struct webserver *				Webserver_New			( struct Core * core, const char * ip, const int port, const int timeout_sec );
void 							Webserver_JoinCore		( struct webserver * webserver );
void							Webserver_Delete		( struct webserver * webserver );

#endif  // SRC_WEBSERVER_H_

