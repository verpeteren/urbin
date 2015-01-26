#include <stdlib.h>
#include <string.h>

#include "../core/utils.h"
#include "sqlclient.h"
/*****************************************************************************/
/* Module                                                                    */
/*****************************************************************************/

/*****************************************************************************/
/* Static etc.                                                               */
/*****************************************************************************/
static struct sqlclient_t *			Sqlclient_New				( const struct core_t * core, const enum sqlAdapter_t adapter, const char * hostName, const uint16_t port, const char * loginName, const char *password, const char * dbName, const unsigned char timeoutSec );
static void							Sqlclient_ConnectToIp		( struct dns_cb_data * dnsData );
static void							Sqlclient_CloseConn			( struct sqlclient_t * sqlclient );
static void 						Sqlclient_PushQuery 		( struct sqlclient_t * sqlclient, struct query_t * query );
static struct query_t * 			Sqlclient_PopQuery			( struct sqlclient_t * sqlclient );

/*****************************************************************************/
/* Query     .                                                               */
/*****************************************************************************/
void Query_New( struct sqlclient_t * sqlclient, const char * sqlStatement, const size_t paramCount, const char ** paramValues, const queryHandler_cb_t callback, void * args, const clearFunc_cb_t clearFuncCb ) {
	struct query_t * query;
	size_t i, j;
	struct {unsigned char good:1;
			unsigned char query:1;
			unsigned char statement:1;
			unsigned char length:1;
			unsigned char values:1;
			unsigned char params:1;
			} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	query = NULL;
	i = 0;
	cleanUp.good = ( ( query = malloc( sizeof( * query ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.query = 1;
		query->paramCount = paramCount;
		query->cbHandler = callback;
		query->clearFuncCb = clearFuncCb;
		query->cbArgs = args;
		PR_INIT_CLIST( &query->mLink );
		query->sqlclient = sqlclient;
#if HAVE_MYSQL == 1
		query->result.my.statementId = 0;
		query->result.my.vars = NULL;
		query->result.my.resBuf = NULL;
#endif
		cleanUp.good = ( ( query->statement = Xstrdup( sqlStatement ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.statement = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.statement = 1;
		cleanUp.good = ( ( query->paramLengths = malloc( sizeof( *query->paramLengths ) * query->paramCount ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.length = 1;
		cleanUp.good = ( ( query->paramValues = malloc( sizeof( *query->paramValues ) * query->paramCount ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.values = 1;
		for ( i = 0; i < query->paramCount; i++ ) {
			query->paramLengths[i] = strlen( paramValues[i] );
 			//  @TODO:  Mysql insert with parameters is not running correctly
			cleanUp.good = ( ( query->paramValues[i] = Xstrdup( paramValues[i] ) ) != NULL );
			if ( ! cleanUp.good ) {
				break;
			}
			cleanUp.params = 1;
		}
	}
	if ( cleanUp.good ) {
		Sqlclient_PushQuery( sqlclient, query );
		//  Let's see if we can submit this immediately to the server
			Sqlclient_PopQuery( sqlclient );
		if ( sqlclient->socketFd < 1 ) {
			Core_GetHostByName( (struct core_t *) sqlclient->core, sqlclient->hostName, Sqlclient_ConnectToIp, (void *) sqlclient );
		}
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.statement ) {
			free( ( char * ) query->statement );	query->statement = NULL;
		}
		if ( cleanUp.length ) {
			free( query->paramLengths ); query->paramLengths = NULL;
		}
		if ( cleanUp.values ) {
			free( query->paramValues ); query->paramValues = NULL;
		}
		if ( cleanUp.params ) {
			for ( j = 0; j < i; j++ ) {
				free( (char *) query->paramValues[j] ); query->paramValues[j] = NULL;
			}
		}
		if ( cleanUp.query ) {
			if ( query->clearFuncCb != NULL && query->cbArgs != NULL ) {
				query->clearFuncCb( query->cbArgs );
			}
			query->clearFuncCb = NULL;
			query->cbArgs = NULL;
			free( query ); query = NULL;
		}
	}
}

void Query_Delete( struct query_t * query ) {
	size_t i;

	if ( query != NULL ) {
		if ( query->clearFuncCb != NULL && query->cbArgs != NULL ) {
			query->clearFuncCb( query->cbArgs );
		}
		query->clearFuncCb = NULL;
		query->cbArgs = NULL;
		query->cbHandler = NULL;
		for ( i = 0; i < query->paramCount; i++ ) {
			free( (char *) query->paramValues[i] ); query->paramValues[i] = NULL;
		}
		free( ( char * ) query->statement ); query->statement = NULL;
		free( query->paramLengths ); query->paramLengths = NULL;
		free( query->paramValues ); query->paramValues = NULL;
		query->paramCount = 0;
		switch ( query->sqlclient->adapter ) {
		case SQLADAPTER_POSTGRESQL:
			if ( query->result.pg.res != NULL ) {
				PQclear( query->result.pg.res ); query->result.pg.res = NULL;
			}
			break;
#if HAVE_MYSQL == 1
		case SQLADAPTER_MYSQL:
			free( query->result.my.resBuf ); query->result.my.resBuf = NULL;
			free( query->result.my.vars ); query->result.my.vars = NULL;
			if ( query->result.my.res != NULL ) {
				mysac_free_res( query->result.my.res ); query->result.my.res = NULL;
			}
			break;
#endif
		default:
			break;
		}
		Core_Log( query->sqlclient->core, LOG_INFO, __FILE__ , __LINE__, "Delete query free-ed" );
		query->sqlclient = NULL;
		PR_INIT_CLIST( &query->mLink );
		free( query ); query = NULL;
	}
}

/*****************************************************************************/
/* POSTGRESQL                                                                */
/*****************************************************************************/
static void							Postgresql_HandleRead_cb	( picoev_loop * loop, const int fd, const int events, void * cbArgs );
static void							Postgresql_HandleWrite_cb	( picoev_loop * loop, const int fd, const int events, void * cbArgs );
static void							Postgresql_HandleConnect_cb	( picoev_loop * loop, const int fd, const int events, void * cbArgs );

static void Postgresql_HandleRead_cb	( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	struct {unsigned char good:1;} cleanUp;
	int done;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cbArgs;
	query = sqlclient->currentQuery;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READ ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		if ( PQstatus( sqlclient->connection.pg.conn ) == CONNECTION_OK ) {
			if ( PQconsumeInput( sqlclient->connection.pg.conn ) == 1 ) {
				if ( PQisBusy( sqlclient->connection.pg.conn ) == 0 ) {
					do {
						query->result.pg.res = PQgetResult( sqlclient->connection.pg.conn );
						done = ( query->result.pg.res == NULL );
						if ( ! done && query->cbHandler != NULL ) {
							if ( query->result.pg.res != NULL ) {
								//  Most of the time all the results are returned in the first loop, but just in case we have to call it again.
								query->cbHandler( query );
							}
							PQclear( query->result.pg.res ); query->result.pg.res = NULL;
						}
					} while ( ! done );
					//  We're done with this query, go back to start and collect 20k
					Query_Delete( query ); query = NULL;
					sqlclient->currentQuery = NULL;
					picoev_del( loop, fd );
					picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeoutSec, Postgresql_HandleWrite_cb, cbArgs );
				}
			}
	 	} else  {
			Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, PQerrorMessage( sqlclient->connection.pg.conn ) );
			Sqlclient_CloseConn( sqlclient );
		}
	}
}

static void Postgresql_HandleFlush_cb( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;
	int flush;
	struct {unsigned char good:1;} cleanUp;;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cbArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READWRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		flush = PQflush( sqlclient->connection.pg.conn );
		if ( flush == 1 ) {
			//  loop once more
		} else if ( flush == 0 ) {
			picoev_del( loop, fd );
			picoev_add( loop, fd, PICOEV_READ, sqlclient->timeoutSec, Postgresql_HandleRead_cb, cbArgs );
		} else {
			Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, PQerrorMessage( sqlclient->connection.pg.conn ) );
			Sqlclient_CloseConn( sqlclient );
		}
	}
}


static void Postgresql_HandleWrite_cb( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	struct {unsigned char good:1;} cleanUp;;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cbArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_WRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		//  Let's get some work
		query = Sqlclient_PopQuery( sqlclient );
		if ( query != NULL ) {
			cleanUp.good = ( PQstatus( sqlclient->connection.pg.conn ) == CONNECTION_OK );
			if ( cleanUp.good ) {
				if ( query->paramCount == 0 ) {
					cleanUp.good = ( PQsendQuery( sqlclient->connection.pg.conn, query->statement ) == 1 );
				} else {
					//  Send out the query and the parameters to the datebase engine. only Version 2 protocoll, and only one command per statement
					cleanUp.good = ( PQsendQueryParams( sqlclient->connection.pg.conn, query->statement, (int) query->paramCount, NULL, query->paramValues, (const int *) query->paramLengths, NULL, 0 ) == 1 );
				}
			}
			if ( cleanUp.good ) {
				picoev_del( loop, fd );
				picoev_add( loop, fd, PICOEV_READWRITE, sqlclient->timeoutSec, Postgresql_HandleFlush_cb, cbArgs );
			} else {
				Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, PQerrorMessage( sqlclient->connection.pg.conn ) );
				Sqlclient_CloseConn( sqlclient );
			}
		}
	}
}

static void Postgresql_HandleConnect_cb( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;

	sqlclient = (struct sqlclient_t *) cbArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READWRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		PostgresPollingStatusType status;
		ConnStatusType statusType;

		statusType = PQstatus( sqlclient->connection.pg.conn );
		if ( statusType == CONNECTION_BAD ) {
			Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, PQerrorMessage( sqlclient->connection.pg.conn ) );
			Sqlclient_CloseConn( sqlclient );
		} else {
			status = PQconnectPoll( sqlclient->connection.pg.conn );
			if ( status == PGRES_POLLING_FAILED ) {
				Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, PQerrorMessage( sqlclient->connection.pg.conn ) );
				Sqlclient_CloseConn( sqlclient );
			} else if ( status == PGRES_POLLING_OK ) {
				statusType = PQstatus( sqlclient->connection.pg.conn );
				if ( statusType == CONNECTION_OK ) {
					//  We are connected, and have the database, we are good to go.
					picoev_del( loop, fd );
					picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeoutSec, Postgresql_HandleWrite_cb, cbArgs );
				}
			}
		}
	}
}

