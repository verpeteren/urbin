#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "webserver.h"
#include "../core/utils.h"


/*****************************************************************************/
/* Module                                                                    */
/*****************************************************************************/

/*****************************************************************************/
/* Static etc                                                                */
/*****************************************************************************/
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
	{ MIMETYPE_GZ,				"gz",	"image/gz"},
	{ MIMETYPE_TAR,				"tar",	"image/tar" },
	{ MIMETYPE_XML,				"xml",	"application/xml" },
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
#define HTTP_SERVER_TEMPLATE_SIZE	(ssize_t) ( strlen( HTTP_SERVER_TEMPLATE ) + \
								( STRING_LENGTH_OF_INT( webclient->response.httpCode ) ) + \
								( STRING_LENGTH_OF_INT( webclient->response.contentLength ) ) + \
								strlen( connectionString ) + \
								strlen( contentTypeString ) + \
								strlen( dateString ) + \
								strlen( PR_NAME ) + \
								strlen( PR_VERSION ) \
								- ( 2 * 7 ) + 1 )

static struct route_t * 		Route_New				( const char * pattern, const enum routeType_t routeType, void * details, const OnigOptionType regexOptions, void * cbArgs );
static void						Route_Delete			( struct route_t * route );

static void						Webserver_HandleRead_cb	( picoev_loop * loop, int fd, int events, void * cbArgs );
static void						Webserver_HandleWrite_cb( picoev_loop * loop, int fd, int events, void * cbArgs );
static void						Webserver_HandleAccept_cb( picoev_loop * loop, int fd, int events, void * cbArgs );
static void 					Webserver_FindRoute		( struct webserver_t * webserver, struct webclient_t * webclient );
static void						Webserver_AddRoute		( struct webserver_t * webserver, struct route_t * route );
static void						Webserver_DelRoute		( struct webserver_t * webserver, struct route_t * route );

static struct webclient_t *		Webclient_New			( struct webserver_t * webserver, int socketFd);
static void						Webclient_PrepareRequest( struct webclient_t * webclient );
static void						Webclient_RenderRoute	( struct webclient_t * webclient );
static void 					Webclient_CloseConn		( struct webclient_t * webclient );
static void 					Webclient_Delete		( struct webclient_t * webclient );


