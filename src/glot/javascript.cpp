#include <stdlib.h>
#include <string.h>

#include "javascript.h"
#include "../feature/sqlclient.h"
#include "../feature/webserver.h"
#include "../core/utils.h"

#define MAIN_OBJ_NAME "Hard"

struct payload_t {
	JSContext *						cx;
	JS::RootedObject 				objRoot;
	JS::RootedValue *				fnValRoot;
	JS::HandleValueArray *			argsHVA;
	bool							repeat;
};
struct script_t {
	char *						fileNameWithPath;
	JSScript *					bytecode;
	PRCList						mLink;
};

static struct payload_t * 			Payload_New						( JSContext * cx, JSObject * object, JS::RootedValue * fnVal, JS::HandleValueArray * cbArgs, const bool repeat );
static int 							Payload_Timing_ResultHandler_cb	( void * cbArgs );
static void 						Payload_Delete					( struct payload_t * payload );
static struct script_t * 			Javascript_AddScript			( struct javascript_t * javascript, const char * fileName );
static int jsInterpretersAlive = 0;

inline void JAVASCRIPT_MODULE_ACTION( const struct javascript_t * javascript, const char * action ) {
	JSObject * hardObj;
	jsval hardVal;

	hardObj = NULL;
	hardVal = JSVAL_NULL;
	JSAutoRequest			ar( javascript->context );
	JSAutoCompartment		ac( javascript->context, javascript->globalObj );
	JS::RootedObject		globalObjRoot( javascript->context, javascript->globalObj );
	JS::HandleObject		globalObjHandle( globalObjRoot );
	JS::RootedValue			rValRoot( javascript->context );
	JS::MutableHandleValue	rValMut( &rValRoot );
	JS::RootedValue			hardValRoot( javascript->context, hardVal );
	JS::HandleValue			hardValHandle( hardValRoot );
	JS::MutableHandleValue	hardValMut( &hardValRoot );
	JS::RootedObject		hardObjRoot( javascript->context, hardObj );
	JS::HandleObject		hardObjHandle( hardObjRoot );
	JS::MutableHandleObject	hardObjMut( &hardObjRoot );
	JS_GetProperty( javascript->context, globalObjHandle, MAIN_OBJ_NAME, hardValMut );
	JS_ValueToObject( javascript->context, hardValHandle, hardObjMut );
	JS_CallFunctionName( javascript->context, hardObjHandle, action, JS::HandleValueArray::empty( ), rValMut );
}

/**
 * The spidermonkey module is loaded.
 *
 * @name	Hard.onLoad
 * @event
 * @public
 * @since	0.0.8a
 * @returns	{null}
 *
 * @example
 * this.Hard.onLoad = function( ) {
 * 	console.log( "loaded" );
 * };
 *
 * @see	Hard.onReady
 * @see	Hard.onUnload
 */
unsigned char JavascriptModule_Load( const struct core_t * core, struct module_t * module, void * cbArgs ) {
	struct javascript_t * javascript;
	cfg_t * glotSection, * javascriptSection;
	char * path, * name;
	struct {unsigned char good:1;
			unsigned char javascript:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	javascript = NULL;
	glotSection = cfg_getnsec( (cfg_t *) core->config, "glot", 0 );
	javascriptSection = cfg_getnsec( glotSection, "javascript", 0 );
	path = cfg_getstr( javascriptSection, (char *) "path" );
	name = cfg_getstr( javascriptSection, (char *) "main" );
	if ( module->instance == NULL ) {
		//  this should not happen!
		cleanUp.good = ( ( javascript = Javascript_New( core, path, name ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
		module->instance = ( void * ) javascript;
		JAVASCRIPT_MODULE_ACTION( javascript, "onLoad" );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.javascript ) {
			Javascript_Delete( javascript ); javascript = NULL;
		}
	}
	return ( cleanUp.good ) ? 1: 0;
}

/**
 * The javascript module is loaded and ready to run.
 *
 * @name	Hard.onReady
 * @event
 * @public
 * @since	0.0.8a
 * @returns	{null}
 *
 * @example
 * this.Hard.onReady = function( ) {
 * 	console.log( "loaded and ready" );
 * };
 *
 * @see	Hard.onLoad
 * @see	Hard.onUnload
 */
unsigned char JavascriptModule_Ready( const struct core_t * core, struct module_t * module, void * args ) {
	struct javascript_t * javascript;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	if ( module->instance != NULL ) {
		javascript = (struct javascript_t * ) module->instance;
		JAVASCRIPT_MODULE_ACTION( javascript, "onReady" );
	}
	return 1;
}

/**
 * The javascript module is stopping.
 *
 * @name	Hard.onUnload
 * @event
 * @public
 * @since	0.0.8a
 * @returns	{null}
 *
 * @example
 * this.Hard.onUnload = function( ) {
 * 	console.log( "unloading" );
 * };
 *
 * @see	Hard.onLoad
 * @see	Hard.onReady
 * @see	Hard.shutdown
 */
unsigned char JavascriptModule_Unload( const struct core_t * core, struct module_t * module, void * args ) {
	struct javascript_t * javascript;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	if ( module->instance != NULL ) {
		javascript = (struct javascript_t * ) module->instance;
		JAVASCRIPT_MODULE_ACTION( javascript, "onUnload" );
		Javascript_Delete( javascript ); javascript = NULL;
		module->instance = NULL;
	}
	return 1;
}

/**
 * Sql client connection object.
 *
 * @name Hard.Sqlclient
 * @private
 * @object
 */
#define SET_PROPERTY_ON( handle, key, value ) do {\
	JS::RootedValue valRoot( cx, value ); \
	JS::HandleValue valHandle( valRoot ); \
	cleanUp.good = ( JS_DefineProperty( cx, handle, key, valHandle, attrs, JS_PropertyStub, JS_StrictPropertyStub ) == true ); \
} while ( 0 );

#define CONNOBJ_GET_PROP_STRING( property, var ) do { \
	if ( JS_GetProperty( cx, connObjHandle, property, valueMut ) ) { \
		if ( value.isString( ) ) { \
			cleanUp.good = ( ( var = JS_EncodeString( cx, value.toString( ) ) ) != NULL ); \
		} else { \
			cleanUp.good = 0; \
		} \
	} else { \
		cleanUp.good = 0; \
	} \
} while ( 0 );

#define CONNOBJ_GET_PROP_NR( property, var ) do { \
	if ( JS_GetProperty( cx, connObjHandle, property, valueMut ) ) { \
		if ( value.isNumber( ) ) { \
			var = ( int ) value.toNumber( ); \
		} else if ( value.isString( ) ) { \
			char * dummy; \
			cleanUp.good = ( ( dummy = JS_EncodeString( cx, value.toString( ) ) ) != NULL ); \
			var = atoi( dummy );\
			JS_free( cx, dummy ); dummy = NULL; \
		} else { \
			cleanUp.good = 0; \
		} \
	} else { \
		cleanUp.good = 0; \
	} \
} while ( 0 );