#define SET_DEFAULTS_FOR_CONN( type, sectionName ) do { \
	struct cfg_t * sqlSection, * modulesSection; \
	\
	prt = 0; \
	timeSec = 0; \
	modulesSection = cfg_getnsec( (cfg_t *) core->config, "modules", 0 ); \
	sqlSection = cfg_getnsec( modulesSection, sectionName, 0 ); \
	if ( port == 0 ) { \
		prt = (uint16_t) cfg_getint( sqlSection, "port" ); \
	} \
	if ( prt == 0 ) { \
		prt = PR_CFG_MODULES_ ## type ## SQLCLIENT_PORT; \
	} \
	if ( dbName == NULL ) { \
		dbName = cfg_getstr( sqlSection, "database" );\
	} \
	if ( dbName == NULL ) { \
		dbName = PR_CFG_MODULES_ ## type ## SQLCLIENT_DATABASE; \
	} \
	if ( timeoutSec == 0 ) { \
		timeSec = (unsigned char) cfg_getint( sqlSection, "timeout_sec" ); \
	} \
	if ( timeSec == 0 ) { \
		timeSec = PR_CFG_MODULES_ ## type ## SQLCLIENT_TIMEOUT_SEC; \
	} \
} while ( 0 );

struct sqlclient_t * Postgresql_New( const struct core_t * core, const char * hostName, const uint16_t port, const char * loginName, const char * password, const char * dbName, const unsigned char timeoutSec ) {
	uint16_t prt;
	unsigned char timeSec;

