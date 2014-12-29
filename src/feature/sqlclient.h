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


struct query_t;

typedef void					( * queryHandler_cb_t )			( const struct query_t * query );
typedef struct sqlclient_t *	( * sqlNew_cb_t)				( const struct core_t * core, const char * hostName, const char * ip, const uint16_t port, const char * loginName, const char * password, const char * dbName, const unsigned char timeoutSec );

enum sqlAdapter_t {
	SQLADAPTER_POSTGRESQL,
#if HAVE_MYSQL == 1
	SQLADAPTER_MYSQL
#endif
};
struct query_t;

struct sqlclient_t {
	const struct core_t *		core;
	struct query_t *			currentQuery;
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
	const char *				ip;
	const char *				loginName;
	const char *				password;
	const char *				dbName;
	int							socketFd;
	unsigned char				timeoutSec;
	struct query_t *			queries;
	uint16_t					port;
};

struct query_t{
	const struct sqlclient_t *	sqlclient;
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
	struct PRCListStr			mLink;
};

#if HAVE_MYSQL == 1
struct sqlclient_t *			Mysql_New					( const struct core_t * core, const char * hostName, const char * ip, const uint16_t port, const char * loginName, const char * password, const char * dbName, const unsigned char timeoutSec );
#endif
struct sqlclient_t *			Postgresql_New				( const struct core_t * core, const char * hostName, const char * ip, const uint16_t port, const char * loginName, const char * password, const char * dbName, const unsigned char timeoutSec );
void							Sqlclient_Delete			( struct sqlclient_t * sqlclient );

void 							Query_New					( struct sqlclient_t * sqlclient, const char * sqlStatement, const size_t paramCount, const char ** paramValues, const queryHandler_cb_t callback, void * args );
void							Query_Delete				( struct query_t * query );

#ifdef __cplusplus
}
#endif

#endif  // SRC_FEATURE_SQLCLIENT_H_
