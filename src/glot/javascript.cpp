#include <stdlib.h>
#include <string.h>

#include "javascript.h"
#include "../feature/sqlclient.h"
#include "../feature/webserver.h"
#include "../core/utils.h"


static int jsInterpretersAlive = 0;

static bool Javascript_IncludeScript( struct javascript_t * javascript, const char * cfile );

#define JAVASCRIPT_MODULE_ACTION( action ) do { \
		JSAutoRequest			ar( javascript->context ); \
		JSAutoCompartment		ac( javascript->context, javascript->globalObj ); \
		JS::HandleObject		globalObjHandle( javascript->globalObj ); \
		JS::RootedValue			rVal( javascript->context ); \
		JS::MutableHandleValue	rValMut( &rVal); \
		JS::RootedValue			hardVal( javascript->context ); \
		JS::HandleValue			hardValHandle( hardVal ); \
		JS::MutableHandleValue	hardValMut( &hardVal ); \
		JS::RootedObject		hardObj( javascript->context ); \
		JS::HandleObject		hardObjHandle( hardObj ); \
		JS::MutableHandleObject	hardObjMut( &hardObj ); \
		JS_GetProperty( javascript->context, globalObjHandle, "Hard", hardValMut ); \
		cleanUp.good = ( ( JS_ValueToObject( javascript->context, hardValHandle, hardObjMut ) ) == true ); \
		JS_CallFunctionName( javascript->context, hardObjHandle, action, JS::HandleValueArray::empty(), rValMut ); \
	} while ( 0 );

/**
 * The spidermonkey module is loaded.
 *
 * @name	Hard.onLoad
 * @event
 * @public:
 * @since	0.0.5b
 * @returns	{null}
 *
 * @example
 * this.Hard.onLoad = function(){
 * 	console.log("loaded" );
 * };
 *
 * @see	Hard.onReady
 * @see	Hard.onUnload
 */