	SET_DEFAULTS_FOR_CONN( PG, "postgresqlclient" ) \
	return Sqlclient_New( core, SQLADAPTER_POSTGRESQL, hostName, prt, loginName, password, dbName, timeSec );
}

#if HAVE_MYSQL == 1
/*****************************************************************************/
/* MYSQL                                                                     */
/*****************************************************************************/
static void							Mysql_HandleRead_cb		( picoev_loop * loop, const int fd, const int events, void * cbArgs );
static void							Mysql_HandleWrite_cb	( picoev_loop * loop, const int fd, const int events, void * cbArgs );
static void							Mysql_HandleSetParams_cb( picoev_loop * loop, const int fd, const int events, void * cbArgs );
static void							Mysql_HandleSetDb_cb	( picoev_loop * loop, const int fd, const int events, void * cbArgs );
static void							Mysql_HandleConnect_cb	( picoev_loop * loop, const int fd, const int events, void * cbArgs );

static void	Mysql_HandleRead_cb	( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	int retCode;
	struct {unsigned char good:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cbArgs;
	query = sqlclient->currentQuery;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READWRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		retCode = mysac_io( sqlclient->connection.my.conn );
		 if ( retCode == MYERR_WANT_WRITE ||  retCode == MYERR_WANT_READ )  {
			// pass once more
		} else if ( retCode == 0 ) {
			if ( query->cbHandler != NULL ) {
				query->result.my.res = mysac_get_res( sqlclient->connection.my.conn );
				query->cbHandler( query );
				mysac_free_res( query->result.my.res ); query->result.my.res = NULL;
			}
			//  We're done with this query, go back to start and collect 20k
			Query_Delete( query ); query = NULL;
			sqlclient->connection.my.conn->call_it = NULL;
			sqlclient->currentQuery = NULL;
			picoev_del( loop, fd );
			picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeoutSec, Mysql_HandleWrite_cb, cbArgs );
		} else {
			Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, mysac_error( sqlclient->connection.my.conn ) );
			Sqlclient_CloseConn( sqlclient );
		}
	}
}

