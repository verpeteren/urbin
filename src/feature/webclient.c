#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "webclient.h"
#include "../core/utils.h"

extern const char * MethodDefinitions[ ];
extern const char * ConnectionDefinitions[];

static void						Webclient_HandleRead_cb		( picoev_loop * loop, int fd, int events, void * wcArgs );
static void						Webclient_HandleWrite_cb	( picoev_loop * loop, int fd, int events, void * wcArgs );
static void						Webclient_HandleConnect_cb	( picoev_loop * loop, int fd, int events, void * wsArgs );
static void 					Webclient_Connect			( struct webclient_t * webclient );
static void 					Webclient_PushWebpage		( struct webclient_t * webclient, struct webpage_t * webpage );
static struct webpage_t * 		Webclient_PopWebpage		( struct webclient_t * webclient );

static struct buffer_t * 		Webpage_TopLine				( struct webpage_t * webpage );
static void 					Webpage_Delete				( struct webpage_t * webpage );


static void Webclient_CloseConn( struct webclient_t * webclient ) {
	picoev_del( webclient->core->loop, webclient->socketFd );
	close( webclient->socketFd ); webclient->socketFd = 0;
}

#define RAWLENG 1024
#define CONTLENG 1024
static void Webclient_HandleRead_cb( picoev_loop * loop, int fd, int events, void * wcArgs ) {
	struct webclient_t * webclient;
	struct webpage_t * webpage;
	struct buffer_t * rawBuffer;
	ssize_t bytesRead;
	struct {unsigned char headers:1;
			unsigned char content:1;
			unsigned char raw:1;
			unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webclient = (struct webclient_t *) wcArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Webclient_CloseConn( webclient );
	} else if ( ( events & PICOEV_READ ) != 0 ) {
		picoev_set_timeout( loop, fd, webclient->timeoutSec );
		webpage = webclient->currentWebpage;
		//  @FIXME:  this is wrong, all buffers are mixedup
		cleanUp.good = ( ( rawBuffer = Buffer_New( RAWLENG ) ) != NULL );
		if ( cleanUp.good ) {
			cleanUp.raw = 1;
			//  BUFFER HACK
			bytesRead = read( fd, rawBuffer->bytes, CONTLENG );
			rawBuffer->used = (size_t) bytesRead;
			switch ( bytesRead ) {
			case 0:
				Webclient_CloseConn( webclient );
				 break;
			case -1:
				if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
					//  loop once more
			  } else {
				Webclient_CloseConn( webclient );
			 }
			 break;
			default:
				//  @FIXME:  check if readall, if fit in headers, then parse headers,  parse headers,  if corntent length in headers, reade more if http end then issue callback, ojay, probably read the specs
				picoev_del( loop, fd );
				picoev_add( loop, fd, PICOEV_READ, webclient->timeoutSec, Webclient_HandleRead_cb, wcArgs );
				printf( "%s", rawBuffer->bytes );
				if ( webpage->handlerCb != NULL ) {
					webpage->handlerCb( webpage );
				}
				Webpage_Delete( webpage ); webpage = NULL;
				webclient->currentWebpage = NULL;
				if ( CONNECTION_KEEPALIVE ==  webclient->connection ) {
					picoev_del( loop, fd );
					picoev_add( loop, fd, PICOEV_WRITE, webclient->timeoutSec, Webclient_HandleWrite_cb, wcArgs );
				} else {
					Webclient_CloseConn( webclient );
				}
			  break;
			}
		} else {
			Webclient_CloseConn( webclient );
		}
	}
}