void * JavascriptModule_Load( struct core_t * core ) {
	struct javascript_t * javascript;
	cfg_t * glotSection, * javascriptSection;
	char * path, * name;
	struct {unsigned char good:1;
			unsigned char javascript:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	glotSection = cfg_getnsec( core->config, "glot", 0 );
	javascriptSection = cfg_getnsec( glotSection, "Javascript", 0 );
	path = cfg_getstr( javascriptSection, (char *) "path" );
	name = cfg_getstr( javascriptSection, (char *) "main" );
	cleanUp.good = ( ( javascript = Javascript_New( core, path, name ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
		JAVASCRIPT_MODULE_ACTION( "onLoad" )
	}
	if (! cleanUp.good ) {
		if ( cleanUp.javascript ) {
			Javascript_Delete( javascript );
		}
	}

	return (void *) javascript;
}

/**
 * The javascript module is loaded and ready to run.
 *
 * @name	Hard.onReady
 * @event
 * @public
 * @since	0.0.5b
 * @returns	{null}
 *
 * @example
 * this.Hard.onReady = function(){
 * 	console.log("loaded and ready" );
 * };
 *
 * @see	Hard.onLoad
 * @see	Hard.onUnload
 */
void JavascriptModule_Ready( struct core_t * core, void * args ) {
	struct javascript_t * javascript;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	javascript = (struct javascript_t * ) args;
	JAVASCRIPT_MODULE_ACTION( "onReady" )
}

/**
 * The javascript module is stopping.
 *
 * @name	Hard.onUnload
 * @event:
 * @public
 * @since	0.0.5b
 * @returns	{null}
 *
 * @example
 * this.Hard.onUnload = function(){
 * 	console.log("unloading" );
 * };
 *
 * @see	Hard.onLoad
 * @see	Hard.onReady
 * @see	Hard.shutdown
*/
void JavascriptModule_Unload( struct core_t * core, void * args ) {
	struct javascript_t * javascript;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	javascript = (struct javascript_t * ) args;
	JAVASCRIPT_MODULE_ACTION( "onUnload" )

	Javascript_Delete( javascript );
}
#if 0
/*
 ===============================================================================
 Sqlclient OBJECT
 ===============================================================================
 */

/**
 * Sql client connection object.
 *
 * @name Hard.Sqlclient
 * @private:
 * @object
 */
#define SQL_CLIENT_QUERY_RESULT_HANDLER_CB( dbResultType, formatter ) \
	JSContext * cx; \
	struct jsPayload_t_s * payload; \
	JSCompartment * oldCompartment; \
	dbResultType * result; \
	jsval paramVal[2], retVal; \
	JSObject * resultObj; \
	bool good; \
	\
	good = ( ( result = ( dbResultType * ) query->result ) != NULL ); \
	if ( good ) { \
		payload = ( struct jsPayload_t_s * ) query->cbArgs; \
		cx = payload->cx; \
		JS_BeginRequest( payload->cx ); \
		oldCompartment = JS_EnterCompartment( payload->cx, payload->globalObj ); \
		JS_AddValueRoot( payload->cx, paramVal ); \
		resultObj = formatter( payload->cx, result ); \
		paramVal[0] = OBJECT_TO_JSVAL( resultObj ); \
		paramVal[1] = INT_TO_JSVAL( ( int ) returnCode ); \
		JS_RemoveObjectRoot( payload->cx, &resultObj ); \
		JS_CallFunctionValue( payload->cx, payload->obj, payload->fnVal, 2, paramVal, &retVal ); \
		JS_RemoveValueRoot( payload->cx, paramVal ); \
		\
		delete payload; 		payload = NULL; \
		JS_LeaveCompartment( cx, oldCompartment ); \
		JS_EndRequest( cx );\
	}

#define SQL_CLIENT_QUERY( className, handler ) \
	className * sql; \
	jsval paramList, value, dummy1, dummy2; \
	JSObject * sqlObj, * paramObj, * globalObj, * paramIter; \
	JSString * jStatement, * sValue; \
	JSBool success; \
	jsid *indexId; \
	unsigned int nParams, i; \
	struct jsPayload_t_s * payload; \
	const char ** cParamValues; \
	char * cStatement; \
	struct {unsigned int payload:1; } cleanUp; \
	bool good; \
	\
	memset( &cleanUp, 0, sizeof( cleanUp ) ); \
	i = 0; \
	sql = NULL; \
	nParams = 0; \
	cParamValues = NULL; \
	cStatement = NULL; \
	payload = NULL; \
	sqlObj = JS_THIS_OBJECT( cx, vpn ); \
	globalObj = JS_GetGlobalForScopeChain( cx ); \
	good = ( ( payload = new struct jsPayload_t_s( cx, sqlObj, globalObj ) ) != NULL ); \
	if ( good ) { \
		cleanUp.payload = 1; \
		good = ( ( sql = static_cast<className *>( JS_GetPrivate( sqlObj ) ) ) != NULL ); \
	} \
	if ( good ) { \
		good = ( JS_ConvertArguments( cx, 3, JS_ARGV( cx, vpn ), "S*f", &jStatement, &dummy1, &dummy2 ) == JS_TRUE ); \
	} \
	if ( good ) { \
		good = ( ( cStatement = JS_EncodeString( cx, jStatement ) ) != NULL ); \
	} \
	if ( good ) { \
		paramList = JS_ARGV( cx, vpn )[1]; \
		if ( JSVAL_IS_NULL( paramList ) || JSVAL_IS_PRIMITIVE( paramList ) ) { \
		} else { \
			indexId = NULL; \
			paramObj = JSVAL_TO_OBJECT( paramList ); \
			JS_GetArrayLength( cx, paramObj, &nParams ); \
			if ( nParams > 0 ) { \
				good = ( ( cParamValues = ( const char ** ) new char*[nParams] ) != NULL ); \
				if ( good ) { \
					paramIter = JS_NewPropertyIterator( cx, paramObj ); \
					if ( paramIter != NULL ) { \
						do { \
							success = JS_NextProperty( cx, paramObj, indexId ); \
							if ( JS_GetPropertyById( cx, paramObj, *indexId, &value ) ) { \
								sValue = JS_ValueToString( cx, value ); \
								good = ( ( cParamValues[i] = JS_EncodeString( cx, sValue ) ) != NULL ); \
								if ( ! good ) { \
									break; \
								} \
								i++; \
							} \
						} while ( success == JS_TRUE && *indexId != JSID_VOID && good ); \
					} \
				} \
			} \
		} \
	} \
	if ( good ) { \
		good = ( JS_ConvertValue( cx, JS_ARGV( cx, vpn )[2], JSTYPE_FUNCTION, &payload->fnVal ) == JS_TRUE ); \
	} \
	if ( good ) { \
		JS_AddValueRoot( cx, &payload->fnVal ); \
		good = sql->Query( cStatement, nParams, cParamValues, handler, ( void * ) payload ); \
	} \
	if ( good ) { \
		JS_SET_RVAL( cx, vpn, JSVAL_TRUE ); \
	} else { \
		if ( cleanUp.payload ) { \
			delete payload; payload = NULL; \
		} \
		JS_SET_RVAL( cx, vpn, JSVAL_FALSE ); \
	} \
	for ( i = 0;  i < nParams; i++ ) { \
		JS_free( cx, ( char * ) cParamValues[i] ); \
	} \
	delete[ ] cParamValues; 	cParamValues = NULL; \
	JS_free( cx, cStatement ); 	cStatement = NULL; \
	return ( good ) ? JS_TRUE : JS_FALSE;

#if HAVE_MYSQL == 1
/*
 ===============================================================================
 Mysql OBJECT
 ===============================================================================
 */

static void JsnMysqlClientFinalize( JSFreeOp * fop, JSObject * myqlObj );

static inline JSObject *  MysqlQueryResultToJS( JSContext * cx, void * rawRes );
static inline JSObject *  MysqlQueryResultToJS( JSContext * cx, void * rawRes ) {
	JSObject * record, * resultArray;
	JSString * jstr;
	MYSAC_ROW *row;
	MYSAC_RES * result;
	jsval jValue, currentVal;
	unsigned int rowId, rowCount;
	int colId, colCount;
	char * cFieldName, * cValue;
	bool good;
	//  @TODO: check memory allocs and evt cleanup
	result = static_cast<MYSAC_RES *>( rawRes );
	good = ( ( resultArray = JS_NewArrayObject( cx, 0, NULL ) ) != NULL );
	if ( good ) {
		JS_AddObjectRoot( cx, &resultArray );
		if ( result != NULL ) {
			rowCount = ( unsigned int ) mysac_num_rows( result );
			if ( rowCount > 0 ) {
				colCount = mysac_field_count( result );
				rowId = 0;
				while ( good && ( row = mysac_fetch_row( result ) ) != NULL ) {
					good = ( (record = JS_NewObject( cx, NULL, NULL, NULL ) ) != NULL );
					if ( ! good ) {
						break;
					}
					JS_AddObjectRoot( cx, &record );
					currentVal = OBJECT_TO_JSVAL( record );
					JS_SetElement( cx, resultArray, rowId, &currentVal );
					JS_RemoveObjectRoot( cx, &record );
					for (colId = 0; colId < colCount; colId++) {
						cFieldName = ( ( MYSAC_RES * )result)->cols[colId].name;
						cValue = row[colId].blob;
						if ( cValue == NULL ) {
							jValue = JSVAL_NULL;
						} else {
							good = ( ( jstr = JS_NewStringCopyZ( cx, cValue ) ) != NULL );
							if ( good ) {
								jValue = STRING_TO_JSVAL( jstr  );
							} else {
								jValue = JSVAL_VOID;  //  not quite true
							}
							if ( !good ) {
								break;
							}
						}
						JS_SetProperty( cx, record, cFieldName, &jValue );
					}
					rowId++;
				}
			}
			//  lastOid = mysac_insert_id( MYSAC *  	m );
			//  affected += int mysac_affected_rows ( MYSAC *  	mysac );
		}
	}

	return resultArray;
}

static void MysqlClientQueryResultHandler_cb( SqlCs_s * query, void * cbArgs, enum CsCode returnCode ) {
	SQL_CLIENT_QUERY_RESULT_HANDLER_CB( MYSAC_RES, MysqlQueryResultToJS )
}
/**
 * Submit a command or a query over the mysql connection.
 *
 * The results of the command handled by a javascript function.
 *
 * @name	Weld.MysqlClient.query
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}					If the query could be registered successfully it returns true; else false is returned
 * @param	{string}	statement		A sql command or query.
 * @param	{array}		[parameters]	A list of parameters that can be used in a query.<p>default: []</p>
 * @param	{function}	fn				The callback function ({response} query, int returnCode SuccessCode (..)
 *
 * @example
 * var pg = this.Weld.PostgresqlClient('hostaddr=10.0.0.25 dbname=apedevdb user=apedev password=vedepa port=5432;', 60);
 * pg.query('SELECT name, sales FROM sales WHERE customer ='$1' );' , ['foobar' ], function( res, returnCode) {
 * 	if ( returnCode == 0) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++) {
 * 				row  = res[rowId];
 * 				console.log(rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	});
 *
 * @see	Weld.MysqlClient
 * @see	Weld.PostgresqlClient.query
 */
static JSBool JsnMysqlClientQuery( JSContext * cx, unsigned argc, jsval * vpn ) {
	SQL_CLIENT_QUERY( prMyClient , MysqlClientQueryResultHandler_cb )
}

JSClass jsnMysqlClientClass = {
	"MysqlClient",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnMysqlClientFinalize,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec jsnMysqlClientMethods[ ] = {
	JS_FS( "query", JsnMysqlClientQuery, 3, 0 ),
	JS_FS_END
};

/**
 * Connect to a mysql server.
 *
 * @name	Weld.MysqlClient
 * @constructor
 * @public
 * @since	0.0.5b
 * @returns	{object}							The mysql client javascript instance
 * @param	{string}		hostString			The connection string. Please note: this is the ip:host combination (e.g. '10.0.0.25:3361')
 * @param	{string}		userName			The username for the mysql database.
 * @param	{string}		password			The password corresponding for this username
 * @param	{string}		databaseName		The database to connect to
 * @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' int the mysql section of the configurationFile.<br />Please note also the connect-timeout settings: in /etc/mysql/my.cnf as the mysql server may close idle connections as well.</p>
 *
 * @example
 * var my = this.Weld.MysqlClient('10.0.0.25:3361, 'apedev', 'vedepa', 'apedevdb', 60);
 * pg.query('SELECT name, sales FROM sales WHERE customer ='$1' );' , ['foobar' ], function( res, returnCode) {
 * 	if ( returnCode == 0) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++) {
 * 				row  = res[rowId];
 * 				console.log(rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	});
 * @see	Weld.MysqlClient.query
 * @see	Weld.PostgreslClient
 */
static JSBool JsnMysqlClientClassConstructor( JSContext * cx, unsigned argc, jsval * vpn ) {
	prMyClient * mysql;
	prJavascript * instance;
	JSObject * globalObj, * mysqlObj;
	JSString * jHostString, * jUserName, * jPassword, * jDbName;;
	char * cHostString, * cUserName, * cPassword, * cDbName;
	int timeout, len;
	bool good;

	timeout = 0;
	cHostString = cUserName = cPassword = cDbName = NULL;
	good = ( JS_ConvertArguments( cx, argc, JS_ARGV( cx, vpn ), "SSSS/i", &jHostString, &jUserName, &jPassword, &jDbName, &timeout )  == JS_TRUE );
	if ( good ) {
		good = ( ( cHostString = JS_EncodeString( cx, jHostString ) ) != NULL );
	}
	if ( good ) {
		good = ( ( cUserName = JS_EncodeString( cx, jUserName ) ) != NULL );
	}
	if ( good ) {
		good = ( ( cPassword = JS_EncodeString( cx, jPassword ) ) != NULL );
	}
	if ( good ) {
		good = ( ( cDbName = JS_EncodeString( cx, jDbName ) ) != NULL );
	}
	if ( good ) {
		globalObj = JS_GetGlobalForScopeChain( cx );
		instance = static_cast<prJavascript *>( JS_GetPrivate( globalObj ) );
		good = ( ( mysql = new prMyClient( instance->core, cHostString, cUserName, cPassword, cDbName, timeout ) ) != NULL );
		good = good && mysql->clientState > CLIENTSTATE_DESTRUCTED;
	}
	if ( good ) {
		mysqlObj = JS_NewObjectForConstructor( cx, &jsnMysqlClientClass, vpn );
		JS_DefineFunctions( cx, mysqlObj, jsnMysqlClientMethods );
		JS_SetPrivate( mysqlObj, ( void * ) mysql );
		JS_SET_RVAL( cx, vpn, OBJECT_TO_JSVAL( mysqlObj ) );
	} else {
		JS_SET_RVAL( cx, vpn, JSVAL_VOID );
	}
	len = strlen(cPassword );
	JS_free( cx, cHostString ); cHostString = NULL;
	JS_free( cx, cUserName ); cUserName = NULL;
	JS_free( cx, cPassword ); 	memset(cPassword, '\0', len );
	JS_free( cx, cDbName ); cDbName = NULL;
	return ( good ) ? JS_TRUE : JS_FALSE;
}

static void JsnMysqlClientFinalize( JSFreeOp * fop, JSObject * mysqlObj ) {
	struct prMyClient *mysql;

	if ( ( mysql = static_cast<prMyClient*>( JS_GetPrivate( mysqlObj ) ) ) != NULL ) {
		delete mysql; mysql = NULL;
	}
}
#endif
/*
 ===============================================================================
 Postgresql OBJECT
 ===============================================================================
 */

static void JsnPostgresqlClientFinalize( JSFreeOp * fop, JSObject * postgresqlObj );

static inline JSObject *  PostgresqlQueryResultToJS( JSContext * cx, void * rawRes );
static inline JSObject *  PostgresqlQueryResultToJS( JSContext * cx, void * rawRes ) {
	JSObject * record, * resultArray;
	ExecStatusType status;
	PGresult * result;
	JSString * jStr;
	jsval jValue, currentVal;
	Oid dataType;
	unsigned int rowId, rowCount;
	int colId, colCount;
	char * cFieldName, * cValue;
	bool good;
	//  @TODO: check memory allocs and evt cleanup
	result = static_cast<PGresult *>( rawRes );
	good = ( ( resultArray = JS_NewArrayObject( cx, 0 ) ) != NULL );
		JS::RootedObject resultObject( resultArray );
	//if ( good ) {
		//JS_AddObjectRoot( cx, &resultArray );
		if ( result != NULL ) {
			status = PQresultStatus( result );
			switch ( status ) {
			case PGRES_COPY_OUT:		  //  FT
			case PGRES_COPY_IN:			  //  FT
			case PGRES_COPY_BOTH:		  //  FT
				//  for the moment, we do not handle all these at all
				break;
			case PGRES_NONFATAL_ERROR:	  //  FT
			case PGRES_BAD_RESPONSE: 	  //  FT
			case PGRES_FATAL_ERROR:
				//  @code = -1;
				break;
			case PGRES_EMPTY_QUERY:       //  FT
			case PGRES_COMMAND_OK:
			default:
				break;
			case PGRES_SINGLE_TUPLE:	  //  FT
			case PGRES_TUPLES_OK:
				rowCount = ( unsigned int ) PQntuples( result );
				colCount = PQnfields( result );
				for ( rowId = 0; rowId < rowCount; rowId++ ) {
					good = ( (record = JS_NewObject( cx, NULL, NULL, NULL ) ) != NULL );
					if ( ! good ) {
						break;
					}
					JS_AddObjectRoot( cx, &record );
					currentVal = OBJECT_TO_JSVAL( record );
					JS_SetElement( cx, resultArray, rowId, &currentVal );
					JS_RemoveObjectRoot( cx, &record );
					for ( colId = 0; colId < colCount; colId++ ) {
						cFieldName = PQfname( result, colId );  //  speedup might be possible by caching this
						dataType = PQftype( result, colId );
						if ( PQgetisnull( result, rowId, colId ) == 1 ) {
							jValue = JSVAL_NULL;
						} else {
							cValue = PQgetvalue( result, rowId, colId );
							switch ( dataType ) {
							//  it is possible to make a even better mapping to postgresql data types to JSAPI DATA TYPES: this relies on the settings in /usr/include/postgresql/catalog/pg_type.h
							case 16:      //  bool
								jValue = ( strcmp( cValue, "t" ) == 0 ) ? JSVAL_TRUE : JSVAL_FALSE;
								break;
							case 2278:    //  void
								jValue = JSVAL_VOID;
							case 20:
							case 21:
							case 23:
							case 26:      //  int
							case 700:
							case 701:
							case 1700:    //  digits
								jValue = JS_NumberValue( ( double ) atof( cValue ) );
								break;
								//  case 702: case 703: case 704: case 1082: case 1083: case 1114: case 1184: case 1186: case 1266://time
							case 1114: {
								int year, month, day, hour, min, sec, f, found;
								found = sscanf( cValue, "%4d-%2d-%2d %2d:%2d:%2d.%6d", &year, &month, &day, &hour, &min, &sec, &f );
								if ( 7 == found ) {
									JSObject * dateObj = JS_NewDateObject( cx, year, month, day, hour, min, sec );
									jValue = OBJECT_TO_JSVAL( dateObj );
									break;
								}  //  else ft
							}
							case 17:
							case 18:
							case 19:
							case 25:
							case 142:
							case 143:
							case 194:
							default:
								good = ( ( jStr = JS_NewStringCopyZ( cx, cValue ) ) != NULL );
								if ( good ) {
									jValue = STRING_TO_JSVAL( jStr  );
								} else {
									jValue = JSVAL_VOID;  //  not quite true
								}
								break;
							}
						}
						JS_SetProperty( cx, record, cFieldName, &jValue );  //  hmm, what if postgresql columns do not have ascii chars, ecma does not allow that...?
					}
				}
				break;
			}
			//  lastOid = ( unsigned long ) PQoidValue( res );
			//  affected += atoi( PQcmdTuples( res ) );
		}
	//}

	return resultArray;
}

static void PostgresqlClientQueryResultHandler_cb( struct SqlCs_s* query, void * cbArgs, enum CsCode returnCode ) {
	SQL_CLIENT_QUERY_RESULT_HANDLER_CB( PGresult, PostgresqlQueryResultToJS )
}

/**
 * Submit a command or a query over the postgresql connection.
 *
 * The results of the command handled by a javascript function.
 *
 * @name	Weld.PostgresqlClient.query
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}					If the query could be registered successfully it returns true; else false is returned
 * @param	{string}	statement		A sql command or query.
 * @param	{array}		[parameters]	A list of parameters that can be used in a query.<p>default: []</p>
 * @param	{function}	fn				The callback function ({response} query, int returnCode SuccessCode (..)
 *
 * @example
 * var pg = this.Weld.PostgresqlClient('hostaddr=10.0.0.25 dbname=apedevdb user=apedev password=vedepa port=5432;', 60);
 * pg.query('SELECT name, sales FROM sales WHERE customer ='$1' );' , ['foobar' ], function( res, returnCode) {
 * 	if ( returnCode == 0) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++) {
 * 				row  = res[rowId];
 * 				console.log(rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	});
 *
 * @see	Weld.PostgresqlClient
 * @see	Weld.MysqlClient.query
 */
static JSBool JsnPostgresqlCientQuery( JSContext * cx, unsigned argc, jsval * vpn ) {
	SQL_CLIENT_QUERY( prPgClient, PostgresqlClientQueryResultHandler_cb )
}

JSClass jsnPostgresqlClientClass = {
	"PostgresqlClient",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnPostgresqlClientFinalize,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec jsnPostgresqlClientMethods[ ] = {
	JS_FS( "query", JsnPostgresqlCientQuery, 3, 0 ),
	JS_FS_END
};

/**
* Connect to a postgresql server.
*
* @name	Weld.PostgresqlClient
* @constructor
* @public
* @since	0.0.5b
* @returns	{object}							The postgresql client javascript instance
* @param	{string}		connectionString	The connection string. Please note: The `hostaddr` must be an ip address, but Hostname may be given so that the .pgpass can work. Refer to http://www.postgresql.org/docs/9.3/static/libpq-connect.html#LIBPQ-PARAMKEYWORDS for more details.
* @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' int the postgresql section of the configurationFile.</p>
*
* @example
* var pg = this.Weld.PostgresqlClient('hostaddr=10.0.0.25 dbname=apedevdb user=apedev password=vedepa port=5432;', 60);
* pg.query('SELECT name, sales FROM sales WHERE customer ='$1' );' , ['foobar' ], function( res, returnCode) {
* 	if ( returnCode == 0) {
* 		for ( var rowId = 0; rowId < res.length; rowId++) {
* 				row  = res[rowId];
* 				console.log(rowId + ' ' + row.name + ' ' + row.sales );
* 			}
* 		}
* 	});
* @see	Weld.PostgresqlClient.query
* @see	Weld.MysqlClient
*/
static JSBool JsnPostgresqlClientClassConstructor( JSContext * cx, unsigned argc, jsval * vpn ) {
	prPgClient * postgresql;
	prJavascript * instance;
	JSObject * globalObj, * postgresqlObj;
	JSString * jConnectionString;
	char * cConnectionString;
	int timeout;
	bool good;

	timeout = 0;
	cConnectionString = NULL;
	good = ( JS_ConvertArguments( cx, argc, JS_ARGV( cx, vpn ), "S/i", &jConnectionString, &timeout ) == JS_TRUE );
	if ( good ) {
		good = ( ( cConnectionString = JS_EncodeString( cx, jConnectionString ) ) != NULL );
	}
	if ( good ) {
		globalObj = JS_GetGlobalForScopeChain( cx );
		instance = static_cast<prJavascript *>( JS_GetPrivate( globalObj ) );;
		good = ( ( postgresql = new prPgClient( instance->core, cConnectionString, timeout ) ) != NULL );
		good = good && postgresql->clientState > CLIENTSTATE_DESTRUCTED;
	}
	if ( good ) {
		postgresqlObj = JS_NewObjectForConstructor( cx, &jsnPostgresqlClientClass, vpn );
		JS_DefineFunctions( cx, postgresqlObj, jsnPostgresqlClientMethods );
		JS_SetPrivate( postgresqlObj, ( void * ) postgresql );
		JS_SET_RVAL( cx, vpn, OBJECT_TO_JSVAL( postgresqlObj ) );
	} else {
		JS_SET_RVAL( cx, vpn, JSVAL_VOID );
	}
	memset( cConnectionString, '\0', strlen( cConnectionString ) );
	JS_free( cx, cConnectionString ); cConnectionString = NULL;
	return ( good ) ? JS_TRUE : JS_FALSE;
	}

	static void JsnPostgresqlClientFinalize( JSFreeOp * fop, JSObject * postgresqlObj ) {
	struct prPgClient *postgresql;

	if ( ( postgresql = static_cast<prPgClient*>( JS_GetPrivate( postgresqlObj ) ) ) != NULL ) {
		delete postgresql; postgresql = NULL;
	}
}
#endif
/*
 ===============================================================================
 Webserver OBJECT
 ===============================================================================
 */
/**
 * Webserver response object.
 *
 * @name Hard.Webserver.req
 * @private:
 * @object
 */
static void JsnWebserverFinalize( JSFreeOp * fop, JSObject * webserverObj );

static const JSClass jsnWebserverClass = {
	"Webserver",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnWebserverFinalize, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsnWebserverMethods[ ] = {
	//JS_FS( "addRoute", JsnWebserverAddRoute, 2, 0 ),
	//JS_FS( "addDocumentRoot", JsnWebserverAddDocumentRoot, 2, 0 ),
	JS_FS_END };

/**
 * Start a webserver.
 *
 * @name	Hard.Webserver
 * @constructor
 * @public:
 * @since	0.0.5b
 * @returns	{object}					The web server javascript javascript
 * @param	{string}		serverIp	The Ip address that the server will listen to.<p>default: The value for 'server_address' int the http section of the configurationFile.</p>
 * @param	{integer}		[port]		The port that the server should bind to.<p>default: The value for 'port' int the http section of the configurationFile.</p>
 * @param	{integer}		[timeout]	The timeout for valid connections<p>default: The value for 'timeout' int the http section of the configurationFile.</p>
 *
 * @example
 * var ws = this.Hard.Webserver( '10.0.0.25', 8888, 60 );
 * ws.addRoute('^/a$', function(client, returnCode) {
 * 	console.log('got '  + client.url);
 * 	client.response.content='<html><h1>response </h1><html>';
 * 	});
 * ws.addDocumentRoot('^/static/(.*)', '/var/www/static/');
 *
 * @see	Hard.Webserver.addRoute
 * @see	HarHardtpServer.addDocumentRoot
 */
static bool JsnWebserverClassConstructor( JSContext * cx, unsigned argc, /*JS::MutableHandleValue */jsval * vpn ) {
	struct webserver_t * webserver;
	struct javascript_t * javascript;
	JSString * jServerIp;
	JS::CallArgs args;
	const char * cDocumentRoot, *cPath;
	char * cServerIp;
	int port, timeoutSec;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vpn );
	timeoutSec = 0;
	cDocumentRoot = NULL;
	cPath = NULL;
	port = 0;
	cServerIp = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S/ii", &jServerIp, &port, &timeoutSec ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cServerIp = JS_EncodeString( cx, jServerIp ) ) != NULL );
	}
	JS::RootedObject thisObj( cx, JS_THIS_OBJECT( cx, vpn ) );
	if ( thisObj != NULL ) {
		cleanUp.good = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( javascript = (struct javascript_t *) JS_GetPrivate( thisObj ) ) != NULL );
	}
	if (cleanUp.good ) {

		cleanUp.good = ( ( webserver = Webserver_New( javascript->core, cServerIp, (uint16_t) port, (unsigned char) timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ){
		cleanUp.good = ( Webserver_DocumentRoot( webserver, cPath, cDocumentRoot )== 1);
	}
	if ( cleanUp.good ) {
		JS::RootedObject webserverObj( cx, JS_NewObject( cx, &jsnWebserverClass, JS::NullPtr(), JS::NullPtr() ) );
		JS::HandleObject webserverObjHandle( webserverObj );
		JS_DefineFunctions( cx, webserverObjHandle, jsnWebserverMethods );
		JS_SetPrivate( webserverObj, ( void * ) webserver );
		Webserver_JoinCore( webserver );
		args.rval().setObject( *webserverObj );
	} else {
		args.rval().setUndefined();
	}

	JS_free( cx, cServerIp );
	return ( cleanUp.good ) ? true: false;
}

static void JsnWebserverFinalize( JSFreeOp * fop, JSObject * webserverObj ) {
	struct webserver_t * webserver;

	if ( ( webserver = ( struct webserver_t * ) JS_GetPrivate( webserverObj ) ) != NULL ) {
		Webserver_Delete( webserver ); webserver = NULL;
	}
}
/*===============================================================================
 * Hard javascript object.
 *
 * @name Hard
 * @public
 * @namespace
 ===============================================================================
 */
static bool JsnFunctionStub( JSContext * cx, unsigned argc, jsval * vpn );
static bool JsnFunctionStub( JSContext * cx, unsigned argc, jsval * vpn ) {
	return true;
}

/**
 * Shutdown the engine after a timeout.
 *:
 * This will stop the engine.
 *
 * @name	Hard.shutdown
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{null}
 * @param	{integer}		[timeout]	Time to wait before the shut down should start. Defaults to 1 second.
 *
 * @example
 *Hard.shutdown( 10 );
 */
static bool JsnHardShutdown( JSContext * cx, unsigned argc, /*JS::MutableHandleValue*/ jsval * vpn ) {
	struct javascript_t * javascript;
	JS::CallArgs args;
	int timeout;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vpn );
	JS::RootedObject thisObj( cx, JS_THIS_OBJECT( cx, vpn ) );
	if ( argc == 0 ) {
		timeout = 1;
	} else {
		cleanUp.good = ( JS_ConvertArguments( cx, args, "i", &timeout ) == true );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( javascript = (struct javascript_t *) JS_GetPrivate( thisObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		javascript->core->keepOnRunning = 0;
	}
	return ( cleanUp.good ) ? true : false;
}

static const JSClass jsnHardClass = {
	"Hard",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsnHardMethods[ ] = {
	JS_FS( "shutdown", 			JsnHardShutdown, 1, 0 ),
	JS_FS( "onInit", 			JsnFunctionStub, 0, 0),
	JS_FS_END
};

/**
===============================================================================
* Standard javascript object.
* Log messages to the console and the logger
*
* @name console
* @public
* @namespace
===============================================================================
*/

/**
* Log a message.
*
* Messages will be logged to the console and the syslog in the back.
*
* @name	console.log
* @function
* @public
* @since	0.0.5b
* @returns	{undefined}
* @param	{string}		message	Message to be logged.
*
* @example
* console.log( 'hello world' );
*/
static bool JsnConsoleLog( JSContext * cx, unsigned argc, jsval * vpn ) {
	struct javascript_t * javascript;
	JSObject *  consoleObj;
	JSString * jString;
	JS::CallArgs args;
	const char *fileName;
	unsigned int lineNo;
	char *cString;
	struct {unsigned char good:1;
			unsigned char cstring:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cString = NULL;
	args = CallArgsFromVp( argc, vpn );
	args.rval().setUndefined();
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jString ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cString = JS_EncodeString( cx, jString ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.cstring = 1;
		consoleObj = JS_THIS_OBJECT( cx, vpn );
		cleanUp.good = ( ( javascript = (struct javascript_t *)( JS_GetPrivate( consoleObj ) ) ) != NULL );
	}
	if ( cleanUp.good ) {
#if DEBUG && 0
		JSScript *script;
		//  @FIXME:  act on this depending on the compiled javascript debug build
		JS_DescribeScriptedCaller( cx, &script, &lineNo );
		fileName = JS_GetScriptFilename( cx, script );
#else
		fileName = __FILE__;
		lineNo = __LINE__;
#endif
		LOG( javascript->core, LOG_INFO, "[%s:%d] : %s", fileName, lineNo, cString );
	}
	if ( cleanUp.cstring ) {
		JS_free( cx, cString );
	}

	return ( cleanUp.good ) ? true : false;
}

static const JSClass jsnConsoleClass = {
	"console",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsnConsoleMethods[ ] = {
	JS_FS( "log",					JsnConsoleLog, 1, 0 ),
	JS_FS_END
};

/**
===============================================================================
* Standard javascript object.
*
* @name global
* @public
* @namespace
===============================================================================
*/
struct jsPayload_t {
	JSContext *						cx;
	JS::RootedObject				obj;
	JS::RootedValue *				fnVal;
	JS::HandleValueArray *			args;
	bool							repeat;
};

static void JsPayload_Delete( struct jsPayload_t * payload ) {
	JSAutoRequest ar( payload->cx );
	payload->fnVal = NULL;
	JS_free( payload->cx, payload->args ); payload->args = NULL;
	JS_free( payload->cx, payload ); payload = NULL;
}

static struct jsPayload_t * JsPayload_New( JSContext * cx, JSObject * object, JS::RootedValue * fn, JS::HandleValueArray * args, bool repeat ) {
	struct jsPayload_t * payload;
	struct {unsigned char good:1;
			unsigned char payload:1;
			unsigned char args:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( payload = (struct jsPayload_t* ) JS_malloc( cx, sizeof( * payload ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		payload->cx = cx;
		payload->obj = object;
		payload->fnVal = fn;
		payload->args = args;
		payload->repeat = repeat;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.args ) {
			JS_free( cx, payload->args ); payload->args = NULL;
		}
		if ( cleanUp.payload ) {
			JS_free( cx, payload ); payload = NULL;
		}
	}
	return payload;
}

/**
* Load an other javascript file.
*
* Files will be interpreted only upon loading. Once the file has been loaded, any changes to the file will be ignored. The files will be relative to the 'javascript/scripts_path' configuration settings.
*
* @name	include
* @function
* @public
* @since	0.0.5b
* @returns	{null}
* @param	{string}		scriptname	File that will be interpreted upon loading.
*
* @example
* try {
* 	include( './framework/orm.js' );
* 	include( './models/customers.js' );
* 	include( './models/purchases.js' );
* 	include( './bizlog/invoice.js' );
* 	include( './tools/pdf.js' );
* 	include( './layouts/invoice.js' );
* 	include( './bizlog/archive.js' );
* } catch ( e ) {
* 	console.log( 'loading failed: ' + e.getMessage() );
* }
*/
static bool JsnGlobalInclude( JSContext * cx, unsigned argc, jsval * vpn ) {
	struct javascript_t * javascript;
	JSString * jFile;
	JSObject * globalObj;
	JS::CallArgs args;
	char * cFile;
	bool success;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cFile = NULL;
	args = CallArgsFromVp( argc, vpn );
	cleanUp.good = ( argc == 1 );
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jFile ) == true );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cFile = JS_EncodeString( cx, jFile ) ) != NULL );
	}
	if ( cleanUp.good ) {
		globalObj = JS_THIS_OBJECT( cx, vpn );
		cleanUp.good = ( ( javascript = (struct javascript_t *)( JS_GetPrivate( globalObj ) ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( Javascript_IncludeScript( javascript, cFile ) ) == true );
		JS_free( cx, cFile );
	}

	success = ( cleanUp.good ) ? true : false;
	args.rval().setBoolean( success);
	return success;
}

static int TimerHandler_cb( void * cbArgs ) {
	struct jsPayload_t * payload;
	JSCompartment * oldCompartment;
	int again;

	payload = ( struct jsPayload_t * ) cbArgs;
	JS::RootedValue args( payload->cx );
	JS::RootedValue retVal( payload->cx );
	JS_BeginRequest( payload->cx );
	oldCompartment = JS_EnterCompartment( payload->cx, payload->obj );
	JS::MutableHandleValue retValMut( &retVal );
	JS::HandleObject objHandle( payload->obj );
	JS::HandleValue fnValHandle( *payload->fnVal );
	JS_CallFunctionValue( payload->cx, objHandle, fnValHandle, JS::HandleValueArray( args ), retValMut );
	if ( payload->repeat ) {
		again = 1;
	} else {
		JsPayload_Delete( payload );
		again = 0;
	}

	JS_LeaveCompartment( payload->cx, oldCompartment );
	JS_EndRequest( payload->cx );

	return again;
}

#define JAVASCRIPT_GLOBAL_SET_TIMER( repeat ) do { \
		struct javascript_t * javascript; \
		struct jsPayload_t * payload; \
		struct timing_t * timing; \
		JSObject * globalObj; \
		JS::CallArgs args; \
		int ms; \
		struct { 	unsigned char payload:1; \
					unsigned char timer:1; \
					unsigned char good:1;} cleanUp; \
		  /*  @TODO:  DRY JsnGlobalSetTimeout && JsnGlobalSetInterval */ \
		payload = NULL; \
		timing = NULL; \
		JS::RootedValue dummy( cx ); \
		JS::HandleValue dummyHandle( dummy ); \
		JS::RootedValue fnVal( cx ); \
		JS::MutableHandleValue fnValMut( &fnVal ); \
		args = CallArgsFromVp( argc, vpn ); \
		memset( &cleanUp, 0, sizeof( cleanUp ) ); \
		globalObj = JS_THIS_OBJECT( cx, vpn ); \
		javascript = (struct javascript_t *)( JS_GetPrivate( globalObj ) ); \
		cleanUp.good = ( JS_ConvertArguments( cx, args, "fi", &dummy, &ms ) == true ); \
		if ( cleanUp.good ) { \
			cleanUp.good = ( JS_ConvertValue( cx, dummyHandle, JSTYPE_FUNCTION, fnValMut ) == true ); \
		} \
		if ( cleanUp.good ) { \
			if (args.length() > 2 ) { \
				JS::HandleValueArray argsAt2 = JS::HandleValueArray::fromMarkedLocation( argc - 2, vpn ); \
				cleanUp.good = ( ( payload = JsPayload_New( cx, globalObj, &fnVal, &argsAt2, false ) ) != NULL ); \
			} else { \
				JS::HandleValueArray argsAt2 = JS::HandleValueArray::empty(); \
				cleanUp.good = ( ( payload = JsPayload_New( cx, globalObj, &fnVal, &argsAt2, false) ) != NULL ); \
			} \
		} \
		if ( cleanUp.good ) { \
			cleanUp.payload = 1; \
			cleanUp.good = ( ( timing = Core_AddTiming( javascript->core, (unsigned int) ms, repeat, TimerHandler_cb, (void * ) payload ) ) != NULL ); \
		} \
		if ( cleanUp.good ) { \
			cleanUp.timer = 1; \
			timing->clearFunc_cb = (timerHandler_cb_t) JsPayload_Delete; \
			args.rval().setInt32( (int32_t) timing->identifier ); \
		} else { \
			if ( cleanUp.timer ) { \
				Core_DelTiming( javascript->core, timing ); \
			} \
			if ( cleanUp.payload ) { \
				JsPayload_Delete( payload ); payload = NULL; \
			} \
			args.rval().setUndefined(); \
		} \
		 \
		return ( cleanUp.good ) ? true : false; \
	} while ( 0 );

/**
* Calls a function after a certain delay.
*
* @name	setTimeout
* @function
* @public
* @since	0.0.5b
* @returns	{integer}				An Id that eventually can be used to stop the timeout, or null on error.
* @param	{function}	fn			A Javascript function that will be called after a delay.
* @param	{integer}	delay		The delay in ms.
* @param	{mixed}		[arguments]	Parameters arg1, arg2, ..
*
* @example
* var timeoutId = setInterval(function(a, b) {
*  	Hard.log('Foo : ' + a + ' Bar : ' + b);
*  }, 3000, 'foo', 'bar');
*  ClearTimeout(timeoutId);
*
* @see	setInterval
* @see	clearTimeout
* @see	clearInterval
*/

static bool JsnGlobalSetTimeout( JSContext * cx, unsigned argc, jsval * vpn ) {
	JAVASCRIPT_GLOBAL_SET_TIMER( 0 )
}

/**
* Calls a function repeatedly, with a fixed time delay between each call to that function.
*
* @name	setInterval
* @function
* @public
* @since	0.0.5b
* @returns	{integer}				An Id that eventually can be used to stop the timeout, or null on error.
* @param	{function}	fn			A Javascript function that will be called after a delay.
* @param	{integer}	delay		The delay in ms.
* @param	{mixed}		[arguments]	Parameters arg1, arg2, ..
*
* @example
* var timeoutId = setInterval(function(a, b) {
*  	Hard.log('Foo : ' + a + ' Bar : ' + b);
*  }, 3000, 'foo', 'bar');
*  clearInterval(timeoutId);
*
* @see	setTimeout
* @see	clearTimeout
* @see	clearInterval
*/

static bool JsnGlobalSetInterval( JSContext * cx, unsigned argc, jsval * vpn ) {
	JAVASCRIPT_GLOBAL_SET_TIMER( 0 )
}
/**
* Cancel a timeout created by setTimeout.
*
* @name	clearTimeout
* @function
* @public
* @since	0.0.5b
* @returns	{null}
* @param	{integer}		timerId	Reference to a timer that needs to  be stopped.
*
* @example
* var timeoutId = setInterval(function(a, b) {
*  	Hard.log('Foo : ' + a + ' Bar : ' + b);
*  }, 3000, 'foo', 'bar');
*  ClearTimeout(timeoutId);
*
* @see	setInterval
* @see	setTimeout
* @see	clearInterval
*/
/**
* Cancel a timeout created by setInterval.
*
* @name	clearInterval
* @function
* @public
* @since	0.0.5b
* @returns	{null}
* @param	{integer}		timerId	Reference to a timer that needs to  be stopped.
*
* @example
* var timeoutId = setInterval(function(a, b) {
*  	Hard.log('Foo : ' + a + ' Bar : ' + b);
*  }, 3000, 'foo', 'bar');
*  clearInterval(timeoutId);
*
* @see	setInterval
* @see	setTimeout
* @see	clearTimeout
*/
static bool JsnGlobalClearTimeout( JSContext * cx, unsigned argc, /*JS::MutableHandleValue*/ jsval * vpn ) {
	struct javascript_t * javascript;
	unsigned int identifier;
	JSObject * globalObj;
	JS::CallArgs args;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vpn );
	args.rval().setUndefined();
	globalObj = JS_THIS_OBJECT( cx, vpn );
	cleanUp.good = ( JS_ConvertArguments( cx, args, "i", &identifier ) == true );
	if ( cleanUp.good ) {
		javascript = (struct javascript_t *) JS_GetPrivate( globalObj );
		Core_DelTimingId( javascript->core, identifier );
	}

	return (cleanUp.good ) ? true: false;
}

static const JSClass jsnGlobalClass = {
	"global",
	JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS | JSCLASS_IS_GLOBAL | JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsnGlobalClassFuncs[ ] = {
	JS_FS( "setTimeout",    JsnGlobalSetTimeout, 2, 0 ),
	JS_FS( "setInterval",   JsnGlobalSetInterval, 2, 0 ),
	JS_FS( "clearTimeout",  JsnGlobalClearTimeout, 1, 0 ),
	JS_FS( "clearInterval", JsnGlobalClearTimeout, 1, 0 ),
	JS_FS( "include",       JsnGlobalInclude, 1, 0 ),
	JS_FS_END
};

/*
===============================================================================
BOILERPLATE
===============================================================================
*/
static bool Javascript_IncludeScript( struct javascript_t * javascript, const char * cfile ) {
	char fileNameWithPath[512];
	struct { unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	JSAutoRequest ar( javascript->context );
	memset( fileNameWithPath, '\0', sizeof( fileNameWithPath ) );
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	strncpy( fileNameWithPath, javascript->path, 255 );
	strncat( fileNameWithPath, cfile, 255 );
	LOG( javascript->core, LOG_INFO, "[%14s] Loading script %s", __FILE__, fileNameWithPath );
	if (cleanUp.good ) {
		JS::HandleObject globalObjHandle( javascript->globalObj );
		JS::RootedScript script( javascript->context );
		JS::CompileOptions options( javascript->context );
		options.setIntroductionType( "js include" )
			.setUTF8(true)
			.setFileAndLine( cfile, 1)
			.setCompileAndGo( true )
			.setNoScriptRval( true );
		cleanUp.good = ( JS::Compile( javascript->context, globalObjHandle, options, fileNameWithPath, &script ) == true );
	}
	if ( cleanUp.good ) {
		LOG( javascript->core, LOG_INFO, "[%14s] Failed loading script %s", __FILE__, fileNameWithPath );
	}
	if ( ! cleanUp.good ){
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Function that is called when the first Javascript file is loaded correctly.
 *
 * @name	Hard.onInit
 * @event
 * @public
 *
 * @returns	{null}
 */
static int Javascript_Run( struct javascript_t * javascript ) {

	JSAutoCompartment ac( javascript->context, javascript->globalObj );
	JS::RootedValue rVal( javascript->context );
	JS::MutableHandleValue rValMut( &rVal );

	JS::RootedObject consoleObj( javascript->context, JS_NewObject( javascript->context, &jsnConsoleClass, JS::NullPtr(), JS::NullPtr() ));
	JS::HandleObject consoleObjHandle( consoleObj );
	JS_SetPrivate( consoleObj, (void * ) javascript );
	JS_DefineFunctions( javascript->context, consoleObjHandle, jsnConsoleMethods );

	JS::RootedObject hardObj( javascript->context, JS_NewObject( javascript->context, &jsnHardClass, JS::NullPtr(), JS::NullPtr() ));
	JS::HandleObject hardObjHandle( consoleObj );
	JS_SetPrivate( hardObj, (void * ) javascript );
	JS_DefineFunctions( javascript->context, hardObjHandle, jsnHardMethods );

	JS::RootedObject WebserverObj( javascript->context, JS_InitClass( javascript->context, hardObjHandle, JS::NullPtr(), &jsnWebserverClass, JsnWebserverClassConstructor, 3, nullptr, nullptr, nullptr, nullptr ) );
#if 0
#if HAVE_MYSQL == 1
	JS::RootedObject MysqlclientObj( javascript->context, JS_InitClass( javascript->context, hardObj, JS::NullPtr(), &jsnMysqlClientClass, JsnMysqlClientClassConstructor, 5, nullptr, nullptr, nullptr, nullptr ) );
#endif
	JS::RootedObject PgsqlclientObj( javascript->context, JS_InitClass( javascript->context, hardObj, JS::NullPtr(), &jsnPgsqlClientClass, JsnPgsqlClientClassConstructor, 5, nullptr, nullptr, nullptr, nullptr ) );
#endif
	JS_CallFunctionName( javascript->context, hardObjHandle, "onInit", JS::HandleValueArray::empty(), rValMut );
	return 0;
}

static int Javascript_Init( struct javascript_t * javascript, struct core_t * core ) {
	struct {unsigned char good:1;
			unsigned char global:1;
			unsigned char standard:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	JSAutoRequest ar = JSAutoRequest( javascript->context );
	JS::CompartmentOptions compartmentOptions;
	compartmentOptions.setVersion( JSVERSION_LATEST );
	JS::PersistentRootedObject globalObj( javascript->context, JS_NewGlobalObject( javascript->context, &jsnGlobalClass, nullptr, JS::DontFireOnNewGlobalHook, compartmentOptions ));
	JS::HandleObject globalHandle( globalObj );
	cleanUp.good = ( globalObj != NULL );
	if ( cleanUp.good ) {
		cleanUp.global = 1;
		javascript->globalObj = globalObj;
	}
	JSAutoCompartment ac( javascript->context, javascript->globalObj );
	cleanUp.good = ( ( JS_InitStandardClasses( javascript->context, globalHandle ) ) == true );
	if ( cleanUp.good ) {
		cleanUp.standard = 1;
		JS_FireOnNewGlobalObject( javascript->context, globalHandle );
		cleanUp.good = ( JS_DefineFunctions( javascript->context, globalHandle, jsnGlobalClassFuncs ) == true );
	}
	if ( cleanUp.good ) {
		JS_SetPrivate( javascript->globalObj, ( void * ) javascript );
		cleanUp.good = ( Javascript_Run( javascript ) != 0 );
	}

	return cleanUp.good;
}

static void JsnReportError( JSContext * cx, const char * message, JSErrorReport * report ) {
	struct javascript_t * javascript;
	const char * fileName;
	const char * state;
	int level;

	javascript = (struct javascript_t *) JS_GetContextPrivate( cx );
	//  http://egachine.berlios.de/embedding-sm-best-practice/ar01s02.html#id2464522
	fileName = (report->filename != NULL)? report->filename : "<no filename>";
#ifdef VERBOSE
	if ( report->linebuf ) {
		int where;

		if ( report->tokenptr ) {
			where = report->tokenptr - report->linebuf;
			if ( ( where >= 0 ) && ( where < 80 ) ) {
				printf("@%d\n", where);
				where += 6;
				while ( --where > 0 ) {
					fputc(' ', stderr );
				}
				fprintf( stderr, "^\n" );
			}
		}
	}
#endif
	level = LOG_ERR;
	state = "Error";
	if (JSREPORT_IS_WARNING( report->flags)  ) {
		state = "Warning";
		level = LOG_WARNING;
	}
	if (JSREPORT_IS_STRICT( report->flags ) ) {
		state = "Strict";
		level = LOG_ERR;
	}
	if (JSREPORT_IS_EXCEPTION( report->flags ) ) {
		state = "Exception";
		level = LOG_CRIT;
	}
	LOG( javascript->core, level, "[%s:%d] : %s/%d: %s", fileName, report->lineno, state, report->errorNumber, message );
}

struct javascript_t * Javascript_New( struct core_t * core, const char * path, const char * fileName ) {
	struct javascript_t * javascript;
#define MAX_PATH_LENGTH 512
	char fullPath[MAX_PATH_LENGTH];
	struct {	unsigned char good:1;
				unsigned char path:1;
				unsigned char jsinit:1;
				unsigned char runtime:1;
				unsigned char context:1;
				unsigned char init:1;
				unsigned char javascript:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( javascript = (struct javascript_t *) malloc( sizeof( * javascript ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
		cleanUp.good = ( ( javascript->path = strdup( path ) ) != NULL );
	}
	if 	( cleanUp.good ) {
		cleanUp.path = 1;
		if ( jsInterpretersAlive == 0 ) {
			cleanUp.good = ( JS_Init( ) == true );
			jsInterpretersAlive++;
		}
	}
	if ( cleanUp.good ) {
		cleanUp.jsinit = 1;
		cleanUp.good = ( ( javascript->runtime = JS_NewRuntime( 8L * 1024L * 1024L, 2L * 1024L * 1024L ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.runtime = 1;
		cleanUp.good = ( ( javascript->context = JS_NewContext( javascript->runtime, 8192 ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.context = 1;
		JS_SetContextPrivate( javascript->context, (void *)javascript );
		JS::RuntimeOptions options = JS::RuntimeOptionsRef( javascript->context);
		options.setBaseline( true )
					.setExtraWarnings( true )
					.setIon( true )
					.setNativeRegExp( true )
					.setStrictMode( true )
					.setVarObjFix( true )
					.setAsmJS( false )
					.setWerror( false );
#ifdef JS_GC_ZEAL
		JS_SetGCZeal( javascript->context, 2 , 1 );
		options.setWerror( true );
#endif
		JS_SetErrorReporter( javascript->runtime, JsnReportError );
		cleanUp.good = ( Javascript_Init( javascript, core ) == 1 );
	}
	if ( cleanUp.good ) {
		cleanUp.init = 1;
		FullPath( fullPath, MAX_PATH_LENGTH, path, fileName );
		cleanUp.good =  Javascript_IncludeScript( javascript, fullPath );
	}
	if ( cleanUp.good ) {
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.context ) {
			JS_DestroyContext( javascript->context );
		}
		if ( cleanUp.runtime ) {
			JS_DestroyRuntime( javascript->runtime );
		}
		if ( cleanUp.jsinit ) {
			if ( jsInterpretersAlive > 0 ) {
				JS_ShutDown( );
				jsInterpretersAlive--;
			}
		}
		if ( cleanUp.path ) {
			free( (char *) javascript->path ); javascript->path = NULL;
		}
		if ( cleanUp.javascript ) {
			free( javascript ); javascript = NULL;
		}
	}

	return javascript;
}

void Javascript_Delete( struct javascript_t * javascript ) {

	JSAutoRequest ar( javascript->context );
	JSAutoCompartment ac( javascript->context, javascript->globalObj );
	JS_DestroyContext( javascript->context );
	JS_DestroyRuntime( javascript->runtime );

	free( (char *) javascript->path ); javascript->path = NULL;
	free( javascript );

	if ( jsInterpretersAlive > 0 ) {
		JS_ShutDown( );
		jsInterpretersAlive--;
	}
}
