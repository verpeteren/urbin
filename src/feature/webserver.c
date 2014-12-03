#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <math.h>


#include "webserver.h"

const char * MethodDefinitions[ ] = {
	"GET",
	"POST"
};

struct Mime MimeTypeDefinitions[] = {
	{ MIMETYPE_Html,				"html", "text/html" },
	{ MIMETYPE_Txt,					"txt", "text/plain" },
	{ MIMETYPE_Css,					"css", "text/css" },
	{ MIMETYPE_Htm,					"htm", "text/html" },
	{ MIMETYPE_Js,					"js", "application/javascript" },
	{ MIMETYPE_Gif,					"gif", "image/gif" },
	{ MIMETYPE_Jpg,					"jpg", "image/jpg" },
	{ MIMETYPE_Jpeg,				"jpeg", "image/jpeg"},
	{ MIMETYPE_Png,					"png", "image/png" },
	{ MIMETYPE_Ico,					"ico", "image/ico" },
	{ MIMETYPE_Zip,					"zip", "image/zip" },
	{ MIMETYPE_Gz,					"gz", "image/gz"  },
	{ MIMETYPE_Tar,					"tar", "image/tar" },
	{ MIMETYPE_Xml,					"xml", "application/xml  " },
	{ MIMETYPE_Svg,					"svg", "image/svg+xml" },
	{ MIMETYPE_Json,				"json", "application/json" },
	{ MIMETYPE_Csv,					"csv", " application/vnd.ms-excel" },
	{ __MIMETYPE_Last,				'\0', "application/octet-stream"}
};

//  http://stackoverflow.com/questions/4143000/find-the-string-length-of-an-int
#define STRING_LENGTH_OF_INT( value ) (ssize_t) ( value == 0 ? 1 : ( log10( value ) + 1 ) )
#define HTTP_SERVER_TEMPLATE "HTTP/1.1 %d OK\r\nContent-Length: %d\r\nConnection: %s\r\nContent-Type: %s\r\nDate: %s\r\nServer: %s/%s\r\n\r\n"
#define HTTP_SERVER_TEMPLATE_ARGS webclient->response.httpCode, \
									webclient->response.contentLength, \
									connectionString, \
									contentTypeString, \
									dateString, \
									PR_NAME, \
									PR_VERSION
#define HTTP_SERVER_TEMPLATE_SIZE (ssize_t) ( strlen( HTTP_SERVER_TEMPLATE ) + \
									( STRING_LENGTH_OF_INT( webclient->response.httpCode ) ) + \
									( STRING_LENGTH_OF_INT( webclient->response.contentLength ) ) + \
									strlen( connectionString ) + \
									strlen( contentTypeString ) + \
									strlen( dateString ) + \
									strlen( PR_NAME ) + \
									strlen( PR_VERSION ) \
									- ( 2 * 7 ) + 1 )

static void Webserver_HandleRead_cb( picoev_loop* loop, int fd, int events, void* cb_arg );
static void Webserver_HandleWrite_cb( picoev_loop* loop, int fd, int events, void* cb_arg );
static void Webserver_HandleAccept_cb( picoev_loop* loop, int fd, int events, void* cb_arg );

void SetupSocket( int fd ) {
	int on, r;

	on = 1;
	r = setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof( on ) );
	assert( r == 0 );
	r = fcntl( fd, F_SETFL, O_NONBLOCK );
	assert( r == 0 );
}

static void CloseConn( struct webclient * webclient ) {
	picoev_del( webclient->webserver->core->loop, webclient->socketFd );
	close( webclient->socketFd );
	Webclient_Delete( webclient );
}

struct webclient * Webclient_New( struct webserver * webserver, int socketFd) {
	struct webclient * webclient;
	struct {unsigned int good:1;
		unsigned int webclient:1;
		} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( webclient = malloc( sizeof( *webclient) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.webclient = 1;
		webclient->socketFd = socketFd;
		webclient->webserver = webserver;
		webclient->header = NULL;
		webclient->response.start = time( 0 );
		webclient->response.end = 0;
		webclient->response.mimeType = MIMETYPE_Html;
		webclient->response.headersSent = 0;
		webclient->response.contentSent = 0;
		webclient->connection = CONNECTION_Close;
		webclient->mode = MODE_Get;
		memset( webclient->buffer, '\0', HTTP_BUFF_LENGTH );
		webclient->response.httpCode = HTTPCODE_Error;
		webclient->response.contentLength = 0;
		webclient->response.content = NULL;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.webclient ){
			free( webclient ); webclient = NULL;
		}
	}