static void Webclient_HandleWrite_cb( picoev_loop * loop, int fd, int events, void * wcArgs ) {
	struct webclient_t * webclient;
	struct webpage_t * webpage;
	ssize_t bytesWritten;
	size_t totalBytes, len;
	struct {unsigned char good:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webclient = (struct webclient_t *) wcArgs;
	totalBytes = 0;
	bytesWritten = 0;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Webclient_CloseConn( webclient );
	} else if ( ( events & PICOEV_WRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, webclient->timeoutSec );
		//  Let's get some work
		webpage = Webclient_PopWebpage( webclient );
		if ( webpage != NULL ) {
			cleanUp.good = ( Webpage_TopLine( webpage ) != NULL );
			if ( cleanUp.good ) {
				//  @FIXME: writebytes in a loop,,,, check if everything is written. etc
				picoev_del( loop, fd );
				picoev_add( loop, fd, PICOEV_READ, webclient->timeoutSec, Webclient_HandleRead_cb, wcArgs );
				len = webpage->request.topLine->used;
				totalBytes += len;
				printf( "%s", webpage->request.topLine->bytes );
				bytesWritten += write( fd, webpage->request.topLine->bytes, len );
				totalBytes += 2;
				bytesWritten += write( fd, "\r\n", 2 );
				printf( "\r\n" );
				if ( webpage->request.headers != NULL ) {
					len = webpage->request.headers->used;
					totalBytes += len;
					printf( "%s", webpage->request.headers->bytes );
					bytesWritten += write( fd, webpage->request.headers->bytes, len );  //  headers need to end with \r\n
				}
				totalBytes += 2;
				printf( "\r\n" );
				bytesWritten += write( fd, "\r\n", 2 );
				if ( webpage->request.content != NULL ) {
					len = webpage->request.content->used;
					totalBytes += len;
					printf( "%s", webpage->request.content->bytes );
					bytesWritten += write( fd, webpage->request.content->bytes, len );
				}
				switch ( bytesWritten ) {
					case 0:
						Webclient_CloseConn( webclient );
						break;
					case -1:
						if ( errno == EAGAIN || errno == EWOULDBLOCK ) {
							//pass
						} else {
							Webclient_CloseConn( webclient );
						}
					default:
						if ( (size_t) bytesWritten != totalBytes ) {
							Webclient_CloseConn( webclient );
						} else {
						}
						break;
				}
			} else {
				Webclient_CloseConn( webclient );
			}
		}
	}
}

static void Webclient_HandleConnect_cb( picoev_loop * loop, int fd, int events, void * wcArgs ) {
	struct webclient_t * webclient;

	webclient = (struct webclient_t *) wcArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Webclient_CloseConn( webclient );
	} else if ( ( events & PICOEV_READWRITE ) != 0 ) {
		picoev_del( loop, fd );
		picoev_add( loop, fd, PICOEV_WRITE, webclient->timeoutSec, Webclient_HandleWrite_cb, wcArgs );
	}
}

