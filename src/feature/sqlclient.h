#ifndef SRC_FEATURE_SQLCLIENT_H_
#define SRC_FEATURE_SQLCLIENT_H_

#include <stdint.h>
#include <stddef.h>

#include "../core/core.h"

#include <libpq-fe.h>
#if HAVE_MYSQL == 1
#include <mysac.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE_MYSQL == 1
#define MYSQL_BUFS		( 1024 * 1024 )
#endif


typedef struct _Query_t Query_t;
typedef struct _Sqlclient_t Sqlclient_t;

typedef void					( * queryHandler_cb_t )			( const Query_t * query );
typedef Sqlclient_t *			( * sqlNew_cb_t)				( const Core_t * core, const char * hostName, const uint16_t port, const char * loginName, const char * password, const char * dbName, const unsigned char timeoutSec );

enum sqlAdapter_t {
	SQLADAPTER_POSTGRESQL,
#if HAVE_MYSQL == 1
	SQLADAPTER_MYSQL
#endif
};

typedef struct _Sqlclient_t {
	Core_t *					core;
	Query_t *					currentQuery;
	enum sqlAdapter_t			adapter;
	union {
		struct{
			PGconn *						conn;
						} 			pg;
#if HAVE_MYSQL == 1
		struct {
			MYSAC *							conn;
						}			my;
#endif
							}	connection;
	const char *				hostName;
	const char *				loginName;
	const char *				password;
	const char *				dbName;
	int							socketFd;
	uint8_t						timeoutSec;
	Query_t *					queries;
	uint16_t					port;
} Sqlclient_t;

typedef struct _Query_t{
	const Sqlclient_t *			sqlclient;
	const char *				statement;
	const char **				paramValues;
	size_t *					paramLengths;
	size_t						paramCount;
	union {
		struct {
			PGresult * 					res;
		}							pg;
#if HAVE_MYSQL == 1
			struct {
				MYSAC_RES * 				res;
				unsigned int				statementId;
				MYSAC_BIND * 				vars;
				char *						resBuf;
			}						my;
#endif
						} 		result;
	void *						cbArgs;
	queryHandler_cb_t			cbHandler;
	clearFunc_cb_t				clearFuncCb;
	struct PRCListStr			mLink;
} Query_t;

#if HAVE_MYSQL == 1
Sqlclient_t *					Mysql_New					( const Core_t * core, const char * hostName, const uint16_t port, const char * loginName, const char * password, const char * dbName, const uint8_t timeoutSec );
#endif
Sqlclient_t *					Postgresql_New				( const Core_t * core, const char * hostName, const uint16_t port, const char * loginName, const char * password, const char * dbName, const uint8_t timeoutSec );
void							Sqlclient_Delete			( Sqlclient_t * sqlclient );

void 							Query_New					( Sqlclient_t * sqlclient, const char * sqlStatement, const size_t paramCount, const char ** paramValues, const queryHandler_cb_t callback, void * args, const clearFunc_cb_t clearFuncCb );
void							Query_Delete				( Query_t * query );

#ifdef __cplusplus
}
#endif

#endif  // SRC_FEATURE_SQLCLIENT_H_
