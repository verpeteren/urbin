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
#include <sys/stat.h>
#include <sys/sendfile.h>

#include "webserver.h"

const char * MethodDefinitions[ ] = {
	"GET",
	"POST"
};

struct mimeDetail_t MimeTypeDefinitions[] = {
	{ MIMETYPE_HTML,				"html", "text/html" },
	{ MIMETYPE_TXT,					"txt", "text/plain" },
	{ MIMETYPE_CSS,					"css", "text/css" },
	{ MIMETYPE_HTM,					"htm", "text/html" },
	{ MIMETYPE_JS,					"js", "application/javascript" },
	{ MIMETYPE_GIF,					"gif", "image/gif" },
	{ MIMETYPE_JPG,					"jpg", "image/jpg" },
	{ MIMETYPE_JPEG,				"jpeg", "image/jpeg"},
	{ MIMETYPE_PNG,					"png", "image/png" },
	{ MIMETYPE_ICO,					"ico", "image/ico" },
	{ MIMETYPE_ZIP,					"zip", "image/zip" },
	{ MIMETYPE_GZ,					"gz", "image/gz"  },
	{ MIMETYPE_TAR,					"tar", "image/tar" },
	{ MIMETYPE_XML,					"xml", "application/xml  " },
	{ MIMETYPE_SVG,					"svg", "image/svg+xml" },
	{ MIMETYPE_JSON,				"json", "application/json" },
	{ MIMETYPE_CSV,					"csv", " application/vnd.ms-excel" },
	{ __MIMETYPE_LAST,				'\0', "application/octet-stream"}
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
static void Webclient_RenderRoute( struct webclient * webclient );

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

struct webclient * Webclient_New( struct webserver_t * webserver, int socketFd) {
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
		webclient->response.mimeType = MIMETYPE_HTML;
		webclient->response.contentType = CONTENTTYPE_BUFFER;
		webclient->response.httpCode = HTTPCODE_OK;
		webclient->response.headersSent = 0;
		webclient->response.contentSent = 0;
		webclient->connection = CONNECTION_CLOSE;
		webclient->mode = MODE_GET;
		memset( webclient->buffer, '\0', HTTP_BUFF_LENGTH );
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
/*
void WebClient_Render( struct webclient * webclient) {
		webclient->response.httpCode = HTTPCODE_OK;
		webclient->response.mimeType = MIMETYPE_Html;
		webclient->response.content = strdup( 	"<html><body><h1>It works!</h1>" "\n"
												"<p>This is the default web page for this server.</p>" "\n"
												"<p>The web server software is running but no content has been added, yet.</p>" "\n"
												"</body></html>" "\n" ) ) != NULL );
		webclient->response.contentLength = strlen( webclient->response.content );
}
*/
static void Webclient_RenderRoute( struct webclient * webclient ) {
		const char * documentRoot;
		char * fullPath;
		const char * requestedPath;
		size_t fullPathLength, pathLength;
		struct {unsigned int good:1;
				unsigned int fullPath:1;
				unsigned int content:1;} cleanUp;
		int exists;
		size_t j, len;
		struct stat fileStat;

		memset( &cleanUp, '\0', sizeof( cleanUp ) );
		fullPath = NULL;
		webclient->response.contentType = CONTENTTYPE_FILE;
		documentRoot = "/var/www";
		requestedPath = "/index.html";
		pathLength = strlen( requestedPath );
		//  check that the file is not higher then the documentRoot ( ../../../../etc/passwd )
		for ( j = 0; j < pathLength - 1; j++ ) {
			if ( requestedPath[j] == '.' && requestedPath[ j + 1] == '.' ) {
				webclient->response.httpCode = HTTPCODE_FORBIDDEN;
				break;
			}
		}
		cleanUp.good = ( webclient->response.httpCode == HTTPCODE_OK );
		if ( cleanUp.good ) {
			fullPathLength = strlen( documentRoot ) + pathLength + 13;  //  13: that is  '/' + '/' + 'index.html' + '\0'
			cleanUp.good = ( ( fullPath = malloc( fullPathLength ) ) != NULL );   //  if all goes successfull, this is stored in ->content, wich is free'd normally

		}
		if ( cleanUp.good ) {
			cleanUp.fullPath = 1;
		}
		if ( cleanUp.good ) {
			cleanUp.fullPath = 1;
			snprintf( fullPath, fullPathLength, "%s/%s", documentRoot, requestedPath );
			exists = stat( fullPath, &fileStat );
			if ( exists == 0 ) {
				webclient->response.httpCode = HTTPCODE_OK;
				if ( S_ISDIR( fileStat.st_mode ) ) {
					//  if it is a dir, and has a index.html file that is readable, use that
					snprintf( fullPath, fullPathLength, "%s/%s/index.html", documentRoot, requestedPath );
					exists = stat( fullPath, &fileStat );
					if ( exists != 0 ) {
						//  @TODO: this is the place where a directory index handler can step into the arena
						webclient->response.httpCode = HTTPCODE_NOTFOUND;
					}
				}
				//  all looks ok, we have accecss to a file
				if ( webclient->response.httpCode == HTTPCODE_OK ) {
					webclient->response.contentLength = fileStat.st_size;
					webclient->response.content = fullPath;
					fullPathLength = strlen( fullPath );
					//  determine mimetype
					for ( j = 0; j < __MIMETYPE_LAST; j++ ) {
						len = strlen( MimeTypeDefinitions[j].ext );
						if ( strncmp( &fullPath[fullPathLength - len], MimeTypeDefinitions[j].ext, len ) == 0 ) {
							webclient->response.mimeType = MimeTypeDefinitions[j].mime;
							break;
						}
					}
					//  TODO: this is the place where a module handler can step into the arena ( e.g. .js / .py / .php )
				}
			} else {
				webclient->response.httpCode = HTTPCODE_NOTFOUND;
			}
		}
		switch ( webclient->response.httpCode ) {
		case HTTPCODE_ERROR:
			if ( webclient->response.contentType == CONTENTTYPE_FILE && webclient->response.content ) {
				free( webclient->response.content ); webclient->response.content = NULL;
			}
			webclient->response.contentType = CONTENTTYPE_BUFFER;
			webclient->response.mimeType = MIMETYPE_HTML;
			cleanUp.good  = ( ( webclient->response.content =  strdup( 	"<html><body><h1>Internal Server Error</h1>" "\n"
																		"</body></html>" "\n" ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.content = 1;
				webclient->response.contentLength = strlen( webclient->response.content );
			}
			break;
		case HTTPCODE_FORBIDDEN:
			if ( webclient->response.contentType == CONTENTTYPE_FILE && webclient->response.content ) {
				free( webclient->response.content ); webclient->response.content = NULL;
			}
			webclient->response.contentType = CONTENTTYPE_BUFFER;
			webclient->response.mimeType = MIMETYPE_HTML;
			cleanUp.good = ( ( webclient->response.content =  strdup( 	"<html><body><h1>Forbidden</h1>" "\n"
																			"</body></html>" "\n" ) ) != NULL );
			webclient->response.contentLength = strlen( webclient->response.content );
			if ( cleanUp.good ) {
				cleanUp.content = 1;
				webclient->response.contentLength = strlen( webclient->response.content );
			}
			break;
		case HTTPCODE_NOTFOUND:
			if ( webclient->response.contentType == CONTENTTYPE_FILE && webclient->response.content ) {
				free( webclient->response.content ); webclient->response.content = NULL;
			}
			webclient->response.contentType = CONTENTTYPE_BUFFER;
			webclient->response.mimeType = MIMETYPE_HTML;
			cleanUp.good = ( ( webclient->response.content = strdup( 	"<html><body><h1>Not Found</h1>" "\n"
																			"</body></html>" "\n" ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.content = 1;
				webclient->response.contentLength = strlen( webclient->response.content );
			}
			break;
		case HTTPCODE_OK:  //  ft
			//break;
		case HTTPCODE_NONE:  //  ft
			//break;
		default:
			break;
		}
		if ( ! cleanUp.good ) {
			if ( cleanUp.content )  {
				free( webclient->response.content ); webclient->response.content = NULL;
			}
			if ( cleanUp.fullPath )  {
				free( fullPath ); fullPath = NULL;
			}
		}
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
		if ( strncmp( webclient->header->RequestMethod, "POST", webclient->header->RequestMethodLen) == 0 ) {
			webclient->mode = MODE_POST;
		}
		for ( i = 0; i < webclient->header->HeaderSize; i++ ) {
			field = &webclient->header->Fields[i];
			if ( strncmp( field->FieldName, "Connection", field->FieldNameLen ) == 0 ) { //todo: RTFSpec! only if http1.1 yadayadyada...
				if ( strncmp( field->Value, "Keep-Alive", field->ValueLen ) == 0 ) {
					webclient->connection = CONNECTION_KEEPALIVE;
				} else 	if ( strncmp( field->Value, "close", field->ValueLen ) == 0 ) {
					webclient->connection = CONNECTION_CLOSE;
				}
			}
		}
	}
	if ( cleanUp.good ) {
		//Route found!
		Webclient_RenderRoute( webclient );
	}
	if ( cleanUp.good ) {
		cleanUp.content = ( webclient->response.contentLength > 0 );
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
	webclient->response.contentLength = 0;
	webclient->response.start = time( 0 );
	webclient->response.end = 0;
	webclient->response.httpCode = HTTPCODE_OK;
	webclient->response.mimeType = MIMETYPE_HTML;
	webclient->response.contentType = CONTENTTYPE_BUFFER;
	webclient->response.headersSent = 0;
	webclient->response.contentSent = 0;
	webclient->connection = CONNECTION_CLOSE;
	webclient->mode = MODE_GET;
	if ( webclient->response.content ) {
		free( webclient->response.content ); webclient->response.content = NULL;
	}
}
void Webclient_Delete( struct webclient * webclient ) {
	if ( webclient->header )  {
		h3_request_header_free( webclient->header );
	}
	webclient->response.httpCode = HTTPCODE_NONE;
	webclient->response.contentLength = 0;
	webclient->response.start = 0;
	webclient->response.end = 0;
	webclient->response.headersSent = 0;
	webclient->response.contentSent = 0;
	webclient->response.mimeType = MIMETYPE_HTML;
	webclient->connection = CONNECTION_CLOSE;
	webclient->response.contentType = CONTENTTYPE_BUFFER;
	webclient->mode = MODE_GET;
	if ( webclient->response.content )  {
		free( webclient->response.content ); webclient->response.content = NULL;
	}

	free ( webclient ); webclient = NULL;
}

static void Webserver_HandleAccept_cb( picoev_loop* loop, int fd, int events, void* ws_arg ) {
	int newFd;
	struct webserver_t * webserver;
	struct webclient * webclient;

	webserver = (struct webserver_t *) ws_arg;
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
/*			int flags;

			flags = MSG_DONTWAIT | MSG_NOSIGNAL;*/
			webclient->response.end = time( 0 );	//todo
			contentTypeString = "text/html"; // mime.applicationString;
			contentTypeString  = MimeTypeDefinitions[webclient->response.mimeType].applicationString;
			connectionString = (webclient->connection == CONNECTION_CLOSE ) ?  "Close" : "Keep-Alive";
			gmtime_r( &webclient->response.end , &tm);
			strftime( &dateString[0], 30, "%a, %d %b %Y %H:%M:%S %Z", &tm );
			headerLength = HTTP_SERVER_TEMPLATE_SIZE;
			headerLength = (ssize_t) snprintf( headerBuffer, headerLength, HTTP_SERVER_TEMPLATE, HTTP_SERVER_TEMPLATE_ARGS );
			wroteHeader = write( fd, headerBuffer, headerLength /*, flags | MSG_MORE*/ );
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
			if ( webclient->response.contentLength == 0 ) {
				webclient->response.contentSent = 1;
			} else {
				int fileHandle;
				ssize_t wroteContent;
				struct {unsigned int good:1;
						unsigned int fileHandle:1;
				} cleanUp;

				memset( &cleanUp, 0, sizeof( cleanUp ) );
				wroteContent = 0;
				switch( webclient->response.contentType ){
				case  CONTENTTYPE_FILE:
					cleanUp.good = ( ( fileHandle = open( webclient->response.content, O_RDONLY | O_NONBLOCK ) ) > 0 );
					if ( cleanUp.good ) {
						cleanUp.fileHandle = 1;
						wroteContent = sendfile( fd, fileHandle, 0, webclient->response.contentLength );
					}
					if ( cleanUp.fileHandle ) {
						close( fileHandle );
					}
					break;
				default: // FT
				case  CONTENTTYPE_BUFFER:
					wroteContent = write( fd, webclient->response.content, webclient->response.contentLength /*, flags*/ );
				break;
				}
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
			if ( webclient->connection == CONNECTION_KEEPALIVE) {
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


struct webserver_t * Webserver_New( struct core_t * core, const char * ip, const uint16_t port, const int timeout_sec ) {
	struct {unsigned int good:1;
		unsigned int ip:1;
		unsigned int socket:1;
		unsigned int webserver:1;} cleanUp;
	struct webserver_t * webserver;
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
void Webserver_JoinCore( struct webserver_t * webserver ) {
	picoev_add( webserver->core->loop, webserver->socketFd, PICOEV_READ, 0, Webserver_HandleAccept_cb, (void * ) webserver );
}

void Webserver_Delete( struct webserver_t * webserver ) {
	if ( picoev_is_active( webserver->core->loop, webserver->socketFd ) )  {
		picoev_del( webserver->core->loop, webserver->socketFd );
	}
	shutdown( webserver->socketFd, SHUT_RDWR);
	close( webserver->socketFd );
	webserver->socketFd = 0;
	free( (char *) webserver->ip ); webserver->ip = NULL;
	free( webserver );
}