/*****************************************************************************/
/* Route                                                                    */
/*****************************************************************************/
static struct route_t * Route_New( const char * pattern, enum routeType_t routeType, void * details, const OnigOptionType regexOptions, void * cbArgs ) {
	struct route_t * route;
	OnigErrorInfo einfo;
	UChar * pat;
	char * lastChar;
	struct { unsigned char good:1;
			unsigned char route:1;
			unsigned char onig:1;
			unsigned char documentRoot:1;
			unsigned char orgPattern:1;
			} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( route = malloc( sizeof( *route ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.route = 1;
		route->cbArgs = cbArgs;
		cleanUp.good = ( ( route->orgPattern = Xstrdup( pattern ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.orgPattern = 1;
		pat = ( unsigned char * ) route->orgPattern;
		cleanUp.good = ( onig_new( &route->urlRegex, pat, pat + strlen( ( char * ) pat ), regexOptions, ONIG_ENCODING_ASCII, ONIG_SYNTAX_DEFAULT, &einfo ) == ONIG_NORMAL );
	}
	if ( cleanUp.good ) {
		cleanUp.onig = 1;
		route->routeType = routeType;
		PR_INIT_CLIST( &route->mLink );
		switch ( route->routeType ) {
			case ROUTETYPE_DOCUMENTROOT:
				cleanUp.good = ( ( route->details.documentRoot = Xstrdup( (char*) details ) ) != NULL );
				if ( cleanUp.good ) {
					cleanUp.documentRoot = 1;
					lastChar = (char *) &route->details.documentRoot[ strlen( route->details.documentRoot ) - 1];
					if ( '/' == *lastChar ) {
						*lastChar = '\0';
					}
				}
				break;
			case ROUTETYPE_DYNAMIC:
				route->details.handlerCb = (dynamicHandler_cb_t) details;
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
			route->cbArgs = NULL;
			free( route ); route = NULL;
		}
	}
	return route;
}

static void Route_Delete( struct route_t * route ) {
	if ( route->routeType == ROUTETYPE_DOCUMENTROOT ) {
		free( (char *) route->details.documentRoot ); route->details.documentRoot = NULL;
	}
	onig_free( route->urlRegex ); route->urlRegex = NULL;
	free( (char *) route->orgPattern ); route->orgPattern = NULL;
	route->cbArgs = NULL;
	free( route ); route = NULL;
}

/*****************************************************************************/
/* Webclient                                                                 */
/*****************************************************************************/
static struct webclient_t * Webclient_New( struct webserver_t * webserver, int socketFd) {
	struct webclient_t * webclient;
	struct {unsigned char good:1;
			unsigned char webclient:1; } cleanUp;

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
		if ( cleanUp.webclient ) {
			free( webclient ); webclient = NULL;
		}
	}

	return webclient;
}
const char * Webclient_GetUrl( const struct webclient_t * webclient ) {
	char * url;
	int len;
	struct {unsigned char good:1;
			unsigned char url:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	len = webclient->header->RequestURILen;
	cleanUp.good = ( ( url = malloc( len + 1 ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		snprintf( url, len + 1, "%s", webclient->header->RequestURI );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.url ) {
			free( url ); url = NULL;
		}
	}
	return url;
}

const char * Webclient_GetIp( const struct webclient_t * webclient ) {
	char * ip;
	struct sockaddr_storage addr;
	socklen_t len;
	struct {unsigned char good:1;
			unsigned char ip:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( ip = malloc( INET6_ADDRSTRLEN ) ) != NULL );
	if ( cleanUp.good ) {
		len = sizeof addr;
		getpeername( webclient->socketFd, (struct sockaddr *) &addr, &len );
		if ( AF_INET == addr.ss_family ) {
			struct sockaddr_in *s = (struct sockaddr_in *) &addr;
			inet_ntop( AF_INET, &s->sin_addr, ip, INET6_ADDRSTRLEN );
		} else { // AF_INET6
			struct sockaddr_in6 *s = (struct sockaddr_in6 *) &addr;
			inet_ntop( AF_INET6, &s->sin6_addr, ip, INET6_ADDRSTRLEN );
		}
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.ip ) {
			free( ip ); ip = NULL;
		}
	}
	return ip;
}

#define WEBCLIENT_RENDER( name, webpage ) \
static void Webclient_Render##name( struct webclient_t * webclient) { \
	struct {unsigned char good:1; \
			unsigned char content:1; } cleanUp; \
	\
	memset( &cleanUp, 0, sizeof( cleanUp ) ); \
	if ( webclient->response.contentType == CONTENTTYPE_FILE && webclient->response.content ) { \
		free( webclient->response.content ); webclient->response.content = NULL; \
	} \
	webclient->response.contentType = CONTENTTYPE_BUFFER; \
	webclient->response.mimeType = MIMETYPE_HTML; \
	cleanUp.good = ( ( webclient->response.content = Xstrdup( webpage ) ) != NULL ); \
	if ( cleanUp.good ) { \
		cleanUp.content = 1; \
		webclient->response.contentLength = strlen( webclient->response.content ); \
	} \
	if ( ! cleanUp.good ) { \
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
	if ( route == NULL ) {
		Webclient_RenderNotFound( webclient );
	} else {
		if ( route->routeType == ROUTETYPE_DYNAMIC ) {
			route->details.handlerCb( route->cbArgs );
		} else if ( route->routeType == ROUTETYPE_DOCUMENTROOT ) {
			struct stat fileStat;
			const char * documentRoot;
			char * fullPath;
			char * requestedPath;
			size_t fullPathLength, pathLength;
			int exists;
			size_t j, len;
			struct {unsigned char good:1;
					unsigned char fullPath:1;
					unsigned char requestedPath:1; } cleanUp;

			memset( &cleanUp, '\0', sizeof( cleanUp ) );
			fullPath = NULL;
			webclient->response.contentType = CONTENTTYPE_FILE;
			documentRoot = route->details.documentRoot;
			pathLength = webclient->webserver->region->end[1] - webclient->webserver->region->beg[1] + 1;
			cleanUp.good = ( ( requestedPath = malloc( pathLength + 1 ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.requestedPath = 1;
				snprintf( requestedPath, pathLength, "%s", &webclient->header->RequestURI[webclient->webserver->region->beg[1]] );
			}
			//  check that the file is not higher then the documentRoot. Such as  ../../../../etc/passwd
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
				FullPath( fullPath, fullPathLength ,documentRoot, requestedPath );
				exists = stat( fullPath, &fileStat );
				if ( exists == 0 ) {
					webclient->response.httpCode = HTTPCODE_OK;
					if ( S_ISDIR( fileStat.st_mode ) ) {
						//  if it is a dir, and has a index.html file that is readable, use that
						snprintf( fullPath, fullPathLength, "%s/%s/index.html", documentRoot, requestedPath );
						exists = stat( fullPath, &fileStat );
						if ( exists != 0 ) {
							//  this is the place where a directory index handler can step into the arena;
							//  but we will not do that, because: https://github.com/monkey/monkey/blob/master/plugins/dirlisting/dirlisting.c#L153
							webclient->response.httpCode = HTTPCODE_NOTFOUND;
						}
					}
					//  all looks ok, we have access to a file
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
						//  this is the place where a module handler can step into the arena. e.g. .js / .py / .php
						//  but we will not do this for spidermonkey, because we load that upon startup only once
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
				//  break;
			case HTTPCODE_NONE:  //  ft
				//  break;
			default:
				break;
			}
			if ( cleanUp.requestedPath ) {
				//  always clean up
				free( requestedPath );	requestedPath = NULL;
			}
			if ( ! cleanUp.good ) {
				if ( cleanUp.fullPath ) {
					free( fullPath ); fullPath = NULL;
				}
			}
		}
	}
}

static void Webclient_PrepareRequest( struct webclient_t * webclient ) {
	HeaderField * field;
	size_t i;
	struct {unsigned char good:1;
			unsigned char h3:1;
			unsigned char content:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( webclient->header = h3_request_header_new( ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.h3 = 1;
	}
	cleanUp.good = ( ( h3_request_header_parse( webclient->header, webclient->buffer, strlen( webclient->buffer ) ) ) == 0 );
	if ( cleanUp.good ) {
		if ( strncmp( webclient->header->RequestMethod, "POST", webclient->header->RequestMethodLen ) == 0 ) {
			webclient->mode = MODE_POST;
		}
		for ( i = 0; i < webclient->header->HeaderSize; i++ ) {
			field = &webclient->header->Fields[i];
			if ( strncmp( field->FieldName, "Connection", field->FieldNameLen ) == 0 ) {  //  @TODO:  RTFSpec! only if http1.1 yadayadyada...
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
			h3_request_header_free( webclient->header ); webclient->header = NULL;
		}
	}
}

static void Webclient_Reset( struct webclient_t * webclient ) {
	if ( webclient->header != NULL ) {
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
	if ( webclient->response.content != NULL ) {
		free( webclient->response.content ); webclient->response.content = NULL;
	}
}

static void Webclient_CloseConn( struct webclient_t * webclient ) {
	picoev_del( webclient->webserver->core->loop, webclient->socketFd );
	close( webclient->socketFd );
	Webclient_Delete( webclient );
}

static void Webclient_Delete( struct webclient_t * webclient ) {
	if ( webclient->header != NULL ) {
		h3_request_header_free( webclient->header ); webclient->header = NULL;
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
	if ( webclient->response.content != NULL ) {
		free( webclient->response.content ); webclient->response.content = NULL;
	}

	free( webclient ); webclient = NULL;
}

/*****************************************************************************/
/* Webserver                                                                 */
/*****************************************************************************/
int Webserver_DocumentRoot	( struct webserver_t * webserver, const char * pattern, const char * documentRoot ) {
	struct route_t * route;
	struct { unsigned char good:1;
			unsigned char route:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( route = Route_New( pattern, ROUTETYPE_DOCUMENTROOT, (void * ) documentRoot, webserver->regexOptions, NULL ) ) != NULL);
	if ( cleanUp.good ) {
		cleanUp.route = 1;
		Webserver_AddRoute( webserver, route );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.route ) {
			Route_Delete( route ); route = NULL;
		}
	}

	return ( cleanUp.good == 1 );
}

int Webserver_DynamicHandler( struct webserver_t * webserver, const char * pattern, const dynamicHandler_cb_t handlerCb, void * cbArgs ) {
	struct route_t * route;
	struct { unsigned char good:1;
			unsigned char route:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( route = Route_New( pattern, ROUTETYPE_DYNAMIC, (void * ) handlerCb, webserver->regexOptions, cbArgs ) ) != NULL);
	if ( cleanUp.good ) {
		cleanUp.route = 1;
		Webserver_AddRoute( webserver, route );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.route ) {
			Route_Delete( route ); route = NULL;
		}
	}

	return ( cleanUp.good == 1 );
}
static void Webserver_HandleAccept_cb( picoev_loop * loop, int fd, int events, void * ws_arg ) {
	struct webserver_t * webserver;
	struct webclient_t * webclient;
	int newFd;

	webserver = (struct webserver_t *) ws_arg;
	newFd = accept( fd, NULL, NULL );
	if ( -1 != newFd ) {

		SetupSocket( newFd, 1 );
		webclient = Webclient_New( webserver, newFd );
		picoev_add( loop, newFd, PICOEV_READ, webserver->timeoutSec , Webserver_HandleRead_cb, (void *) webclient );
	}
}

static void Webserver_HandleRead_cb( picoev_loop * loop, int fd, int events, void * wc_arg ) {
	struct webclient_t * webclient;
	ssize_t r;

	webclient = (struct webclient_t *) wc_arg;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout */
		Webclient_CloseConn( webclient );
	} else {
		/* update timeout, and read */
		picoev_set_timeout( loop, fd, webclient->webserver->timeoutSec );
		r = read( fd, webclient->buffer, HTTP_BUFF_LENGTH );
		webclient->buffer[r] = '\0';
		switch ( r ) {
		case 0: /* connection closed by peer */
			Webclient_CloseConn( webclient );
			break;
		case -1: /* error */
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /* try again later */
				break;
			} else { /* fatal error */
				Webclient_CloseConn( webclient );
			}
			break;
		default: /* got some data, send back */
			picoev_del( loop, fd );
			Webclient_PrepareRequest( webclient );
			picoev_add( loop, fd, PICOEV_WRITE, webclient->webserver->timeoutSec , Webserver_HandleWrite_cb, wc_arg );
			break;
		}
	}
}

static void Webserver_HandleWrite_cb( picoev_loop * loop, int fd, int events, void * wc_arg ) {
	struct webclient_t * webclient;
	int connClosed;

	webclient = (struct webclient_t *) wc_arg;
	connClosed = 0;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout */
		Webclient_CloseConn( webclient );
		connClosed = 1;
	} else {
		/* update timeout, and write */
		picoev_set_timeout( loop, fd, webclient->webserver->timeoutSec );
		if ( ! webclient->response.headersSent ) {
			ssize_t headerLength, wroteHeader;
			struct tm * tm_info;
			char headerBuffer[HTTP_BUFF_LENGTH];
			const char * contentTypeString;
			const char * connectionString;
			char dateString[30];
/*			int flags;

			flags = MSG_DONTWAIT | MSG_NOSIGNAL;  */
			webclient->response.end = time( 0 );
			contentTypeString = MimeTypeDefinitions[webclient->response.mimeType].applicationString;
			connectionString = ( CONNECTION_CLOSE == webclient->connection ) ? "Close" : "Keep-Alive";
			time( &webclient->response.end );
			tm_info = localtime( &webclient->response.end );
			strftime( &dateString[0], 30, "%a, %d %b %Y %H:%M:%S %Z", tm_info );
			headerLength = HTTP_SERVER_TEMPLATE_SIZE;
			headerLength = (ssize_t) snprintf( headerBuffer, headerLength, HTTP_SERVER_TEMPLATE, HTTP_SERVER_TEMPLATE_ARGS );
			wroteHeader = write( fd, headerBuffer, headerLength /*  , flags | MSG_MORE  */ );
			switch ( wroteHeader ) {
			case 0: /* the other end cannot keep up */
				break;
			case -1: /*  error  */
				if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /*  try again later  */
					break;
				} else { /*  fatal error  */
					Webclient_CloseConn( webclient );
					connClosed = 1;
				}
				break;
			default: /*  got some data, send back  */
				if ( headerLength != wroteHeader ) {
					Webclient_CloseConn( webclient ); /*  failed to send all data at once, close  */
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
				struct {unsigned char good:1;
						unsigned char fileHandle:1;
				} cleanUp;

				memset( &cleanUp, 0, sizeof( cleanUp ) );
				wroteContent = 0;
				switch ( webclient->response.contentType ) {
				case CONTENTTYPE_FILE:
					cleanUp.good = ( ( fileHandle = open( webclient->response.content, O_RDONLY | O_NONBLOCK ) ) > 0 );
					if ( cleanUp.good ) {
						cleanUp.fileHandle = 1;
						wroteContent = sendfile( fd, fileHandle, 0, webclient->response.contentLength );
					}
					if ( cleanUp.fileHandle ) {
						close( fileHandle );
					}
					break;
				default: //  FT
				case CONTENTTYPE_BUFFER:
					wroteContent = write( fd, webclient->response.content, webclient->response.contentLength /* , flags  */ );
				break;
				}
				switch ( wroteContent ) {
				case 0: /*  the other end cannot keep up  */
					break;
				case -1: /*  error  */
					if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /*  try again later  */
						break;
					} else { /*  fatal error  */
						Webclient_CloseConn( webclient );
						connClosed = 1;
					}
					break;
				default: /*  got some data, send back  */
					if ( webclient->response.contentLength != wroteContent ) {
						Webclient_CloseConn( webclient ); /*  failed to send all data at once, close  */
						connClosed = 1;
					} else {
						webclient->response.contentSent = 1;
					}
					break;
				}
			}
		}
		if ( ! connClosed && webclient->response.headersSent && webclient->response.contentSent ) {
			//  listen again
			if ( CONNECTION_KEEPALIVE == webclient->connection ) {
				picoev_del( loop, fd );
				Webclient_Reset( webclient );
				picoev_add( loop, fd, PICOEV_READ, webclient->webserver->timeoutSec , Webserver_HandleRead_cb, wc_arg );
			} else {
				Webclient_CloseConn( webclient );
				connClosed = 1;
			}
		}
	}
}


struct webserver_t * Webserver_New( const struct core_t * core, const char * ip, const uint16_t port, const unsigned char timeoutSec ) {
	struct webserver_t * webserver;
	struct sockaddr_in listenAddr;
	struct cfg_t * webserverSection, * modulesSection;
	int flag, listenBacklog;
	struct {unsigned char good:1;
			unsigned char ip:1;
			unsigned char socket:1;
			unsigned char onig:1;
			unsigned char webserver:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	flag = 1;
	cleanUp.good = ( ( webserver = malloc( sizeof( * webserver ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.webserver = 1;
		webserver->core = core;
		webserver->socketFd = 0;
		webserver->routes = NULL;
		modulesSection = cfg_getnsec( (cfg_t *) core->config, "modules", 0 );
		webserverSection = cfg_getnsec( modulesSection, "webserver", 0 );
		webserver->regexOptions = ONIG_OPTION_SINGLELINE | ONIG_OPTION_FIND_LONGEST | ONIG_OPTION_CAPTURE_GROUP;  //  | ONIG_OPTION_IGNORECASE | ONIG_OPTION_DEFAULT;
		if ( port == 0 ) {
			webserver->port = (uint16_t) cfg_getint( webserverSection, "port" );
		}
		if ( webserver->port == 0 ) {
			webserver->port = PR_CFG_MODULES_WEBSERVER_PORT;
		}
		webserver->timeoutSec = timeoutSec;
		if ( timeoutSec == 0 ) {
			webserver->timeoutSec = (unsigned char) cfg_getint( webserverSection, "timeout_sec" );
		}
		if ( webserver->timeoutSec == 0 ) {
			webserver->timeoutSec = PR_CFG_MODULES_WEBSERVER_TIMEOUT_SEC;
		}
		if ( ip == NULL ) {
			ip = cfg_getstr( webserverSection, "ip" );
		}
		if ( ip == NULL ) {
			ip = PR_CFG_MODULES_WEBSERVER_IP;
		}
		listenBacklog = cfg_getint( webserverSection, "listen_backlog" );
		if ( listenBacklog == 0 ) {
			listenBacklog = PR_CFG_MODULES_WEBSERVER_LISTEN_BACKLOG;
		}
		cleanUp.good = ( ( webserver->ip = Xstrdup( ip ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.ip = 1;
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
		listenAddr.sin_addr.s_addr = inet_addr( webserver->ip );
		cleanUp.good = ( bind( webserver->socketFd, (struct sockaddr *) &listenAddr, sizeof( listenAddr ) ) == 0 );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( listen( webserver->socketFd, listenBacklog ) == 0 );
	}
	if ( cleanUp.good ) {
		SetupSocket( webserver->socketFd, 1 );
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
			shutdown( webserver->socketFd, SHUT_RDWR );
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

FEATURE_JOINCORE( Webserver, webserver )

static void Webserver_AddRoute( struct webserver_t * webserver, struct route_t * route ) {
	if ( route != NULL ) {
		if ( webserver->routes == NULL ) {
			webserver->routes = route;
		} else {
			PR_APPEND_LINK( &route->mLink, &webserver->routes->mLink );
		}
	}
}

static void Webserver_DelRoute( struct webserver_t * webserver, struct route_t * route ) {
	struct route_t * routeFirst, * routeNext;
	PRCList * next;

	routeFirst = webserver->routes;
	if ( route == routeFirst ) {
		next = PR_NEXT_LINK( &route->mLink );
		if ( next  == &route->mLink ) {
			webserver->routes = NULL;
		} else {
			routeNext = FROM_NEXT_TO_ITEM( struct route_t );
			webserver->routes = routeNext;
		}
	}
	PR_REMOVE_AND_INIT_LINK( &route->mLink );
	Route_Delete( route ); route = NULL;
}

static void Webserver_FindRoute( struct webserver_t * webserver, struct webclient_t * webclient ) {
	struct route_t * route, *firstRoute;
	PRCList * next;
	unsigned char * range, * end, * start;
	int r, found;
	const char * url;
	struct {unsigned char good:1;
			unsigned char url; }cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webclient->route = NULL;
	cleanUp.good = ( ( url = Webclient_GetUrl( webclient ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		start = ( unsigned char * ) url;
		end = start + strlen( url );
		range = end;
		firstRoute = route = webserver->routes;
		if ( firstRoute != NULL ) {
			do {
				next = PR_NEXT_LINK( &route->mLink );
				r = onig_search( route->urlRegex, ( unsigned char * ) url, end, start, range, webserver->region, webserver->regexOptions );
				found = ( r >= 0 );
				if ( found ) {
					webclient->route = webserver->routes;
					break;
				}
				route = FROM_NEXT_TO_ITEM( struct route_t );
			} while ( route != firstRoute );
		}
	}
	if ( cleanUp.url ) {
		//  allways clean up
		free( (char *) url ); url = NULL;
	}
	if ( ! cleanUp.good ) {
	}
}

void Webserver_Delete( struct webserver_t * webserver ) {
	struct route_t * firstRoute;
	//  clean the routes
	firstRoute = webserver->routes;
	while  ( firstRoute != NULL ) {
		Webserver_DelRoute( webserver, firstRoute );
		firstRoute = webserver->routes;
	}
	//  clean the rest
	onig_region_free( webserver->region, 1 ); webserver->region = NULL;
	webserver->regexOptions = 0;
	if ( picoev_is_active( webserver->core->loop, webserver->socketFd ) ) {
		picoev_del( webserver->core->loop, webserver->socketFd );
	}
	shutdown( webserver->socketFd, SHUT_RDWR );
	close( webserver->socketFd );
	webserver->socketFd = 0;
	free( (char *) webserver->ip ); webserver->ip = NULL;
	free( webserver ); webserver = NULL;
}

