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
static void 					Webclient_GenTopLine			( struct webclient_t * webclient, struct webpage_t * webpage );
static void 					Webclient_GenHeader			( struct webclient_t * webclient, struct webpage_t * webpage );
static void 					Webclient_PushWebpage		( struct webclient_t * webclient, struct webpage_t * webpage );
static struct webpage_t * 		Webclient_PopWebpage		( struct webclient_t * webclient );

static void 					Webpage_Delete				( struct webpage_t * webpage );

static void Webclient_CloseConn( struct webclient_t * webclient ) {
	picoev_del( webclient->core->loop, webclient->socketFd );
	close( webclient->socketFd ); webclient->socketFd = 0;
}

static void Webclient_HandleRead_cb( picoev_loop * loop, int fd, int events, void * wcArgs ) {
	struct webclient_t * webclient;
	struct webpage_t * webpage;
	ssize_t didReadBytes, canReadBytes;
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
		if ( webpage->response.buffer != NULL ) {
			cleanUp.good = 1;
		} else {
			cleanUp.good = ( ( webpage->response.buffer = Buffer_New( HTTP_READ_BUFFER_LENGTH ) ) != NULL );
		}
tryToReadMoreWebclient:
		if ( cleanUp.good ) {
			canReadBytes = webpage->response.buffer->size - webpage->response.buffer->used;
			cleanUp.raw = 1;
			//  BUFFER HACK
			didReadBytes = read( fd, &webpage->response.buffer->bytes[webpage->response.buffer->used], canReadBytes );
			switch ( didReadBytes ) {
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
				webpage->response.buffer->used += (size_t) didReadBytes;
				if ( didReadBytes == canReadBytes ) {
					// There is more to read
					if ( webpage->response.buffer->used > HTTP_READ_BUFFER_LIMIT ) {
						Webclient_CloseConn( webclient );
						break;
					}
					cleanUp.good = ( Buffer_Increase( webpage->response.buffer, HTTP_READ_BUFFER_LENGTH ) == PR_SUCCESS );
					goto tryToReadMoreWebclient;
				}
				webpage->response.buffer->bytes[webpage->response.buffer->used] = '\0';
				if ( h3_request_header_parse( webpage->response.header, webpage->response.buffer->bytes, webpage->response.buffer->used ) == 0 ) {
					//  @TODO:  check the H3_ERROR enum, how we are doing right now
				}
				if ( webpage->handlerCb != NULL ) {
					webpage->handlerCb( webpage );
				}
				Webpage_Delete( webpage ); webpage = NULL;
				webclient->currentWebpage = NULL;
				if ( CONNECTION_KEEPALIVE == webclient->connection ) {
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
	size_t len;
	ssize_t wroteBytes;
	int done;

	webclient = (struct webclient_t *) wcArgs;
	done = 0;
	len = 0;
	wroteBytes = 0;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Webclient_CloseConn( webclient );
	} else if ( ( events & PICOEV_WRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, webclient->timeoutSec );
		//  Let's get some work
		webpage = Webclient_PopWebpage( webclient );
		if ( webpage != NULL ) {
tryToWriteMoreWebclient:
			switch ( webpage->sendingNow ) {
			case SENDING_NONE:  // we start here
				webpage->sendingNow = SENDING_TOPLINE;
				// fallthrough
			case SENDING_TOPLINE:
				if ( webpage->request.topLine == NULL ) {
					Webclient_GenTopLine( webclient, webpage );
				}
				if ( webpage->request.topLine != NULL ) {
					len = webpage->request.topLine->used;
					wroteBytes = write( fd, &webpage->request.topLine->bytes[webpage->wroteBytes], len - webpage->wroteBytes );
				} else {
					Webclient_CloseConn( webclient );
				}
				break;
			case SENDING_HEADER:
				if ( webpage->request.headers == NULL ) {
					Webclient_GenHeader( webclient, webpage );
				}
				if ( webpage->request.headers != NULL ) {
					len = webpage->request.headers->used;
					wroteBytes = write( fd, &webpage->request.headers->bytes[webpage->wroteBytes], len - webpage->wroteBytes );  //  headers need to end with \r\n\n
				} else {
					Webclient_CloseConn( webclient );
				}
				if ( webpage->mode == MODE_GET ) {
					done = 1;
				}
				break;
			case SENDING_CONTENT:
				if ( webpage->request.content == NULL ) {
					done = 1;
				} else {
					len = webpage->request.content->used;
					wroteBytes = write( fd, &webpage->request.content->bytes[webpage->wroteBytes], len - webpage->wroteBytes );
					done = ( (size_t) webpage->wroteBytes + wroteBytes == len );
				}
				break;
			case SENDING_FILE:  //  Not applicable
			default:
				break;
			}
			if ( done ) {
				picoev_del( loop, fd );
				picoev_add( loop, fd, PICOEV_READ, webclient->timeoutSec, Webclient_HandleRead_cb, wcArgs );
			} else {
				switch ( wroteBytes ) {
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
					webpage->wroteBytes += wroteBytes;
					if ( len == (size_t) webpage->wroteBytes ) {
						webpage->wroteBytes = 0;
						switch ( webpage->sendingNow ) {
						case SENDING_TOPLINE:
							webpage->sendingNow = SENDING_HEADER;
							goto tryToWriteMoreWebclient;
							break;
						case SENDING_HEADER:
							webpage->sendingNow = SENDING_CONTENT;
							goto tryToWriteMoreWebclient;
							break;
						case SENDING_CONTENT:  //  FT
						case SENDING_FILE:  //  Not applicable
						case SENDING_NONE:  //  FT
						default:
							break;
						}
						break;
					}
				}
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
	size_t headerLen;
	size_t addcrcn;
	struct {unsigned char good:1;
			unsigned char webpage:1;
			unsigned char uri:1;
			unsigned char h3:1;
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
		webpage->response.header = NULL;
		webpage->response.buffer = NULL;
		webpage->sendingNow = SENDING_NONE;
		webpage->wroteBytes = 0;
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
	addcrcn = 0;
	if ( cleanUp.good ) {
		if ( headers != NULL ) {
			//  use the header, make shure it ends with \r\n\r\n
			headerLen = strlen( headers );
 			if ( headerLen > 4	 && ( headers[headerLen - 4] == '\r' && headers[headerLen - 3] == '\n' )
								 && ( headers[headerLen - 2] == '\r' && headers[headerLen - 1] == '\n' ) ) {
				addcrcn = 0;
			} else if ( headerLen > 2 && ( headers[headerLen - 2] == '\r' && headers[headerLen - 1] == '\n' ) ) {
				addcrcn = 2;
			} else {
				addcrcn = 4;
				//  @TODO:  edge cases
			}
			cleanUp.good = ( ( webpage->request.headers = Buffer_New( headerLen + addcrcn + 1 ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.headers = 1;
				cleanUp.good = ( Buffer_Append( webpage->request.headers, headers, headerLen ) == PR_SUCCESS );
			}
			if ( cleanUp.good ) {
				while ( addcrcn != 0 ) {
					cleanUp.good = ( Buffer_Append( webpage->request.headers, "\r\n", 2 ) == PR_SUCCESS );
					addcrcn -= 2;
				}
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
		cleanUp.good = ( ( webpage->response.header = h3_request_header_new( ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.h3 = 1;
		if ( webclient->currentWebpage != NULL || webclient->webpages != NULL ) {
			webclient->connection = CONNECTION_KEEPALIVE;
		}
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
		if ( cleanUp.h3 ) {
			h3_request_header_free( webpage->response.header ); webpage->response.header = NULL;
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
			webpage->response.buffer = NULL;
			webpage->response.httpCode = HTTPCODE_NONE;
			webpage->clearFuncCb = NULL;
			free( webpage ); webpage = NULL;
		}
	}

	return webpage;
}

#define HTTP_VERSION "HTTP/1.1"
static void Webclient_GenTopLine( struct webclient_t * webclient, struct webpage_t * webpage ) {
	size_t modeStringLen, hostLen, topLineLen, versionLen;
	const char * modeString;
	struct {unsigned char good:1;
			unsigned char topLine:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	modeString = MethodDefinitions[webpage->mode];
	modeStringLen = strlen( modeString );
	versionLen = strlen( HTTP_VERSION );
	hostLen = (size_t) ( webpage->uri.hostText.afterLast - webpage->uri.hostText.first );
	topLineLen = modeStringLen + 3 * hostLen + versionLen + 6; //  2x' ' + 1x'/'+ 1x'\0' + 1 x \r\n
	if ( webpage->request.topLine == NULL ) {
		cleanUp.good = ( ( webpage->request.topLine = Buffer_New( topLineLen ) ) != NULL );
	} else {
		cleanUp.good = ( Buffer_Reset( webpage->request.topLine, topLineLen ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, modeString, modeStringLen ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, " /", 2 ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		//  BUFFER HACK
		uriEscapeExA( webpage->uri.pathHead->text.first, webpage->uri.pathHead->text.afterLast, &webpage->request.topLine->bytes[webpage->request.topLine->used], URI_TRUE, URI_FALSE );
		webpage->request.topLine->used = strlen( webpage->request.topLine->bytes );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, " ", 1 ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, HTTP_VERSION, versionLen ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.topLine, "\r\n", 2 ) == PR_SUCCESS );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.topLine ) {
			free( webpage->request.topLine ); webpage->request.topLine = NULL;
		}
	}
}

static void Webclient_GenHeader( struct webclient_t * webclient, struct webpage_t * webpage ) {
	size_t hostLen, connLen, headerLen;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	hostLen = (size_t) ( webpage->uri.hostText.afterLast - webpage->uri.hostText.first );
	connLen = ( CONNECTION_CLOSE == webclient->connection ) ? 5 : 10;
	headerLen = 27 + hostLen + connLen;
	cleanUp.good = ( ( webpage->request.headers = Buffer_New( headerLen ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.headers, "Host: ", 6 ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.headers, webpage->uri.hostText.first, hostLen ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.headers, "\r\nConnection: ", 14 ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.headers, ConnectionDefinitions[webclient->connection], connLen ) == PR_SUCCESS );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Buffer_Append( webpage->request.headers, "\r\n\r\n", 4 ) == PR_SUCCESS );
	}
}

static void Webpage_Delete( struct webpage_t * webpage ) {
	if ( webpage->clearFuncCb != NULL && webpage->cbArgs != NULL ) {
		webpage->clearFuncCb( webpage->cbArgs );
	}
	//  request
	if ( webpage->request.topLine != NULL ) {
		Buffer_Delete( webpage->request.topLine ); webpage->request.topLine = NULL;
	}
	if ( webpage->request.headers != NULL ) {
		Buffer_Delete( webpage->request.headers ); webpage->request.headers = NULL;
	}
	if ( webpage->request.content != NULL ) {
		Buffer_Delete( webpage->request.content ); webpage->request.content = NULL;
	}
	//  response
	if ( webpage->response.header != NULL ) {
		h3_request_header_free( webpage->response.header ); webpage->response.header = NULL;
	}
	if ( webpage->response.buffer != NULL ) {
		Buffer_Delete( webpage->response.buffer ); webpage->response.buffer = NULL;
	}
	webpage->response.httpCode = HTTPCODE_NONE;
	webpage->handlerCb = NULL;
	webpage->mode = MODE_GET;
	webpage->sendingNow = SENDING_NONE;
	webpage->wroteBytes = 0;
	webpage->cbArgs = NULL;
	webpage->clearFuncCb = NULL;
	uriFreeUriMembersA( &webpage->uri );
	PR_INIT_CLIST( &webpage->mLink );
	free( webpage->url ); webpage->url = NULL;
	free( webpage ); webpage = NULL;
}

static void Webclient_ConnectToIp( struct dns_cb_data * dnsData ) {
	struct webclient_t * webclient;
	struct sockaddr_in serverAddr;
	char *ip;
	struct { unsigned char good:1;
			unsigned char ip:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	ip = NULL;
	webclient = (struct webclient_t *) dnsData->context;
	webclient->core->dns.actives--;
	cleanUp.good = ( dnsData->addr_len > 0 );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( ip = DnsData_ToString( dnsData ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.ip = 1;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons( webclient->port );
		serverAddr.sin_addr.s_addr = inet_addr( ip );
		cleanUp.good = ( connect( webclient->socketFd, (struct sockaddr *) &serverAddr, sizeof( serverAddr ) ) == 0 );
	}
	if ( cleanUp.good ) {
		SetupSocket( webclient->socketFd, 1 );
		picoev_add( webclient->core->loop, webclient->socketFd, PICOEV_READWRITE, webclient->timeoutSec , Webclient_HandleConnect_cb, (void *) webclient );
	}
	//  always cleanup
	if ( cleanUp.ip ) {
		free( ip ); ip = NULL;
	}
}

static void Webclient_Connect( struct webclient_t * webclient ) {
	UriUriA * uri;
	char portString[7];
	size_t len;
	struct {unsigned char conn:1;
			unsigned char hostName:1;
			unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	memset( &portString[0], '\0', sizeof( portString ) );
	cleanUp.good = ( ( webclient->socketFd = socket( AF_INET, SOCK_STREAM, 0 ) ) != -1 );
	if ( cleanUp.good ) {
		uri = &webclient->currentWebpage->uri;
		len = (size_t) ( uri->hostText.afterLast - uri->hostText.first );
		cleanUp.good = ( ( webclient->hostName = malloc( len + 1 ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.hostName = 1;
		snprintf( webclient->hostName, len + 1, "%s", uri->hostText.first );
		len = (size_t) ( uri->portText.afterLast - uri->portText.first );
		snprintf( &portString[0], len + 1, "%s", uri->portText.first );
		webclient->port = (uint16_t) atoi( &portString[0] );
		if ( webclient->port == 0 ) {
			if ( uri->scheme.first != NULL ) {
				if ( strncmp( uri->scheme.first, "https", 5 ) == 0 ) {
					webclient->port = 443;
				} else if ( strncmp( uri->scheme.first, "http", 4 ) == 0 ) {
					webclient->port = 80;
				}
			} else {
				webclient->port = 80;
			}
		}
		Core_GetHostByName( webclient->core, webclient->hostName, Webclient_ConnectToIp, (void *) webclient );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.hostName ) {
			free( webclient->hostName ); webclient->hostName = NULL;
		}
	}
}

struct webclient_t * Webclient_New( const struct core_t * core, enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb, const uint8_t timeoutSec ) {
	struct webpage_t * webpage;
	struct webclient_t * webclient;
	struct {unsigned char good:1;
			unsigned char webpage:1;
			unsigned char webclient:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webpage = NULL;
	cleanUp.good = ( ( webclient = malloc( sizeof( * webclient ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.webclient = 1;
		webclient->webpages = NULL;
		webclient->currentWebpage = NULL;
		webclient->socketFd = 0;
		webclient->hostName = NULL;
		webclient->core = (struct core_t *) core;
		webclient->connection = CONNECTION_CLOSE;
		webclient->timeoutSec = (timeoutSec == 0) ? (uint8_t) PR_CFG_MODULES_WEBCLIENT_TIMEOUT_SEC: timeoutSec;
		//  we cannot connect at this point as the host and the port are in the url
	}
	if ( cleanUp.good ) {
		//  here we try to connect and queue at the same time
		cleanUp.good = ( ( webpage = Webpage_New( webclient, mode, url, headers, content, handlerCb, cbArgs, clearFuncCb ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.webpage = 1;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.webpage ) {
			Webpage_Delete( webpage ); webpage = NULL;
		}
		if ( cleanUp.webclient ) {
			webclient->timeoutSec = 0;
			webclient->webpages = NULL;
			webclient->currentWebpage = NULL;
			webclient->core = NULL;
			webclient->socketFd = 0;
			webclient->hostName = NULL;
			free( webclient ); webclient = NULL;
		}
	}

	return webclient;
}

struct webpage_t * Webclient_Queue( struct webclient_t * webclient, enum requestMode_t mode, const char * url, const char * headers, const char * content, const webclientHandler_cb_t handlerCb, void * cbArgs, const clearFunc_cb_t clearFuncCb ) {
	return Webpage_New( webclient, mode, url, headers, content, handlerCb, cbArgs, clearFuncCb );
}
#define CHECK_SAME_CONN( field ) do \
	if ( current->uri.field.first != NULL && current->uri.field.afterLast != NULL && \
		 webpage->uri.field.first != NULL && webpage->uri.field.afterLast != NULL \
		 ) { \
			len = (size_t) ( current->uri.field.afterLast - current->uri.field.first ); \
			good.good = good.field = ( strncmp( current->uri.field.first, webpage->uri.field.first, len ) == 0 ); \
	} else { \
		good.good = good.field = 1; \
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

	memset( &good, 0, sizeof( good ) );
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
				webclient->connection = CONNECTION_CLOSE;
				webclient->webpages = NULL;
			} else {
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
	while ( ( webpage = Webclient_PopWebpage( webclient ) ) != NULL ) {
		Webpage_Delete( webpage ); webclient->currentWebpage = NULL;  //  pop sets also current webpage
	}
	//  clean the rest
	webclient->timeoutSec = 0;
	webclient->socketFd = 0;
	webclient->core = NULL;
	webclient->connection = CONNECTION_CLOSE;
	if ( webclient->hostName != NULL ) {
		free( webclient->hostName ); webclient->hostName = NULL;
	}
	free( webclient ); webclient = NULL;
}