	return webclient;

}
void Webclient_Route( struct webclient * webclient ) {
	struct {unsigned int good:1;
			unsigned int h3:1;
			unsigned int content:1;} cleanUp;
	size_t i;
	HeaderField * field;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( webclient->header = h3_request_header_new( ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.h3 = 1;
	}
	cleanUp.good = ( ( h3_request_header_parse( webclient->header, webclient->buffer, strlen( webclient->buffer ) ) ) == 0  );
	if ( cleanUp.good ) {
		//printf ("%s", webclient->buffer);
//		fprintf( stdout, "Method: %.*s\n", webclient->header->RequestMethodLen, webclient->header->RequestMethod) ;
//	    fprintf( stdout, "Request-URI: %.*s\n", webclient->header->RequestURILen, webclient->header->RequestURI );
  //  	fprintf( stdout, "HTTP-Version: %.*s\n", webclient->header->HTTPVersionLen, webclient->header->HTTPVersion );
//		foreaderFieldList
		//char * conn;
		//conn = h3_get_connection( webclient->header->Fields );
		//h3_get_connection( webclient->header );
				//strndup( webclient->header->Connection, webclient->header->ConnectionLen );
		//printf("------------>%s\n", conn );


		if ( strncmp( webclient->header->RequestMethod, "POST", webclient->header->RequestMethodLen) == 0 ) {
			webclient->mode = MODE_Post;
		}
		for ( i = 0; i < webclient->header->HeaderSize; i++ ) {
			field = &webclient->header->Fields[i];
			if ( strncmp( field->FieldName, "Connection", field->FieldNameLen ) == 0 ) { //todo: RTFSpec! only if http1.1 yadayadyada...
				if ( strncmp( field->Value, "Keep-Alive", field->ValueLen ) == 0 ) {
					webclient->connection = CONNECTION_KeepAlive;
				} else 	if ( strncmp( field->Value, "close", field->ValueLen ) == 0 ) {
					webclient->connection = CONNECTION_Close;
				}
			}
		}
	}
	if ( cleanUp.good ) {
		webclient->response.httpCode = HTTPCODE_OK;
		webclient->response.mimeType = MIMETYPE_Html;
		cleanUp.good = ( ( webclient->response.content = strdup( "<html><body><h1>It works!</h1>" "\n"
																"<p>This is the default web page for this server.</p>" "\n"
																"<p>The web server software is running but no content has been added, yet.</p>" "\n"
																"</body></html>" "\n" ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.content = 1;
		webclient->response.contentLength = strlen( webclient->response.content );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.content ) {
			free( webclient->response.content ); webclient->response.content = NULL;
			webclient->response.contentLength = 0;
		}
		if ( cleanUp.h3 ) {
			h3_request_header_free( webclient->header );
		}
	}

}

void Webclient_Reset( struct webclient * webclient ) {
	if ( webclient->header )  {
		h3_request_header_free( webclient->header ); webclient->header = NULL;
	}
	memset( webclient->buffer, '\0', strlen( webclient->buffer ) );
	webclient->response.httpCode = HTTPCODE_Error;
	webclient->response.contentLength = 0;
	webclient->response.start = time( 0 );
	webclient->response.end = 0;
	webclient->response.mimeType = MIMETYPE_Html;
	webclient->response.headersSent = 0;
	webclient->response.contentSent = 0;
	webclient->connection = CONNECTION_Close;
	webclient->mode = MODE_Get;
	if ( webclient->response.content ) {
		free( webclient->response.content ); webclient->response.content = NULL;
	}
}
void Webclient_Delete( struct webclient * webclient ) {
	if ( webclient->header )  {
		h3_request_header_free( webclient->header );
	}
	webclient->response.httpCode = HTTPCODE_Error;
	webclient->response.contentLength = 0;
	webclient->response.start = 0;
	webclient->response.end = 0;
	webclient->response.headersSent = 0;
	webclient->response.contentSent = 0;
	webclient->response.mimeType = MIMETYPE_Html;
	webclient->connection = CONNECTION_Close;
	webclient->mode = MODE_Get;
	if ( webclient->response.content )  {
		free( webclient->response.content ); webclient->response.content = NULL;
	}

	free ( webclient ); webclient = NULL;
}

static void Webserver_HandleAccept_cb( picoev_loop* loop, int fd, int events, void* ws_arg ) {
	int newFd;
	struct webserver * webserver;
	struct webclient * webclient;

	webserver = (struct webserver *) ws_arg;
	newFd  = accept( fd, NULL, NULL );
	if (newFd != -1) {
		//printf( "connected: %d\n", newFd );
		SetupSocket( newFd );
		webclient  = Webclient_New( webserver, newFd );
		picoev_add( loop, newFd, PICOEV_READ, webserver->timeout_sec , Webserver_HandleRead_cb, (void *) webclient  );
	}
}

static void Webserver_HandleRead_cb( picoev_loop* loop, int fd, int events, void* wc_arg ) {
	struct webclient * webclient;
	ssize_t r;

	webclient = (struct webclient *) wc_arg;
	if ( ( events & PICOEV_TIMEOUT) != 0 ) {
		/* timeout */
		CloseConn( webclient );
	} else  {
		/* update timeout, and read */
		picoev_set_timeout( loop, fd, webclient->webserver->timeout_sec );
		r = read( fd, webclient->buffer, HTTP_BUFF_LENGTH );
		webclient->buffer[r] = '\0';
		switch ( r ) {
		case 0: /* connection closed by peer */
			CloseConn( webclient );
			break;
		case -1: /* error */
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /* try again later */
				break;
			} else { /* fatal error */
				CloseConn( webclient );
			}
			break;
		default: /* got some data, send back */
			picoev_del( loop, fd );
			Webclient_Route( webclient );
			picoev_add( loop, fd, PICOEV_WRITE, webclient->webserver->timeout_sec , Webserver_HandleWrite_cb, wc_arg );
			break;
		}
	}
}


static void Webserver_HandleWrite_cb( picoev_loop* loop, int fd, int events, void* wc_arg ) {
	struct webclient * webclient;
	int connClosed;

	webclient = (struct webclient *) wc_arg;
	connClosed = 0;
	if ( ( events & PICOEV_TIMEOUT) != 0 ) {
		/* timeout */
		CloseConn( webclient );
		connClosed = 1;
	} else  {
		/* update timeout, and write */

		picoev_set_timeout( loop, fd, webclient->webserver->timeout_sec );
		if ( ! webclient->response.headersSent ) {
			ssize_t headerLength, wroteHeader;
			struct tm tm;
			char headerBuffer[HTTP_BUFF_LENGTH];
			const char * contentTypeString;
			const char * connectionString;
			char dateString[30];

			webclient->response.end = time( 0 );	//todo
			contentTypeString = "text/html"; // mime.applicationString;
			contentTypeString  = MimeTypeDefinitions[webclient->response.mimeType].applicationString;
			connectionString = (webclient->connection == CONNECTION_Close ) ?  "Close" : "Keep-Alive";
			gmtime_r( &webclient->response.end , &tm);
			strftime( &dateString[0], 30, "%a, %d %b %Y %H:%M:%S %Z", &tm );
			headerLength = HTTP_SERVER_TEMPLATE_SIZE;
			headerLength = (ssize_t) snprintf( headerBuffer, headerLength, HTTP_SERVER_TEMPLATE, HTTP_SERVER_TEMPLATE_ARGS );
			wroteHeader = write( fd, headerBuffer, headerLength );
			switch ( wroteHeader ) {
			case 0: /* the other end cannot keep up */
				break;
			case -1: /* error */
				if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /* try again later */
					break;
				} else { /* fatal error */
					CloseConn( webclient );
					connClosed = 1;
				}
				break;
			default: /* got some data, send back */
				if ( headerLength !=  wroteHeader ) {
					CloseConn( webclient ); /* failed to send all data at once, close */
					connClosed = 1;
				} else {
					webclient->response.headersSent = 1;
				}
				break;
			}
		}
		if ( ! connClosed && webclient->response.headersSent ) {
			ssize_t wroteContent;

			if ( ! webclient->response.content ) {
				webclient->response.contentSent = 1;
			} else {
				wroteContent = write( fd, webclient->response.content, webclient->response.contentLength );
				switch ( wroteContent ) {
				case 0: /* the other end cannot keep up */
					break;
				case -1: /* error */
					if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /* try again later */
						break;
					} else { /* fatal error */
						CloseConn( webclient );
						connClosed = 1;
					}
					break;
				default: /* got some data, send back */
					if ( webclient->response.contentLength !=  wroteContent ) {
						CloseConn( webclient ); /* failed to send all data at once, close */
						connClosed = 1;
					} else {
						webclient->response.contentSent = 1;
					}
					break;
				}
			}
		}
		if ( ! connClosed && webclient->response.headersSent && webclient->response.contentSent ) {
			//listen again
			if ( webclient->connection == CONNECTION_KeepAlive) {
				picoev_del( loop, fd );
				Webclient_Reset( webclient );
				picoev_add( loop, fd, PICOEV_READ, webclient->webserver->timeout_sec , Webserver_HandleRead_cb, wc_arg );
			} else {
				CloseConn( webclient );
				connClosed = 1;
			}
		}
	}
}


