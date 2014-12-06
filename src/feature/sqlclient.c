#include <stdlib.h>
#include <string.h>

#include "sqlclient.h"


static struct sqlclient_t *			Sqlclient_New				( struct core_t * core, enum sqlAdapter_t adapter, const char * hostName, const char * ip, uint16_t port, const char * loginName, const char *password, const char * dbName );
static void							Sqlclient_Connect			( struct sqlclient_t * sqlclient );
static void							Sqlclient_CloseConn			( struct sqlclient_t * sqlclient );
static struct query_t * 			Sqlclient_PopQuery			( struct sqlclient_t * sqlclient );


/*****************************************************************************/
/* POSTGRESQL                                                                */
/*****************************************************************************/

static void							Postgresql_HandleRead_cb	( picoev_loop* loop, int fd, int events, void* cb_arg );
static void							Postgresql_HandleWrite_cb	( picoev_loop* loop, int fd, int events, void* cb_arg );
static void							Postgresql_HandleConnect_cb	( picoev_loop* loop, int fd, int events, void* cb_arg );

static void Postgresql_HandleRead_cb	( picoev_loop* loop, int fd, int events, void* cb_arg ) {
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	int good;

	sqlclient = (struct sqlclient_t *) cb_arg;
	query = sqlclient->currentQuery;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READ ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeout_sec );
		good = ( PQstatus( sqlclient->connection.pg ) == CONNECTION_OK );
		if ( good ) {
			good = ( PQconsumeInput( sqlclient->connection.pg ) == 1 );
		}
		if ( good ) {
			if ( PQisBusy( sqlclient->connection.pg ) != 1 ) {
				do {
					query->result.pg = PQgetResult( sqlclient->connection.pg );
					if ( query->cbHandler != NULL ) {
						if ( query->result.pg != NULL ) {
							// Most of the time all the results are returend in the first loop, but just in case we have to call it again.
							query->cbHandler( query );
						}
						PQclear( query->result.pg ); query->result.pg = NULL;
					}
				} while ( query->result.pg != NULL );
			}
		}
		//  We're done with this query, go back to start and collect 20k
		sqlclient->currentQuery = NULL;
		picoev_del( loop, fd ) ;
		picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeout_sec, Postgresql_HandleWrite_cb, cb_arg );
	}
}

static void Postgresql_HandleWrite_cb( picoev_loop* loop, int fd, int events, void* cb_arg ) {
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	int good;

	sqlclient = (struct sqlclient_t *) cb_arg;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_WRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeout_sec );
		//  Let's get some work
		query = Sqlclient_PopQuery( sqlclient );
		if ( query ) {
			if ( query->paramCount == 0 ) {
				good = ( PQsendQuery( sqlclient->connection.pg, query->statement ) == 1 );
			} else {
				//  Send out the query and the parameters to the datebase engine
				picoev_del( loop, fd ) ;
				picoev_add( loop, fd, PICOEV_READ, sqlclient->timeout_sec, Postgresql_HandleRead_cb, cb_arg );
				//  only Version 2 protocoll, and only one command per statement
				good = ( PQsendQueryParams( sqlclient->connection.pg, query->statement, (int) query->paramCount, NULL, query->paramValues, (const int *) query->paramLengths, NULL, 0 ) == 1 );
			}
			if ( good ) {
				good = ( PQflush( sqlclient->connection.pg ) == 0 );
			} else {
				Sqlclient_CloseConn( sqlclient );
			}
		}
	}
}

static void Postgresql_HandleConnect_cb( picoev_loop* loop, int fd, int events, void* cb_arg ) {
	struct sqlclient_t * sqlclient;

	sqlclient = (struct sqlclient_t *) cb_arg;
	if ( ( events & PICOEV_TIMEOUT) != 0 )  {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READ ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeout_sec );
		PostgresPollingStatusType status;
		ConnStatusType statusType;
		if ( ( statusType = PQstatus( sqlclient->connection.pg ) ) == CONNECTION_BAD ) {
			Sqlclient_CloseConn( sqlclient );
		} else {
			status = PQconnectPoll( sqlclient->connection.pg );
		}
		if ( status != PGRES_POLLING_FAILED && status != PGRES_POLLING_OK ) {
			if ( PQstatus( sqlclient->connection.pg ) != CONNECTION_OK ) {
				Sqlclient_CloseConn( sqlclient );
			} else {
				//  We are connected ( and have the database ), we are good to go.
				picoev_del( loop, fd ) ;
				picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeout_sec, Postgresql_HandleWrite_cb, cb_arg );
			}
		} else {
			Sqlclient_CloseConn( sqlclient );
		}
	}
}

