#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
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

const char * ConnectionDefinitions[] = {
	"close",
	"Keep-Alive"
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
static struct route_t * 		Route_New						( const char * pattern, const enum routeType_t routeType, void * details, const OnigOptionType regexOptions, void * cbArgs, const clearFunc_cb_t clearFuncCb );
static void						Route_Delete					( struct route_t * route );
static void						Webserver_HandleRead_cb			( picoev_loop * loop, int fd, int events, void * wcArgs );
static void						Webserver_HandleWriteContent_cb	( picoev_loop * loop, int fd, int events, void * wcArgs );
static void						Webserver_HandleWriteHeader_cb	( picoev_loop * loop, int fd, int events, void * wcArgs );
static void						Webserver_HandleWriteTopLine_cb	( picoev_loop * loop, int fd, int events, void * wcArgs );
static void						Webserver_HandleAccept_cb		( picoev_loop * loop, int fd, int events, void * wsArgs );
static void 					Webserver_FindRoute				( const struct webserver_t * webserver, struct webserverclient_t * webserverclient );
static void						Webserver_AddRoute				( struct webserver_t * webserver, struct route_t * route );
static void						Webserver_DelRoute				( struct webserver_t * webserver, struct route_t * route );
static struct webserverclient_t *Webserverclient_New			( struct webserver_t * webserver, int socketFd);
static void						Webserverclient_PrepareRequest	( struct webserverclient_t * webserverclient );
static void						Webserverclient_RenderRoute		( struct webserverclient_t * webserverclient );
static void 					Webserverclient_CloseConn		( struct webserverclient_t * webserverclient );
static void 					Webserverclient_Delete			( struct webserverclient_t * webserverclient );

static struct namedRegex_t * NamedRegex_New( const struct webserverclient_t * 	webserverclient, const int numGroups ) {
	struct namedRegex_t * namedRegex;
	struct { unsigned char good:1;
			unsigned char ng:1;
			unsigned char kv:1; }cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( namedRegex = malloc( sizeof( *namedRegex ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.ng = 1;
		namedRegex->numGroups = numGroups;
		namedRegex->webserverclient = webserverclient;
		cleanUp.good = ( ( namedRegex->kvPairs = calloc( 2 * namedRegex->numGroups, sizeof( *namedRegex->kvPairs ) ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.kv = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.kv ) {
			free( namedRegex->kvPairs ); namedRegex->kvPairs = NULL;
		}
		if ( cleanUp.good ) {
			free( namedRegex ); namedRegex = NULL;
		}
	}
	return namedRegex;
}

static int Webclient_NamedGroup_cb( const UChar* name, const UChar* nameEnd, int ngroupNum, int* group_nums, regex_t* reg, void* cbArgs ) {
	struct namedRegex_t * namedRegex;
	OnigRegion * region;
	int i, gn, startPos, endPos;
	const char * start;
	size_t slot, j;
	int len;
	struct { unsigned char good:1; }cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	namedRegex = (struct namedRegex_t *) cbArgs;
	region = namedRegex->webserverclient->region;
	//  printf( "once %d %d\n", ngroupNum, *group_nums );
	slot = 0;
	for ( i = 0; i < ngroupNum; i++ ) {
		gn = group_nums[i];
		onig_name_to_backref_number( reg, name, nameEnd, region );
		slot = ( gn -1 ) * 2;
		startPos = region->beg[gn];
		endPos = region->end[gn];
		len = endPos - startPos;
		start = namedRegex->webserverclient->header->RequestURI + startPos;
		cleanUp.good = ( ( namedRegex->kvPairs[slot] = Xstrdup( (const char *) name ) ) != NULL );
		if ( cleanUp.good ) {
			cleanUp.good = ( ( namedRegex->kvPairs[slot + 1] = calloc( 1, len + 1 ) ) != NULL );
		}
		if ( cleanUp.good ) {
			strncat( namedRegex->kvPairs[ slot+ 1], start, len );
		} else {
			break;
		}
		//  printf( "%d   %s\t%s\t%d\t%d\t%d\n", gn, namedRegex->kvPairs[slot], namedRegex->kvPairs[ slot + 1 ], startPos, endPos, len  );
	}
	if ( ! cleanUp.good ) {
		for ( j = 0; j < slot; j++ ) {
			free( namedRegex->kvPairs[slot] ); namedRegex->kvPairs[slot] = NULL;
		}
		namedRegex->numGroups = 0;
	}
	return ( cleanUp.good ) ? 0 : 1;  /* 0: continue */
}

struct namedRegex_t * Webserverclient_GetNamedGroups( struct webserverclient_t * webserverclient ) {
	struct namedRegex_t * namedRegex;
	struct { unsigned char good:1; }cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( namedRegex = NamedRegex_New( webserverclient, onig_number_of_names( webserverclient->route->urlRegex ) ) ) != NULL );
	if ( cleanUp.good ) {
		onig_foreach_name( webserverclient->route->urlRegex, Webclient_NamedGroup_cb, (void * ) namedRegex );
	}
	return namedRegex;
}

void NamedRegex_Delete( struct namedRegex_t * namedRegex ) {
	size_t j;

	for ( j = 0; j < namedRegex->numGroups * 2; j++ ) {
		free( namedRegex->kvPairs[j] ); namedRegex->kvPairs[j] = NULL;
	}
	namedRegex->numGroups = 0;
	free( namedRegex->kvPairs ); namedRegex->kvPairs = NULL;
	free( namedRegex ); namedRegex = NULL;
}

/*****************************************************************************/
	/* Route                                                                    */
	/*****************************************************************************/
	static struct route_t * Route_New( const char * pattern, enum routeType_t routeType, void * details, const OnigOptionType regexOptions, void * cbArgs, const clearFunc_cb_t clearFuncCb ) {
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
			route->clearFuncCb = clearFuncCb;
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
					route->details.handlerCb = (webserverHandler_cb_t) details;
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
				if ( route->clearFuncCb != NULL && route->cbArgs != NULL ) {
					route->clearFuncCb( route->cbArgs );
				}
				route->clearFuncCb = NULL;
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
		if ( route->clearFuncCb != NULL && route->cbArgs != NULL ) {
			route->clearFuncCb( route->cbArgs );
		}
		route->clearFuncCb = NULL;
		route->cbArgs = NULL;
		free( route ); route = NULL;
	}

	/*****************************************************************************/
	/* webserverclient                                                                 */
	/*****************************************************************************/
	static struct webserverclient_t * Webserverclient_New( struct webserver_t * webserver, int socketFd) {
	struct webserverclient_t * webserverclient;
	struct {unsigned char good:1;
			unsigned char onig:1;
			unsigned char webserverclient:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( webserverclient = malloc( sizeof( *webserverclient) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.webserverclient = 1;
		webserverclient->socketFd = socketFd;
		webserverclient->webserver = webserver;
		webserverclient->header = NULL;
		webserverclient->route = NULL;
		webserverclient->response.start = time( 0 );
		webserverclient->response.end = 0;
		webserverclient->response.mimeType = MIMETYPE_HTML;
		webserverclient->response.contentType = CONTENTTYPE_BUFFER;
		webserverclient->response.httpCode = HTTPCODE_OK;
		webserverclient->connection = CONNECTION_CLOSE;
		webserverclient->mode = MODE_GET;
		memset( webserverclient->buffer, '\0', HTTP_BUFF_LENGTH );
		if ( webserverclient == CONTENTTYPE_BUFFER ) {
			webserverclient->response.content.dynamic.buffer = NULL;
		} else {
			webserverclient->response.content.file.contentLength = 0;
			webserverclient->response.content.file.fileName = NULL;
		}
		cleanUp.good = ( ( webserverclient->region = onig_region_new( ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.onig = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.onig ) {
			onig_region_free( webserverclient->region, 1 ); webserverclient->region = NULL;
		}
		if ( cleanUp.webserverclient ) {
			free( webserverclient ); webserverclient = NULL;
		}
	}

	return webserverclient;
}

unsigned char Webserverclientresponse_SetContent( struct webserverclientresponse_t * response, const char * content ) {
	size_t contentLen;
	struct { unsigned char good:1;
			unsigned char content:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	if ( response->content.dynamic.buffer != NULL ) {
		contentLen = strlen( content );
		cleanUp.good = ( Buffer_Reset( response->content.dynamic.buffer, contentLen ) == 1 );
	} else {
		cleanUp.good = ( ( response->content.dynamic.buffer = Buffer_NewText( content ) ) != NULL );
	}
	if ( cleanUp.good ) {
		response->contentType = CONTENTTYPE_BUFFER;
		cleanUp.content = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.content )  {
			Buffer_Delete( response->content.dynamic.buffer ); response->content.dynamic.buffer = NULL;
		}
	}

	return ( cleanUp.good ) ? 1 : 0;
}

unsigned char Webserverclientresponse_SetCode( struct webserverclientresponse_t * response, const unsigned int code ) {
	enum httpCode_t codeId;
	unsigned char found;

	found = 0;
	for ( codeId = HTTPCODE_NONE; codeId < __HTTPCODE_LAST; codeId++ ) {
		if ( codeId == code ) {
			response->httpCode = codeId;
			found = 1;
			break;
		}
	}
	return found;
}

unsigned char Webserverclientresponse_SetMime( struct webserverclientresponse_t * response, const char * mimeString ) {
	enum mimeType_t mimeId;
	struct mimeDetail_t * mimeDetail;
	unsigned char found;

	found = 0;
	for ( mimeId = MIMETYPE_HTML; mimeId < __MIMETYPE_LAST; mimeId++ ) {
		mimeDetail = &MimeTypeDefinitions[mimeId];
		if ( strcmp( mimeString, mimeDetail->ext ) == 0 || strcmp( mimeString, mimeDetail-> applicationString ) == 0 ) {
			response->mimeType = mimeId;
			found = 1;
			break;
		}
	}
	return found;
}


const char * Webserverclient_GetUrl( const struct webserverclient_t * webserverclient ) {
	char * url;
	int len;
	struct {unsigned char good:1;
			unsigned char url:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	len = webserverclient->header->RequestURILen;
	cleanUp.good = ( ( url = malloc( len + 1 ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		snprintf( url, len + 1, "%s", webserverclient->header->RequestURI );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.url ) {
			free( url ); url = NULL;
		}
	}
	return url;
}

const char * Webserverclient_GetIp( const struct webserverclient_t * webserverclient ) {
	char * ip;
	struct sockaddr_storage addr;
	socklen_t len;
	struct {unsigned char good:1;
			unsigned char ip:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( ip = malloc( INET6_ADDRSTRLEN ) ) != NULL );
	if ( cleanUp.good ) {
		len = sizeof addr;
		getpeername( webserverclient->socketFd, (struct sockaddr *) &addr, &len );
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

#define WEBSERVERCLIENT_RENDER( name, webpage ) \
static void Webserverclient_Render_##name( struct webserverclient_t * webserverclient) { \
	struct {unsigned char good:1; \
			unsigned char content:1; } cleanUp; \
	\
	memset( &cleanUp, 0, sizeof( cleanUp ) ); \
	if ( webserverclient->response.contentType == CONTENTTYPE_FILE && webserverclient->response.content.file.fileName ) { \
		free( (char *) webserverclient->response.content.file.fileName ); webserverclient->response.content.file.fileName = NULL; \
		webserverclient->response.content.file.contentLength = 0; \
		webserverclient->response.contentType = CONTENTTYPE_BUFFER; \
	} \
	webserverclient->response.mimeType = MIMETYPE_HTML; \
	Webserverclientresponse_SetContent( &webserverclient->response, webpage ); \
}

//WEBSERVERCLIENT_RENDER( Ok, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>OK</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Created, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Created</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Accepted, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Accepted</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Non_authoritative_information, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Non-Authoritative Information</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( No_content, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>No Content</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Reset_content, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Reset Content</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Partial_content, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Partial Content</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Bad_request, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Bad Request</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Unauthorized, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Unauthorized</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Payment_required, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Payment Required</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Forbidden, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Forbidden</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Not_found, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Not Found</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Method_not_allowed, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Method Not Allowed</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Not_acceptable, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Not Acceptable</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Proxy_authentication_required, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Proxy Authentication Required</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Request_timeout, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Request Timeout</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Conflict, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Conflict</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Gone, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Gone</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Length_required, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Length Required</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Precondition_failed, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Precondition Failed</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Request_entity_too_large, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Request Entity Too Large</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Request_uri_too_long, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Request-URI Too Long</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Unsupported_media_type, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Unsupported Media Type</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Requested_range_not_satisfiable, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Requested Range Not Satisfiable</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Expectation_failed, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Expectation Failed</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Continue, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Continue</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Switching_protocols, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Switching Protocols</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Multiple_choices, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Multiple Choices</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Moved_permanently, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Moved Permanently</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Found, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Found</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( See_other, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>See Other</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Not_modified, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Not Modified</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Use_proxy, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Use Proxy</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Temporary_redirect, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Temporary Redirect</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Internal_server_error, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Internal Server Error</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Not_implemented, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Not Implemented</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Bad_gateway, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Bad Gateway</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Service_unavailable, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Service Unavailable</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Gateway_timeout, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>Gateway Timeout</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )
WEBSERVERCLIENT_RENDER( Http_version_not_supported, "<html>" "\n\t" "<body>" "\n\t\t" "<h1>HTTP Version Not Supported</h1>" "\n\t" "</body>" "\n" "</html>" "\n" )

static void Webserverclient_RenderRoute( struct webserverclient_t * webserverclient ) {
	struct route_t * route;
	//  @TODO:  urgently refractor  this routine, 280 lines is too much!
	route = webserverclient->route;
	if ( route == NULL ) {
		Webserverclient_Render_Not_found( webserverclient );
	} else {
		if ( route->routeType == ROUTETYPE_DYNAMIC ) {
			route->details.handlerCb( webserverclient );
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
			webserverclient->response.contentType = CONTENTTYPE_FILE;
			documentRoot = route->details.documentRoot;
			pathLength = webserverclient->region->end[1] - webserverclient->region->beg[1] + 1;
			cleanUp.good = ( ( requestedPath = malloc( pathLength + 1 ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.requestedPath = 1;
				snprintf( requestedPath, pathLength, "%s", &webserverclient->header->RequestURI[webserverclient->region->beg[1]] );
			}
			//  check that the file is not higher then the documentRoot. Such as  ../../../../etc/passwd
			for ( j = 0; j < pathLength - 1; j++ ) {
				if ( requestedPath[j] == '.' && requestedPath[ j + 1] == '.' ) {
					webserverclient->response.httpCode = HTTPCODE_FORBIDDEN;
						break;
				}
			}
			cleanUp.good = ( webserverclient->response.httpCode == HTTPCODE_OK );
			if ( cleanUp.good ) {
				fullPathLength = strlen( documentRoot ) + pathLength + 13;  //  13: that is  '/' + '/' + 'index.html' + '\0'
				cleanUp.good = ( ( fullPath = malloc( fullPathLength ) ) != NULL );   //  if all goes successfull, this is stored in ->content, wich is free'd normally

			}
			if ( cleanUp.good ) {
				cleanUp.fullPath = 1;
				FullPath( fullPath, fullPathLength ,documentRoot, requestedPath );
				exists = stat( fullPath, &fileStat );
				if ( exists == 0 ) {
					webserverclient->response.httpCode = HTTPCODE_OK;
					if ( S_ISDIR( fileStat.st_mode ) ) {
						//  if it is a dir, and has a index.html file that is readable, use that
						snprintf( fullPath, fullPathLength, "%s/%s/index.html", documentRoot, requestedPath );
						exists = stat( fullPath, &fileStat );
						if ( exists != 0 ) {
							//  this is the place where a directory index handler can step into the arena;
							//  but we will not do that, because: https://github.com/monkey/monkey/blob/master/plugins/dirlisting/dirlisting.c#L153
							webserverclient->response.httpCode = HTTPCODE_NOT_FOUND;
						}
					}
					//  all looks ok, we have access to a file
					if ( webserverclient->response.httpCode == HTTPCODE_OK ) {
						webserverclient->response.content.file.contentLength = fileStat.st_size;
						webserverclient->response.content.file.fileName = fullPath;
						fullPathLength = strlen( fullPath );
						//  determine mimetype
						for ( j = 0; j < __MIMETYPE_LAST; j++ ) {
							len = strlen( MimeTypeDefinitions[j].ext );
							if ( strncmp( &fullPath[fullPathLength - len], MimeTypeDefinitions[j].ext, len ) == 0 ) {
								webserverclient->response.mimeType = MimeTypeDefinitions[j].mime;
								break;
							}
						}
						//  this is the place where a module handler can step into the arena. e.g. .js / .py / .php
						//  but we will not do this for spidermonkey, because we load that upon startup only once
					}
				} else {
					webserverclient->response.httpCode = HTTPCODE_NOT_FOUND;
				}
			}
			switch ( webserverclient->response.httpCode ) {
			//  most common
			case HTTPCODE_OK:
				// WebserverRenderOk( webserverclient );
				break;
			case HTTPCODE_INTERNAL_SERVER_ERROR:
				Webserverclient_Render_Internal_server_error( webserverclient );
				break;
			case HTTPCODE_UNAUTHORIZED:
				Webserverclient_Render_Unauthorized( webserverclient );
				break;
			case HTTPCODE_FORBIDDEN:
				Webserverclient_Render_Forbidden( webserverclient );
				break;
			case HTTPCODE_NOT_FOUND:
				Webserverclient_Render_Not_found( webserverclient );
				break;
			//  other
			case HTTPCODE_CREATED:
				Webserverclient_Render_Created( webserverclient );
				break;
			case HTTPCODE_ACCEPTED:
				Webserverclient_Render_Accepted( webserverclient );
				break;
			case HTTPCODE_NON_AUTHORITATIVE_INFORMATION:
				Webserverclient_Render_Non_authoritative_information( webserverclient );
				break;
			case HTTPCODE_NO_CONTENT:
				Webserverclient_Render_No_content( webserverclient );
				break;
			case HTTPCODE_RESET_CONTENT:
				Webserverclient_Render_Reset_content( webserverclient );
				break;
			case HTTPCODE_PARTIAL_CONTENT:
				Webserverclient_Render_Partial_content( webserverclient );
				break;
			case HTTPCODE_BAD_REQUEST:
				Webserverclient_Render_Bad_request( webserverclient );
				break;
			case HTTPCODE_PAYMENT_REQUIRED:
				Webserverclient_Render_Payment_required( webserverclient );
				break;
			case HTTPCODE_METHOD_NOT_ALLOWED:
				Webserverclient_Render_Method_not_allowed( webserverclient );
				break;
			case HTTPCODE_NOT_ACCEPTABLE:
				Webserverclient_Render_Not_acceptable( webserverclient );
				break;
			case HTTPCODE_PROXY_AUTHENTICATION_REQUIRED:
				Webserverclient_Render_Proxy_authentication_required( webserverclient );
				break;
			case HTTPCODE_REQUEST_TIMEOUT:
				Webserverclient_Render_Request_timeout( webserverclient );
				break;
			case HTTPCODE_CONFLICT:
				Webserverclient_Render_Conflict( webserverclient );
				break;
			case HTTPCODE_GONE:
				Webserverclient_Render_Gone( webserverclient );
				break;
			case HTTPCODE_LENGTH_REQUIRED:
				Webserverclient_Render_Length_required( webserverclient );
				break;
			case HTTPCODE_PRECONDITION_FAILED:
				Webserverclient_Render_Precondition_failed( webserverclient );
				break;
			case HTTPCODE_REQUEST_ENTITY_TOO_LARGE:
				Webserverclient_Render_Request_entity_too_large( webserverclient );
				break;
			case HTTPCODE_REQUEST_URI_TOO_LONG:
				Webserverclient_Render_Request_uri_too_long( webserverclient );
				break;
			case HTTPCODE_UNSUPPORTED_MEDIA_TYPE:
				Webserverclient_Render_Unsupported_media_type( webserverclient );
				break;
			case HTTPCODE_REQUESTED_RANGE_NOT_SATISFIABLE:
				Webserverclient_Render_Requested_range_not_satisfiable( webserverclient );
				break;
			case HTTPCODE_EXPECTATION_FAILED:
				Webserverclient_Render_Expectation_failed( webserverclient );
				break;
			case HTTPCODE_CONTINUE:
				Webserverclient_Render_Continue( webserverclient );
				break;
			case HTTPCODE_SWITCHING_PROTOCOLS:
				Webserverclient_Render_Switching_protocols( webserverclient );
				break;
			case HTTPCODE_MULTIPLE_CHOICES:
				Webserverclient_Render_Multiple_choices( webserverclient );
				break;
			case HTTPCODE_MOVED_PERMANENTLY:
				Webserverclient_Render_Moved_permanently( webserverclient );
				break;
			case HTTPCODE_FOUND:
				Webserverclient_Render_Found( webserverclient );
				break;
			case HTTPCODE_SEE_OTHER:
				Webserverclient_Render_See_other( webserverclient );
				break;
			case HTTPCODE_NOT_MODIFIED:
				Webserverclient_Render_Not_modified( webserverclient );
				break;
			case HTTPCODE_USE_PROXY:
				Webserverclient_Render_Use_proxy( webserverclient );
				break;
			case HTTPCODE_TEMPORARY_REDIRECT:
				Webserverclient_Render_Temporary_redirect( webserverclient );
				break;
			case HTTPCODE_NOT_IMPLEMENTED:
				Webserverclient_Render_Not_implemented( webserverclient );
				break;
			case HTTPCODE_BAD_GATEWAY:
				Webserverclient_Render_Bad_gateway( webserverclient );
				break;
			case HTTPCODE_SERVICE_UNAVAILABLE:
				Webserverclient_Render_Service_unavailable( webserverclient );
				break;
			case HTTPCODE_GATEWAY_TIMEOUT:
				Webserverclient_Render_Gateway_timeout( webserverclient );
				break;
			case HTTPCODE_HTTP_VERSION_NOT_SUPPORTED:
				Webserverclient_Render_Http_version_not_supported( webserverclient );
				break;
			//  cruft
			case HTTPCODE_NONE:  //  ft
			case __HTTPCODE_LAST:  //  ft
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

static void Webserverclient_PrepareRequest( struct webserverclient_t * webserverclient ) {
	HeaderField * field;
	size_t i;
	const char * close, * keepAlive;
	struct {unsigned char good:1;
			unsigned char h3:1;
			unsigned char content:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( webserverclient->header = h3_request_header_new( ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.h3 = 1;
	}
	cleanUp.good = ( ( h3_request_header_parse( webserverclient->header, webserverclient->buffer, strlen( webserverclient->buffer ) ) ) == 0 );
	if ( cleanUp.good ) {
		if ( strncmp( webserverclient->header->RequestMethod, "POST", webserverclient->header->RequestMethodLen ) == 0 ) {
			webserverclient->mode = MODE_POST;
		}
		for ( i = 0; i < webserverclient->header->HeaderSize; i++ ) {
			field = &webserverclient->header->Fields[i];
			if ( strncmp( field->FieldName, "Connection", field->FieldNameLen ) == 0 ) {  //  @TODO:  RTFSpec! only if http/1.1 yadayadyada... http://i.stack.imgur.com/whhD1.png
				close = ConnectionDefinitions[CONNECTION_CLOSE];
				keepAlive = ConnectionDefinitions[CONNECTION_KEEPALIVE];
				if ( strncmp( field->Value, close, field->ValueLen ) == 0 ) {
					webserverclient->connection = CONNECTION_KEEPALIVE;
				} else 	if ( strncmp( field->Value, keepAlive, field->ValueLen ) == 0 ) {
					webserverclient->connection = CONNECTION_CLOSE;
				}
			}
		}
	}
	if ( cleanUp.good ) {
		Webserver_FindRoute( webserverclient->webserver, webserverclient );
		Webserverclient_RenderRoute( webserverclient );
	}
	if ( cleanUp.good ) {
		if ( webserverclient->response.contentType == CONTENTTYPE_FILE ) {
			cleanUp.content = ( webserverclient->response.content.file.contentLength > 0 );
		} else {
			cleanUp.content = ( webserverclient->response.content.dynamic.buffer != NULL );
		}
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.content ) {
			if ( webserverclient->response.contentType == CONTENTTYPE_FILE ) {
				free( (char * ) webserverclient->response.content.file.fileName ); webserverclient->response.content.file.fileName = NULL;
				webserverclient->response.content.file.contentLength = 0;
			} else {
				Buffer_Delete( webserverclient->response.content.dynamic.buffer ); webserverclient->response.content.dynamic.buffer = NULL;
			}
		}
		if ( cleanUp.h3 ) {
			h3_request_header_free( webserverclient->header ); webserverclient->header = NULL;
		}
	}
}

static void Webserverclient_Reset( struct webserverclient_t * webserverclient ) {
	if ( webserverclient->header != NULL ) {
			h3_request_header_free( webserverclient->header ); webserverclient->header = NULL;
		}
		memset( webserverclient->buffer, '\0', strlen( webserverclient->buffer ) );
		webserverclient->route = NULL;
		webserverclient->response.start = time( 0 );
		webserverclient->response.end = 0;
		webserverclient->response.httpCode = HTTPCODE_OK;
		webserverclient->response.mimeType = MIMETYPE_HTML;
		webserverclient->connection = CONNECTION_CLOSE;
		webserverclient->mode = MODE_GET;
		if ( webserverclient->response.contentType == CONTENTTYPE_FILE ) {
			free( (char * ) webserverclient->response.content.file.fileName ); webserverclient->response.content.file.fileName = NULL;
			webserverclient->response.content.file.contentLength = 0;
		} else {
			Buffer_Delete( webserverclient->response.content.dynamic.buffer ); webserverclient->response.content.dynamic.buffer = NULL;
		}
		webserverclient->response.contentType = CONTENTTYPE_BUFFER;
		onig_region_free( webserverclient->region, 0 );
	}

static void Webserverclient_CloseConn( struct webserverclient_t * webserverclient ) {
		webserverclient->response.end = time( 0 );
		picoev_del( webserverclient->webserver->core->loop, webserverclient->socketFd );
		close( webserverclient->socketFd );
		Webserverclient_Delete( webserverclient );
	}

static void Webserverclient_Delete( struct webserverclient_t * webserverclient ) {
	if ( webserverclient->header != NULL ) {
		h3_request_header_free( webserverclient->header ); webserverclient->header = NULL;
	}
	webserverclient->route = NULL;
	webserverclient->response.httpCode = HTTPCODE_NONE;
	webserverclient->response.start = 0;
	webserverclient->response.end = 0;
	webserverclient->response.mimeType = MIMETYPE_HTML;
	webserverclient->connection = CONNECTION_CLOSE;
	webserverclient->mode = MODE_GET;
	onig_region_free( webserverclient->region, 1 ); webserverclient->region = NULL;
	if ( webserverclient->response.contentType == CONTENTTYPE_FILE ) {
		free( (char * ) webserverclient->response.content.file.fileName ); webserverclient->response.content.file.fileName = NULL;
		webserverclient->response.content.file.contentLength = 0;
	} else {
		Buffer_Delete( webserverclient->response.content.dynamic.buffer ); webserverclient->response.content.dynamic.buffer = NULL;
	}
	webserverclient->response.contentType = CONTENTTYPE_BUFFER;


	free( webserverclient ); webserverclient = NULL;
}

/*****************************************************************************/
/* Webserver                                                                 */
/*****************************************************************************/
int Webserver_DocumentRoot	( struct webserver_t * webserver, const char * pattern, const char * documentRoot ) {
	struct route_t * route;
	struct { unsigned char good:1;
			unsigned char route:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( route = Route_New( pattern, ROUTETYPE_DOCUMENTROOT, (void * ) documentRoot, webserver->regexOptions, NULL, NULL ) ) != NULL);
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

int Webserver_DynamicHandler( struct webserver_t * webserver, const char * pattern, const webserverHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb ) {
	struct route_t * route;
	struct { unsigned char good:1;
			unsigned char route:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( route = Route_New( pattern, ROUTETYPE_DYNAMIC, (void * ) handlerCb, webserver->regexOptions, cbArgs, clearFuncCb ) ) != NULL);
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
static void Webserver_HandleAccept_cb( picoev_loop * loop, int fd, int events, void * wsArg ) {
	struct webserver_t * webserver;
	struct webserverclient_t * webserverclient;
	int newFd;

	webserver = (struct webserver_t *) wsArg;
	newFd = accept( fd, NULL, NULL );
	if ( -1 != newFd ) {
		SetupSocket( newFd, 1 );
		webserverclient = Webserverclient_New( webserver, newFd );
		picoev_add( loop, newFd, PICOEV_READ, webserver->timeoutSec , Webserver_HandleRead_cb, (void *) webserverclient );
	}
}

static void Webserver_HandleRead_cb( picoev_loop * loop, int fd, int events, void * wcArg ) {
	struct webserverclient_t * webserverclient;
	ssize_t r;

	webserverclient = (struct webserverclient_t *) wcArg;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout */
		Webserverclient_CloseConn( webserverclient );
	} else {
		/* update timeout, and read */
		picoev_set_timeout( loop, fd, webserverclient->webserver->timeoutSec );
		r = read( fd, webserverclient->buffer, HTTP_BUFF_LENGTH );
		webserverclient->buffer[r] = '\0';
		switch ( r ) {
		case 0: /* connection closed by peer */
			Webserverclient_CloseConn( webserverclient );
			break;
		case -1: /* error */
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /* try again later */
				break;
			} else { /* fatal error */
				Webserverclient_CloseConn( webserverclient );
			}
			break;
		default: /* got some data, send back */
			picoev_del( loop, fd );
			Webserverclient_PrepareRequest( webserverclient );
			picoev_add( loop, fd, PICOEV_WRITE, webserverclient->webserver->timeoutSec , Webserver_HandleWriteTopLine_cb, wcArg );
			break;
		}
	}
}

static void Webserver_HandleWriteFile_cb( picoev_loop * loop, int fd, int events, void * wcArg ) {
	struct webserverclient_t * webserverclient;
	size_t contentLen;
	ssize_t wroteBytes;
	int done, fileHandle;
	struct { unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webserverclient = (struct webserverclient_t *) wcArg;
	done = 0;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout */
		Webserverclient_CloseConn( webserverclient );
	} else {
		/* update timeout, and write */
		picoev_set_timeout( loop, fd, webserverclient->webserver->timeoutSec );
		contentLen = webserverclient->response.content.file.contentLength;
		if ( contentLen == 0 ) {
			done = 1;
		} else {
			cleanUp.good = ( ( fileHandle = open( webserverclient->response.content.file.fileName, O_RDONLY | O_NONBLOCK ) ) > 0 );
			if ( cleanUp.good ) {
				wroteBytes = sendfile( fd, fileHandle, 0, webserverclient->response.content.file.contentLength );
				close( fileHandle );
				switch ( wroteBytes ) {
				case 0: /*  the other end cannot keep up  */
					break;
				case -1: /*  error  */
					if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /*  try again later  */
						break;
					} else { /*  fatal error  */
						Webserverclient_CloseConn( webserverclient );
					}
					break;
				default: /*  got some data, send back  */
					if ( contentLen != (size_t) wroteBytes) {
						Webserverclient_CloseConn( webserverclient ); /*  failed to send all data at once, close  */
					} else {
						done = 1;
					}
					break;
				}
			}
		}
		if ( done ) {
				webserverclient->response.end = time( 0 );
			if ( CONNECTION_KEEPALIVE == webserverclient->connection ) {
				picoev_del( loop, fd );
				Webserverclient_Reset( webserverclient );
				picoev_add( loop, fd, PICOEV_READ, webserverclient->webserver->timeoutSec , Webserver_HandleRead_cb, wcArg );
			} else {
				Webserverclient_CloseConn( webserverclient );
			}
		}
	}
}

static void Webserver_HandleWriteContent_cb( picoev_loop * loop, int fd, int events, void * wcArg ) {
	struct webserverclient_t * webserverclient;
	size_t contentLen;
	ssize_t wroteBytes;
	int done;

	webserverclient = (struct webserverclient_t *) wcArg;
	done = 0;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout */
		Webserverclient_CloseConn( webserverclient );
	} else {
		/* update timeout, and write */
		picoev_set_timeout( loop, fd, webserverclient->webserver->timeoutSec );
		contentLen = webserverclient->response.content.dynamic.buffer->used;
		if ( contentLen == 0 ) {
			done = 1;
		} else {
			wroteBytes = write( fd, webserverclient->response.content.dynamic.buffer->bytes, webserverclient->response.content.dynamic.buffer->used );
			switch ( wroteBytes ) {
			case 0: /*  the other end cannot keep up  */
				break;
			case -1: /*  error  */
				if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /*  try again later  */
					break;
				} else { /*  fatal error  */
					Webserverclient_CloseConn( webserverclient );
				}
				break;
			default: /*  got some data, send back  */
				if ( contentLen != (size_t) wroteBytes ) {
					Webserverclient_CloseConn( webserverclient ); /*  failed to send all data at once, close  */
				}
				break;
			}
		}
		if ( done ) {
			if ( CONNECTION_KEEPALIVE == webserverclient->connection ) {
				picoev_del( loop, fd );
				Webserverclient_Reset( webserverclient );
				picoev_add( loop, fd, PICOEV_READ, webserverclient->webserver->timeoutSec , Webserver_HandleRead_cb, wcArg );
			} else {
				Webserverclient_CloseConn( webserverclient );
			}
		}
	}
}

#define HTTP_HEADER_LENGTH 2048
static void Webserver_HandleWriteHeader_cb( picoev_loop * loop, int fd, int events, void * wcArg ) {
	struct webserverclient_t * webserverclient;
	size_t headerLen, contentLen;
	ssize_t wroteBytes;
	char header[HTTP_HEADER_LENGTH];
	char dateString[30];
	const char * contentTypeString;
	const char * connectionString;
	struct tm * tm_info;

	webserverclient = (struct webserverclient_t *) wcArg;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout */
		Webserverclient_CloseConn( webserverclient );
	} else {
		/* update timeout, and write */
		picoev_set_timeout( loop, fd, webserverclient->webserver->timeoutSec );
		contentLen = ( webserverclient->response.contentType == CONTENTTYPE_FILE ) ? webserverclient->response.content.file.contentLength: webserverclient->response.content.dynamic.buffer->used;
		contentTypeString = MimeTypeDefinitions[webserverclient->response.mimeType].applicationString;
		connectionString = ( CONNECTION_CLOSE == webserverclient->connection ) ? "Close" : "Keep-Alive";
		time( &webserverclient->response.end );
		tm_info = localtime( &webserverclient->response.end );
		strftime( &dateString[0], 30, "%a, %d %b %Y %H:%M:%S %Z", tm_info );
		headerLen = snprintf( &header[0], HTTP_HEADER_LENGTH, "Content-Length: %d\r\nConnection: %s\r\nContent-Type: %s\r\nDate: %s\r\nServer: %s/%s\r\n\r\n", contentLen, connectionString, contentTypeString, dateString, PR_NAME, PR_VERSION );
		wroteBytes = write( fd, header, headerLen );
		switch ( wroteBytes ) {
		case 0: /* the other end cannot keep up */
			break;
		case -1: /*  error  */
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /*  try again later  */
				break;
			} else { /*  fatal error  */
				Webserverclient_CloseConn( webserverclient );
			}
			break;
		default: /*  got some data, send back  */
			if ( headerLen != (size_t) wroteBytes ) {
				Webserverclient_CloseConn( webserverclient ); /*  failed to send all data at once, close  */
			} else {
				picoev_del( loop, fd );
				if ( webserverclient->response.contentType == CONTENTTYPE_FILE) {
					picoev_add( loop, fd, PICOEV_WRITE, webserverclient->webserver->timeoutSec , Webserver_HandleWriteFile_cb, wcArg );
				} else {
					picoev_add( loop, fd, PICOEV_WRITE, webserverclient->webserver->timeoutSec , Webserver_HandleWriteContent_cb, wcArg );
				}
			}
			break;
		}
	}
}

#define HTTP_TOPLINE_LENGTH 20
static void Webserver_HandleWriteTopLine_cb( picoev_loop * loop, int fd, int events, void * wcArg ) {
	struct webserverclient_t * webserverclient;
	size_t topLineLen;
	ssize_t wroteBytes;
	char topLine[HTTP_TOPLINE_LENGTH];

	webserverclient = (struct webserverclient_t *) wcArg;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		/* timeout */
		Webserverclient_CloseConn( webserverclient );
	} else {
		/* update timeout, and write */
		picoev_set_timeout( loop, fd, webserverclient->webserver->timeoutSec );
		topLineLen = snprintf( &topLine[0], HTTP_TOPLINE_LENGTH, "HTTP/1.1 %d OK\r\n", webserverclient->response.httpCode );
		wroteBytes = write( fd, topLine, topLineLen );
		switch ( wroteBytes ) {
		case 0: /* the other end cannot keep up */
			break;
		case -1: /*  error  */
			if ( errno == EAGAIN || errno == EWOULDBLOCK ) { /*  try again later  */
				break;
			} else { /*  fatal error  */
				Webserverclient_CloseConn( webserverclient );
			}
			break;
		default: /*  got some data, send back  */
			if ( topLineLen != (size_t) wroteBytes ) {
				Webserverclient_CloseConn( webserverclient ); /*  failed to send all data at once, close  */
			} else {
				picoev_del( loop, fd );
				picoev_add( loop, fd, PICOEV_WRITE, webserverclient->webserver->timeoutSec , Webserver_HandleWriteHeader_cb, wcArg );
			}
			break;
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
		//webserver->regexOptions = ONIG_OPTION_SINGLELINE | ONIG_OPTION_FIND_LONGEST | ONIG_OPTION_CAPTURE_GROUP;  //  | ONIG_OPTION_IGNORECASE | ONIG_OPTION_DEFAULT;
		webserver->regexOptions = ONIG_OPTION_DEFAULT;
		webserver->port = port;
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
	}
	if ( cleanUp.good ) {
		Core_Log( webserver->core, LOG_INFO, __FILE__ , __LINE__, "New Webserver allocated" );
	}
	if ( ! cleanUp.good ) {
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
			PR_INSERT_BEFORE( &route->mLink, &webserver->routes->mLink );
		}
		Core_Log( webserver->core, LOG_INFO, __FILE__ , __LINE__, "New Route allocated" );
	}
}

static void Webserver_DelRoute( struct webserver_t * webserver, struct route_t * route ) {
	struct route_t * routeNext;
	PRCList * next;

	if ( PR_CLIST_IS_EMPTY( &route->mLink ) ) {
		webserver->routes = NULL;
	} else {
		next = PR_NEXT_LINK( &route->mLink );
		routeNext = FROM_NEXT_TO_ITEM( struct route_t );
		webserver->routes = routeNext;
	}
	PR_REMOVE_AND_INIT_LINK( &route->mLink );
	Route_Delete( route ); route = NULL;
	Core_Log( webserver->core, LOG_INFO, __FILE__ , __LINE__, "Delete Route free-ed" );
}

static void Webserver_FindRoute( const struct webserver_t * webserver, struct webserverclient_t * webserverclient ) {
	struct route_t * route, *firstRoute;
	PRCList * next;
	unsigned char * range, * end, * start;
	int r, found;
	const char * url;
	struct {unsigned char good:1;
			unsigned char url; }cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webserverclient->route = NULL;
	cleanUp.good = ( ( url = Webserverclient_GetUrl( webserverclient ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		start = ( unsigned char * ) url;
		end = start + strlen( url );
		range = end;
		firstRoute = route = webserver->routes;
		if ( firstRoute != NULL ) {
			do {
				next = PR_NEXT_LINK( &route->mLink );
				r = onig_search( route->urlRegex, ( unsigned char * ) url, end, start, range, webserverclient->region, webserver->regexOptions );
				found = ( r >= 0 );
				if ( found ) {
					webserverclient->route = route;
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
	webserver->regexOptions = 0;
	if ( picoev_is_active( webserver->core->loop, webserver->socketFd ) ) {
		picoev_del( webserver->core->loop, webserver->socketFd );
	}
	shutdown( webserver->socketFd, SHUT_RDWR );
	close( webserver->socketFd );
	webserver->socketFd = 0;
	free( (char *) webserver->ip ); webserver->ip = NULL;
	Core_Log( webserver->core, LOG_INFO, __FILE__ , __LINE__, "Delete Webserver free-ed" );
	webserver->core = NULL;
	free( webserver ); webserver = NULL;
}

