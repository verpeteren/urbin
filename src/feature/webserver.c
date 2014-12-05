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
	{ MIMETYPE_HTML,			"html",	"text/html" },
	{ MIMETYPE_TXT,				"txt",	"text/plain" },
	{ MIMETYPE_CSS,				"css",	"text/css" },
	{ MIMETYPE_HTM,				"htm",	"text/html" },
	{ MIMETYPE_JS,				"js",	"application/javascript" },
	{ MIMETYPE_GIF,				"gif",	"image/gif" },
	{ MIMETYPE_JPG,				"jpg",	"image/jpg" },
	{ MIMETYPE_JPEG,			"jpeg",	"image/jpeg"},
	{ MIMETYPE_PNG,				"png",	"image/png" },
	{ MIMETYPE_ICO,				"ico",	"image/ico" },
	{ MIMETYPE_ZIP,				"zip",	"image/zip" },
	{ MIMETYPE_GZ,				"gz",	"image/gz"  },
	{ MIMETYPE_TAR,				"tar",	"image/tar" },
	{ MIMETYPE_XML,				"xml",	"application/xml  " },
	{ MIMETYPE_SVG,				"svg",	"image/svg+xml" },
	{ MIMETYPE_JSON,			"json",	"application/json" },
	{ MIMETYPE_CSV,				"csv",	"application/vnd.ms-excel" },
	{ __MIMETYPE_LAST,			'\0',	"application/octet-stream"}
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

static void						SetupSocket				( int fd );

static struct route_t * 		Route_New				( const char * pattern, enum routeType_t routeType, void * details, const OnigOptionType regexOptions );
static void						Route_Delete				( struct route_t * route );

static struct webclient_t *		Webclient_New			( struct webserver_t * webserver, int socketFd);
static void						Webclient_PrepareRequest( struct webclient_t * webclient );
static void						Webclient_RenderRoute	( struct webclient_t * webclient );
static void 					Webclient_Delete		( struct webclient_t * webclient );

static void						Webserver_HandleRead_cb	( picoev_loop* loop, int fd, int events, void* cb_arg );
static void						Webserver_HandleWrite_cb( picoev_loop* loop, int fd, int events, void* cb_arg );
static void						Webserver_HandleAccept_cb( picoev_loop* loop, int fd, int events, void* cb_arg );
static void 					Webserver_FindRoute		( struct webserver_t * webserver, struct webclient_t * webclient );
static int						Webserver_RegisterRoute	( struct webserver_t * webserver, struct route_t * route );

static void SetupSocket( int fd ) {
	int on, r;

	on = 1;
	r = setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof( on ) );
	assert( r == 0 );
	r = fcntl( fd, F_SETFL, O_NONBLOCK );
	assert( r == 0 );
}

static void CloseConn( struct webclient_t * webclient ) {
	picoev_del( webclient->webserver->core->loop, webclient->socketFd );
	close( webclient->socketFd );
	Webclient_Delete( webclient );
}

