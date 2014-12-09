#ifndef SRC_FEATURE_SQLCLIENT_H_
#define SRC_FEATURE_SQLCLIENT_H_

#include <stdint.h>

#include <libpq-fe.h>
#include <mysac.h>

#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MYSQL_BUFS ( 1024 * 1024 )


struct query_t;

typedef void                   ( * queryHandler_cb_t )			( struct query_t * query );

enum sqlAdapter_t {
	SQLADAPTER_POSTGRESQL,
	SQLADAPTER_MYSQL
};
struct query_t;

struct sqlclient_t {
	struct core_t *				core;
	struct query_t *			currentQuery;
	enum sqlAdapter_t			adapter;
	union {
		PGconn *					pg;
		MYSAC *						my;
							}	connection;
	const char *				hostName;
	const char *				ip;
	const char *				hostString;
	const char *				loginName;
	const char *				password;
	const char *				dbName;
	int							socketFd;
	int							timeoutSec;
	unsigned int				statementId; //  only needed for mysac
	struct query_t *			queries;
	uint16_t					port;
};

struct query_t{
	struct sqlclient_t *		sqlclient;
	const char *				statement;
	const char **				paramValues;
	size_t *					paramLengths;
	size_t						paramCount;
	unsigned int				statementId;
	union {
		PGresult * 					pg;
		MYSAC_RES * 				my;
						} 		result;
	void *						cbArgs;
	queryHandler_cb_t			cbHandler;
	struct PRCListStr			mLink;
};

struct sqlclient_t *			Mysql_New					( struct core_t * core, const char * hostName, const char * ip, uint16_t port, const char * loginName, const char *password, const char * dbName, int timeoutSec );
struct sqlclient_t *			Postgresql_New				( struct core_t * core, const char * hostName, const char * ip, uint16_t port, const char * loginName, const char *password, const char * dbName, int timeoutSec );
void							Sqlclient_Delete			( struct sqlclient_t * sqlclient );

void 							Query_New					( struct sqlclient_t * sqlclient, const char * sqlStatement, size_t paramCount, const char ** paramValues, queryHandler_cb_t callback, void * args );
void							Query_Delete				( struct query_t * query );

#ifdef __cplusplus
}
#endif

#endif  // SRC_FEATURE_SQLCLIENT_H_