struct webserver * Webserver_New( struct Core * core, const char * ip, const int port, const int timeout_sec ) {
	struct {unsigned int good:1;
		unsigned int ip:1;
		unsigned int socket:1;
		unsigned int webserver:1;} cleanUp;
	struct webserver * webserver;
	struct sockaddr_in listenAddr;
	int flag;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	flag = 1;
	cleanUp.good = ( ( webserver = malloc( sizeof( * webserver ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.webserver = 1;
		webserver->core = core;
		webserver->socketFd = 0;
		webserver->timeout_sec  = ( timeout_sec < 0 ) ? WEBSERVER_TIMEOUT_SEC: timeout_sec ;
		webserver->ip = strdup( ip );
	}
	if ( cleanUp.good ) {
		cleanUp.ip = 1;
		webserver->port = port;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( webserver->socketFd = socket( AF_INET, SOCK_STREAM, 0 ) ) != -1 );
	}
	if ( cleanUp.good ) {
		cleanUp.socket = 1;
		cleanUp.good = ( setsockopt( webserver->socketFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof( flag ) ) == 0 );
	}
	if ( cleanUp.good ) {
		listenAddr.sin_family = AF_INET;
		listenAddr.sin_port = htons( webserver->port );
		listenAddr.sin_addr.s_addr = inet_addr(  webserver->ip );
		cleanUp.good = ( bind( webserver->socketFd, (struct sockaddr*) &listenAddr, sizeof( listenAddr ) ) == 0 );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( listen( webserver->socketFd, LISTEN_BACKLOG ) == 0 );
	}
	if ( cleanUp.good ) {
		SetupSocket( webserver->socketFd  );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.socket ) {
			shutdown( webserver->socketFd, SHUT_RDWR);
			close( webserver->socketFd );
		}
		if ( cleanUp.ip ) {
			free( (char * ) webserver->ip ); ip = NULL;
		}
		if ( cleanUp.webserver ) {
			free( webserver ); webserver = NULL;
		}
	}

	return webserver;
}
void Webserver_JoinCore( struct webserver * webserver ) {
	picoev_add( webserver->core->loop, webserver->socketFd, PICOEV_READ, 0, Webserver_HandleAccept_cb, (void * ) webserver );
}

void Webserver_Delete( struct webserver * webserver ) {
	if ( picoev_is_active( webserver->core->loop, webserver->socketFd ) )  {
		picoev_del( webserver->core->loop, webserver->socketFd );
	}
	shutdown( webserver->socketFd, SHUT_RDWR);
	close( webserver->socketFd );
	webserver->socketFd = 0;
	free( (char *) webserver->ip ); webserver->ip = NULL;
	free( webserver );
}