static void	Mysql_HandleSetParams_cb( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	int retCode, readyToSend;
	size_t i;
	struct { unsigned char good:1;
			unsigned char vars:1;
			unsigned char id:1;
			unsigned char my:1;
			unsigned char resBuf:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	readyToSend = 0;
	sqlclient = (struct sqlclient_t *) cbArgs;
	query = sqlclient->currentQuery;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READWRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		if ( sqlclient->connection.my.conn->call_it != NULL ) {
			retCode = mysac_io( sqlclient->connection.my.conn );
			if ( retCode == MYERR_WANT_READ || retCode == MYERR_WANT_WRITE ) {
				//  loop once more
			} else if ( retCode == 0 ) {
				readyToSend = 1;
			} else {
				Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, mysac_error( sqlclient->connection.my.conn ) );
				Sqlclient_CloseConn( sqlclient );
			}
		}
		if ( query->paramCount == 0 ) {
			readyToSend = 1;
		} else if ( query->paramCount >0 && query->result.my.statementId != 0 ) {
			readyToSend = 1;
		}
		if ( readyToSend ) {
			cleanUp.good = ( ( query->result.my.resBuf = malloc( MYSQL_BUFS ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.resBuf = 1;
				cleanUp.good = ( ( query->result.my.res = ( void * ) mysac_init_res( query->result.my.resBuf, MYSQL_BUFS ) ) != NULL );
			}
			if ( cleanUp.good ) {
				cleanUp.my = 1;
				if ( query->paramCount > 0 ) {
					cleanUp.good = ( ( query->result.my.vars = (MYSAC_BIND *) calloc( sizeof( MYSAC_BIND ), query->paramCount ) ) != NULL );
				}
			}
			if ( cleanUp.good && query->paramCount > 0 ) {
				cleanUp.vars = 1;
				for ( i = 0; i < query->paramCount; i++ ) {
					query->result.my.vars[i].type = MYSQL_TYPE_VAR_STRING;
					query->result.my.vars[i].value = ( void * ) query->paramValues[i];
					query->result.my.vars[i].value_len = (int) query->paramLengths[i];
					query->result.my.vars[i].is_null = 0;
				}
			}
			if ( cleanUp.good ) {
				picoev_del( loop, fd );
				picoev_add( loop, fd, PICOEV_READWRITE, sqlclient->timeoutSec, Mysql_HandleRead_cb, cbArgs );
				if ( query->paramCount == 0 ) {
					//  Send out the query to the database engine
					mysac_b_set_query( sqlclient->connection.my.conn, query->result.my.res, query->statement, strlen( query->statement) );
				} else {
					//  Send out the parameters to the database engine for the prepared statement statementId
					mysac_set_stmt_execute( sqlclient->connection.my.conn, ( MYSAC_RES * ) query->result.my.res, query->result.my.statementId, query->result.my.vars, (int) query->paramCount );
				}
			}
			if ( cleanUp.good ) {
				cleanUp.id = 1;
			}
			if ( ! cleanUp.good ) {
				if ( cleanUp.my ) {
					mysac_free_res( query->result.my.res ); query->result.my.res = NULL;
				}
				if ( cleanUp.id ) {
					query->result.my.statementId = 0;
				}
				if ( cleanUp.resBuf ) {
					free( query->result.my.resBuf ); query->result.my.resBuf = NULL;
				}
				if ( cleanUp.vars ) {
					free( query->result.my.vars ); query->result.my.vars = NULL;
				}
			}
		}
	}
}

static void	Mysql_HandleWrite_cb( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	int retCode, readyToSend;
	struct { unsigned char good:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cbArgs;
	readyToSend = 0;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_WRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		query = Sqlclient_PopQuery( sqlclient );
		if ( query != NULL ) {
			if ( sqlclient->connection.my.conn->call_it == NULL ) {
				readyToSend = 1;
			} else {
				retCode = mysac_io( sqlclient->connection.my.conn );
				if ( retCode == MYERR_WANT_READ || retCode == MYERR_WANT_WRITE ) {
					//  loop once more
				} else if ( retCode == 0 ) {
					readyToSend = 1;
				} else {
					Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, mysac_error( sqlclient->connection.my.conn ) );
					Sqlclient_CloseConn( sqlclient );
				}
			}
			if ( readyToSend ) {
				//  Let's get some work
				picoev_del( loop, fd );
				picoev_add( loop, fd, PICOEV_READWRITE, sqlclient->timeoutSec, Mysql_HandleSetParams_cb, cbArgs );
				if ( query->paramCount > 0 ) {
					//  Mysac needs to prepare the statement
					mysac_b_set_stmt_prepare( sqlclient->connection.my.conn, &query->result.my.statementId, query->statement, (int) strlen( query->statement ) );
				}
			}
		}
	}
}