static struct webpage_t * Webpage_New( struct webclient_t * webclient, const enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb ) {
	struct webpage_t * webpage;
	UriParserStateA state;
	size_t headerLen, hostLen, connLen;
	size_t addcrcn;
	struct {unsigned char good:1;
			unsigned char webpage:1;
			unsigned char uri:1;
			unsigned char headers:1;
			unsigned char content:1;
			unsigned char url:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( webpage = malloc( sizeof( *webpage ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.webpage = 1;
		webpage->handlerCb = handlerCb;
		webpage->cbArgs = cbArgs;
		webpage->clearFuncCb = clearFuncCb;
		webpage->mode = mode;
		webpage->request.topLine = NULL;
		webpage->request.headers = NULL;
		webpage->request.content = NULL;
		webpage->response.headers = NULL;
		webpage->response.content = NULL;
		webpage->response.httpCode = HTTPCODE_NONE;
		cleanUp.good = ( ( webpage->url = Xstrdup( url ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		PR_INIT_CLIST( &webpage->mLink );
		state.uri = &webpage->uri;
		cleanUp.good = ( uriParseUriA( &state, webpage->url ) == URI_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.uri = 1;
		cleanUp.good = ( webpage->uri.scheme.first == NULL || strncmp( webpage->uri.scheme.first, "http", 4 ) == 0 );
	}
	hostLen = 0;
	addcrcn = 0;
	if ( cleanUp.good ) {
		if ( headers != NULL ) {
			//  use the header, make shure it ends with \r\n
			headerLen = strlen( headers );
			if ( headerLen > 2 && headers[headerLen - 2] != '\r' && headers[headerLen - 1] != '\n' ) {
				addcrcn = 2;
			} else {
				//  @TODO:  edge case
			}
			cleanUp.good = ( ( webpage->request.headers = Buffer_New( headerLen + addcrcn + 1 ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.headers = 1;
				cleanUp.good = ( Buffer_Append( webpage->request.headers, headers, headerLen + addcrcn ) == 1 );
			}
			if ( cleanUp.good ) {
				if ( addcrcn ) {
					cleanUp.good = ( Buffer_Append( webpage->request.headers, "\r\n", 2 ) == 1 );
				}
			}
		} else {
			//  set a default header with host + connection minimum
			hostLen = (size_t) ( webpage->uri.hostText.afterLast - webpage->uri.hostText.first );
			connLen = ( CONNECTION_CLOSE == webclient->connection ) ? 5 : 10;
			headerLen = 25 + hostLen + connLen;
			cleanUp.good = ( ( webpage->request.headers = Buffer_New( headerLen ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.headers = 1;
				cleanUp.headers = 1;
				cleanUp.good = ( Buffer_Append( webpage->request.headers, "Host: ", 6 ) == 1 );
			}
			if ( cleanUp.good ) {
				cleanUp.good = ( Buffer_Append( webpage->request.headers, webpage->uri.hostText.first, hostLen ) == 1 );
			}
			if ( cleanUp.good ) {
				cleanUp.good = ( Buffer_Append( webpage->request.headers, "\r\nConnection: ", 14 ) == 1 );
			}
			if ( cleanUp.good ) {
				cleanUp.good = ( Buffer_Append( webpage->request.headers, ConnectionDefinitions[webclient->connection], connLen ) == 1 );
			}
			if ( cleanUp.good ) {
				cleanUp.good = ( Buffer_Append( webpage->request.headers, "\r\n", 2 ) == 1 );
			}
		}
	}
	if ( cleanUp.good ) {
		if ( content != NULL ) {
			cleanUp.good = ( ( webpage->request.content = Buffer_NewText( content ) ) != NULL );
		}
	}
	if ( cleanUp.good ) {
		cleanUp.content = 1;
		Webclient_PushWebpage( webclient, webpage );
		//  Let's see if we can submit this immediately to the server
		Webclient_PopWebpage( webclient );
		if ( webclient->socketFd < 1 ) {
			Webclient_Connect( webclient );
		}
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.content ) {
			free( webpage->request.content ); webpage->request.content = NULL;
		}
		if ( cleanUp.headers ) {
			free( webpage->request.headers ); webpage->request.headers = NULL;
		}
		if ( cleanUp.uri ) {
			uriFreeUriMembersA( &webpage->uri );
		}
		if ( cleanUp.url ) {
			free( webpage->url ); webpage->url = NULL;
		}
		if ( cleanUp.webpage ) {
			if ( webpage->clearFuncCb != NULL && webpage->cbArgs != NULL ) {
				webpage->clearFuncCb( webpage->cbArgs );
			}
			webpage->handlerCb = NULL;
			webpage->mode = MODE_GET;
			webpage->cbArgs = NULL;
			webpage->request.topLine = NULL;
			webpage->request.headers = NULL;
			webpage->request.content = NULL;
			webpage->response.headers = NULL;
			webpage->response.content = NULL;
			webpage->response.httpCode = HTTPCODE_NONE;
			webpage->clearFuncCb = NULL;
			free( webpage ); webpage = NULL;
		}
	}

	return webpage;
}

#define HTTP_VERSION "HTTP/1.1"
static struct buffer_t * Webpage_TopLine( struct webpage_t * webpage ) {
	size_t modeStringLen, hostLen, topLineLen, versionLen;
	const char * modeString;
	struct {unsigned char good:1;
			unsigned char topLine:1;} cleanUp;
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	//  portLen = ( uri->portText.first != NULL ) ? ( uri->portText.afterLast - uri->portText.first ) : 0;
	modeString = MethodDefinitions[webpage->mode];
	modeStringLen = strlen( modeString );
	versionLen = strlen( HTTP_VERSION );
	hostLen = (size_t) ( webpage->uri.hostText.afterLast - webpage->uri.hostText.first );
	topLineLen = modeStringLen + 3 * hostLen +  versionLen + 4; //  2x' ' + 1x'/'+ 1x'\0'
	if ( webpage->request.topLine == NULL ) {
		cleanUp.good = ( ( webpage->request.topLine = Buffer_New( topLineLen ) ) != NULL );
	} else {
		cleanUp.good = ( Buffer_Reset( webpage->request.topLine, topLineLen ) == 1 );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, modeString, modeStringLen ) == 1 );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, " /", 2 ) == 1 );
	}
	if ( cleanUp.good ) {
		//  BUFFER HACK
		uriEscapeExA( webpage->uri.pathHead->text.first, webpage->uri.pathHead->text.afterLast, &webpage->request.topLine->bytes[webpage->request.topLine->used], URI_TRUE, URI_FALSE );
		webpage->request.topLine->used = strlen( webpage->request.topLine->bytes );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, " ", 1 ) == 1 );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, HTTP_VERSION, versionLen ) == 1 );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.topLine ) {
			free( webpage->request.topLine ); webpage->request.topLine = NULL;
		}
	}
	return webpage->request.topLine;
}

static void Webpage_Delete( struct webpage_t * webpage ) {
	if ( webpage->clearFuncCb != NULL && webpage->cbArgs != NULL ) {
		webpage->clearFuncCb( webpage->cbArgs );
	}
	if ( webpage->request.topLine != NULL ) {
		Buffer_Delete( webpage->request.topLine ); webpage->request.topLine = NULL;
	}
	if ( webpage->request.headers != NULL ) {
		Buffer_Delete( webpage->request.headers ); webpage->request.headers = NULL;
	}
	if ( webpage->request.content != NULL ) {
		Buffer_Delete( webpage->request.content ); webpage->request.content = NULL;
	}
	webpage->response.httpCode = HTTPCODE_NONE;
	webpage->handlerCb = NULL;
	webpage->mode = MODE_GET;
	webpage->cbArgs = NULL;
	webpage->clearFuncCb = NULL;
	uriFreeUriMembersA( &webpage->uri );
	free( webpage->url ); webpage->url = NULL;
	free( webpage ); webpage = NULL;
}

static void Webclient_Connect( struct webclient_t * webclient ) {
	struct sockaddr_in serverAddr;
	UriUriA * uri;
	char portString[7];
	size_t len;
	struct {unsigned char conn:1;
			unsigned char ev:1;
			unsigned char socket:1;
			unsigned char ip:1;
			unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	memset( &portString[0], '\0', sizeof( portString ) );
	cleanUp.good = ( ( webclient->socketFd = socket( AF_INET, SOCK_STREAM, 0 ) ) != -1 );
	if ( cleanUp.good ) {
		uri = &webclient->currentWebpage->uri;
		len = (size_t) ( uri->hostText.afterLast - uri->hostText.first );
		cleanUp.good = ( ( webclient->ip = malloc( len + 1 ) ) != NULL ); // for now: assume hostName = Ip
	}
	if ( cleanUp.good )  {
		cleanUp.ip = 1;
		snprintf( webclient->ip, len + 1, "%s", uri->hostText.first );
		len = (size_t) ( uri->portText.afterLast - uri->portText.first );
		snprintf( &portString[0], len + 1, "%s", uri->portText.first );
		webclient->port = (uint16_t) atoi( &portString[0] );
		if ( webclient->port == 0 ) {
			if ( uri->scheme.first != NULL )  {
				if ( strncmp( uri->scheme.first, "https", 5 ) == 0 ) {
					webclient->port = 443;
				} else if ( strncmp( uri->scheme.first, "http", 4 ) == 0 ) {
					webclient->port = 80;
				}
			} else {
				webclient->port = 80;
			}
		}
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons( webclient->port );
		serverAddr.sin_addr.s_addr = inet_addr( webclient->ip );
		cleanUp.good =  ( connect( webclient->socketFd, (struct sockaddr *) &serverAddr, sizeof( serverAddr ) ) != -1 );
	}
	if ( cleanUp.good ) {
		SetupSocket( webclient->socketFd, 1 );
		picoev_add( webclient->core->loop, webclient->socketFd, PICOEV_READWRITE, webclient->timeoutSec , Webclient_HandleConnect_cb, (void *) webclient );
	}
	if ( cleanUp.good ) {
		cleanUp.ev = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.socket ) {
			close( webclient->socketFd ); webclient->socketFd = 0;
		}
		if ( cleanUp.ip ) {
			free( webclient->ip ); webclient->ip = NULL;
		}
		if ( cleanUp.ev ) {
			picoev_del( webclient->core->loop, webclient->socketFd );
		}
	}
}

struct webclient_t * Webclient_New( const struct core_t * core, enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb, const unsigned char timeoutSec ) {
	struct webpage_t * webpage;
	struct webclient_t * webclient;
	struct cfg_t * webclientSection, * modulesSection;
	modulesSection = cfg_getnsec( (cfg_t *) core->config, "modules", 0 );
	webclientSection = cfg_getnsec( modulesSection, "webclient", 0 );
	unsigned char timeSec;
	struct {unsigned char good:1;
			unsigned char webpage:1;
			unsigned char webclient:1;} cleanUp;