struct sqlclient_t * Postgresql_New( struct core_t * core, const char * hostName, const char * ip, uint16_t port, const char * loginName, const char *password, const char * dbName ) {
	return Sqlclient_New( core, SQLADAPTER_POSTGRESQL, hostName, ip, port, loginName, password, dbName );
}

/*****************************************************************************/
/* MYSQL                                                                     */
/*****************************************************************************/
static void							Mysql_HandleRead_cb		( picoev_loop* loop, int fd, int events, void* cb_arg );
static void							Mysql_HandleWrite_cb	( picoev_loop* loop, int fd, int events, void* cb_arg );
static void							Mysql_HandleSetParams_cb( picoev_loop* loop, int fd, int events, void* cb_arg );
static void							Mysql_HandleSetDb_cb	( picoev_loop* loop, int fd, int events, void* cb_arg );
static void							Mysql_HandleConnect_cb	( picoev_loop* loop, int fd, int events, void* cb_arg );

static void	Mysql_HandleRead_cb	( picoev_loop* loop, int fd, int events, void* cb_arg ) {
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	int retCode, good;

	sqlclient = (struct sqlclient_t *) cb_arg;
	query = sqlclient->currentQuery;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READ ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeout_sec );
		retCode = mysac_io( sqlclient->connection.my );
		good = ( retCode != MYERR_WANT_WRITE && retCode != MYERR_WANT_READ );
		if ( good ) {
			if ( query->cbHandler != NULL ) {
				query->cbHandler( query );
				mysac_free_res( query->result.my ); query->result.my = NULL;
			}
		}
		//  We're done with this query, go back to start and collect 20k
		sqlclient->currentQuery = NULL;
		picoev_del( loop, fd ) ;
		picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeout_sec, Mysql_HandleWrite_cb, cb_arg );
	}
}

static void	Mysql_HandleSetParams_cb( picoev_loop* loop, int fd, int events, void* cb_arg ){
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	//  @TODO:  check memory handling of resBUf and vars
	char resBuf[MYSQL_BUFS];
	MYSAC_BIND * vars;
	int retCode;
	size_t i;
	struct { unsigned int good:1;
			unsigned int vars:1;
			unsigned int ev:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cb_arg;
	query = sqlclient->currentQuery;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_WRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeout_sec );
		cleanUp.good = ( ( vars = (MYSAC_BIND *) calloc( sizeof (MYSAC_BIND), query->paramCount ) ) != NULL );
		if ( cleanUp.good ) {
			cleanUp.vars = 1;
			for ( i = 0; i <  query->paramCount; i++ ) {
				vars[i].type = MYSQL_TYPE_VAR_STRING;
				vars[i].value = ( void * ) query->paramValues[i];
				vars[i].value_len = (int) query->paramLengths[i];
				vars[i].is_null = 0;
			}
		}
		if ( cleanUp.good ) {
			cleanUp.good = ( ( query->result.my = ( void * ) mysac_init_res( resBuf, MYSQL_BUFS ) ) != NULL );
		}
		if ( cleanUp.good ) {
			cleanUp.good = ( mysac_set_stmt_execute( sqlclient->connection.my, ( MYSAC_RES * ) query->result.my, query->statementId, vars, (int) query->paramCount ) == 0 );
		}
		if ( cleanUp.good ) {
			//  Send out the query and the parameters to the datebase engine
			picoev_del( loop, fd );
			picoev_add( loop, fd, PICOEV_READ, sqlclient->timeout_sec, Mysql_HandleRead_cb, cb_arg );
			cleanUp.ev = 1;
			retCode = mysac_io( sqlclient->connection.my );
			cleanUp.good = ( retCode != MYERR_WANT_WRITE && retCode != MYERR_WANT_READ );
		}
		if ( ! cleanUp.good ) {
			if ( cleanUp.vars ) {
				free( vars ); vars = NULL;  //  @TODO: further cleanup
			}
		}
	}
}

