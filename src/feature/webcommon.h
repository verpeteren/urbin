#ifndef SRC_FEATURE_WEBCOMMON_H_
#define SRC_FEATURE_WEBCOMMON_H_


#include <time.h>
#include <stdint.h>

#include <h3.h>

#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_READ_BUFFER_LENGTH 2048
#define HTTP_READ_BUFFER_LIMIT  1024 * 1024 * 50  //  50 MB

#define H3_HEADERS_BLOCK( header, start, end, found ) do {\
	start = end = NULL; \
	if ( (header)->Fields != NULL ) { \
		start = (char *) (header)->Fields[0].Value;\
		end = (char *) ( (header)->Fields[(header)->HeaderSize].Value + (header)->Fields[(header)->HeaderSize].ValueLen ) ;\
	} else if ( (header)->RequestLineEnd > (header)->RequestLineEnd ) { \
		end = start = ( char *) ( (header)->RequestLineEnd + 4 ); \
	} \
	found = ( start != NULL ); \
} while( 0 );

enum requestMode_t {
	MODE_GET 								 = 0,
	MODE_POST 								 = 1
};

enum connection_t{
	CONNECTION_CLOSE 						 = 0,
	CONNECTION_KEEPALIVE 					 = 1
};

//  from https://docs.python.org/2/howto/urllib2.html
enum httpCode_t {
	HTTPCODE_NONE							 = 0,
	HTTPCODE_ERROR							 = 500,
	HTTPCODE_OK								 = 200,
	HTTPCODE_CREATED						 = 201,
	HTTPCODE_ACCEPTED						 = 202,
	HTTPCODE_NON_AUTHORITATIVE_INFORMATION	 = 203,
	HTTPCODE_NO_CONTENT						 = 204,
	HTTPCODE_RESET_CONTENT					 = 205,
	HTTPCODE_PARTIAL_CONTENT				 = 206,
	HTTPCODE_BAD_REQUEST					 = 400,
	HTTPCODE_UNAUTHORIZED					 = 401,
	HTTPCODE_PAYMENT_REQUIRED				 = 402,
	HTTPCODE_FORBIDDEN						 = 403,
	HTTPCODE_NOT_FOUND						 = 404,
	HTTPCODE_METHOD_NOT_ALLOWED				 = 405,
	HTTPCODE_NOT_ACCEPTABLE					 = 406,
	HTTPCODE_PROXY_AUTHENTICATION_REQUIRED	 = 407,
	HTTPCODE_REQUEST_TIMEOUT				 = 408,
	HTTPCODE_CONFLICT						 = 409,
	HTTPCODE_GONE							 = 410,
	HTTPCODE_LENGTH_REQUIRED				 = 411,
	HTTPCODE_PRECONDITION_FAILED			 = 412,
	HTTPCODE_REQUEST_ENTITY_TOO_LARGE		 = 413,
	HTTPCODE_REQUEST_URI_TOO_LONG			 = 414,
	HTTPCODE_UNSUPPORTED_MEDIA_TYPE			 = 415,
	HTTPCODE_REQUESTED_RANGE_NOT_SATISFIABLE = 416,
	HTTPCODE_EXPECTATION_FAILED				 = 417,
	HTTPCODE_CONTINUE						 = 100,
	HTTPCODE_SWITCHING_PROTOCOLS			 = 101,
	HTTPCODE_MULTIPLE_CHOICES				 = 300,
	HTTPCODE_MOVED_PERMANENTLY				 = 301,
	HTTPCODE_FOUND							 = 302,
	HTTPCODE_SEE_OTHER						 = 303,
	HTTPCODE_NOT_MODIFIED					 = 304,
	HTTPCODE_USE_PROXY						 = 305,
	HTTPCODE_TEMPORARY_REDIRECT				 = 307,
	HTTPCODE_INTERNAL_SERVER_ERROR			 = 500,
	HTTPCODE_NOT_IMPLEMENTED				 = 501,
	HTTPCODE_BAD_GATEWAY					 = 502,
	HTTPCODE_SERVICE_UNAVAILABLE			 = 503,
	HTTPCODE_GATEWAY_TIMEOUT				 = 504,
	HTTPCODE_HTTP_VERSION_NOT_SUPPORTED		 = 505,
	__HTTPCODE_LAST							 = 999,
};



#ifdef __cplusplus
}
#endif

#endif  // SRC_FEATURE_WEBCOMMON_H_