#define SQL_CLASS_CONSTRUCTOR( engine_new, jsnClass ) do { \
	struct sqlclient_t * sqlclient; \
	struct javascript_t * instance; \
	JSObject * globalObj, * sqlclientObj, * connObj, * thisObj; \
	char * cHostName, * cIp, * cUserName, * cPassword, * cDbName; \
	int port, timeoutSec; \
	JS::CallArgs args; \
	struct {unsigned char good:1;} cleanUp; \
	\
	memset( &cleanUp, 0, sizeof( cleanUp ) ); \
	args = CallArgsFromVp( argc, vpn ); \
	cUserName = NULL; \
	cHostName = NULL; \
	cIp = NULL; \
	cUserName = NULL; \
	cPassword = NULL; \
	cDbName = NULL; \
	timeoutSec = 0; \
	port = 0; \
	cleanUp.good = ( JS_ConvertArguments( cx, args, "o/i", &connObj, &timeoutSec ) == true ); \
	if ( cleanUp.good ) { \
		jsval value; \
		\
		value = JSVAL_NULL; \
		JS::RootedObject connObjRoot( cx, connObj ); \
		JS::HandleObject connObjHandle( connObjRoot ); \
		JS::RootedValue valueRoot( cx, value ); \
		JS::MutableHandleValue valueMut( &valueRoot ); \
		/*  Refer to http://www.postgresql.org/docs/9.3/static/libpq-connect.html#LIBPQ-PARAMKEYWORDS for more details. */ \
		CONNOBJ_GET_PROP_STRING( "host", cHostName ); \
		CONNOBJ_GET_PROP_STRING( "ip", cIp ); \
		CONNOBJ_GET_PROP_STRING( "user", cUserName ); \
		CONNOBJ_GET_PROP_STRING( "password", cPassword ); \
		CONNOBJ_GET_PROP_STRING( "db", cDbName ); \
		CONNOBJ_GET_PROP_NR( "port", port ); \
	} \
	thisObj = JS_THIS_OBJECT( cx, vpn ); \
	if ( cleanUp.good ) { \
		cleanUp.good = ( thisObj != NULL ); \
	} \
	JS::RootedObject thisObjRoot( cx, thisObj ); \
	if ( cleanUp.good ) { \
		globalObj = JS_GetGlobalForObject( cx, &args.callee( ) ); \
		instance = (struct javascript_t * ) JS_GetPrivate( globalObj ); \
		cleanUp.good = ( ( sqlclient = engine_new( instance->core, cHostName, cIp, (uint16_t) port, cUserName, cPassword, cDbName, (unsigned char) timeoutSec ) ) != NULL ); \
	} \
	if ( cleanUp.good ) { \
		sqlclientObj = JS_NewObjectForConstructor( cx, jsnClass, args ); \
		JS_SetPrivate( sqlclientObj, ( void * ) sqlclient ); \
		args.rval( ).setObject( *sqlclientObj ); \
	} else { \
		args.rval( ).setNull( ); \
	} \
	\
	memset( cPassword, '\0', strlen( cPassword ) ); /*  @TODO:  clean jPassword */ \
	JS_free( cx, cHostName ); cHostName = NULL; \
	JS_free( cx, cIp ); cIp = NULL; \
	JS_free( cx, cUserName ); cUserName = NULL; \
	JS_free( cx, cPassword ); cPassword = NULL; \
	return ( cleanUp.good ) ? true : false; \
} while ( 0 );


#define SQL_CLIENT_QUERY_RESULT_HANDLER_CB( formatter, sub ) do { \
	struct payload_t * payload; \
	JSContext * cx; \
	JSCompartment * oldCompartment; \
	JSObject * resultObj, * globalObj; \
	jsval paramValArray[1], retVal; \
	\
	payload = ( struct payload_t * ) query->cbArgs; \
	globalObj = NULL; \
	retVal = JSVAL_NULL; \
	paramValArray[0] = JSVAL_NULL; \
	cx = payload->cx; \
	JS_BeginRequest( cx ); \
	oldCompartment = JS_EnterCompartment( cx, globalObj ); \
	globalObj = JS_GetGlobalForObject( cx, payload->objRoot ); \
	JS::RootedValue paramValArrayRoot( cx, paramValArray[0] ); \
	resultObj = formatter( cx, query->result.sub.res ); \
	paramValArray[0] = OBJECT_TO_JSVAL( resultObj ); \
	JS::HandleValueArray 	paramValArrayHandle( paramValArrayRoot ); \
	JS::HandleValue 		fnValHandle( *payload->fnValRoot ); \
	JS::HandleObject 		objHandle( payload->objRoot ); \
	JS::RootedValue 		retValRoot( cx, retVal ); \
	JS::MutableHandleValue 	retValMut( &retValRoot ); \
	JS_CallFunctionValue( cx, objHandle, fnValHandle, paramValArrayHandle, retValMut ); \
	\
	delete payload; 		payload = NULL; \
	JS_LeaveCompartment( cx, oldCompartment ); \
	JS_EndRequest( cx ); \
} while ( 0 );

#define SQL_CLIENT_QUERY( handler ) do { \
	struct payload_t * payload; \
	struct sqlclient_t * sqlclient; \
	jsval paramList, value, fnVal; \
	JSObject * sqlObj; \
	JSString * jStatement; \
	JS::CallArgs args; \
	unsigned int nParams, i; \
	\
	const char ** cParamValues; \
	char * cStatement; \
	struct {unsigned char good:1; \
		unsigned char params:1; \
		unsigned char statement:1; \
		unsigned char payload:1; } cleanUp; \
	\
	memset( &cleanUp, 0, sizeof( cleanUp ) ); \
	fnVal = JSVAL_NULL; \
	paramList = JSVAL_NULL; \
	value = JSVAL_NULL; \
	JS::RootedValue 		fnValRoot( cx, fnVal ); \
	JS::HandleValue 		fnValHandle( fnValRoot ); \
	JS::MutableHandleValue 	fnValMut( &fnValRoot ); \
	JS::RootedValue 		paramListRoot( cx, paramList ); \
	JS::HandleValue 		paramListHandle( paramListRoot ); \
	JS::HandleValueArray 	paramListHandleArray( paramListRoot); \
	i = 0; \
	sqlclient = NULL; \
	nParams = 0; \
	cParamValues = NULL; \
	cStatement = NULL; \
	payload = NULL; \
	args = CallArgsFromVp( argc, vpn ); \
	sqlObj = JS_THIS_OBJECT( cx, vpn ); \
	cleanUp.good = ( ( sqlclient = (struct sqlclient_t *) JS_GetPrivate( sqlObj ) ) != NULL ); \
	if ( cleanUp.good ) { \
		cleanUp.good = ( JS_ConvertArguments( cx, args, "S*f", &jStatement, &paramListRoot, &fnValRoot ) == true ); \
	} \
	if ( cleanUp.good ) { \
		cleanUp.good = ( JS_ConvertValue( cx, fnValHandle, JSTYPE_FUNCTION, fnValMut ) == true ); \
	} \
	if ( cleanUp.good ) { \
		if ( paramList.isNullOrUndefined( ) ) { \
			/*  it is a query like "SELECT user FROM users WHERE user_id = 666", 				null, 		function( result ) {console.log( result );} ); */\
			nParams = 0; \
		} else if ( paramList.isString( ) || paramList.isNumber( ) ) { \
			/*  it is a query like "SELECT user FROM users WHERE user_id = $1",				 	666, 		function( result ) {console.log( result );} ); */ \
			nParams = 1; \
			cleanUp.good = ( ( cParamValues = ( const char ** ) new char*[nParams] ) != NULL ); \
			if ( cleanUp.good ) { \
				cleanUp.params = 1; \
				if ( paramList.isNumber( ) ) { \
					paramList.setString( paramList.toString( ) ); \
				} \
				cleanUp.good = ( ( cParamValues[i] = JS_EncodeString( cx, value.toString( ) ) ) != NULL ); \
			} \
		} else { \
			/*  it is a query like "SELECT user FROM users WHERE user_id BETWEEN $1 AND $2",	[664, 668],	function( result ) {console.log( result );} ); */ \
			JSObject * paramObj, * paramIter; \
			jsid indexId; \
			bool success; \
			\
			indexId = JSID_VOID; \
			paramObj = &paramList.toObject( ); \
			JS::RootedObject 		paramObjRoot( cx, paramObj ); \
			JS::HandleObject 		paramObjHandle( paramObjRoot ); \
			JS_GetArrayLength( cx, paramObjHandle, &nParams ); \
			if ( nParams > 0 ) { \
				cleanUp.good = ( ( cParamValues = ( const char ** ) new char*[nParams] ) != NULL ); \
				if ( cleanUp.good ) { \
					cleanUp.params = 1; \
					paramIter = JS_NewPropertyIterator( cx, paramObjHandle ); \
					if ( paramIter != NULL ) { \
						do { \
							JS::RootedId			indexIdRoot( cx, indexId ); \
							JS::HandleId			indexIdHandle( indexIdRoot ); \
							JS::MutableHandleId		indexIdMut( &indexIdRoot ); \
							JS::RootedValue 		valueRoot( cx, value ); \
							JS::MutableHandleValue	valueMut( &valueRoot ); \
							success = JS_NextProperty( cx, paramObjHandle, indexIdMut ); \
							if ( JS_GetPropertyById( cx, paramObjHandle, indexIdHandle, valueMut ) ) { \
								cleanUp.good = ( ( cParamValues[i] = JS_EncodeString( cx, value.toString( ) ) ) != NULL ); \
								i++; \
							} \
						} while ( success == true && indexId != JSID_VOID && cleanUp.good ); \
					} \
				} \
			} \
		} \
	} \
	if ( cleanUp.good ) { \
		JS::HandleValueArray dummy = JS::HandleValueArray::empty( ); \
		cleanUp.good = ( ( payload = Payload_New( cx, sqlObj, &fnValRoot, &dummy, false ) ) != NULL ); \
	} \
	if ( cleanUp.good ) { \
		cleanUp.payload = 1; \
		cleanUp.good = ( ( cStatement = JS_EncodeString( cx, jStatement ) ) != NULL ); \
	} \
	if ( cleanUp.good ) { \
		cleanUp.statement = 1; \
		Query_New( sqlclient, cStatement, nParams, cParamValues, handler, ( void * ) payload ); \
	} \
	/*  always cleanup  */ \
	for ( i = 0; i < nParams; i++ ) { \
		JS_free( cx, ( char * ) cParamValues[i] ); cParamValues[i] = NULL; \
	} \
	if ( cleanUp.params ) { \
		delete[ ] cParamValues; 	cParamValues = NULL; \
	} \
	if ( cleanUp.statement ) { \
		JS_free( cx, cStatement ); 	cStatement = NULL; \
	} \
	/*  get ready to return */ \
	if ( cleanUp.good ) { \
		args.rval( ).setBoolean( true ); \
	} else { \
		if ( cleanUp.payload ) { \
			delete payload; payload = NULL; \
		} \
		args.rval( ).setBoolean( false ); \
	} \
	return ( cleanUp.good ) ? true : false; \
} while ( 0 );