static void	Mysql_HandleWrite_cb( picoev_loop* loop, int fd, int events, void* cb_arg ){
	struct sqlclient_t * sqlclient;
	struct query_t * query;
	int retCode;
	struct { unsigned int good:1;
			unsigned int ev:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	sqlclient = (struct sqlclient_t *) cb_arg;
	if ( ( events & PICOEV_TIMEOUT ) != 0 ) {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_WRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeout_sec );
		//  Let's get some work
		query = Sqlclient_PopQuery( sqlclient );
		if ( query ) {
			picoev_del( loop, fd ) ;
			picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeout_sec, Mysql_HandleSetParams_cb, cb_arg );
			cleanUp.ev = 1;
			//  Mysac needs to perpare the statement
			cleanUp.good = ( mysac_b_set_stmt_prepare( sqlclient->connection.my, &query->statementId, query->statement, (int) strlen( query->statement ) ) == 0 );
			if ( cleanUp.good ) {
				retCode = mysac_io( sqlclient->connection.my );
				cleanUp.good = ( retCode != MYERR_WANT_WRITE && retCode != MYERR_WANT_READ );
			}
		}
		if ( ! cleanUp.good ) {
			if ( cleanUp.ev ) {
				Sqlclient_CloseConn( sqlclient );
			}
		}
	}
}

static void Mysql_HandleSetDb_cb( picoev_loop* loop, int fd, int events, void* cb_arg ) {
	struct sqlclient_t * sqlclient;
	int retCode;

	sqlclient = (struct sqlclient_t *) cb_arg;
	if ( ( events & PICOEV_TIMEOUT) != 0 )  {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READWRITE ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeout_sec );
		retCode = mysac_io( sqlclient->connection.my );
		if ( retCode != MYERR_WANT_WRITE && retCode != MYERR_WANT_READ ) {
			// yes, we are connected and have a database, we are good to go
			picoev_del( loop, fd );
			picoev_add( loop, fd, PICOEV_WRITE, sqlclient->timeout_sec, Mysql_HandleWrite_cb, cb_arg );
		} else {
			Sqlclient_CloseConn( sqlclient );
		}
	}
}

static void Mysql_HandleConnect_cb( picoev_loop* loop, int fd, int events, void* cb_arg ) {
	struct sqlclient_t * sqlclient;
	int retCode;

	sqlclient = (struct sqlclient_t *) cb_arg;
	if ( ( events & PICOEV_TIMEOUT) != 0 )  {
		Sqlclient_CloseConn( sqlclient );
	} else if ( ( events & PICOEV_READ ) != 0 ) {
		picoev_set_timeout( loop, fd, sqlclient->timeout_sec );
		retCode = mysac_io( sqlclient->connection.my );
		if ( retCode != MYERR_WANT_WRITE && retCode != MYERR_WANT_READ ) {
			picoev_del( loop, fd );
			picoev_add( loop, fd, PICOEV_READWRITE, sqlclient->timeout_sec, Mysql_HandleSetDb_cb, cb_arg );
			//  Yes, we are connected, now set the database, as mysac needs that first
			mysac_set_database( sqlclient->connection.my, sqlclient->dbName );
			if ( mysac_send_database( sqlclient->connection.my ) == MYERR_BAD_STATE ) {
				Sqlclient_CloseConn( sqlclient );
			}
		} else {
			Sqlclient_CloseConn( sqlclient );
		}
	}
}

struct sqlclient_t * Mysql_New( struct core_t * core, const char * hostName, const char * ip, uint16_t port, const char * loginName, const char *password, const char * dbName ) {
	return Sqlclient_New( core, SQLADAPTER_MYSQL, hostName, ip, port, loginName, password, dbName );
}

/*****************************************************************************/
/* Generic                                                                   */
/*****************************************************************************/
static struct sqlclient_t * Sqlclient_New( struct core_t * core, enum sqlAdapter_t adapter, const char * hostName, const char * ip, uint16_t port, const char * loginName, const char *password, const char * dbName ) {
	struct {unsigned int sqlclient:1;
			unsigned int hostName:1;
			unsigned int ip:1;
			unsigned int loginName:1;
			unsigned int password:1;
			unsigned int pool:1;
			unsigned int dbName:1;
			unsigned int good:1;
			} cleanUp;
	struct sqlclient_t * sqlclient;