	memset(  &cleanUp, 0, sizeof(  cleanUp  )  );
	timeSec = timeoutSec;
	webpage = NULL;
	cleanUp.good = (  (  webclient = malloc(  sizeof(  * webclient  )  )  ) != NULL  );
	if (  cleanUp.good  ) {
		cleanUp.webclient = 1;
		webclient->timeoutSec = timeoutSec;
		webclient->webpages = NULL;
		webclient->currentWebpage = NULL;
		webclient->socketFd = 0;
		webclient->ip = NULL;
		webclient->core = core;
		webclient->connection = CONNECTION_CLOSE;
		if ( timeoutSec == 0 ) {
			timeSec = (unsigned char) cfg_getint( webclientSection, "timeout_sec" );
		}
		if ( timeSec == 0 ) {
			timeSec = PR_CFG_MODULES_WEBCLIENT_TIMEOUT_SEC;
		}
		//  we cannot connect at this point as the host and the port are in the url
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( webpage = Webpage_New( webclient, mode, url, headers, content, handlerCb, cbArgs, clearFuncCb ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.webpage = 1;
		// if the url is valid, the push routine tries to connect immediately
		Webclient_PushWebpage( webclient, webpage );
	}
	if (  ! cleanUp.good  ) {
		if ( cleanUp.webpage ) {
			Webpage_Delete( webpage ); webpage = NULL;
		}
		if ( cleanUp.webclient ) {
			webclient->timeoutSec = 0;
			webclient->webpages = NULL;
			webclient->currentWebpage = NULL;
			webclient->core = NULL;
			webclient->socketFd = 0;
			webclient->ip = NULL;
			free(  webclient  ); webclient = NULL;
		}
	}

	return webclient;
}

struct webpage_t * Webclient_Queue( struct webclient_t * webclient, enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb ) {
	return Webpage_New( webclient, mode, url, headers, content, handlerCb, cbArgs, clearFuncCb );
}
//  @TODO:  if the first was set and the next has defaults this breaks. e.g. http://www.urbin.info:80/index.html vs www:urbin.info/benchmark.html it even gets uglier if the dns name and the ip adress are the same
#define CHECK_SAME_CONN( field ) do \
	if ( current->uri.field.first != NULL && current->uri.field.afterLast != NULL && \
		 webpage->uri.field.first != NULL && webpage->uri.field.afterLast != NULL \
		 ) { \
			len = (size_t) ( current->uri.field.afterLast - current->uri.field.first ); \
			good.field = ( strncmp( current->uri.field.first, webpage->uri.field.first, len ) == 0 ); \
	} else { \
		good.field = 1; \
	} \
	while ( 0 );

static int Webclient_CanUseConn( struct webclient_t * webclient, struct webpage_t * webpage ) {
	struct webpage_t * current;
	size_t len;
	struct {unsigned char good:1;
			unsigned char scheme:1;
			unsigned char userInfo:1;
			unsigned char hostText:1;
			unsigned char portText:1;
			} good;

	memset(  &good, 0, sizeof(  good  )  );
	current = webclient->currentWebpage;
	if ( current == NULL ) {
		//  no connection yet, can proceed
		good.good = 1;
	} else {
		//  investigate uriEqualsUriA function
		CHECK_SAME_CONN( scheme );
		CHECK_SAME_CONN( userInfo );
		CHECK_SAME_CONN( hostText );
		CHECK_SAME_CONN( portText );
	}
	return ( good.good ) ? 1 : 0;
}

static void Webclient_PushWebpage( struct webclient_t * webclient, struct webpage_t * webpage ) {
	if ( webpage != NULL ) {

		if ( Webclient_CanUseConn( webclient, webpage ) ) {
			if ( webclient->webpages == NULL ) {
				webclient->webpages = webpage;
			} else {
				PR_INSERT_BEFORE( &webpage->mLink, &webclient->webpages->mLink );
			}
			Core_Log( webclient->core, LOG_INFO, __FILE__ , __LINE__, "New Webpage allocated" );
		}
	}
}

static struct webpage_t * Webclient_PopWebpage( struct webclient_t * webclient ) {
	PRCList * next;

	if ( webclient->currentWebpage == NULL ) {
		if ( webclient->webpages != NULL ) {
			webclient->currentWebpage = webclient->webpages;
			if ( PR_CLIST_IS_EMPTY( &webclient->currentWebpage->mLink ) ) {
				webclient->webpages = NULL;
				webclient->connection = CONNECTION_CLOSE;
			} else {
				webclient->connection = CONNECTION_KEEPALIVE;
				next = PR_NEXT_LINK( &webclient->currentWebpage->mLink );
				webclient->webpages = FROM_NEXT_TO_ITEM( struct webpage_t );
			}
			PR_REMOVE_AND_INIT_LINK( &webclient->currentWebpage->mLink );
		}
	}
	return webclient->currentWebpage;
}

void Webclient_Delete( struct webclient_t * webclient ) {
	struct webpage_t * webpage;
	//  clean the webpages
	if ( webclient->currentWebpage != NULL ) {
		Webpage_Delete( webclient->currentWebpage ); webclient->currentWebpage = NULL;
	}
	do {
		webpage = Webclient_PopWebpage( webclient );
		Webpage_Delete( webpage ); webclient->currentWebpage = NULL;  //  pop sets also current webpage
	} while ( webpage != NULL );
	//  clean the rest
	webclient->timeoutSec = 0;
	webclient->socketFd = 0;
	webclient->core = NULL;
	webclient->connection = CONNECTION_CLOSE;
	if ( webclient->ip != NULL ) {
		free( webclient->ip ); webclient->ip = NULL;
	}
	free(  webclient  ); webclient = NULL;
}