static void Mysql_HandleSetDb_cb( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;
	int retCode;
	struct { unsigned char good:1;
			unsigned char ev:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cbArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READWRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		retCode = mysac_io( sqlclient->connection.my.conn );
		if ( retCode == MYERR_WANT_WRITE || retCode == MYERR_WANT_READ ) {
			//  loop once more
		} else if ( retCode == 0 ) {
			//  yes, we are connected and have a database, we are good to go
			picoev_del( loop, fd );
			picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeoutSec, Mysql_HandleWrite_cb, cbArgs );
		} else {
			Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, mysac_error( sqlclient->connection.my.conn ) );
			Sqlclient_CloseConn( sqlclient );
		}
	}
}

static void Mysql_HandleConnect_cb( picoev_loop * loop, const int fd, const int events, void * cbArgs ) {
	struct sqlclient_t * sqlclient;
	int retCode;
	struct { unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cbArgs;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READWRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeoutSec );
		retCode = mysac_io( sqlclient->connection.my.conn );
		if ( retCode == MYERR_WANT_WRITE || retCode == MYERR_WANT_READ ) {
			// loop once more
		} else if ( retCode  == 0 ) {
			//  Yes, we are connected, now set the database, as mysac needs that first
			picoev_del( loop, fd );
			picoev_add( loop, fd, PICOEV_READWRITE, sqlclient->timeoutSec, Mysql_HandleSetDb_cb, cbArgs );
			mysac_set_database( sqlclient->connection.my.conn, sqlclient->dbName );
		} else {
			Core_Log( sqlclient->core, LOG_ERR, __FILE__ , __LINE__, mysac_error( sqlclient->connection.my.conn ) );
			Sqlclient_CloseConn( sqlclient );
		}
	}
}