	sqlclient = NULL;
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good =  ( ( ( sqlclient = malloc( sizeof( *sqlclient ) ) ) != NULL ) );
	if ( cleanUp.good) {
		sqlclient->core = core;
		sqlclient->port = port;
		sqlclient->adapter = adapter;
		switch ( sqlclient->adapter ) {
			case SQLADAPTER_POSTGRESQL: //  ft
			case SQLADAPTER_MYSQL:		//  ft
			default:
				sqlclient->connection.pg = NULL;
			break;
		}
		sqlclient->hostName = sqlclient->ip = sqlclient->loginName = sqlclient->password = sqlclient->dbName = NULL;
	}
	//  @TODO:  drop parameter ip and do a hostname lookup (well actually at 'connect' as the ip address could change)
	if ( hostName == NULL && ip == NULL ) {
		return NULL;
	}else if ( hostName == NULL && ip != NULL ) {
		hostName = ip;
	} else if ( hostName != NULL && ip == NULL ) {
		ip = hostName;
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
		cleanUp.good = ( ( sqlclient->hostName = strdup( hostName ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.hostName = 1;
			cleanUp.good = ( ( sqlclient->ip = strdup( ip ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.ip = 1;
		cleanUp.good = ( ( sqlclient->loginName = strdup( loginName ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.loginName = 1;
		cleanUp.good = ( ( sqlclient->password = strdup( password ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.password = 1;
		cleanUp.good = ( ( sqlclient->dbName  = strdup( dbName) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.dbName = 1;
		cleanUp.good = ( ( sqlclient->pool = pool_list_new( ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.pool = 1;
		sqlclient->timeout_sec = SQLCLIENT_TIMEOUT_SEC;
		sqlclient->socketFd = 0;
		sqlclient->statementId = 0;
		sqlclient->currentQuery = NULL;
		Sqlclient_Connect( sqlclient );
	}
	if ( ! cleanUp.good) {
		if ( cleanUp.pool ) {
			pool_list_delete( sqlclient->pool );
		}
		if ( cleanUp.hostName ) {
			free( ( char* ) sqlclient->hostName ); sqlclient->hostName = NULL;
		}
		if ( cleanUp.ip) {
			free( ( char * ) sqlclient->ip); 	sqlclient->ip= NULL;
		}
		if ( cleanUp.loginName ) {
			free( ( char * ) sqlclient->loginName ); 	sqlclient->loginName = NULL;
		}
		if ( cleanUp.password ) {
			memset( ( char * ) sqlclient->password, '\0', strlen( sqlclient->password ) );
			free( ( char * ) sqlclient->password ); 	sqlclient->password = NULL;
		}
		if ( cleanUp.dbName ) {
			free( ( char * ) sqlclient->dbName ); 		sqlclient->dbName = NULL;
		}
		if ( cleanUp.sqlclient ) {
			free( sqlclient ); sqlclient = NULL;
		}
	}

	return sqlclient;
}

static void Sqlclient_Connect ( struct sqlclient_t * sqlclient ) {
	struct {unsigned int conn:1;
			unsigned int connString:1;
			unsigned int socket:1;
			unsigned int good:1;} cleanUp;
	char * connString;
	size_t len;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	connString = NULL;
	switch ( sqlclient->adapter ) {
		case SQLADAPTER_POSTGRESQL:
			if ( sqlclient->connection.pg == NULL ) {
#define CONNSTRING_TEMPLATE "host=%s port=%d dbname=%s user=%s password=%s hostaddr=%s"
				len = 1 + strlen( CONNSTRING_TEMPLATE ) + strlen( sqlclient->hostName ) + 5 + strlen( sqlclient->dbName ) + strlen( sqlclient->loginName ) + strlen( sqlclient->password ) + strlen( sqlclient->ip ) - 12 ;
				cleanUp.good = ( ( connString = calloc( len, 1 ) ) != NULL );
				if ( cleanUp.good ) {
					cleanUp.connString = 1;
				snprintf( connString, len, CONNSTRING_TEMPLATE, sqlclient->hostName, sqlclient->port, sqlclient->dbName, sqlclient->loginName, sqlclient->password, sqlclient->ip );
#undef CONNSTRING_TEMPLATE
					cleanUp.good = ( ( sqlclient->connection.pg = PQconnectStart( connString ) ) != NULL );
				}
				if ( cleanUp.good ) {
					cleanUp.conn = 1;
					cleanUp.good = ( ( sqlclient->socketFd = PQsocket( sqlclient->connection.pg ) ) > 0 );
				}
				if ( cleanUp.good ) {
					cleanUp.conn = 1;
					picoev_add( sqlclient->core->loop, sqlclient->socketFd, PICOEV_READ, sqlclient->timeout_sec, Postgresql_HandleConnect_cb, (void * ) sqlclient );
					PQsetnonblocking( sqlclient->connection.pg, 1 );
				}
				if ( ! cleanUp.good ) {
					if ( cleanUp.conn ) {
						PQfinish( sqlclient->connection.pg );  sqlclient->connection.pg= NULL;
					}
				}
			}
			break;
		case SQLADAPTER_MYSQL:
			if ( sqlclient->connection.my == NULL ) {
#define CONNSTRING_TEMPLATE "%s:%d"
				len = 1 + strlen( CONNSTRING_TEMPLATE ) + 5 + strlen( sqlclient->hostName ) - 4;
				cleanUp.good = ( ( connString = calloc( len, 1 ) ) != NULL );
				if ( cleanUp.good ) {
					cleanUp.connString = 1;
					snprintf( connString, len, CONNSTRING_TEMPLATE, sqlclient->hostName, sqlclient->port);
#undef CONNSTRING_TEMPLATE
					if ( cleanUp.good ) {
						cleanUp.good = ( ( sqlclient->connection.my = mysac_new( MYSQL_BUFS ) ) != NULL );
					}
					if ( cleanUp.good ) {
						cleanUp.conn = 1;
						cleanUp.good = ( ( sqlclient->socketFd =  mysac_get_fd( sqlclient->connection.my ) ) > 0 );
					}
					if ( cleanUp.good ) {
						picoev_add( sqlclient->core->loop, sqlclient->socketFd, PICOEV_READ, sqlclient->timeout_sec, Mysql_HandleConnect_cb, (void * ) sqlclient );
						SetupSocket( sqlclient->socketFd );
						mysac_setup( sqlclient->connection.my, connString, sqlclient->loginName, sqlclient->password, sqlclient->dbName, 0);
					}
					if ( ! cleanUp.good ) {
						if ( cleanUp.conn ) {
							mysac_close(  sqlclient->connection.my ); sqlclient->connection.my = NULL;
						}
					}
				}
			}
			break;
		default:
			break;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.connString ) {
			memset( connString, 0, strlen( connString ) );  //  password is in the postgresql sqlclient string
			free( connString ); connString = NULL;
		}
	}
}

static void Sqlclient_CloseConn ( struct sqlclient_t * sqlclient ) {
	picoev_del( sqlclient->core->loop, sqlclient->socketFd );
	switch ( sqlclient->adapter ) {
	case SQLADAPTER_POSTGRESQL:
		if ( sqlclient->connection.pg != NULL ) {
			PQfinish( sqlclient->connection.pg ); sqlclient->connection.pg = NULL;
		}
		break;
	case SQLADAPTER_MYSQL:
		if ( sqlclient->connection.my != NULL ) {
			free( sqlclient->connection.my->buf ); sqlclient->connection.my->buf = NULL;
			mysac_close(  sqlclient->connection.my ); sqlclient->connection.my = NULL;
		}
		break;
	default:
		break;
	}
	sqlclient->socketFd = 0;
}

static struct query_t * Sqlclient_PopQuery ( struct sqlclient_t * sqlclient ) {
	struct query_t * query;

	query = NULL;
	if ( sqlclient->currentQuery == NULL ) {
		query = (struct query_t *) pool_list_pop( sqlclient->pool );
		sqlclient->currentQuery = query;
	}
	return query;
}

void Sqlclient_Delete ( struct sqlclient_t * sqlclient ) {
	Sqlclient_CloseConn( sqlclient );
	sqlclient->timeout_sec = 0;
	sqlclient->statementId = 0;
	sqlclient->currentQuery = NULL;
	memset( ( char * ) sqlclient->password, '\0', strlen( sqlclient->password ) );
	free( ( char * ) sqlclient->hostName ); 	sqlclient->hostName = NULL;
	free( ( char * ) sqlclient->ip ); 			sqlclient->ip = NULL;
	free( ( char * ) sqlclient->loginName );	sqlclient->loginName = NULL;
	free( ( char * ) sqlclient->password );		sqlclient->password = NULL;
	free( ( char * ) sqlclient->dbName ); 		sqlclient->dbName = NULL;
	pool_list_delete( sqlclient->pool ); 		sqlclient->pool = NULL;
	free( sqlclient ); 							sqlclient = NULL;

}

void Query_New ( struct sqlclient_t * sqlclient, const char * sqlStatement, size_t paramCount, const char ** paramValues, queryHandler_cb_t callback, void * args ) {
	struct {unsigned int good:1;
			unsigned int query:1;
			unsigned int statement:1;
			unsigned int length:1;
			unsigned int values:1;
			unsigned int params:1;
			} cleanUp;
	struct query_t * query;
	size_t i, j;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	query = NULL;
	cleanUp.good = ( ( query = malloc( sizeof( * query ) ) ) != NULL );
	if ( cleanUp.good )  {
		cleanUp.query = 1;
		query->paramCount = paramCount;
		query->cbHandler = callback;
		query->cbArgs = args;
		query->next = NULL;
		query->sqlclient = sqlclient;
		query->statementId = 0;
		cleanUp.good = ( ( query->statement = strdup( sqlStatement ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.statement = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.statement = 1;
		cleanUp.good = ( ( query->paramLengths = malloc( sizeof( *query->paramLengths ) * query->paramCount ) )  != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.length = 1;
		cleanUp.good = ( ( query->paramValues = malloc( sizeof( *query->paramValues) * query->paramCount ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.values = 1;
		for ( i = 0; i < query->paramCount; i++ ) {
			query->paramLengths[i] = strlen( paramValues[i] );
			cleanUp.good = ( ( query->paramValues[i] = strdup( paramValues[i] ) ) != NULL );
			if ( ! cleanUp.good ) {
				break;
			}
			cleanUp.params = 1;
		}
	}
	if ( cleanUp.good ) {
		pool_list_push( sqlclient->pool, ( void * ) query );
		// Let's see if we can submit this immediately to the server
			if ( sqlclient->socketFd < 1 ) {
				Sqlclient_Connect( sqlclient );
			}
			Sqlclient_PopQuery( sqlclient );
		}
	if ( ! cleanUp.good ) {
		if ( cleanUp.statement ) {
			free( ( char * ) query->statement );	query->statement = NULL;
		}
		if ( cleanUp.length ) {
			free( query->paramLengths ) ; query->paramLengths = NULL;
		}
		if ( cleanUp.values ) {
			free( query->paramValues ); query->paramValues = NULL;
		}
		if ( cleanUp.params ) {
			for ( j = 0; j < i ; j++ ) {
				free( (char *) query->paramValues[j] ); query->paramValues[j] = NULL;
			}
		}
	}
}

void Query_Delete ( struct query_t * query ) {
	size_t i;

	for ( i = 0; i < query->paramCount; i++ ) {
		free( (char *) query->paramValues[i] ); query->paramValues[i] = NULL;
	}
	free( ( char * ) query->statement ); query->statement = NULL;
	free( query->paramLengths ); query->paramLengths = NULL;
	free( query->paramValues ); query->paramValues = NULL;
	query->cbHandler = NULL;
	query->cbArgs = NULL;
	query->paramCount = 0;
	switch ( query->sqlclient->adapter ) {
	case SQLADAPTER_POSTGRESQL:
		if ( query->result.pg != NULL ) {
			PQclear( query->result.pg ); query->result.pg = NULL;
		}
		break;
	case SQLADAPTER_MYSQL:
		if ( query->result.my != NULL ) {
			mysac_free_res( query->result.my ); query->result.my = NULL;
		}
		break;
	default:
		break;
	}
}