static struct route_t * Route_New( const char * pattern, enum routeType_t routeType, void * details, const OnigOptionType regexOptions ) {
	struct route_t * route;
	OnigErrorInfo einfo;
	UChar* pat;
	struct { unsigned int good:1;
			unsigned int route:1;
			unsigned int onig:1;
			unsigned int documentRoot:1;
			unsigned int orgPattern:1;
			} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( route = malloc( sizeof( *route ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.route = 1;
		cleanUp.good = ( ( route->orgPattern = strdup( pattern ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.orgPattern = 1;
		pat = ( unsigned char * ) route->orgPattern;
		cleanUp.good = ( onig_new( &route->urlRegex, pat, pat + strlen( ( char * ) pat ), regexOptions, ONIG_ENCODING_ASCII, ONIG_SYNTAX_DEFAULT, &einfo ) == ONIG_NORMAL );
	}
	if ( cleanUp.good ) {
		cleanUp.onig = 1;
		route->routeType = routeType;
		switch( route->routeType ) {
			case ROUTETYPE_DOCUMENTROOT:
				cleanUp.good = ( ( route->details.documentRoot = strdup( details ) ) != NULL );
				if ( cleanUp.good ) {
					cleanUp.documentRoot = 1;
				}
				break;
			case ROUTETYPE_DYNAMIC:
				route->details.handler_cb = (dynamicHandler_cb_t) details;
				break;
			default:
				break;
		}
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.documentRoot ) {
			free( (char *) route->details.documentRoot ); route->details.documentRoot = NULL;
		}
		if ( cleanUp.onig ) {
			onig_free( route->urlRegex ); route->urlRegex = NULL;
		}
		if ( cleanUp.orgPattern ) {
			free( (char * ) route->orgPattern ); route->orgPattern = NULL;
		}
		if ( cleanUp.route ) {
			free( route ); route = NULL;
		}
	}
	return route;
}

int Webserver_DocumentRoot	( struct webserver_t * webserver, const char * pattern, const char * documentRoot ) {
	struct route_t * route;
	struct { unsigned int good:1;
			unsigned int route:1;
			unsigned int registered:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( route = Route_New( pattern, ROUTETYPE_DOCUMENTROOT, (void * ) documentRoot, webserver->regexOptions ) ) != NULL);
	if ( cleanUp.good ){
		cleanUp.route = 1;
		cleanUp.registered = ( ( Webserver_RegisterRoute( webserver, route ) ) == 1 );
	}
	if ( ! cleanUp.good ){
		if ( cleanUp.registered ) {
			//pass
		}
		if ( cleanUp.route ) {
			Route_Delete( route ); route = NULL;
		}
	}

	return ( cleanUp.good == 1 );
}

int Webserver_DynamicHandler( struct webserver_t * webserver, const char * pattern, dynamicHandler_cb_t handler_cb ) {
	struct route_t * route;
	struct { unsigned int good:1;
			unsigned int route:1;
			unsigned int registered:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( route = Route_New( pattern, ROUTETYPE_DYNAMIC, (void * ) handler_cb, webserver->regexOptions ) ) != NULL);
	if ( cleanUp.good ){
		cleanUp.route = 1;
		cleanUp.registered = ( ( Webserver_RegisterRoute( webserver, route ) ) == 1 );
	}
	if ( ! cleanUp.good ){
		if ( cleanUp.registered ) {
			//pass
		}
		if ( cleanUp.route ) {
			Route_Delete( route ); route = NULL;
		}
	}

	return ( cleanUp.good == 1 );
}

static void Route_Delete ( struct route_t * route ) {
	if ( route->routeType == ROUTETYPE_DOCUMENTROOT ) {
		free( (char *)route->details.documentRoot ); route->details.documentRoot = NULL;
	}
	onig_free( route->urlRegex ); route->urlRegex = NULL;
	free( (char *) route->orgPattern ); route->orgPattern = NULL;
	free( route ); route = NULL;
}

static struct webclient_t * Webclient_New( struct webserver_t * webserver, int socketFd) {
	struct webclient_t * webclient;
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
		webclient->route = NULL;
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
void WebClient_Render( struct webclient_t * webclient) {
		webclient->response.httpCode = HTTPCODE_OK;
		webclient->response.mimeType = MIMETYPE_Html;
		webclient->response.content = strdup( 	"<html><body><h1>It works!</h1>" "\n"
												"<p>This is the default web page for this server.</p>" "\n"
												"<p>The web server software is running but no content has been added, yet.</p>" "\n"
												"</body></html>" "\n" ) ) != NULL );
		webclient->response.contentLength = strlen( webclient->response.content );
}
*/

#define WEBCLIENT_RENDER( name, webpage ) \
static void Webclient_Render##name( struct webclient_t * webclient) { \
	struct {unsigned int good:1; \
			unsigned int content:1; } cleanUp; \
	\
	memset( &cleanUp, 0, sizeof( cleanUp ) ); \
	if ( webclient->response.contentType == CONTENTTYPE_FILE && webclient->response.content ) { \
		free( webclient->response.content ); webclient->response.content = NULL; \
	} \
	webclient->response.contentType = CONTENTTYPE_BUFFER; \
	webclient->response.mimeType = MIMETYPE_HTML; \
	cleanUp.good = ( ( webclient->response.content = strdup( webpage ) ) != NULL ); \
	if ( cleanUp.good ) { \
		cleanUp.content = 1; \
		webclient->response.contentLength = strlen( webclient->response.content ); \
	} \
	if  ( ! cleanUp.good ) { \
		if ( cleanUp.good ) { \
			cleanUp.content = 1; \
			free( webclient->response.content ); webclient->response.content = NULL; \
			webclient->response.contentLength = 0; \
		} \
	} \
}

WEBCLIENT_RENDER( Forbidden, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Forbidden</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBCLIENT_RENDER( NotFound, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Not found</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBCLIENT_RENDER( Error, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Internal server error</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )

static void Webclient_RenderRoute( struct webclient_t * webclient ) {
	struct route_t * route;

	route = webclient->route;
	if ( ! route ) {
		Webclient_RenderNotFound( webclient );
	} else {
		if ( route->routeType == ROUTETYPE_DYNAMIC ) {
			route->details.handler_cb( webclient );
		} else if ( route->routeType == ROUTETYPE_DOCUMENTROOT ) {
			const char * documentRoot;
			char * fullPath;
			char * requestedPath;
			size_t fullPathLength, pathLength;
			struct {unsigned int good:1;
					unsigned int fullPath:1;
					unsigned int requestedPath:1; } cleanUp;
			int exists;
			size_t j, len;
			struct stat fileStat;

			memset( &cleanUp, '\0', sizeof( cleanUp ) );
			fullPath = NULL;
			webclient->response.contentType = CONTENTTYPE_FILE;
			documentRoot = route->details.documentRoot;
			// @FIXME:  this!
			pathLength = webclient->webserver->region->end[1] - webclient->webserver->region->beg[1] + 1;
			cleanUp.good = ( ( requestedPath = malloc( pathLength + 1 ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.requestedPath = 1;
				snprintf( requestedPath, pathLength, "%s", &webclient->header->RequestURI[webclient->webserver->region->beg[1]] );
			}
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
						//  @TODO: this is the place where a module handler can step into the arena ( e.g. .js / .py / .php )
					}
				} else {
					webclient->response.httpCode = HTTPCODE_NOTFOUND;
				}
			}
			switch ( webclient->response.httpCode ) {
			case HTTPCODE_ERROR:
				Webclient_RenderError( webclient );
				break;
			case HTTPCODE_FORBIDDEN:
				Webclient_RenderForbidden( webclient );
				break;
			case HTTPCODE_NOTFOUND:
				Webclient_RenderNotFound( webclient );
				break;
			case HTTPCODE_OK:  //  ft
				//break;
			case HTTPCODE_NONE:  //  ft
				//break;
			default:
				break;
			}
			if ( cleanUp.requestedPath ) {
				//always clean up
				free( requestedPath );	requestedPath = NULL;
			}
			if ( ! cleanUp.good ) {
				if ( cleanUp.fullPath )  {
					free( fullPath ); fullPath = NULL;
				}
			}
		}
	}
}

static void Webclient_PrepareRequest( struct webclient_t * webclient ) {
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
			if ( strncmp( field->FieldName, "Connection", field->FieldNameLen ) == 0 ) { //  @TODO: RTFSpec! only if http1.1 yadayadyada...
				if ( strncmp( field->Value, "Keep-Alive", field->ValueLen ) == 0 ) {
					webclient->connection = CONNECTION_KEEPALIVE;
				} else 	if ( strncmp( field->Value, "close", field->ValueLen ) == 0 ) {
					webclient->connection = CONNECTION_CLOSE;
				}
			}
		}
	}
	if ( cleanUp.good ) {
		Webserver_FindRoute( webclient->webserver, webclient );
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

static void Webclient_Reset( struct webclient_t * webclient ) {
	if ( webclient->header )  {
		h3_request_header_free( webclient->header ); webclient->header = NULL;
	}
	memset( webclient->buffer, '\0', strlen( webclient->buffer ) );
	webclient->route = NULL;
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
	onig_region_free( webclient->webserver->region, 0 );
	if ( webclient->response.content ) {
		free( webclient->response.content ); webclient->response.content = NULL;
	}
}
static void Webclient_Delete( struct webclient_t * webclient ) {
	if ( webclient->header )  {
		h3_request_header_free( webclient->header );
	}
	webclient->route = NULL;
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
	struct webclient_t * webclient;

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
	struct webclient_t * webclient;
	ssize_t r;

	webclient = (struct webclient_t *) wc_arg;
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
			Webclient_PrepareRequest( webclient );
			picoev_add( loop, fd, PICOEV_WRITE, webclient->webserver->timeout_sec , Webserver_HandleWrite_cb, wc_arg );
			break;
		}
	}
}


static void Webserver_HandleWrite_cb( picoev_loop* loop, int fd, int events, void* wc_arg ) {
	struct webclient_t * webclient;
	int connClosed;

	webclient = (struct webclient_t *) wc_arg;
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
			webclient->response.end = time( 0 );
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
		unsigned int onig:1;
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
		webserver->route = NULL;
		webserver->timeout_sec  = ( timeout_sec < 0 ) ? WEBSERVER_TIMEOUT_SEC: timeout_sec;
		webserver->regexOptions = ONIG_OPTION_SINGLELINE | ONIG_OPTION_FIND_LONGEST | ONIG_OPTION_CAPTURE_GROUP;  //  | ONIG_OPTION_IGNORECASE | ONIG_OPTION_DEFAULT;
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
		cleanUp.good = ( ( webserver->region = onig_region_new( ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.onig = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.onig ) {
			onig_region_free( webserver->region, 1 ); webserver->region = NULL;
		}
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

static int Webserver_RegisterRoute( struct webserver_t * webserver, struct route_t * route ) {
	//  @TODO: add more routes, no deletion
	if ( webserver->route ) {
			Route_Delete( webserver->route );
	}
	webserver->route = route;

	return 1;
}

static void  Webserver_FindRoute( struct webserver_t * webserver, struct webclient_t * webclient ) {
	struct route_t * route;
	unsigned char * range, * end, * start;
	int r, found, len;
	char * url;
	struct {unsigned int good:1;
			unsigned int url; }cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webclient->route = NULL;
	len =  webclient->header->RequestURILen;
	cleanUp.good = ( ( url = malloc( len + 1 ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		snprintf( url, len + 1, "%s", webclient->header->RequestURI );
		start = ( unsigned char * ) url;
		end = start + strlen( url );
		range = end;
		//  @TODO: search more routes, see webserver_RegisterRoute
		route = webserver->route;
		if ( route ) {
			r = onig_search( route->urlRegex, ( unsigned char * ) url, end, start, range, webserver->region, webserver->regexOptions );
			found = ( r >= 0 );
			if ( found )  {
				webclient->route = webserver->route;
				//break;
			}
		}
	}
	if ( cleanUp.url ) {
		// allways clean up
		free( url ); url = NULL;
	}
	if ( ! cleanUp.good ) {
	}
}

void Webserver_Delete( struct webserver_t * webserver ) {
	if ( webserver->route ) {
		//  @TODO: delete more routes, see Webserver_RegisterRoute
		Route_Delete( webserver->route ); webserver->route = NULL;
	}
	onig_region_free( webserver->region, 1 ); webserver->region = NULL;
	webserver->regexOptions = 0;
	if ( picoev_is_active( webserver->core->loop, webserver->socketFd ) )  {
		picoev_del( webserver->core->loop, webserver->socketFd );
	}
	shutdown( webserver->socketFd, SHUT_RDWR);
	close( webserver->socketFd );
	webserver->socketFd = 0;
	free( (char *) webserver->ip ); webserver->ip = NULL;
	free( webserver );
}