#if HAVE_MYSQL == 1
/*
   ===============================================================================
   Mysql OBJECT
   ===============================================================================
*/

static void JsnMysqlclient_Finalizer( JSFreeOp * fop, JSObject * myqlObj );

static JSObject * Mysqlclient_Query_ResultToJS( JSContext * cx, const void * rawRes );
static JSObject * Mysqlclient_Query_ResultToJS( JSContext * cx, const void * rawRes ) {
	JSObject * recordObj, * resultArray;
	JSString * jstr;
	MYSAC_ROW *row;
	MYSAC_RES * result;
	jsval jValue, currentVal;
	unsigned int rowId, rowCount, colId, colCount;
	char * cFieldName, * cValue;
	struct { unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	result = (MYSAC_RES *) rawRes;
	cleanUp.good = ( ( resultArray = JS_NewArrayObject( cx, 0 ) ) != NULL );
	if ( cleanUp.good ) {
		JS::RootedObject resultArrayRoot( cx, resultArray );
		JS::HandleObject resultArrayHandle( resultArrayRoot );
		if ( result != NULL ) {
			rowCount = ( unsigned int ) mysac_num_rows( result );
			if ( rowCount > 0 ) {
				colCount = mysac_field_count( result );
				rowId = 0;
				while ( cleanUp.good && ( row = mysac_fetch_row( result ) ) != NULL ) {
					jValue = JSVAL_NULL;
					currentVal = JSVAL_NULL;
					JS::RootedObject 		recordObjRoot( cx, recordObj );
					JS::HandleObject 		recordObjHandle( recordObjRoot );
					cleanUp.good = ( (		recordObj = JS_NewObject( cx, NULL, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
					if ( cleanUp.good ) {
						currentVal = OBJECT_TO_JSVAL( recordObj );
						JS::RootedValue 	currentValRoot( cx, currentVal );
						JS::HandleValue 	currentValHandle( currentValRoot );
						JS_SetElement( cx, resultArrayHandle, rowId, currentValHandle );
						for ( colId = 0; colId < colCount; colId++ ) {
							cFieldName = ( ( MYSAC_RES * ) result )->cols[colId].name;
							cValue = row[colId].blob;
							if ( cValue == NULL ) {
								jValue = JSVAL_NULL;
							} else {
								cleanUp.good = ( ( jstr = JS_NewStringCopyZ( cx, cValue ) ) != NULL );
								if ( cleanUp.good ) {
									jValue = STRING_TO_JSVAL( jstr );
								} else {
									jValue = JSVAL_VOID;  //  not quite true
								}
								if ( ! cleanUp.good ) {
									break;
								}
							}
							JS::RootedValue	jValueRoot( cx, jValue );
							JS::HandleValue	jValueHandle( jValueRoot );
							JS_SetProperty( cx, recordObjHandle, cFieldName, jValueHandle );
						}
					}
					rowId++;
				}
			}
			//  lastOid = mysac_insert_id( MYSAC * mysac );
			//  affected += int mysac_affected_rows( MYSAC * mysac );
		}
	}

	return resultArray;
}

static void Mysqlclient_Query_ResultHandler_cb( const struct query_t * query ) {
	SQL_CLIENT_QUERY_RESULT_HANDLER_CB( Mysqlclient_Query_ResultToJS, my )
}
/**
 * Submit a command or a query over the mysql connection.
 *
 * The results of the command handled by a javascript function.
 *
 * @name	Hard.MysqlClient.query
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}					If the query could be registered successfully it returns true; else false is returned
 * @param	{string}	statement		A sql command or query.
 * @param	{array}		[parameters]	A list of parameters that can be used in a query.<p>default: []</p>
 * @param	{function}	fn				The callback function {response}
 *
 * @example
 * var pg = this.Hard.PostgresqlClient( {host : '10.0.0.25', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432}, 60 );
 * pg.query( 'SELECT name, sales FROM sales WHERE customer = '$1' );' , ['foobar' ], function( res ) {
 * 	if ( typeof query == array ) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++ ) {
 * 				row = res[rowId];
 * 				console.log( rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	} );
 *
 * @see	Hard.MysqlClient
 * @see	Hard.PostgresqlClient.query
 */
static bool JsnMysqlclient_Query( JSContext * cx, unsigned argc, jsval * vpn ) {
	SQL_CLIENT_QUERY( Mysqlclient_Query_ResultHandler_cb );
	return false;
}

JSClass jscMysqlclient = {
	"MysqlClient",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnMysqlclient_Finalizer, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static JSFunctionSpec jsmMysqlclient[ ] = {
	JS_FS( "query", JsnMysqlclient_Query, 3, 0 ),
	JS_FS_END
};

/**
 * Connect to a mysql server.
 *
 * @name	Hard.MysqlClient
 * @constructor
 * @public
 * @since	0.0.5b
 * @returns	{object}							The mysql client javascript instance
 * @param	{object}		params				The connection string.
 * @param	{string}		params.host			The host name for auth
 * @param	{string}		params.ip			The host ip
 * @param	{int}			params.port			The port number
 * @param	{string}		params.db			The database name
 * @param	{string}		params.user			The database user
 * @param	{string}		params.password		The database user password
 * @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' in the postgresql section of the configurationFile.</p>
 *
 * @example
 * var my = this.Hard.MysqlClient( {host : '10.0.0.25', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432 }, 60 );
 * my.query( 'SELECT name, sales FROM sales WHERE customer = '$1' );' , ['foobar' ], function( res ) {
 * 	if ( Array.isArray( res ) ) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++ ) {
 * 				row = res[rowId];
 * 				console.log( rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	} );
 * @see	Hard.MysqlClient.query
 * @see	Hard.PostgreslClient
 */
static bool JsnMysqlclient_Constructor( JSContext * cx, unsigned argc, jsval * vpn ) {
	SQL_CLASS_CONSTRUCTOR( Postgresql_New, &jscMysqlclient );
	return false;
}

static void JsnMysqlclient_Finalizer( JSFreeOp * fop, JSObject * mysqlObj ) {
	struct sqlclient_t * mysql;

	if ( ( mysql = (struct sqlclient_t *) JS_GetPrivate( mysqlObj ) ) != NULL ) {
		delete mysql; mysql = NULL;
	}
}
#endif
/*
   ===============================================================================
   Postgresql OBJECT
   ===============================================================================
*/

static void JsnPostgresqlclient_Finalizer( JSFreeOp * fop, JSObject * postgresqlObj );

static JSObject * Postgresqlclient_Query_ResultToJS( JSContext * cx, const void * rawRes );
static JSObject * Postgresqlclient_Query_ResultToJS( JSContext * cx, const void * rawRes ) {
	JSObject * resultArray;
	ExecStatusType status;
	PGresult * result;
	JSString * jStr;
	Oid dataType;
	unsigned int rowId, rowCount;
	int colId, colCount;
	char * cFieldName, * cValue;
	struct {unsigned char good:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	result = (PGresult *) rawRes;
	cleanUp.good = ( ( resultArray = JS_NewArrayObject( cx, 0 ) ) != NULL );
	if ( cleanUp.good ) {
		JS::RootedObject 	resultArrayRoot( cx, resultArray );
		JS::HandleObject 	resultArrayHandle( resultArrayRoot );
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
				case PGRES_EMPTY_QUERY:		  //  FT
				case PGRES_COMMAND_OK:
				default:
					break;
				case PGRES_SINGLE_TUPLE:	  //  FT
				case PGRES_TUPLES_OK:
					rowCount = ( unsigned int ) PQntuples( result );
					colCount = PQnfields( result );
					for ( rowId = 0; rowId < rowCount; rowId++ ) {
						JSObject * recordObj;
						jsval currentVal, jValue;

						recordObj = NULL;
						jValue = JSVAL_NULL;
						currentVal = JSVAL_NULL;
						JS::RootedObject 		recordObjRoot( cx, recordObj );
						JS::HandleObject 		recordObjHandle( recordObjRoot );
						cleanUp.good = ( ( recordObj = JS_NewObject( cx, NULL, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
						if ( ! cleanUp.good ) {
							currentVal = OBJECT_TO_JSVAL( recordObj );
							JS::RootedValue 	currentValRoot( cx, currentVal );
							JS::HandleValue 	currentValHandle( currentValRoot );
							JS_SetElement( cx, resultArrayHandle, (uint32_t) rowId, currentValHandle );
							for ( colId = 0; colId < colCount; colId++ ) {
								cFieldName = PQfname( result, colId );  //  speedup might be possible by caching this
								dataType = PQftype( result, colId );
								if ( PQgetisnull( result, (int) rowId, colId ) == 1 ) {
									jValue = JSVAL_NULL;
								} else {
									cValue = PQgetvalue( result, (int) rowId, colId );
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
												   cleanUp.good = ( ( jStr = JS_NewStringCopyZ( cx, cValue ) ) != NULL );
												   if ( cleanUp.good ) {
													   jValue = STRING_TO_JSVAL( jStr );
												   } else {
													   jValue = JSVAL_VOID;  //  not quite true
												   }
												   break;
									}
								}
								JS::RootedValue jValueRoot( cx, jValue );
								JS::HandleValue jValueHandle( jValueRoot );
								JS_SetProperty( cx, recordObjHandle, cFieldName, jValueHandle );  //  hmm, what if postgresql columns do not have ascii chars, ecma does not allow that...?
							}
						}
					}
					break;
			}
			//  lastOid = ( unsigned long ) PQoidValue( res );
			//  affected += atoi( PQcmdTuples( res ) );
		}
	}

	return resultArray;
}

static void Postgresqlclient_Query_ResultHandler_cb( const struct query_t * query ) {
	SQL_CLIENT_QUERY_RESULT_HANDLER_CB( Postgresqlclient_Query_ResultToJS, pg )
}

/**
 * Submit a command or a query over the postgresql connection.
 *
 * The results of the command handled by a javascript function.
 *
 * @name	Hard.PostgresqlClient.query
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}					If the query could be registered successfully it returns true; else false is returned
 * @param	{string}	statement		A sql command or query.
 * @param	{array}		[parameters]	A list of parameters that can be used in a query.<p>default: []</p>
 * @param	{function}	fn				The callback function {response}
 *
 * @example
 * var pg = this.Hard.PostgresqlClient( {host : '10.0.0.25', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432 }, 60 );
 * pg.query( 'SELECT name, sales FROM sales WHERE customer = '$1' );' , ['foobar' ], function( res ) {
 * 	if ( Array.isArray( res ) ) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++ ) {
 * 				row = res[rowId];
 * 				console.log( rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	} );
 *
 * @see	Hard.PostgresqlClient
 * @see	Hard.MysqlClient.query
 */
static bool JsnPostgresqlclient_Query( JSContext * cx, unsigned argc, jsval * vpn ) {
	SQL_CLIENT_QUERY( Postgresqlclient_Query_ResultHandler_cb );
	return false;
}

JSClass jscPostgresqlclient = {
	"PostgresqlClient",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnPostgresqlclient_Finalizer, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static JSFunctionSpec jsmPostgresqlclient[ ] = {
	JS_FS( "query", JsnPostgresqlclient_Query, 3, 0 ),
	JS_FS_END
};

/**
 * Connect to a postgresql server.
 *
 * @name	Hard.PostgresqlClient
 * @constructor
 * @public
 * @since	0.0.5b
 * @returns	{object}							The postgresql client javascript instance or null on failure
 * @param	{object}		params				The connection string.
 * @param	{string}		params.host			The host name for pg auth
 * @param	{string}		params.ip			The host ip
 * @param	{int}			params.port			The port number
 * @param	{string}		params.db			The database name
 * @param	{string}		params.user			The database user
 * @param	{string}		params.password		The database user password
 * @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' in the postgresql section of the configurationFile.</p>
 *
 * @example
 * var pg = this.Hard.PostgresqlClient( {host : '10.0.0.25', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432 }, 60 );
 * pg.query( 'SELECT name, sales FROM sales WHERE customer = '$1' );' , ['foobar' ], function( res ) {
 * 	if ( Array.isArray( res ) ) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++ ) {
 * 				row = res[rowId];
 * 				console.log( rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	} );
 * @see	Hard.PostgresqlClient.query
 * @see	Hard.MysqlClient
 */


static bool JsnPostgresqlclient_Constructor( JSContext * cx, unsigned argc, jsval * vpn ) {
	SQL_CLASS_CONSTRUCTOR( Postgresql_New, &jscPostgresqlclient );
	return false;
}

static void JsnPostgresqlclient_Finalizer( JSFreeOp * fop, JSObject * postgresqlObj ) {
	struct sqlclient_t * postgresql;

	if ( ( postgresql = (struct sqlclient_t *) JS_GetPrivate( postgresqlObj ) ) != NULL ) {
		delete postgresql; postgresql = NULL;
	}
}
/*
   ===============================================================================
   Webserver OBJECT
   ===============================================================================
*/
extern const char * MethodDefinitions[ ];
/**
 * Webserver response object.
 *
 * @name Hard.Webserver.req
 * @private
 * @object
 */
static void JsnWebserver_Finalizer( JSFreeOp * fop, JSObject * webserverObj );

static JSObject * Webserver_Route_ResultToJS( struct payload_t * payload, JSObject * globalObj, const struct webclient_t * webclient );
static JSObject * Webserver_Route_ResultToJS( struct payload_t * payload, JSObject * globalObj, const struct webclient_t * webclient ) {
	JSContext * cx;
	JSObject * clientObj, * responseObj;
	JSString * jIp, * jUrl, * jMethod;
	const char * ip, * url;
	const unsigned int attrs = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
	struct {unsigned char ip:1;
		unsigned char jip:1;
		unsigned char url:1;
		unsigned char jurl:1;
		unsigned char method:1;
		unsigned char cli:1;
		unsigned char resp:1;
		unsigned char good:1;} cleanUp;
	memset( &cleanUp, 0, sizeof( cleanUp ) );

	clientObj = NULL;
	jIp = jUrl = jMethod = NULL;
	ip = url = NULL;
	cx = payload->cx;
	JSAutoRequest ar( cx );
	JSAutoCompartment ac( cx, globalObj );
	cleanUp.good = ( ( ip = Webclient_GetIp( webclient ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.ip = 1;
		cleanUp.good = ( ( jIp = JS_NewStringCopyZ( cx, ip ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.jip = 1;
		cleanUp.good = ( ( url = Webclient_GetUrl( webclient ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		cleanUp.good = ( ( jUrl = JS_NewStringCopyZ( cx, url ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.jurl = 1;
		cleanUp.good = ( ( jMethod = JS_NewStringCopyZ( cx, MethodDefinitions[webclient->mode] ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.method = 1;
		cleanUp.good = ( ( clientObj = JS_NewObject( cx, NULL, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
	}
	JS::RootedObject clientObjRoot( cx, clientObj );
	JS::HandleObject clientObjHandle( clientObjRoot );
	if ( cleanUp.good ) {
		cleanUp.cli = 1;
		cleanUp.good = ( ( responseObj = JS_NewObject( cx, NULL, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
		//  cleanUp.good = ( ( responseObj = JS_NewObject( cx, &JsnWebserverResponseClass, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
	}
	//  readonly
	if ( cleanUp.good ) {
		cleanUp.resp = 1;
		SET_PROPERTY_ON( clientObjHandle, "ip", STRING_TO_JSVAL( jIp ) );
	}
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( clientObjHandle, "url", STRING_TO_JSVAL( jUrl ) );
	}
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( clientObjHandle, "response", OBJECT_TO_JSVAL( responseObj ) );
	}
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( clientObjHandle, "method", STRING_TO_JSVAL( jMethod ) );
	}
	if ( cleanUp.good ) {
		JS_SetPrivate( responseObj, ( void * ) &webclient->response );
		//  @TODO:  editable fieldsJS_DefineProperties( cx, responseObj, JsnHttpScResponseProp );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.ip ) {
			free( (char *) ip ); ip = NULL;
		}
		if ( cleanUp.jip ) {
			JS_free( cx, jIp ); jIp = NULL;
		}
		if ( cleanUp.url ) {
			free( (char *) url ); url = NULL;
		}
		if ( cleanUp.jurl ) {
			JS_free( cx, jUrl ); jUrl = NULL;
		}
		if ( cleanUp.method ) {
			JS_free( cx, jMethod ); jMethod = NULL;
		}
		if ( cleanUp.resp ) {
			// resoponseObj
		}
		if ( cleanUp.cli ) {
			// clientObj
		}
	}
	return clientObj;
}

static void Webserver_Route_ResultHandler_cb( const struct webclient_t * webclient ) {
	JSObject * clientObj, * globalObj;
	jsval clientObjVal, retVal;
	struct payload_t * payload;
	JSCompartment * oldCompartment;

	retVal = JSVAL_NULL;
	clientObjVal = JSVAL_NULL;
	clientObj = NULL;
	payload = (struct payload_t *) webclient->route->cbArgs;
	if ( payload != NULL ) {
		JS_BeginRequest( payload->cx );
		oldCompartment = JS_EnterCompartment( payload->cx, payload->objRoot );
		globalObj = JS_GetGlobalForCompartmentOrNull( payload->cx, oldCompartment );
		JS::RootedObject 		clientObjRoot( payload->cx, clientObj );
		clientObj = Webserver_Route_ResultToJS( payload, globalObj, webclient );
		JS::RootedValue 		clientValRoot( payload->cx, clientObjVal );
		JS::RootedValue 		retValRoot( payload->cx, retVal );
		JS::MutableHandleValue 	retValMut( &retValRoot );
		JS::HandleObject 		serverObjHandle( payload->objRoot );
		JS::HandleValue 		fnValHandle( *payload->fnValRoot );
		clientObjVal = OBJECT_TO_JSVAL( clientObj );
		JS_CallFunctionValue( payload->cx, serverObjHandle, fnValHandle, JS::HandleValueArray( clientValRoot ), retValMut );
		Payload_Delete( payload ); payload = NULL;
		JS_LeaveCompartment( payload->cx, oldCompartment );
		JS_EndRequest( payload->cx );
	}
}

/**
 * Add a dynamic route to the webserver.
 *
 * A javascript function will handle the request.
 *
 * @name	Hard.Webserver.addRoute
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}				If the route could be registered successfully it returns true; else false is returned
 * @param	{string}		pattern	The regular expression that triggers a callback if there is a match.
 * @param	{function}		fn		The callback function {response}
 *
 * @example
 * var ws = this.Hard.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	console.log( 'got ' + client.url );
 * 	client.response.content = '<html><h1>response</h1><html>';
 * 	} );
 * @see	Hard.Webserver
 * @see	Hard.Webserver.addDocumentRoot
 * @see	Hard.Webclient.get
 */
static bool JsnWebserver_AddDynamicRoute( JSContext * cx, unsigned argc, jsval * vpn ) {
	struct webserver_t * webserver;
	struct payload_t * payload;
	JSObject * webserverObj;
	JSString * jPattern;
	JS::CallArgs args;
	jsval fnVal;
	char * cPattern;
	struct {unsigned char payload:1;
		unsigned char pattern:1;
		unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vpn );
	cPattern = NULL;
	payload = NULL;
	fnVal = JSVAL_NULL;
	webserverObj = JS_THIS_OBJECT( cx, vpn );
	webserver = (struct webserver_t *) JS_GetPrivate( webserverObj );
	JS::RootedValue 		fnValRoot( cx, fnVal );
	JS::HandleValue 		fnValHandle( fnValRoot );
	JS::MutableHandleValue 	fnValMut( &fnValRoot );
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S*", &jPattern, &fnVal ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cPattern = JS_EncodeString( cx, jPattern ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.pattern = 1;
		cleanUp.good = ( ( webserver = (struct webserver_t *) JS_GetPrivate( webserverObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertValue( cx, fnValHandle, JSTYPE_FUNCTION, fnValMut ) == true );
	}
	if ( cleanUp.good ) {
		JS::HandleValueArray empty = JS::HandleValueArray::empty( );
		cleanUp.good = ( ( payload = Payload_New( cx, webserverObj, &fnValRoot, &empty ,false ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		Webserver_DynamicHandler( webserver, cPattern, Webserver_Route_ResultHandler_cb, ( void * ) payload );
	} else {
		if ( cleanUp.payload ) {
			// this is the only thing that needs special care if something went wrong; we cleanup the rest any way,
			delete payload; payload = NULL;
		}
	}
	if ( cleanUp.good ) {
		args.rval( ).setBoolean( true );
	} else {
		args.rval( ).setBoolean( false );
	}
	if ( cleanUp.pattern ) {
		JS_free( cx, cPattern ); cPattern = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Add a route to the webserver to serve static pages.
 *
 * @name	Hard.Webserver.addDocumentRoot
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}						If the route could be registered successfully it returns true; else false is returned
 * @param	{string}		pattern			The regular expression that triggers a lookup in the filesystem. Please not that this is a string and not a RegExp object. The expression must have exactly 1 group. This will act as the placeholder for the requested file.
 * @param	{string}		documentRoot	The folder name that acts as the document root for this webserver.<p>default: The value for 'document_root' in the http section of the configurationFile</p>
 *
 * @example
 * var ws = this.Hard.Webserver( {ip: '10.0.0.25', port: 8888}, 60 );
 * ws.addDocumentRoot( '^/static/(.*)', '/var/www/static/' );
 *
 * @see	Hard.Webserver
 * @see	Hard.Webserver.addRoute
 * @see	Hard.WebClient.get
 */
static bool JsnWebserver_AddDocumentRoot( JSContext * cx, unsigned argc, jsval * vpn ) {
	struct webserver_t * webserver;
	JSObject * webServerObj;
	JS::CallArgs args;
	JSString * jDocumentRoot, * jLocation;
	char * cDocumentRoot, * cLocation;
	struct {	unsigned char location:1;
		unsigned char documentRoot:1;
		unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webServerObj = JS_THIS_OBJECT( cx, vpn );
	args = CallArgsFromVp( argc, vpn );
	jLocation = NULL;
	jDocumentRoot = NULL;
	cDocumentRoot = NULL;
	cLocation = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "SS", &jDocumentRoot, &jLocation ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cDocumentRoot = JS_EncodeString( cx, jDocumentRoot ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cLocation = JS_EncodeString( cx, jLocation ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( webserver = (struct webserver_t *) JS_GetPrivate( webServerObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		Webserver_DocumentRoot( webserver, cDocumentRoot, cLocation );
	}
	if ( cleanUp.good ) {
		args.rval( ).setBoolean( true );
	} else {
		args.rval( ).setBoolean( false );
	}
	if ( cleanUp.documentRoot ) {
		JS_free( cx, cDocumentRoot ); cDocumentRoot = NULL;
	}
	if ( cleanUp.location ) {
		JS_free( cx, cLocation ); cLocation = NULL;
	}
	return ( cleanUp.good ) ? true: false;
}

static const JSClass jscWebserver = {
	"Webserver",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnWebserver_Finalizer, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmWebserver[ ] = {
	JS_FS( "addRoute", 			JsnWebserver_AddDynamicRoute, 2, 0 ),
	JS_FS( "addDocumentRoot", 	JsnWebserver_AddDocumentRoot, 2, 0 ),
	JS_FS_END
};

/**
 * Start a webserver.
 *
 * @name	Hard.Webserver
 * @constructor
 * @public
 * @since	0.0.5b
 * @returns	{object}							The web server javascript javascript
 * @param	{object}		params				The connection string.
 * @param	{string}		params.ip			The Ip address that the server will listen to.<p>default: The value for 'ip' in the webserver  section of the configurationFile.</p>
 * @param	{int}			params.port			The port that the server should bind to.<p>default: The value for 'port' in the webserver section of the configurationFile.</p>
 * @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' in the webserver section of the configurationFile.</p>
 *
 * @example
 * var ws = this.Hard.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	console.log( 'got ' + client.url );
 * 	client.response.content = '<html><h1>response</h1><html>';
 * 	} );
 * ws.addDocumentRoot( '^/static/(.*)', '/var/www/static/' );
 *
 * @see	Hard.Webserver.addRoute
 * @see	HarHardtpServer.addDocumentRoot
 */
static bool JsnWebserver_Constructor( JSContext * cx, unsigned argc, jsval * vpn ) {
	struct webserver_t * webserver;
	struct javascript_t * javascript;
	JSObject * connObj;
	JS::CallArgs args;
	const char * cDocumentRoot, * cPath;
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
	cleanUp.good = ( JS_ConvertArguments( cx, args, "o/i", &connObj, &port, &timeoutSec ) == true );
	if ( cleanUp.good ) {
		jsval value;

		value = JSVAL_NULL;
		JS::RootedObject connObjRoot( cx, connObj );
		JS::HandleObject connObjHandle( connObjRoot );
		JS::RootedValue valueRoot( cx, value );
		JS::MutableHandleValue valueMut( &valueRoot );
		CONNOBJ_GET_PROP_STRING( "ip", cServerIp );
		CONNOBJ_GET_PROP_NR( "port", port );
	}
	JS::RootedObject thisObj( cx, JS_THIS_OBJECT( cx, vpn ) );
	if ( cleanUp.good ) {
		cleanUp.good = ( thisObj != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( javascript = (struct javascript_t *) JS_GetPrivate( thisObj ) ) != NULL );
	}
	if ( cleanUp.good ) {

		cleanUp.good = ( ( webserver = Webserver_New( javascript->core, cServerIp, (uint16_t) port, (unsigned char) timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Webserver_DocumentRoot( webserver, cPath, cDocumentRoot ) == 1 );
	}
	if ( cleanUp.good ) {
		JSObject * webserverObj = 	JS_NewObject( cx, &jscWebserver, JS::NullPtr( ), JS::NullPtr( ) );
		JS_SetPrivate( webserverObj, ( void * ) webserver );
		Webserver_JoinCore( webserver );
		args.rval( ).setObject( *webserverObj );
	} else {
		args.rval( ).setUndefined( );
	}

	JS_free( cx, cServerIp ); cServerIp = NULL;
	return ( cleanUp.good ) ? true: false;
}

static void JsnWebserver_Finalizer( JSFreeOp * fop, JSObject * webserverObj ) {
	struct webserver_t * webserver;

	if ( ( webserver = ( struct webserver_t * ) JS_GetPrivate( webserverObj ) ) != NULL ) {
		Webserver_Delete( webserver ); webserver = NULL;
	}
}
/**
 * Hard javascript object.
 *
 * @name Hard
 * @public
 * @namespace
 */
static bool JsnFunction_Stub( JSContext * cx, unsigned argc, jsval * vpn );
static bool JsnFunction_Stub( JSContext * cx, unsigned argc, jsval * vpn ) {
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
static bool JsnHard_Shutdown( JSContext * cx, unsigned argc, jsval * vpn ) {
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

static const JSClass jscHard = {
	MAIN_OBJ_NAME,
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmHard[ ] = {
	JS_FS( "shutdown", 			JsnHard_Shutdown, 1, 0 ),
	JS_FS( "onLoad", 			JsnFunction_Stub, 0, 0 ),
	JS_FS( "onReady", 			JsnFunction_Stub, 0, 0 ),
	JS_FS( "onUnload", 			JsnFunction_Stub, 0, 0 ),
	JS_FS_END
};

/**
 * Standard javascript object.
 * Log messages to the console and the logger
 *
 * @name console
 * @public
 * @namespace
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
static bool JsnConsole_Log( JSContext * cx, unsigned argc, jsval * vpn ) {
	struct javascript_t * javascript;
	JSObject * consoleObj;
	JSString * jString;
	JS::CallArgs args;
	const char * fileName;
	unsigned int lineNo;
	char * cString;
	struct {unsigned char good:1;
			unsigned char fileName:1;
			unsigned char cstring:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vpn );
	args.rval( ).setUndefined( );
	cString = NULL;
	fileName = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jString ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cString = JS_EncodeString( cx, jString ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.cstring = 1;
		consoleObj = JS_THIS_OBJECT( cx, vpn );
		cleanUp.good = ( ( javascript = (struct javascript_t *) JS_GetPrivate( consoleObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		JS::AutoFilename fileDesc;
		if ( JS::DescribeScriptedCaller( cx, &fileDesc, &lineNo ) == true ) {
			 cleanUp.good = ( ( fileName = Xstrdup( fileDesc.get( ) ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.fileName = 1;
			}
		} else {
			fileName = __FILE__;
			lineNo = __LINE__;
		}
	}
	if ( cleanUp.good ) {
			Core_Log( javascript->core, LOG_INFO, fileName, lineNo, cString );
	}
	//  always cleanUp
	if ( cleanUp.fileName ) {
		free( (char*) fileName ); fileName = NULL;
	}
	if ( cleanUp.cstring ) {
		JS_free( cx, cString ); cString = NULL;
	}

	return ( cleanUp.good ) ? true : false;
}

static const JSClass jscConsole = {
	"console",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmConsole[ ] = {
	JS_FS( "log",					JsnConsole_Log, 1, 0 ),
	JS_FS_END
};

/**
 * Standard javascript object.
 *
 * @name global
 * @public
 * @namespace
 */
static void Payload_Delete( struct payload_t * payload ) {
	JSAutoRequest ar( payload->cx );
	payload->fnValRoot = NULL;
	JS_free( payload->cx, payload->argsHVA ); payload->argsHVA = NULL;
	JS_free( payload->cx, payload ); payload = NULL;
}

static struct payload_t * Payload_New( JSContext * cx, JSObject * object, JS::RootedValue * fnVal, JS::HandleValueArray * cbArgs, const bool repeat ) {
	struct payload_t * payload;
	struct {unsigned char good:1;
		unsigned char payload:1;
		unsigned char args:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( payload = (struct payload_t *) JS_malloc( cx, sizeof( *payload ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		payload->cx = cx;
		payload->objRoot = object;
		payload->fnValRoot = fnVal;
		payload->argsHVA = cbArgs;
		payload->repeat = repeat;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.args ) {
			JS_free( cx, payload->argsHVA ); payload->argsHVA = NULL;
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
 * 	console.log( 'loading failed: ' + e.getMessage( ) );
 * }
 */
static bool JsnGlobal_Include( JSContext * cx, unsigned argc, jsval * vpn ) {
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
		globalObj = JS_GetGlobalForObject( cx, &args.callee( ) );
		cleanUp.good = ( ( javascript = (struct javascript_t *) JS_GetPrivate( globalObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Javascript_AddScript( javascript, cFile ) != NULL );
		JS_free( cx, cFile ); cFile = NULL;
	}

	success = ( cleanUp.good ) ? true : false;
	args.rval( ).setBoolean( success );
	return success;
}

static int Payload_Timing_ResultHandler_cb( void * cbArgs ) {
	struct payload_t * payload;
	JSCompartment * oldCompartment;
	jsval retVal;
	int again;

	again = 0;
	retVal = JSVAL_NULL;
	payload = ( struct payload_t * ) cbArgs;
	if ( payload != NULL ) {
		JS_BeginRequest( payload->cx );
		oldCompartment = 		JS_EnterCompartment( payload->cx, payload->objRoot );
		JS::RootedValue 		retValRoot( payload->cx, retVal );
		JS::MutableHandleValue 	retValMut( &retValRoot );
		JS::HandleObject 		objHandle( payload->objRoot );
		JS::HandleValue 		fnValHandle( *payload->fnValRoot );
		JS_CallFunctionValue( payload->cx, objHandle, fnValHandle, *payload->argsHVA, retValMut );
		if ( payload->repeat ) {
			again = 1;
		} else {
			Payload_Delete( payload ); payload = NULL;
			again = 0;
		}

		JS_LeaveCompartment( payload->cx, oldCompartment );
		JS_EndRequest( payload->cx );
	}
	return again;
}

#define JAVASCRIPT_GLOBAL_SET_TIMER( repeat ) do { \
	struct javascript_t * javascript; \
	struct payload_t * payload; \
	struct timing_t * timing; \
	JSObject * globalObj; \
	JS::CallArgs args; \
	int ms; \
	struct {	unsigned char payload:1; \
				unsigned char timer:1; \
				unsigned char good:1;} cleanUp; \
	memset( &cleanUp, 0, sizeof( cleanUp ) ); \
	JS::RootedValue 		dummy( cx ); \
	JS::HandleValue 		dummyHandle( dummy ); \
	JS::RootedValue 		fnVal( cx ); \
	JS::MutableHandleValue	fnValMut( &fnVal ); \
	payload = NULL; \
	timing = NULL; \
	args = CallArgsFromVp( argc, vpn ); \
	globalObj = JS_GetGlobalForObject( cx, &args.callee( ) ); \
	javascript = (struct javascript_t *) JS_GetPrivate( globalObj ); \
	cleanUp.good = ( JS_ConvertArguments( cx, args, "fi", &dummy, &ms ) == true ); \
	if ( cleanUp.good ) { \
		cleanUp.good = ( JS_ConvertValue( cx, dummyHandle, JSTYPE_FUNCTION, fnValMut ) == true ); \
	} \
	if ( cleanUp.good ) { \
		if ( args.length( ) > 2 ) { \
			JS::HandleValueArray argsAt2 = JS::HandleValueArray::fromMarkedLocation( argc - 2, vpn ); \
			cleanUp.good = ( ( payload = Payload_New( cx, globalObj, &fnVal, &argsAt2, false ) ) != NULL ); \
		} else { \
			JS::HandleValueArray argsAt2 = JS::HandleValueArray::empty( ); \
			cleanUp.good = ( ( payload = Payload_New( cx, globalObj, &fnVal, &argsAt2, false ) ) != NULL ); \
		} \
	} \
	if ( cleanUp.good ) { \
		cleanUp.payload = 1; \
		cleanUp.good = ( ( timing = Core_AddTiming( javascript->core, (unsigned int) ms, repeat, Payload_Timing_ResultHandler_cb, (void * ) payload ) ) != NULL ); \
	} \
	if ( cleanUp.good ) { \
		cleanUp.timer = 1; \
		timing->clearFunc_cb = (timerHandler_cb_t) Payload_Delete; \
		args.rval( ).setInt32( (int32_t) timing->identifier ); \
	} else { \
		if ( cleanUp.timer ) { \
			Core_DelTiming( javascript->core, timing ); \
		} \
		if ( cleanUp.payload ) { \
			Payload_Delete( payload ); payload = NULL; \
		} \
		args.rval( ).setUndefined( ); \
	} \
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
 * var timeoutId = setInterval( function( a, b ) {
 * 	Hard.log( 'Foo : ' + a + ' Bar : ' + b );
 * }, 3000, 'foo', 'bar' );
 * ClearTimeout( timeoutId );
 *
 * @see	setInterval
 * @see	clearTimeout
 * @see	clearInterval
 */

static bool JsnGlobal_SetTimeout( JSContext * cx, unsigned argc, jsval * vpn ) {
	JAVASCRIPT_GLOBAL_SET_TIMER( 0 )
		return false;
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
 * var timeoutId = setInterval( function( a, b ) {
 * 	Hard.log( 'Foo : ' + a + ' Bar : ' + b );
 * }, 3000, 'foo', 'bar' );
 * clearInterval( timeoutId );
 *
 * @see	setTimeout
 * @see	clearTimeout
 * @see	clearInterval
 */

static bool JsnGlobal_SetInterval( JSContext * cx, unsigned argc, jsval * vpn ) {
	JAVASCRIPT_GLOBAL_SET_TIMER( 0 );
	return false;
}
/**
 * Cancel a timeout created by setTimeout.
 *
 * @name	clearTimeout
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{null}
 * @param	{integer}		timerId	Reference to a timer that needs to be stopped.
 *
 * @example
 * var timeoutId = setInterval( function( a, b ) {
 * 	Hard.log( 'Foo : ' + a + ' Bar : ' + b );
 * }, 3000, 'foo', 'bar' );
 * ClearTimeout( timeoutId );
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
 * @param	{integer}		timerId	Reference to a timer that needs to be stopped.
 *
 * @example
 * var timeoutId = setInterval( function( a, b ) {
 * 	Hard.log( 'Foo : ' + a + ' Bar : ' + b );
 * }, 3000, 'foo', 'bar' );
 * clearInterval( timeoutId );
 *
 * @see	setInterval
 * @see	setTimeout
 * @see	clearTimeout
 */
static bool JsnGlobal_ClearTimeout( JSContext * cx, unsigned argc, jsval * vpn ) {
	struct javascript_t * javascript;
	unsigned int identifier;
	JSObject * globalObj;
	JS::CallArgs args;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vpn );
	args.rval( ).setUndefined( );
	globalObj = JS_GetGlobalForObject( cx, &args.callee( ) );
	cleanUp.good = ( JS_ConvertArguments( cx, args, "i", &identifier ) == true );
	if ( cleanUp.good ) {
		javascript = (struct javascript_t *) JS_GetPrivate( globalObj );
		Core_DelTimingId( javascript->core, identifier );
	}

	return ( cleanUp.good ) ? true: false;
}

static const JSClass jscGlobal = {
	"global",
	JSCLASS_NEW_RESOLVE | JSCLASS_GLOBAL_FLAGS | JSCLASS_IS_GLOBAL | JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmGlobal[ ] = {
	JS_FS( "setTimeout",	JsnGlobal_SetTimeout, 2, 0 ),
	JS_FS( "setInterval",	JsnGlobal_SetInterval, 2, 0 ),
	JS_FS( "clearTimeout",	JsnGlobal_ClearTimeout, 1, 0 ),
	JS_FS( "clearInterval",	JsnGlobal_ClearTimeout, 1, 0 ),
	JS_FS( "include",		JsnGlobal_Include, 1, 0 ),
	JS_FS_END
};

/*
   ===============================================================================
   BOILERPLATE
   ===============================================================================
*/
static struct script_t * Script_New( const struct javascript_t * javascript, const char * cFile ) {
	struct script_t * script;
	size_t len;
	struct {unsigned char good:1;
			unsigned char script:1;
			unsigned char bytecode:1;
			unsigned char fn:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	len = strlen( cFile ) + strlen( javascript->path ) + 2;
	cleanUp.good = ( ( script = (struct script_t *) malloc( sizeof( *script ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.script = 1;
		script->bytecode = NULL;
		cleanUp.good = ( ( script->fileNameWithPath = (char *) malloc( len ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.fn = 1;
		snprintf( script->fileNameWithPath, len, "%s/%s", javascript->path, cFile );
		JS::RootedObject		globalObjRoot( javascript->context, javascript->globalObj );
		JS::HandleObject 		globalObjHandle( globalObjRoot );
		JS::RootedScript 		scriptRoot( javascript->context, script->bytecode );
		JS::PersistentRootedScript scriptPer( javascript->context, script->bytecode );
		JS::HandleScript		scriptHandle( scriptRoot );
		JS::MutableHandleScript	scriptMut( &scriptRoot );
		JS::CompileOptions		options( javascript->context );
		options.setIntroductionType( "js include" )
			.setFileAndLine( cFile, 1 )
			.setCompileAndGo( true )
			.setNoScriptRval( true )
			.setUTF8( true );
		cleanUp.good = 0; //  to reuse the JS:: scope, we deviate now from the cleanUp.good style
		if ( JS::Compile( javascript->context, globalObjHandle, options, script->fileNameWithPath, scriptMut ) == true ) {
			cleanUp.bytecode = 1;
			if ( JS_ExecuteScript( javascript->context, globalObjHandle, scriptHandle ) == true ) {
				cleanUp.good = 1;
			}
		}
	}
	if ( cleanUp.good ) {
		Core_Log( javascript->core, LOG_INFO, cFile, 0, "Script loaded" );
	} else {
		Core_Log( javascript->core, LOG_INFO, cFile, 0, "Script could not be loaded" );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.bytecode ) {
			script->bytecode = NULL;
		}
		if ( cleanUp.fn ) {
			free( script->fileNameWithPath ); script->fileNameWithPath = NULL;
		}
		if ( cleanUp.script ) {
			free( script ); script = NULL;
		}
	}
	return script;
}

static void Script_Delete( struct script_t * script ) {
	free( script->fileNameWithPath ); script->fileNameWithPath = NULL;
	script->bytecode = NULL;
	free( script ); script = NULL;
}

static int Javascript_Run( struct javascript_t * javascript ) {
	struct {unsigned char good:1;}cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	JSAutoRequest 				ar( javascript->context );
	JSAutoCompartment 			ac( javascript->context, javascript->globalObj );
	JS::RootedObject			globalObjRoot( javascript->context, javascript->globalObj );
	JS::HandleObject			globalObjHandle( globalObjRoot );

	JSObject * consoleObj;
	consoleObj = 	JS_InitClass( javascript->context, globalObjHandle, JS::NullPtr( ), &jscConsole, nullptr, 0, nullptr, jsmConsole, nullptr, nullptr );
	JS::RootedObject 			consoleObjRoot( javascript->context, consoleObj );
	JS_SetPrivate( consoleObj, (void * ) javascript );

	JSObject * hardObj;
	hardObj = 	JS_InitClass( javascript->context, globalObjHandle, JS::NullPtr( ), &jscHard, nullptr, 0, nullptr, jsmHard, nullptr, nullptr );
	JS::RootedObject			hardObjRoot( javascript->context, hardObj );
	JS::HandleObject			hardObjHandle( hardObjRoot );
	JS_SetPrivate( hardObj, (void * ) javascript );

	JSObject * webserverObj;
	webserverObj = 	JS_InitClass( javascript->context, hardObjHandle, JS::NullPtr( ), &jscWebserver, JsnWebserver_Constructor, 3, nullptr, jsmWebserver, nullptr, nullptr );
	JS::RootedObject webserverObjRoot( javascript->context, webserverObj );
#if HAVE_MYSQL == 1
	JSObject * mysqlObj;
	mysqlObj = JS_InitClass( javascript->context, hardObjHandle, JS::NullPtr( ), &jscMysqlclient, JsnMysqlclient_Constructor, 2, nullptr, jsmMysqlclient, nullptr, nullptr );
	JS::RootedObject mysqlObjRoot( javascript->context, mysqlObj );
#endif
	JSObject * postgresqlObj;
	postgresqlObj =  JS_InitClass( javascript->context, hardObjHandle, JS::NullPtr( ), &jscPostgresqlclient, JsnPostgresqlclient_Constructor, 2, nullptr, jsmPostgresqlclient, nullptr, nullptr );
	JS::RootedObject pgsqlclientObj( javascript->context, postgresqlObj );

	cleanUp.good = ( Javascript_AddScript( javascript, javascript->fileName ) != NULL );

	return cleanUp.good;
}

static int Javascript_Init( struct javascript_t * javascript ) {
	struct {unsigned char good:1;
			unsigned char global:1;
			unsigned char standard:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	JSAutoRequest ar = JSAutoRequest( javascript->context );
	JS::CompartmentOptions compartmentOptions;
	compartmentOptions.setVersion( JSVERSION_LATEST );
	javascript->globalObj = JS_NewGlobalObject( javascript->context, &jscGlobal, nullptr, JS::DontFireOnNewGlobalHook, compartmentOptions );
	JS::PersistentRootedObject globalObjPers( javascript->context, javascript->globalObj );
	JS::RootedObject globalObjRoot( javascript->context, javascript->globalObj );
	JS::HandleObject globalObjHandle( globalObjRoot );
	cleanUp.good = ( javascript->globalObj != NULL );
	if ( cleanUp.good ) {
		cleanUp.global = 1;
	}
	JSAutoCompartment ac( javascript->context, javascript->globalObj );
	cleanUp.good = ( ( JS_InitStandardClasses( javascript->context, globalObjHandle ) ) == true );
	if ( cleanUp.good ) {
		cleanUp.standard = 1;
		JS_FireOnNewGlobalObject( javascript->context, globalObjHandle );
		cleanUp.good = ( JS_DefineFunctions( javascript->context, globalObjHandle, jsmGlobal ) == true );
	}
	if ( cleanUp.good ) {
		JS_SetPrivate( javascript->globalObj, ( void * ) javascript );
		cleanUp.good = ( Javascript_Run( javascript ) ) ? 1: 0;
	}
	return cleanUp.good;
}

static void JsnReport_Error( JSContext * cx, const char * message, JSErrorReport * report ) {
	struct javascript_t * javascript;
	const char * fileName, * state;
	char * messageFmt;
	int level;
	size_t len;
	struct {unsigned char good:1;
			unsigned char msg:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	javascript = (struct javascript_t *) JS_GetContextPrivate( cx );
	//  http://egachine.berlios.de/embedding-sm-best-practice/ar01s02.html#id2464522
	fileName = ( report->filename != NULL )? report->filename : "<no filename>";
	level = LOG_ERR;
	state = "Error";
	if ( JSREPORT_IS_WARNING( report->flags ) ) {
		state = "Warning";
		level = LOG_WARNING;
	}
	if ( JSREPORT_IS_STRICT( report->flags ) ) {
		state = "Strict";
		level = LOG_ERR;
	}
	if ( JSREPORT_IS_EXCEPTION( report->flags ) ) {
		state = "Exception";
		level = LOG_CRIT;
	}
	len = 4 + 1 + 3 + strlen( state ) + strlen( message );
	cleanUp.good = ( ( messageFmt = (char *) malloc( len ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.msg = 1;
		snprintf( messageFmt, len, "%s/%3d:\t%s", state, report->errorNumber, message );
		Core_Log( javascript->core, level, fileName, report->lineno, messageFmt );
	}
	if ( cleanUp.msg ) {
		free( messageFmt ); message = NULL;
	}
}

struct javascript_t * Javascript_New( const struct core_t * core, const char * path, const char * fileName ) {
	struct javascript_t * javascript;
	char * lastChar;
	struct {	unsigned char good:1;
				unsigned char path:1;
				unsigned char fileName:1;
				unsigned char jsinit:1;
				unsigned char runtime:1;
				unsigned char context:1;
				unsigned char init:1;
				unsigned char javascript:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( javascript = (struct javascript_t *) malloc( sizeof( * javascript ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
		javascript->scripts = NULL;
		javascript->core = ( struct core_t *) core;
		cleanUp.good = ( ( javascript->path = Xstrdup( path ) ) != NULL );
	}
	if 	( cleanUp.good ) {
		cleanUp.path = 1;
		lastChar = (char*) &javascript->path[strlen( javascript->path ) - 1];
		if ( '/' == *lastChar ) {
			*lastChar = '\0';
		}
		cleanUp.good = ( ( javascript->fileName = Xstrdup( fileName ) ) != NULL );
	}
	if ( cleanUp.good ) {
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
		JS_SetContextPrivate( javascript->context, (void *) javascript );
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
		JS_SetErrorReporter( javascript->runtime, JsnReport_Error );
		cleanUp.good = ( Javascript_Init( javascript ) == 1 );
	}
	if ( cleanUp.good ) {
		cleanUp.init = 1;
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
		if ( cleanUp.fileName ) {
			free( (char *) javascript->fileName ); javascript->fileName = NULL;
		}
		if ( cleanUp.path ) {
			free( (char *) javascript->path ); javascript->path = NULL;
		}
		if ( cleanUp.javascript ) {
			javascript->scripts = NULL;
			javascript->core = NULL;
			free( javascript ); javascript = NULL;
		}
	}

	return javascript;
}

static struct script_t * Javascript_AddScript( struct javascript_t * javascript, const char * fileName ) {
	struct script_t * script;
	struct {unsigned char good:1;
			unsigned char script:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( (  script = Script_New( javascript, fileName ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.script = 1;
		if ( javascript->scripts == NULL ) {
			javascript->scripts = script;
		} else {
			PR_APPEND_LINK( &script->mLink, &javascript->scripts->mLink );
		}
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.script ) {
				Script_Delete( script ); script = NULL;
		}
	}

	return script;
}

static void Javascript_DelScript( struct javascript_t * javascript, struct script_t * script ) {
	struct script_t * scriptFirst;
	PRCList * next;

	scriptFirst = javascript->scripts;
	if ( script == scriptFirst ) {
		next = PR_NEXT_LINK( &script->mLink );
		if ( next  == &script->mLink ) {
			javascript->scripts = NULL;
		} else {
			scriptFirst = FROM_NEXT_TO_ITEM( struct script_t );
			javascript->scripts = scriptFirst;
		}
	}
	PR_REMOVE_AND_INIT_LINK( &script->mLink );
	Script_Delete( script ); script = NULL;
}

void Javascript_Delete( struct javascript_t * javascript ) {
	struct script_t * firstScript;

	firstScript = javascript->scripts;
	while ( firstScript != NULL ) {
		Javascript_DelScript( javascript, firstScript );
		firstScript = javascript->scripts;
	}
	JSAutoRequest ar( javascript->context );
	JSAutoCompartment ac( javascript->context, javascript->globalObj );
	JS_DestroyContext( javascript->context );
	JS_DestroyRuntime( javascript->runtime );

	free( (char *) javascript->path ); javascript->path = NULL;
	free( (char *) javascript->fileName ); javascript->fileName = NULL;
	javascript->core = NULL;
	free( javascript ); javascript = NULL;

	if ( jsInterpretersAlive > 0 ) {
		JS_ShutDown( );
		jsInterpretersAlive--;
	}
}