struct sqlclient_t * Mysql_New( const struct core_t * core, const char * hostName, const uint16_t port, const char * loginName, const char * password, const char * dbName, const unsigned char timeoutSec ) {
	int prt;
	unsigned char timeSec;

	SET_DEFAULTS_FOR_CONN( MY, "mysqlclient" ) \
	return Sqlclient_New( core, SQLADAPTER_MYSQL, hostName, port, loginName, password, dbName, timeSec );
}
#endif
/*****************************************************************************/
/* Generic                                                                   */
/*****************************************************************************/
static struct sqlclient_t * Sqlclient_New( const struct core_t * core, const enum sqlAdapter_t adapter, const char * hostName, const uint16_t port, const char * loginName, const char * password, const char * dbName, const unsigned char timeoutSec ) {
	struct sqlclient_t * sqlclient;
	struct {unsigned char sqlclient:1;
			unsigned char hostName:1;
			unsigned char loginName:1;
			unsigned char password:1;
			unsigned char dbName:1;
			unsigned char good:1;
			} cleanUp;

	sqlclient = NULL;
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( ( sqlclient = malloc( sizeof( *sqlclient ) ) ) != NULL ) );
	if ( cleanUp.good ) {
		sqlclient->core = (struct core_t *) core;
		sqlclient->port = port;
		sqlclient->adapter = adapter;
		switch ( sqlclient->adapter ) {
#if HAVE_MYSQL == 1
			case SQLADAPTER_MYSQL:
				sqlclient->connection.my.conn = NULL;
#endif
			case SQLADAPTER_POSTGRESQL:
				sqlclient->connection.pg.conn = NULL;
				break;
			default:
				break;
			break;
		}
		sqlclient->hostName = sqlclient->loginName = sqlclient->password = sqlclient->dbName = NULL;
	}
	if ( loginName == NULL ) {
		loginName = "";
	}
	if ( password == NULL ) {
		password = "";
	}
	if ( dbName == NULL ) {
		dbName = "";
	}
		cleanUp.good = ( ( sqlclient->hostName = Xstrdup( hostName ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.hostName = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( sqlclient->loginName = Xstrdup( loginName ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.loginName = 1;
		cleanUp.good = ( ( sqlclient->password = Xstrdup( password ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.password = 1;
		cleanUp.good = ( ( sqlclient->dbName = Xstrdup( dbName ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.dbName = 1;
		sqlclient->queries = NULL;
	}
	if ( cleanUp.good ) {
		sqlclient->timeoutSec = timeoutSec;
		sqlclient->socketFd = 0;
		sqlclient->currentQuery = NULL;
		Core_GetHostByName( sqlclient->core, sqlclient->hostName, Sqlclient_ConnectToIp, (void *) sqlclient );
		Core_Log( sqlclient->core, LOG_INFO, __FILE__ , __LINE__, "New Sqlclient allocated" );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.hostName ) {
			free( (char *) sqlclient->hostName ); sqlclient->hostName = NULL;
		}
		if ( cleanUp.loginName ) {
			free( (char *) sqlclient->loginName ); 	sqlclient->loginName = NULL;
		}
		if ( cleanUp.password ) {
			memset( (char *) sqlclient->password, '\0', strlen( sqlclient->password ) );
			free( (char *) sqlclient->password ); 	sqlclient->password = NULL;
		}
		if ( cleanUp.dbName ) {
			free( (char *) sqlclient->dbName ); 		sqlclient->dbName = NULL;
		}
		if ( cleanUp.sqlclient ) {
			free( sqlclient ); sqlclient = NULL;
		}
	}

	return sqlclient;
}

static void Sqlclient_ConnectToIp( struct dns_cb_data * dnsData ) {
	struct sqlclient_t * sqlclient;
	char * connString;
	size_t len;
	char * ip;
	struct {unsigned char conn:1;
			unsigned char ip:1;
			unsigned char connString:1;
			unsigned char socket:1;
			unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	connString = NULL;
	ip = NULL;
	sqlclient= ( struct sqlclient_t *) dnsData->context;
	sqlclient->core->dns.actives--;
	cleanUp.good = ( ( ip = DnsData_ToString( dnsData ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.ip = 1;
		switch ( sqlclient->adapter ) {
			case SQLADAPTER_POSTGRESQL:
				if ( sqlclient->connection.pg.conn == NULL ) {
#define CONNSTRING_TEMPLATE "host=%s port=%d dbname=%s user=%s password=%s hostaddr=%s"
					len = 1 + strlen( CONNSTRING_TEMPLATE ) + strlen( sqlclient->hostName ) + 5 + strlen( sqlclient->dbName ) + strlen( sqlclient->loginName ) + strlen( sqlclient->password ) + strlen( ip ) - 12;
					cleanUp.good = ( ( connString = calloc( len, 1 ) ) != NULL );
					if ( cleanUp.good ) {
						cleanUp.connString = 1;
					snprintf( connString, len, CONNSTRING_TEMPLATE, sqlclient->hostName, sqlclient->port, sqlclient->dbName, sqlclient->loginName, sqlclient->password, ip );
#undef CONNSTRING_TEMPLATE
						cleanUp.good = ( ( sqlclient->connection.pg.conn = PQconnectStart( connString ) ) != NULL );
					}
					if ( cleanUp.good ) {
						cleanUp.conn = 1;
						cleanUp.good = ( ( sqlclient->socketFd = PQsocket( sqlclient->connection.pg.conn ) ) > 0 );
					}
					if ( cleanUp.good ) {
						cleanUp.conn = 1;
						picoev_add( sqlclient->core->loop, sqlclient->socketFd, PICOEV_READWRITE, sqlclient->timeoutSec, Postgresql_HandleConnect_cb, (void * ) sqlclient );
						PQsetnonblocking( sqlclient->connection.pg.conn, 1 );
					}
					if ( ! cleanUp.good ) {
						if ( cleanUp.conn ) {
							PQfinish( sqlclient->connection.pg.conn ); sqlclient->connection.pg.conn = NULL;
						}
					}
				}
				break;
#if HAVE_MYSQL == 1
			case SQLADAPTER_MYSQL:
				if ( sqlclient->connection.my.conn == NULL ) {
#define CONNSTRING_TEMPLATE "%s:%d"
					len = 1 + strlen( CONNSTRING_TEMPLATE ) + 5 + strlen( ip ) - 4;
					cleanUp.good = ( ( connString = calloc( len, 1 ) ) != NULL );
					if ( cleanUp.good ) {
						cleanUp.connString = 1;
						snprintf( connString, len, CONNSTRING_TEMPLATE, ip, sqlclient->port);
#undef CONNSTRING_TEMPLATE
						if ( cleanUp.good ) {
							cleanUp.good = ( ( sqlclient->connection.my.conn = mysac_new( MYSQL_BUFS ) ) != NULL );
						}
						if ( cleanUp.good ) {
							cleanUp.conn = 1;
							mysac_setup( sqlclient->connection.my.conn, connString, sqlclient->loginName, sqlclient->password, sqlclient->dbName, 0 );
							mysac_connect( sqlclient->connection.my.conn );
							cleanUp.good = ( ( sqlclient->socketFd = mysac_get_fd( sqlclient->connection.my.conn ) ) > 0 );
						}
						if ( cleanUp.good ) {
							SetupSocket( sqlclient->socketFd, 0 );
							picoev_add( sqlclient->core->loop, sqlclient->socketFd, PICOEV_READWRITE, sqlclient->timeoutSec, Mysql_HandleConnect_cb, (void * ) sqlclient );
						}
						if ( ! cleanUp.good ) {
							if ( cleanUp.conn ) {
								mysac_close( sqlclient->connection.my.conn ); sqlclient->connection.my.conn = NULL;
							}
						}
					}
				}
				break;
#endif
			default:
				break;
		}
	}
	// allways cleanup
	if ( cleanUp.ip ) {
		free( ip ); ip = NULL;
	}
	if ( cleanUp.connString ) {
		memset( connString, 0, strlen( connString ) );  //  password is in the postgresql sqlclient string
		free( connString ); connString = NULL;
	}
}

static void Sqlclient_CloseConn( struct sqlclient_t * sqlclient ) {
	picoev_del( sqlclient->core->loop, sqlclient->socketFd );
	switch ( sqlclient->adapter ) {
	case SQLADAPTER_POSTGRESQL:
		if ( sqlclient->connection.pg.conn != NULL ) {
			PQfinish( sqlclient->connection.pg.conn ); sqlclient->connection.pg.conn = NULL;
		}
		break;
#if HAVE_MYSQL == 1
	case SQLADAPTER_MYSQL:
		if ( sqlclient->connection.my.conn != NULL ) {
			free( sqlclient->connection.my.conn->buf ); sqlclient->connection.my.conn->buf = NULL;
			mysac_close( sqlclient->connection.my.conn ); sqlclient->connection.my.conn = NULL;
			//  @TODO:  check if memset on password field is needed
		}
		break;
#endif
	default:
		break;
	}
	sqlclient->socketFd = 0;
}

static void Sqlclient_PushQuery( struct sqlclient_t * sqlclient, struct query_t * query ) {
	if ( query != NULL ) {
		if ( sqlclient->queries == NULL ) {
			sqlclient->queries = query;
		} else {
			PR_INSERT_BEFORE( &query->mLink, &sqlclient->queries->mLink );
		}
		Core_Log( sqlclient->core, LOG_INFO, __FILE__ , __LINE__, "New Query allocated" );
	}
}

static struct query_t * Sqlclient_PopQuery( struct sqlclient_t * sqlclient ) {
	PRCList * next;

	if ( sqlclient->currentQuery == NULL ) {
		if ( sqlclient->queries != NULL ) {
			sqlclient->currentQuery = sqlclient->queries;
			if ( PR_CLIST_IS_EMPTY( &sqlclient->currentQuery->mLink ) ) {
				sqlclient->queries = NULL;
			} else {
				next = PR_NEXT_LINK( &sqlclient->currentQuery->mLink );
				sqlclient->queries = FROM_NEXT_TO_ITEM( struct query_t );
			}
			PR_REMOVE_AND_INIT_LINK( &sqlclient->currentQuery->mLink );
		}
	}
	return sqlclient->currentQuery;
}

void Sqlclient_Delete( struct sqlclient_t * sqlclient ) {
	struct query_t * query;

	//  cleanup the queries
	if ( sqlclient->currentQuery != NULL ) {
		Query_Delete( sqlclient->currentQuery ); sqlclient->currentQuery = NULL;
	}
	while ( ( query = Sqlclient_PopQuery( sqlclient ) ) != NULL ) {
		Query_Delete( query ); sqlclient->currentQuery = NULL;  //  pop sets also current query
	}
	//  cleanup the rest
	Sqlclient_CloseConn( sqlclient );
	sqlclient->timeoutSec = 0;
	memset( (char *) sqlclient->password, '\0', strlen( sqlclient->password ) );
	free( (char *) sqlclient->hostName ); 		sqlclient->hostName = NULL;
	free( (char *) sqlclient->loginName );		sqlclient->loginName = NULL;
	free( (char *) sqlclient->password );		sqlclient->password = NULL;
	free( (char *) sqlclient->dbName ); 		sqlclient->dbName = NULL;
	Core_Log( sqlclient->core, LOG_INFO, __FILE__ , __LINE__, "Delete Sqlclient free-ed" );
	sqlclient->core = NULL;
	free( sqlclient ); 							sqlclient = NULL;
}

