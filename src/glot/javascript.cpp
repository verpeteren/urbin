#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stddef.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "javascript.h"
#include "../feature/webserver.h"
#include "../feature/webclient.h"
#include "../feature/sqlclient.h"
#include "../core/utils.h"

#define MAIN_OBJ_NAME "Urbin"

extern const char * ConnectionDefinitions[];

/* ============================================================================================================================================================== */
/* Naming convention deviations for this file                                                                                                                     */
/*                                                                                                                                                                */
/* Jsn	Javascript Spidermonkey Native function   static bool Jsn"Classname"t_"Function"( JSContext * cx, unsigned argc, jsval * vp )                            */
/* jsc  javascript spidermonkey class             static const JSClass jsc"ClassName"                                                                             */
/* jsp  javascript spidermonkey property          static const JSPropertySpec jsp"ClassName"                                                                      */
/* jsm  javascript spidermonkey method            static const JSFunctionSpec jsm"Classname"[]                                                                    */
/* ============================================================================================================================================================== */

typedef void 					( * queryResultHandler_cb_t )		( const struct query_t * query );

struct payload_t {
	JSContext *						context;
	JS::Heap<JSObject*> 			scopeObj;
	JS::Heap<JS::Value>				fnVal_cb;
	JS::Heap<JS::Value> 			fnVal_cbArg;
	bool							repeat;
};
struct script_t {
	char *							fileNameWithPath;
	JSScript *						bytecode;
	PRCList							mLink;
};

static struct payload_t * 			Payload_New						( JSContext * cx, JSObject * object, jsval fnVal_cb, jsval fnVal_cbArg, const bool repeat );
static int 							Payload_Call					( const struct payload_t * payload );
static int 							Payload_Timing_ResultHandler_cb	( void * cbArgs );
static void 						Payload_Delete					( struct payload_t * payload );
static void 						Payload_Delete_Anon				( void * cbArgs );
static struct script_t * 			Javascript_AddScript			( struct javascript_t * javascript, const char * fileName );
static int jsInterpretersAlive = 0;

inline void JAVASCRIPT_MODULE_ACTION( const struct javascript_t * javascript, const char * action ) {
	JSObject * urbinObj;
	jsval urbinVal;

	urbinObj = NULL;
	urbinVal = JSVAL_NULL;
	JSAutoRequest			ar( javascript->context );
	JSAutoCompartment		ac( javascript->context, javascript->globalObj );
	JS::RootedObject		globalObjRoot( javascript->context, javascript->globalObj );
	JS::HandleObject		globalObjHandle( globalObjRoot );
	JS::RootedValue			rValRoot( javascript->context );
	JS::MutableHandleValue	rValMut( &rValRoot );
	JS::RootedValue			urbinValRoot( javascript->context, urbinVal );
	JS::HandleValue			urbinValHandle( urbinValRoot );
	JS::MutableHandleValue	urbinValMut( &urbinValRoot );
	JS::RootedObject		urbinObjRoot( javascript->context, urbinObj );
	JS::HandleObject		urbinObjHandle( urbinObjRoot );
	JS::MutableHandleObject	urbinObjMut( &urbinObjRoot );
	JS_GetProperty( javascript->context, globalObjHandle, MAIN_OBJ_NAME, urbinValMut );
	JS_ValueToObject( javascript->context, urbinValHandle, urbinObjMut );
	JS_CallFunctionName( javascript->context, urbinObjHandle, action, JS::HandleValueArray::empty( ), rValMut );
}

/**
 * The spidermonkey module is loaded.
 *
 * @name	Urbin.onLoad
 * @event
 * @public
 * @since	0.0.8a
 * @returns	{null}
 *
 * @example
 * Urbin.onLoad = function( ) {
 * 	console.log( "loaded" );
 * };
 *
 * @see	Urbin.onReady
 * @see	Urbin.onUnload
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
			Javascript_Delete( javascript );
		}
	}
	return ( cleanUp.good ) ? 1: 0;
}

/**
 * The javascript module is loaded and ready to run.
 *
 * @name	Urbin.onReady
 * @event
 * @public
 * @since	0.0.8a
 * @returns	{null}
 *
 * @example
 * Urbin.onReady = function( ) {
 * 	console.log( "loaded and ready" );
 * };
 *
 * @see	Urbin.onLoad
 * @see	Urbin.onUnload
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
 * @name	Urbin.onUnload
 * @event
 * @public
 * @since	0.0.8a
 * @returns	{null}
 *
 * @example
 * Urbin.onUnload = function( ) {
 * 	console.log( "unloading" );
 * };
 *
 * @see	Urbin.onLoad
 * @see	Urbin.onReady
 * @see	Urbin.shutdown
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
 * @name Urbin.Sqlclient
 * @private
 * @object
 */
#define SET_PROPERTY_ON( handle, key, value ) do {\
	JS::RootedValue valRoot( cx, value ); \
	JS::HandleValue valHandle( valRoot ); \
	cleanUp.good = ( JS_DefineProperty( cx, handle, key, valHandle, attrs, JS_PropertyStub, JS_StrictPropertyStub ) == true ); \
} while ( 0 );

#define CONNOBJ_GET_PROP_STRING( property, var ) do { \
	if ( JS_GetProperty( cx, connObjHandle, property, valueMut ) == true ) { \
		if ( valueMut.isString( ) ) { \
			cleanUp.good = ( ( var = JS_EncodeString( cx, valueMut.toString( ) ) ) != NULL ); \
		} else { \
			cleanUp.good = 0; \
		} \
	} else { \
		cleanUp.good = 0; \
	} \
} while ( 0 );

#define CONNOBJ_GET_PROP_NR( property, var ) do { \
	if ( JS_GetProperty( cx, connObjHandle, property, valueMut ) == true ) { \
		if ( valueMut.isNumber( ) ) { \
			var = ( int ) valueMut.toNumber( ); \
		} else if ( value.isString( ) ) { \
			char * dummy; \
			\
			cleanUp.good = ( ( dummy = JS_EncodeString( cx, valueMut.toString( ) ) ) != NULL ); \
			var = atoi( dummy );\
			JS_free( cx, dummy ); dummy = NULL; \
		} else { \
			cleanUp.good = 0; \
		} \
	} else { \
		cleanUp.good = 0; \
	} \
} while ( 0 );

static bool SqlClassConstructor( JSContext * cx, unsigned argc, jsval * vp, const sqlNew_cb_t engine_new, const JSClass * jsnClass, const JSFunctionSpec jsms[] )  {
	struct sqlclient_t * sqlclient;
	struct javascript_t * instance;
	JSObject * globalObj, * sqlclientObj, * connObj, * thisObj;
	char * cHostName, * cUserName, * cPassword, * cDbName;
	int port, timeoutSec;
	JS::CallArgs args;
	struct {unsigned char good:1;
			unsigned char cHostName:1;
			unsigned char cUserName:1;
			unsigned char cPassword:1;
			unsigned char cDbName:1;
			unsigned char port:1;
			} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	cUserName = NULL;
	cHostName = NULL;
	cUserName = NULL;
	cPassword = NULL;
	cDbName = NULL;
	timeoutSec = 0;
	port = 0;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "o/i", &connObj, &timeoutSec ) == true );
	if ( cleanUp.good ) {
		jsval value;

		value = JSVAL_NULL;
		JS::RootedObject connObjRoot( cx, connObj );
		JS::HandleObject connObjHandle( connObjRoot );
		JS::RootedValue valueRoot( cx, value );
		JS::MutableHandleValue valueMut( &valueRoot );
		/*  Refer to http://www.postgresql.org/docs/9.3/static/libpq-connect.html#LIBPQ-PARAMKEYWORDS for more details. */ \
		CONNOBJ_GET_PROP_STRING( "host", cHostName );
		CONNOBJ_GET_PROP_STRING( "user", cUserName );
		CONNOBJ_GET_PROP_STRING( "password", cPassword );
		CONNOBJ_GET_PROP_STRING( "db", cDbName );
		CONNOBJ_GET_PROP_NR( "port", port );
	}
	thisObj = JS_THIS_OBJECT( cx, vp );
	if ( cleanUp.good ) {
		cleanUp.good = ( thisObj != NULL );
	}
	JS::RootedObject thisObjRoot( cx, thisObj );
	if ( cleanUp.good ) {
		globalObj = JS_GetGlobalForObject( cx, &args.callee( ) );
		instance = (struct javascript_t * ) JS_GetPrivate( globalObj );
		cleanUp.good = ( ( sqlclient = engine_new( instance->core, cHostName, (uint16_t) port, cUserName, cPassword, cDbName, (unsigned char) timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ) {
		sqlclientObj = JS_NewObjectForConstructor( cx, jsnClass, args );
		JS::RootedObject sqlclientObjRoot( cx, sqlclientObj );
		JS::HandleObject sqlclientObjHandle( sqlclientObjRoot );
		cleanUp.good = ( JS_DefineFunctions( cx, sqlclientObjHandle, jsms ) == true );
		} \
	if ( cleanUp.good ) {
		JS_SetPrivate( sqlclientObj, ( void * ) sqlclient );
		args.rval( ).setObject( *sqlclientObj );
	} else {
		args.rval( ).setNull( );
	}

	memset( cPassword, '\0', strlen( cPassword ) ); /*  @TODO:  clean jPassword */
	JS_free( cx, cHostName ); cHostName = NULL;
	JS_free( cx, cUserName ); cUserName = NULL;
	JS_free( cx, cPassword ); cPassword = NULL;
	JS_free( cx, cDbName ); cDbName = NULL;
	JS_free( cx, cDbName ); cDbName = NULL;
	return ( cleanUp.good ) ? true : false;
}

#define SQL_CLIENT_QUERY_RESULT_HANDLER_CB( formatter, sub ) do { \
	struct payload_t * payload; \
	JSObject * resultObj; \
	jsval queryResultVal; \
	 \
	payload = ( struct payload_t * ) query->cbArgs; \
	JSAutoRequest 			ar( payload->context ); \
	JSAutoCompartment 		ac( payload->context, payload->scopeObj ); \
	resultObj = formatter( payload->context, query->result.sub.res ); \
	JS::RootedObject resultObjRoot( payload->context, resultObj ); \
	queryResultVal = OBJECT_TO_JSVAL( resultObj ); \
	JS::RootedValue resultVal( payload->context, queryResultVal ); \
	payload->fnVal_cbArg = JS::Heap<JS::Value>( queryResultVal ); \
	Payload_Call( payload ); \
	} while ( 0 );

static bool SqlClientQuery( JSContext * cx, unsigned argc, jsval * vp, queryResultHandler_cb_t handler_cb ) {
	struct payload_t * payload;
	struct sqlclient_t * sqlclient;
	jsval paramList, value, fnVal, dummyVal;
	JSObject * sqlObj;
	JSString * jStatement;
	JS::CallArgs args;
	unsigned int nParams;
	size_t i;

	const char ** cParamValues;
	char * cStatement;
	struct {unsigned char good:1;
		unsigned char params:1;
		unsigned char statement:1;
		unsigned char payload:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	value = JSVAL_NULL;
	args = CallArgsFromVp( argc, vp );
	i = 0;
	sqlclient = NULL;
	nParams = 0;
	cParamValues = NULL;
	cStatement = NULL;
	payload = NULL;
	dummyVal = JSVAL_NULL;
	cleanUp.good = ( argc == 3 );
	paramList = JSVAL_NULL;
	if ( cleanUp.good ) {
		paramList = args[1];
		fnVal = args[2];
	}
	sqlObj = JS_THIS_OBJECT( cx, vp );
	JS::RootedObject        sqlObjRoot( cx, sqlObj );
	JS::RootedValue 		paramListRoot( cx, paramList );
	JS::HandleValue 		paramListHandle( paramListRoot );
	JS::HandleValueArray 	paramListHandleArray( paramListRoot);
	JS::RootedValue 		fnValRoot( cx, fnVal );
	JS::HandleValue 		fnValHandle( fnValRoot );
	JS::MutableHandleValue 	fnValMut( &fnValRoot );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( sqlclient = (struct sqlclient_t *) JS_GetPrivate( sqlObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertArguments( cx, args, "S*f", &jStatement, &paramListRoot, &dummyVal ) == true );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertValue( cx, fnValHandle, JSTYPE_FUNCTION, fnValMut ) == true );
	}
	if ( cleanUp.good ) {
		if ( paramList.isNullOrUndefined( ) ) {
			/*  it is a query like "SELECT user FROM users WHERE user_id = 666", 				null, 		function( result ) {console.log( result );} ); */
			nParams = 0;
		} else if ( paramList.isString( ) || paramList.isNumber( ) ) {
			/*  it is a query like "SELECT user FROM users WHERE user_id = $1",				 	666, 		function( result ) {console.log( result );} ); */
			nParams = 1;
			cleanUp.good = ( ( cParamValues = ( const char ** ) malloc( sizeof( *cParamValues ) * nParams ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.params = 1; \
				if ( paramList.isNumber( ) ) {
					paramList.setString( paramList.toString( ) );
				}
				cleanUp.good = ( ( cParamValues[i] = JS_EncodeString( cx, value.toString( ) ) ) != NULL );
			}
		} else {
			/*  it is a query like "SELECT user FROM users WHERE user_id BETWEEN $1 AND $2",	[664, 668],	function( result ) {console.log( result );} ); */
			JSObject * paramObj;
			jsval paramVal;
			jsid indexId;
			bool success;

			indexId = JSID_VOID;
			paramVal = JSVAL_NULL;
			paramObj = &paramList.toObject( );
			JS::RootedObject 		paramObjRoot( cx, paramObj );
			JS::HandleObject 		paramObjHandle( paramObjRoot );
			JS_GetArrayLength( cx, paramObjHandle, &nParams );
			if ( nParams > 0 ) {
				cleanUp.good = ( ( cParamValues = ( const char ** ) malloc( sizeof( *cParamValues ) * nParams ) ) != NULL );
				if ( cleanUp.good ) {
					cleanUp.params = 1;
					JS::AutoIdArray idArray( cx, JS_Enumerate( cx, paramObjHandle ) );
					if ( !! idArray ) {
						success = 1;
						for ( i = 0; success && i < idArray.length( ); i++ ) {
							JS::RootedId 			idRoot( cx, idArray[i] );
							JS::HandleId			idHandle( idRoot );
							JS::RootedValue 		valueRoot( cx, paramVal );
							JS::MutableHandleValue	valueMut( &valueRoot );
							if ( JS_GetPropertyById( cx, paramObjHandle, idHandle, valueMut ) ) {
								success = ( ( cParamValues[i] = JS_EncodeString( cx, valueMut.toString( ) ) ) != NULL );
							}
						}
					}
				}
			}
		}
	}
	if ( cleanUp.good ) {
		jsval dummy;
		cleanUp.good = ( ( payload = Payload_New( cx, sqlObj, fnValMut.get( ), dummy, false ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		cleanUp.good = ( ( cStatement = JS_EncodeString( cx, jStatement ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.statement = 1;
		Query_New( sqlclient, cStatement, nParams, cParamValues, handler_cb, ( void * ) payload, Payload_Delete_Anon );
	}
	/*  always cleanup  */
	for ( i = 0; i < nParams; i++ ) {
		JS_free( cx, ( char * ) cParamValues[i] ); cParamValues[i] = NULL;
	}
	if ( cleanUp.params ) {
		free( cParamValues ); 	cParamValues = NULL;
	}
	if ( cleanUp.statement ) {
		JS_free( cx, cStatement ); 	cStatement = NULL;
	}
	/*  get ready to return */
	if ( cleanUp.good ) {
		args.rval( ).setBoolean( true );
	} else {
		if ( cleanUp.payload ) {
			Payload_Delete( payload ); payload = NULL;
		}
		args.rval( ).setBoolean( false );
	}
	return ( cleanUp.good ) ? true : false;
}

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
	JS::RootedObject resultArrayRoot( cx, resultArray );
	JS::HandleObject resultArrayHandle( resultArrayRoot );
	if ( cleanUp.good ) {
		if ( result != NULL ) {
			mysac_first_row( result );
			rowCount = ( unsigned int ) mysac_num_rows( result );
			if ( rowCount > 0 ) {
				colCount = mysac_field_count( result );
				rowId = 0;
				while ( cleanUp.good && ( row = mysac_fetch_row( result ) ) != NULL ) {
					jValue = JSVAL_NULL;
					currentVal = JSVAL_NULL;
					cleanUp.good = ( ( recordObj = JS_NewObject( cx, NULL, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
					JS::RootedObject 		recordObjRoot( cx, recordObj );
					JS::HandleObject 		recordObjHandle( recordObjRoot );
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
 * @name	Urbin.MysqlClient.query
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}					If the query could be registered successfully it returns true; else false is returned
 * @param	{string}	statement		A sql command or query. Please not that for postgres the placeholders are marked as $n etc. and for mysql these are ?
 * @param	{array}		[parameters]	A list of parameters that can be used in a query.<p>default: []</p>
 * @param	{function}	fn				The callback function {response}
 *
 * @example
 * var my = Urbin.MysqlClient( {host : '10.0.0.25', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 3306}, 60 );
 * my.query( "SELECT name, sales FROM sales WHERE customer = $1 );" , ['foobar' ], function( res ) {
 * 	if ( Array.isArray( rows ) ) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++ ) {
 * 				row = res[rowId];
 * 				console.log( rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	} );
 *
 * @see	Urbin.MysqlClient
 * @see	Urbin.PostgresqlClient.query
 * @name Urbin.MysqlClient.getInsertId
 */
static bool JsnMysqlclient_Query( JSContext * cx, unsigned argc, jsval * vp ) {
	return SqlClientQuery( cx, argc, vp, Mysqlclient_Query_ResultHandler_cb );
}

/**
 * Get the last id insert with auto-increment.
 * <p>Beware: This is the last id insert for the current connection object.</p>
 *
 * @name Urbin.MysqlClient.getInsertId
 * @function
 * @public
 * @since	0.0.10
 *
 * @returns {integer} Used to get the last insert id with auto-increment for this connection.
 *
 * @example
 * sql.query( 'INSERT INTO table VALUES( "a", "b", "c" )', [],  function( res ) {
 * 	console.log( 'Inserted: ' + sql.getInsertId( ) );
 * } );
 * @see	Urbin.MysqlClient
 * @see	Urbin.MysqlClient.query
 */
static bool JsnMysqlclient_GetInsertId( JSContext * cx, unsigned argc, jsval * vp ) {
	struct sqlclient_t * sqlclient;
	JS::CallArgs args;
	JSObject * sqlObj;

	args = CallArgsFromVp( argc, vp );
	sqlObj = JS_THIS_OBJECT( cx, vp );
	JS::RootedObject sqlObjRoot( cx, sqlObj );
	sqlclient = (struct sqlclient_t *) JS_GetPrivate( sqlObj );
	args.rval( ).set( INT_TO_JSVAL( (int32_t ) mysac_insert_id( sqlclient->connection.my.conn ) ) ); //  this cast is not 100% correct.
	return true;
}

static const JSClass jscMysqlclient = {
	"MysqlClient",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnMysqlclient_Finalizer, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmMysqlclient[ ] = {
	JS_FS( "query", JsnMysqlclient_Query, 3, 0 ),
	JS_FS( "getInsertId", JsnMysqlclient_GetInsertId, 0, 0 ),
	JS_FS_END
};

/**
 * Connect to a mysql server.
 *
 * @name	Urbin.MysqlClient
 * @constructor
 * @public
 * @since	0.0.5b
 * @returns	{object}							The mysql client javascript instance
 * @param	{object}		params				The connection string.
 * @param	{string}		params.host			The host name for auth
 * @param	{int}			params.port			The port number
 * @param	{string}		params.db			The database name
 * @param	{string}		params.user			The database user
 * @param	{string}		params.password		The database user password
 * @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' in the postgresql section of the configurationFile.</p>
 *
 * @example
 * var my = Urbin.MysqlClient( {host : 'localhast', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432 }, 60 );
 * my.query( "SELECT name, sales FROM sales WHERE customer = ? );" , ['foobar' ], function( res ) {
 * 	if ( Array.isArray( res ) ) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++ ) {
 * 				row = res[rowId];
 * 				console.log( rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	} );
 * @see	Urbin.MysqlClient.query
 * @name Urbin.MysqlClient.getInsertId
 * @see	Urbin.PostgreslClient
 */
static bool JsnMysqlclient_Constructor( JSContext * cx, unsigned argc, jsval * vp ) {
	return SqlClassConstructor( cx, argc, vp, Mysql_New, &jscMysqlclient, jsmMysqlclient );
}

static void JsnMysqlclient_Finalizer( JSFreeOp * fop, JSObject * mysqlObj ) {
	struct sqlclient_t * mysql;

	if ( ( mysql = (struct sqlclient_t *) JS_GetPrivate( mysqlObj ) ) != NULL ) {
		Sqlclient_Delete( mysql ); mysql = NULL;
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
						jsval recordVal, jValue;

						cleanUp.good = ( ( recordObj = JS_NewObject( cx, NULL, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
						if ( cleanUp.good ) {

							JS::RootedObject 		recordObjRoot( cx, recordObj );
							JS::HandleObject 		recordObjHandle( recordObjRoot );
							for ( colId = 0; colId < colCount; colId++ ) {
								cFieldName = PQfname( result, colId );  //  speedup might be possible by caching this
								dataType = PQftype( result, colId );
								jValue = JSVAL_NULL;
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
								JS::RootedValue 	jValueRoot( cx, jValue );
								JS::HandleValue 	jValueHandle( jValueRoot );
								JS_SetProperty( cx, recordObjHandle, cFieldName, jValueHandle );  //  hmm, what if postgresql columns do not have ascii chars, ecma does not allow that...?
							}
						}
						recordVal = OBJECT_TO_JSVAL( recordObj );
						JS::RootedValue 	recordValRoot( cx, recordVal );
						JS::HandleValue 	recordValHandle( recordValRoot );
						JS_SetElement( cx, resultArrayHandle, (uint32_t) rowId, recordValHandle );
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
 * @name	Urbin.PostgresqlClient.query
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}					If the query could be registered successfully it returns true; else false is returned
 * @param	{string}	statement		A sql command or query. Please not that for postgres the placeholders are marked as $n etc. and for mysql these are ?
 * @param	{array}		[parameters]	A list of parameters that can be used in a query.<p>default: []</p>
 * @param	{function}	fn				The callback function {response}
 *
 * @example
 * var pg = Urbin.PostgresqlClient( {host : '10.0.0.25', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432 }, 60 );
 * pg.query( "SELECT name, sales FROM sales WHERE customer = $1 );" , ['foobar' ], function( res ) {
 * 	if ( Array.isArray( res ) ) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++ ) {
 * 				row = res[rowId];
 * 				console.log( rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	} );
 *
 * @see	Urbin.PostgresqlClient
 * @see	Urbin.MysqlClient.query
 */
static bool JsnPostgresqlclient_Query( JSContext * cx, unsigned argc, jsval * vp ) {
	return SqlClientQuery( cx, argc, vp, Postgresqlclient_Query_ResultHandler_cb );
}

static const JSClass jscPostgresqlclient = {
	"PostgresqlClient",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnPostgresqlclient_Finalizer, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmPostgresqlclient[ ] = {
	JS_FS( "query", JsnPostgresqlclient_Query, 3, 0 ),
	JS_FS_END
};

/**
 * Connect to a postgresql server.
 *
 * @name	Urbin.PostgresqlClient
 * @constructor
 * @public
 * @since	0.0.5b
 * @returns	{object}							The postgresql client javascript instance or null on failure
 * @param	{object}		params				The connection string.
 * @param	{string}		params.host			The host name for pg auth
 * @param	{int}			params.port			The port number
 * @param	{string}		params.db			The database name
 * @param	{string}		params.user			The database user
 * @param	{string}		params.password		The database user password
 * @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' in the postgresql section of the configurationFile.</p>
 *
 * @example
 * var pg = Urbin.PostgresqlClient( {host : 'localhost', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432 }, 60 );
 * pg.query( "SELECT name, sales FROM sales WHERE customer = $1 );" , ['foobar' ], function( res ) {
 * 	if ( Array.isArray( res ) ) {
 * 		for ( var rowId = 0; rowId < res.length; rowId++ ) {
 * 				row = res[rowId];
 * 				console.log( rowId + ' ' + row.name + ' ' + row.sales );
 * 			}
 * 		}
 * 	} );
 * @see	Urbin.PostgresqlClient.query
 * @see	Urbin.MysqlClient
 */


static bool JsnPostgresqlclient_Constructor( JSContext * cx, unsigned argc, jsval * vp ) {
	return SqlClassConstructor( cx, argc, vp, Postgresql_New, &jscPostgresqlclient, jsmPostgresqlclient );
}

static void JsnPostgresqlclient_Finalizer( JSFreeOp * fop, JSObject * postgresqlObj ) {
	struct sqlclient_t * postgresql;

	if ( ( postgresql = (struct sqlclient_t *) JS_GetPrivate( postgresqlObj ) ) != NULL ) {
		Sqlclient_Delete( postgresql ); postgresql = NULL;
	}
}
extern const char * MethodDefinitions[ ];
/**
 * Webserver response object.
 *
 * @name Urbin.Webserver.req
 * @private
 * @object
 */
/*
   ===============================================================================
   Webclient OBJECT
   ===============================================================================
*/

static JSObject * Webclient_Webpage_ResultToJS( struct payload_t * payload, const struct webpage_t * webpage ) {
	//  @FIXME:  allA  SEE webserver_Route_ResultToJS
	JSContext * cx;
	JSObject * webpageObj, * thisObj;
	JSString * jHeaders, * jContent;
	jsval headerVal, contentVal;
	const unsigned int attrs = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
	struct {unsigned char good:1;
			unsigned char obj:1;
			unsigned char content:1;
			unsigned char headers:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cx = payload->context;
	jContent = jHeaders = NULL;
	JSAutoRequest ar( cx );
	JSAutoCompartment ac( cx, payload->scopeObj );
	thisObj = payload->scopeObj;
	JS::RootedObject thisObjRoot( cx, thisObj );
	JS::HandleObject thisObjHandle( thisObjRoot );
	cleanUp.good = ( ( webpageObj =	JS_NewObject( cx, nullptr, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
	JS::RootedObject webpageObjRoot( cx, webpageObj );
	JS::HandleObject webpageObjHandle( webpageObjRoot );
	if ( cleanUp.good )  {
		cleanUp.obj = 1;
		if ( webpage->response.headers == NULL ) {
			headerVal = JSVAL_VOID;
		} else {
			cleanUp.good = ( ( jHeaders = JS_NewStringCopyN( cx, webpage->response.headers->bytes, webpage->response.headers->used ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.headers = 1;
				headerVal = STRING_TO_JSVAL( jHeaders );
			}
		}
		JS::RootedValue headerValRoot( cx, headerVal );
		SET_PROPERTY_ON( webpageObjHandle, "headers", STRING_TO_JSVAL( jHeaders ) );
	}
	if ( cleanUp.good )  {
		if ( webpage->response.content == NULL ) {
			contentVal = JSVAL_VOID;
		} else {
			cleanUp.good = ( ( jContent = JS_NewStringCopyN( cx, webpage->response.content->bytes, webpage->response.content->used ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.content = 1;
				contentVal = STRING_TO_JSVAL( jContent );
			}
			JS::RootedValue contentValRoot( cx, contentVal );
			SET_PROPERTY_ON( webpageObjHandle, "content", contentVal );
		}
	}
	if ( cleanUp.good )  {
		SET_PROPERTY_ON( webpageObjHandle, "code", INT_TO_JSVAL( webpage->response.httpCode ) );
	}

	if ( ! cleanUp.good )  {
		if ( cleanUp.headers )  {
			JS_free( cx, jHeaders ); jHeaders = NULL;
		}
		if ( cleanUp.headers )  {
			JS_free( cx, jContent ); jContent = NULL;
		}
	}
	return webpageObj;
}

static void Webclient_ResultHandler_cb( const struct webpage_t * webpage ) {
	struct payload_t * payload;
	JSObject * webclientObj;
	jsval webclientVal;

	payload = (struct payload_t *) webpage->cbArgs;
	JSAutoRequest 			ar( payload->context );
	JSAutoCompartment 		ac( payload->context, payload->scopeObj );
	webclientObj = Webclient_Webpage_ResultToJS( payload, webpage );
	JS::RootedObject webservClientObjRoot( payload->context, webclientObj );
	JS::RootedValue webservClientObjVal( payload->context, webclientVal );
	webclientVal = OBJECT_TO_JSVAL( webclientObj );
	payload->fnVal_cbArg = JS::Heap<JS::Value>( webclientVal );
	Payload_Call( payload );
}


/**
 * Fetch another webpage on an existing webclient connection.
 *
 * @name	Urbin.Webclient.queue
 * @function
 * @public
 * @since	0.0.10
 * @returns	{object}							The web client javascript
 * @param	{string}		url					The initial url
 * @param	{object}		params				The request parameters
 * @param	{string}		params.method		The request method "GET" or "POST"
 * @param	{string}		params.headers		The headers, if the headers are not set, then HOST and CONNECTION will be set automatically. As long as there are requests in the queue, the connection will be Keep-Alive, else close
 * @param	{string}		params.content		The content to sent out
 *
 * @example
 * showResponse = function( responseObj ) {
 * 	console.log( 'got ' + responseObj.content );
 *	};
 *
 * var wc = Urbin.Webclient( 'http://www.urbin.com/benchmarks.html', showresponse, 60 );
 * wc.queue( 'www.urbin.com/', showresponse );
 *
 * @see	Urbin.Webclient
 */
 static bool JsnWebclient_Queue( JSContext * cx, unsigned argc, jsval * vp ) {
	struct payload_t * payload;
	struct webclient_t * webclient;
	JSObject * webclientObj, * connObj;
	JSString *jUrl;
	JS::CallArgs args;
	jsval value, fnVal, dummyVal;
	enum requestMode_t mode;
	char * cMode, * cHeaders, * cContent, * cUrl;
	struct {unsigned char good:1;
			unsigned char payload:1;
			unsigned char url:1;} cleanUp;

	cleanUp.good = ( argc >= 2 );
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	dummyVal = JSVAL_NULL;
	value = JSVAL_NULL;
	fnVal = args[1];
	mode = MODE_GET;
	payload = NULL;
	cMode = cHeaders = cContent = cUrl = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S*o", &jUrl, &dummyVal, &connObj ) == true );
		//somehow, connObj is not converted to an object
	connObj = &args[2].toObject( );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cUrl = JS_EncodeString( cx, jUrl ) ) != NULL );
	}
	JS::RootedObject connObjRoot( cx, connObj );
	JS::HandleObject connObjHandle( connObjRoot );
	JS::RootedValue valueRoot( cx, value );
	JS::HandleValue valueHandle( valueRoot );
	JS::MutableHandleValue 	valueMut( &valueRoot );
	JS::RootedValue fnValRoot( cx, fnVal );
	JS::HandleValue fnValHandle( fnValRoot );
	JS::MutableHandleValue 	fnValMut( &fnValRoot );
	webclientObj = JS_THIS_OBJECT( cx, vp );
	JS::RootedObject thisObj( cx, webclientObj );
	webclient = ( struct webclient_t *) JS_GetPrivate( webclientObj );
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		cleanUp.good = ( JS_ConvertValue( cx, fnValHandle, JSTYPE_FUNCTION, fnValMut ) == true );
	}
	if ( cleanUp.good ) {
		CONNOBJ_GET_PROP_STRING( "method", cMode );
		CONNOBJ_GET_PROP_STRING( "headers", cHeaders );
		CONNOBJ_GET_PROP_STRING( "content", cContent );
	}
	if ( cleanUp.good ) {
		mode = ( strcmp( cMode, MethodDefinitions[MODE_POST] ) == 0 ) ? MODE_POST : MODE_GET;
	}
	if ( cleanUp.good ) {
		jsval dummy;
		cleanUp.good = ( ( payload = Payload_New( cx, webclientObj, fnValMut.get( ), dummy, false ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		cleanUp.good = ( Webclient_Queue( webclient, mode, cUrl, cHeaders, cContent, Webclient_ResultHandler_cb, (void *) payload, Payload_Delete_Anon ) != NULL );
	}
	args.rval( ).setObject( *webclientObj );
	JS_free( cx, cUrl ); cUrl = NULL;
	JS_free( cx, cMode ); cMode = NULL;
	JS_free( cx, cHeaders ); cHeaders = NULL;
	JS_free( cx, cContent ); cContent = NULL;
	return ( cleanUp.good ) ? true: false;
}

static void JsnWebclient_Finalizer( JSFreeOp * fop, JSObject * webclientObj );

static const JSClass jscWebclient = {
	"Webclient",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JsnWebclient_Finalizer, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmWebclient[ ] = {
	JS_FS( "queue", 			JsnWebclient_Queue, 2, 0 ),
	JS_FS_END
};

/**
 * Start a webclient.
 *
 * @name	Urbin.Webclient
 * @constructor
 * @public
 * @since	0.0.10
 * @returns	{object}							The web client javascript
 * @param	{string}		url					The initial url
 * @param	{object}		params				The request parameters
 * @param	{string}		params.method		The request method "GET" or "POST"
 * @param	{string}		params.headers		The headers, if the headers are not set, then HOST and CONNECTION will be set automatically. As long as there are requests in the queue, the connection will be Keep-Alive, else close
 * @param	{string}		params.content		The content to sent out
 * @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' in the webclient section of the configurationFile.</p>
 *
 * @example
 * showResponse = function( responseObj ) {
 * 	console.log( 'got ' + responseObj.content );
 *	};
 *
 * var wc = Urbin.Webclient( 'http://www.urbin.com/benchmarks.html', showresponse, 60 );
 * wc.queue( 'www.urbin.com/', showresponse );
 *
 * @see	Urbin.Webclient.queue
 */static bool JsnWebclient_Constructor( JSContext * cx, unsigned argc, jsval * vp ) {
	struct payload_t * payload;
	struct webclient_t * webclient;
	struct javascript_t * javascript;
	JSObject * webclientObj, *connObj, *thisObj;
	JSString *jUrl;
	JS::CallArgs args;
	jsval value, fnVal, dummyVal;
	enum requestMode_t mode;
	int timeoutSec;
	char * cMode, * cHeaders, * cContent, * cUrl;
	struct {unsigned char good:1;
			unsigned char payload:1;
			unsigned char url:1;} cleanUp;

	timeoutSec = 0;
	cleanUp.good = ( argc >= 3 );
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	dummyVal = JSVAL_NULL;
	cMode = cHeaders = cContent = cUrl = NULL;
	javascript = NULL;
	value = JSVAL_NULL;
	fnVal = args[1];
	mode = MODE_GET;
	payload = NULL;
	cMode = cHeaders = cContent = cUrl = NULL;
	//  @TODO: refractor core funcion of constructor and Queue into define
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S*o/i", &jUrl, &dummyVal, &connObj, &timeoutSec ) == true );
	//somehow, connObj is not converted to an object
	connObj = &args[2].toObject( );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cUrl = JS_EncodeString( cx, jUrl ) ) != NULL );
	}
	JS::RootedObject connObjRoot( cx, connObj );
	JS::HandleObject connObjHandle( connObjRoot );
	JS::RootedValue valueRoot( cx, value );
	JS::HandleValue valueHandle( valueRoot );
	JS::MutableHandleValue 	valueMut( &valueRoot );
	JS::RootedValue fnValRoot( cx, fnVal );
	JS::HandleValue fnValHandle( fnValRoot );
	JS::MutableHandleValue 	fnValMut( &fnValRoot );
	webclientObj = NULL;
	thisObj = JS_THIS_OBJECT( cx, vp );
	JS::RootedObject thisObjRoot( cx, thisObj );
	javascript = (struct javascript_t *) JS_GetPrivate( thisObj );
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		cleanUp.good = ( JS_ConvertValue( cx, fnValHandle, JSTYPE_FUNCTION, fnValMut ) == true );
	}
	if ( cleanUp.good ) {
		CONNOBJ_GET_PROP_STRING( "method", cMode );
		CONNOBJ_GET_PROP_STRING( "headers", cHeaders );
		CONNOBJ_GET_PROP_STRING( "content", cContent );
	}
	if ( cleanUp.good ) {
		mode = ( strcmp( cMode, MethodDefinitions[MODE_POST] ) == 0 ) ? MODE_POST : MODE_GET;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( webclientObj = 	JS_NewObject( cx, &jscWebclient, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
	}
	JS::RootedObject webclientObjRoot( cx, webclientObj );
	JS::HandleObject webclientObjHandle( webclientObjRoot );
	if ( cleanUp.good ) {
		jsval dummy;
		cleanUp.good = ( ( payload = Payload_New( cx, webclientObj, fnValMut.get( ), dummy, false ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		cleanUp.good = ( JS_DefineFunctions( cx, webclientObjHandle, jsmWebclient ) == true );
	}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		cleanUp.good = ( ( webclient = Webclient_New( javascript->core, mode, cUrl, cHeaders, cContent, Webclient_ResultHandler_cb, (void *) payload, Payload_Delete_Anon, (unsigned char) timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ) {
		JS_SetPrivate( webclientObj, ( void * ) webclient );
		args.rval( ).setObject( *webclientObj );
	} else {
		args.rval( ).setUndefined( );
	}
	JS_free( cx, cUrl ); cUrl = NULL;
	JS_free( cx, cMode ); cMode = NULL;
	JS_free( cx, cHeaders ); cHeaders = NULL;
	JS_free( cx, cContent ); cContent = NULL;
	return ( cleanUp.good ) ? true: false;
}

static void JsnWebclient_Finalizer( JSFreeOp * fop, JSObject * webclientObj ) {
	struct webclient_t * webclient;

	if ( ( webclient = ( struct webclient_t * ) JS_GetPrivate( webclientObj ) ) != NULL ) {
		Webclient_Delete( webclient ); webclient = NULL;
	}
}
/*
   ===============================================================================
   Webserver OBJECT
   ===============================================================================
*/

static void JsnWebserver_Finalizer( JSFreeOp * fop, JSObject * webserverObj );

/**
 * Set the HTTP response
 *
 * In a dynamic route, the body can be set manually
 *
 * @name	Urbin.Webserverclient.setContent
 * @function
 * @public
 * @since	0.0.8a
 * @returns	{Urbin.Webserverclient}
 * @param	{string}		the content of the http response.
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	client.response.setCode( 404 ).setContent( 'Not found!' ).setMime( 'html' );
 * //  or as the longer form
 * 	client.response.setMime( 'text/html' );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.response
 * @see	Urbin.webserverclient.response.setMime
 * @see	Urbin.webserverclient.response.setCode
 */
static bool JsnWebserverclientresponse_SetContent( JSContext * cx, unsigned argc, jsval * vp ) {
	struct webserverclientresponse_t * response;
	JS::CallArgs args;
	JSString * jString;
	JSObject * thisObj;
	char * cString;
	struct {unsigned char good:1;
			unsigned char cstring:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	thisObj = JS_THIS_OBJECT( cx, vp );
	args.rval( ).set( OBJECT_TO_JSVAL( thisObj ) );
	cString = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jString ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cString = JS_EncodeString( cx, jString ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.cstring = 1;
		cleanUp.good = ( ( response = (struct webserverclientresponse_t *) JS_GetPrivate( thisObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Webserverclientresponse_SetContent( response, cString ) ) ? 1 : 0;
	}
	//  always cleanUp
	if ( cleanUp.cstring ) {
		JS_free( cx, cString ); cString = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Set the HTTP return code
 *
 * In a dynamic route, the http code can be set manually
 *
 * @name	Urbin.Webserverclient.setCode
 * @function
 * @public
 * @since	0.0.8a
 * @returns	{Urbin.Webserverclient}
 * @param	{integer}		the return code. e.g. 404 for 'Not Found'
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	client.response.setCode( 404 ).setContent( 'Not found!' ).setMime( 'html' );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.response
 * @see	Urbin.webserverclient.response.setContent
 * @see	Urbin.webserverclient.response.setMime
 */

static bool JsnWebserverclientresponse_SetCode( JSContext * cx, unsigned argc, jsval * vp ) {
	struct webserverclientresponse_t * response;
	JS::CallArgs args;
	JSObject * thisObj;
	int code;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	thisObj = JS_THIS_OBJECT( cx, vp );
	args.rval( ).set( OBJECT_TO_JSVAL( thisObj ) );
	cleanUp.good = ( JS_ConvertArguments( cx, args, "i", &code ) == true );
	if ( cleanUp.good ) {
		thisObj = JS_THIS_OBJECT( cx, vp );
		cleanUp.good = ( ( response = (struct webserverclientresponse_t *) JS_GetPrivate( thisObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Webserverclientresponse_SetCode( response, (unsigned int) code ) ) ? 1 : 0;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Set the HTTP Mimetype header
 *
 * In a dynamic route, the mimetype can be set manually
 *
 * @name	Urbin.Webserverclient.setMime
 * @function
 * @public
 * @since	0.0.8a
 * @returns	{Urbin.Webserverclient}
 * @param	{string}		the mimetype to set. Currently the list of mime types is limited as is the background a typedef is used
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	client.response.setCode( 404 ).setContent( 'Not found!' ).setMime( 'html' );
 * //  or as the longer form
 * 	client.response.setMime( 'text/html' );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.response
 * @see	Urbin.webserverclient.response.setContent
 * @see	Urbin.webserverclient.response.setCode
 */
static bool JsnWebserverclientresponse_SetMime( JSContext * cx, unsigned argc, jsval * vp ) {
	struct webserverclientresponse_t * response;
	JS::CallArgs args;
	JSString * jString;
	JSObject * thisObj;
	char * cString;
	struct {unsigned char good:1;
			unsigned char cstring:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	thisObj = JS_THIS_OBJECT( cx, vp );
	args.rval( ).set( OBJECT_TO_JSVAL( thisObj ) );
	cString = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jString ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cString = JS_EncodeString( cx, jString ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.cstring = 1;
		cleanUp.good = ( ( response = (struct webserverclientresponse_t *) JS_GetPrivate( thisObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( Webserverclientresponse_SetMime( response, cString ) ) ? 1 : 0;
	}
	//  always cleanUp
	if ( cleanUp.cstring ) {
		JS_free( cx, cString ); cString = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Get the named groups that matched the routing regex
 *
 * In a dynamic route, the route can be defined with regexes with named groups. these will be set in an object
 *
 * @name	Urbin.Webserverclient.getNamedGroups
 * @function
 * @public
 * @since	0.0.8a
 * @returns	{object}
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/blog/(?<year>\d{4})/(?<month>\d{1,2})/(?<day>\d{1,2})', function( client ) {
 * 	var params = client.getNamedGroups( );
 * 	console.log( params.year + '-' + params.month + '-' + params.day );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.response
 */

static bool JsnWebserverclient_GetNamedGroups( JSContext * cx, unsigned argc, jsval * vp ) {
	struct webserverclient_t * webserverclient;
	struct namedRegex_t * namedRegex;
	JSObject * thisObj, * matchObj;
	JS::CallArgs args;
	jsval jValue;
	JSString * jString;
	size_t i;
	struct {unsigned char good:1;
			unsigned char named:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	namedRegex = NULL;
	cleanUp.good = ( ( matchObj = 	JS_NewObject( cx, nullptr, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
	JS::RootedObject matchObjRoot( cx, matchObj );
	JS::MutableHandleObject matchObjMut( &matchObjRoot );
	if ( cleanUp.good ) {
		args = CallArgsFromVp( argc, vp );
		thisObj = JS_THIS_OBJECT( cx, vp );
		JS::RootedObject thisObjRoot( cx, thisObj );
		webserverclient = (struct webserverclient_t *) JS_GetPrivate( thisObj );
		namedRegex = Webserverclient_GetNamedGroups( webserverclient );
		if ( namedRegex != NULL ) {
			cleanUp.named = 1;
			for ( i = 0; i < namedRegex->numGroups * 2; i += 2 ) {
				jString = JS_NewStringCopyZ( cx, namedRegex->kvPairs[i + 1] );
				jValue =  STRING_TO_JSVAL( jString );
				JS::RootedValue	jValueRoot( cx, jValue );
				JS::HandleValue	jValueHandle( jValueRoot );
				JS::RootedString jStringRoot( cx, jString );
				JS_SetProperty( cx, matchObjMut, namedRegex->kvPairs[i], jValueHandle );
				//	printf( "%d %s %s\n", i, namedRegex->kvPairs[i], namedRegex->kvPairs[i + 1] );
			}
		}
		args.rval( ).set( OBJECT_TO_JSVAL( matchObjMut.get( ) ) );
	}
	//  always cleanup
	if ( cleanUp.named ) {
		NamedRegex_Delete( namedRegex ); namedRegex = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * The HTTP client object
 *
 * In a dynamic route, this is variable is available in the callback function
 *
 * @name	Urbin.Webserverclient
 * @object
 * @public
 * @since	0.0.8a
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	client.response.setCode( 404 ).setContent( 'Not found!' ).setMime( 'html' );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.response
 * @see	Urbin.webserverclient.getNamedGroups
 * @see	Urbin.webserverclient.ip
 * @see	Urbin.webserverclient.url
 * @see	Urbin.webserverclient.method
 * @see	Urbin.webserverclient.response
 * @see	Urbin.webserverclient.response.setContent
 * @see	Urbin.webserverclient.response.setCode
 * @see	Urbin.webserverclient.response.setMime
 */

static const JSClass jscWebserverclient = {
	"Webserverclient",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmWebserverclient[ ] = {
	JS_FS( "getNamedGroups", 			JsnWebserverclient_GetNamedGroups, 0, 0 ),
	JS_FS_END
};

/**
 * The HTTP response object
 *
 * In a dynamic route, this is variable is available in the callback function
 *
 * @name	Urbin.Webserverclient.response
 * @object
 * @public
 * @since	0.0.8a
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	client.response.setCode( 404 ).setContent( 'Not found!' ).setMime( 'html' );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.response
 * @see	Urbin.webserverclient.request
 * @see	Urbin.webserverclient.request.buffer
 * @see	Urbin.webserverclient.response.setContent
 * @see	Urbin.webserverclient.response.setCode
 * @see	Urbin.webserverclient.response.setMime
 */


static const JSClass jscWebserverclientresponse = {
	"Webserverclientresponse",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmWebserverclientresponse[ ] = {
	JS_FS( "setContent", 			JsnWebserverclientresponse_SetContent, 1, 0 ),
	JS_FS( "setCode", 				JsnWebserverclientresponse_SetCode, 1, 0 ),
	JS_FS( "setMime", 				JsnWebserverclientresponse_SetMime, 1, 0 ),
	JS_FS_END
};
/**
 * The HTTP request object
 *
 * In a dynamic route, this is variable is available in the callback function
 *
 * @name	Urbin.Webserverclient.request
 * @object
 * @public
 * @since	0.0.10
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	console.log( client.request.buffer );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.response
 * @see	Urbin.webserverclient.request
 * @see	Urbin.webserverclient.request.buffer
 * @see	Urbin.webserverclient.response.setContent
 * @see	Urbin.webserverclient.response.setCode
 * @see	Urbin.webserverclient.response.setMime
 */

static const JSClass jscWebserverclientrequest = {
	"Webserverclientresponse",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmWebserverclientrequest[ ] = {
	JS_FS_END
};

/**
 * The Ip address of the connected HTTP client
 *
 * @name	Urbin.Webserverclient.ip
 * @field
 * @public
 * @since	0.0.8a
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	console.log( "ip: " + client.response.ip );
 * 	console.log( "url: " + client.response.url );
 * 	console.log( "method: " + client.response.method );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.url
 * @see	Urbin.webserverclient.method
 * @see	Urbin.webserverclient.response.setContent
 * @see	Urbin.webserverclient.response.setCode
 * @see	Urbin.webserverclient.response.setMime
 */


/**
 * The url requested by of the connected HTTP client
 *
 * @name	Urbin.Webserverclient.url
 * @field
 * @public
 * @since	0.0.8a
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	console.log( 'ip: ' + client.response.ip );
 * 	console.log( 'url: ' + client.response.url );
 * 	console.log( 'method: ' + client.response.method );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.ip
 * @see	Urbin.webserverclient.method
 * @see	Urbin.webserverclient.response.setContent
 * @see	Urbin.webserverclient.response.setCode
 * @see	Urbin.webserverclient.response.setMime
 */

/**
 * The method [POST or GET ] that was requested by the connected HTTP client
 *
 * @name	Urbin.Webserverclient.method
 * @field
 * @public
 * @since	0.0.8a
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	console.log( "ip: " + client.response.ip );
 * 	console.log( "url: " + client.response.url );
 * 	console.log( "method: " + client.response.method );
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.ip
 * @see	Urbin.webserverclient.url
 * @see	Urbin.webserverclient.response.setContent
 * @see	Urbin.webserverclient.response.setCode
 * @see	Urbin.webserverclient.response.setMime
 */


static JSObject * Webserver_Route_ResultToJS( struct payload_t * payload, const struct webserverclient_t * webserverclient );
static JSObject * Webserver_Route_ResultToJS( struct payload_t * payload, const struct webserverclient_t * webserverclient ) {
	JSContext * cx;
	JSObject * webserverclientObj, * responseObj, * requestObj, * thisObj;
	JSString * jIp, * jUrl, * jMethod, * jBuffer; //  @TODO: split buffer into headers and content
	const char * ip, * url;
	const unsigned int attrs = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
	struct {unsigned char ip:1;
		unsigned char jip:1;
		unsigned char url:1;
		unsigned char jurl:1;
		unsigned char jbuffer:1;
		unsigned char method:1;
		unsigned char cli:1;
		unsigned char resp:1;
		unsigned char good:1;} cleanUp;
	memset( &cleanUp, 0, sizeof( cleanUp ) );

	webserverclientObj = requestObj = responseObj = NULL;
	jIp = jUrl = jMethod = jBuffer = NULL;
	ip = url = NULL;
	cx = payload->context;
	JSAutoRequest ar( cx );
	JSAutoCompartment ac( cx, payload->scopeObj );
	thisObj = payload->scopeObj;
	JS::RootedObject thisObjRoot( cx, thisObj );
	JS::HandleObject thisObjHandle( thisObjRoot );
	cleanUp.good = ( ( webserverclientObj = 	JS_InitClass( cx, thisObjHandle, JS::NullPtr( ), &jscWebserverclient, nullptr, 0, nullptr, jsmWebserverclient, nullptr, nullptr ) ) != NULL );
	JS::RootedObject	webserverclientObjRoot( cx, webserverclientObj );
	JS::HandleObject	webserverclientObjHandle( webserverclientObjRoot );
	if ( cleanUp.good ) {
		cleanUp.cli = 1;
		JS_SetPrivate( webserverclientObj, (void * ) webserverclient );
	}
	if ( cleanUp.good ) {
		cleanUp.resp = 1;
		cleanUp.good = ( ( ip = Webserverclient_GetIp( webserverclient ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.ip = 1;
		cleanUp.good = ( ( jIp = JS_NewStringCopyZ( cx, ip ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.jip = 1;
		cleanUp.good = ( ( url = Webserverclient_GetUrl( webserverclient ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.url = 1;
		cleanUp.good = ( ( jUrl = JS_NewStringCopyZ( cx, url ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.jurl = 1;
		cleanUp.good = ( ( jMethod = JS_NewStringCopyZ( cx, MethodDefinitions[webserverclient->mode] ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.method = 1;
		if ( webserverclient->buffer != NULL ) {
			cleanUp.good = ( ( jBuffer = JS_NewStringCopyN( cx, webserverclient->buffer->bytes, webserverclient->buffer->used ) ) != NULL );
		}
	}
	if ( cleanUp.good ) {
		cleanUp.jbuffer = 1;
		cleanUp.good = ( ( responseObj = 	JS_InitClass( cx, webserverclientObjHandle, JS::NullPtr( ), &jscWebserverclientresponse, nullptr, 0, nullptr, jsmWebserverclientresponse, nullptr, nullptr ) ) != NULL );
	}
	JS::RootedObject	responseObjRoot( cx, responseObj );
	JS::HandleObject	responseObjHandle( responseObjRoot );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( requestObj = 	JS_InitClass( cx, webserverclientObjHandle, JS::NullPtr( ), &jscWebserverclientrequest, nullptr, 0, nullptr, jsmWebserverclientrequest, nullptr, nullptr ) ) != NULL );
	}
	JS::RootedObject	requestObjRoot( cx, requestObj );
	JS::HandleObject	requestObjHandle( requestObjRoot );
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( webserverclientObjHandle, "method", STRING_TO_JSVAL( jMethod ) );
	}
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( webserverclientObjHandle, "ip", STRING_TO_JSVAL( jIp ) );
	}
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( webserverclientObjHandle, "url", STRING_TO_JSVAL( jUrl ) );
	}
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( webserverclientObjHandle, "request", OBJECT_TO_JSVAL( requestObj ) );
	}
	if ( cleanUp.good ) {
		JS_SetPrivate( requestObj, ( void * ) &webserverclient->buffer );
	}
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( requestObjHandle, "buffer", STRING_TO_JSVAL( jBuffer ) );
	}
	if ( cleanUp.good ) {
		SET_PROPERTY_ON( webserverclientObjHandle, "response", OBJECT_TO_JSVAL( responseObj ) );
	}
	if ( cleanUp.good ) {
		JS_SetPrivate( responseObj, ( void * ) &webserverclient->response );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.method ) {
			JS_free( cx, jMethod ); jMethod = NULL;
		}
		if ( cleanUp.jip ) {
			JS_free( cx, jIp ); jIp = NULL;
		}
		if ( cleanUp.jurl ) {
			JS_free( cx, jUrl ); jUrl = NULL;
		}
		if ( cleanUp.jbuffer ) {
			JS_free( cx, jBuffer ); jBuffer = NULL;
		}
	}
	// always cleanup
	if ( cleanUp.url ) {
		free( (char *) url ); url = NULL;
	}
	if ( cleanUp.ip ) {
		free( (char *) ip ); ip = NULL;
	}
	return webserverclientObj;
}

static void Webserver_Route_ResultHandler_cb( const struct webserverclient_t * webserverclient ) {
	struct payload_t * payload;
	JSObject * webserverClientObj;
	jsval webserverClientVal;

	payload = (struct payload_t *) webserverclient->route->cbArgs;
	JSAutoRequest 			ar( payload->context );
	JSAutoCompartment 		ac( payload->context, payload->scopeObj );
	webserverClientObj = Webserver_Route_ResultToJS( payload, webserverclient );
	JS::RootedObject webservClientObjRoot( payload->context, webserverClientObj );
	JS::RootedValue webservClientObjVal( payload->context, webserverClientVal );
	webserverClientVal = OBJECT_TO_JSVAL( webserverClientObj );
	payload->fnVal_cbArg = JS::Heap<JS::Value>( webserverClientVal );
	Payload_Call( payload );
}

/**
 * Add a dynamic route to the webserver.
 *
 * A javascript function will handle the request.
 *
 * @name	Urbin.Webserver.addRoute
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}				If the route could be registered successfully it returns true; else false is returned
 * @param	{string}		pattern	The regular expression that triggers a callback if there is a match.
 * @param	{function}		fn		The callback function {response}
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	console.log( 'got ' + client.url );
 * 	client.response.setContent = '<html><h1>response</h1><html>';
 * 	} );
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addDocumentRoot
 * @see	Urbin.webserverclient
 * @see	Urbin.webserverclient.setContent
 * @see	Urbin.webserverclient.setCode
 * @see	Urbin.webserverclient.setMime
 * @see	Urbin.webserverclient.response
 */
static bool JsnWebserver_AddDynamicRoute( JSContext * cx, unsigned argc, jsval * vp ) {
	struct webserver_t * webserver;
	struct payload_t * payload;
	JSObject * webserverObj;
	JSString * jPattern;
	JS::CallArgs args;
	jsval fnVal, dummyVal, paramVal;
	char * cPattern;
	struct {unsigned char payload:1;
		unsigned char pattern:1;
		unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	dummyVal = JSVAL_NULL;
	paramVal = JSVAL_NULL;
	cleanUp.good = ( argc == 2 );
	fnVal = args[1];
	cPattern = NULL;
	payload = NULL;
	webserverObj = JS_THIS_OBJECT( cx, vp );
	JS::RootedObject		webserverObjRoot( cx, webserverObj );
	JS::RootedValue 		fnValRoot( cx, fnVal );
	JS::HandleValue 		fnValHandle( fnValRoot );
	JS::MutableHandleValue 	fnValMut( &fnValRoot );
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertArguments( cx, args, "S*", &jPattern, &dummyVal ) == true );
	}
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
		paramVal = JSVAL_NULL;
		cleanUp.good = ( ( payload = Payload_New( cx, webserverObj, fnValMut.get( ), paramVal, false ) ) != NULL );
			}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		Webserver_DynamicHandler( webserver, cPattern, Webserver_Route_ResultHandler_cb, ( void * ) payload, Payload_Delete_Anon );
	} else {
		if ( cleanUp.payload ) {
			// this is the only thing that needs special care if something went wrong; we cleanup the rest any way,
			Payload_Delete( payload ); payload = NULL;
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
 * @name	Urbin.Webserver.addDocumentRoot
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{boolean}						If the route could be registered successfully it returns true; else false is returned
 * @param	{string}		pattern			The regular expression that triggers a lookup in the filesystem. Please not that this is a string and not a RegExp object. The expression must have exactly 1 group. This will act as the placeholder for the requested file.
 * @param	{string}		documentRoot	The folder name that acts as the document root for this webserver.<p>default: The value for 'document_root' in the http section of the configurationFile</p>
 *
 * @example
 * var ws = Urbin.Webserver( {ip: '10.0.0.25', port: 8888}, 60 );
 * ws.addDocumentRoot( '^/static/(.*)', '/var/www/static/' );
 *
 * @see	Urbin.Webserver
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.webserverclient.get
 */
static bool JsnWebserver_AddDocumentRoot( JSContext * cx, unsigned argc, jsval * vp ) {
	struct webserver_t * webserver;
	JSObject * webServerObj;
	JS::CallArgs args;
	JSString * jDocumentRoot, * jPattern;
	char * cDocumentRoot, * cPattern;
	struct {	unsigned char pattern:1;
		unsigned char documentRoot:1;
		unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	webServerObj = JS_THIS_OBJECT( cx, vp );
	args = CallArgsFromVp( argc, vp );
	jPattern = NULL;
	jDocumentRoot = NULL;
	cDocumentRoot = NULL;
	cPattern = NULL;

	cleanUp.good = ( JS_ConvertArguments( cx, args, "SS", &jPattern, &jDocumentRoot ) == true );
	if ( cleanUp.good ) {
		 cleanUp.good = ( ( cDocumentRoot = JS_EncodeString( cx, jDocumentRoot ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.documentRoot = 1;
		cleanUp.good = ( ( cPattern = JS_EncodeString( cx, jPattern ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.pattern = 1;
		cleanUp.good = ( ( webserver = (struct webserver_t *) JS_GetPrivate( webServerObj ) ) != NULL );
	}
	if ( cleanUp.good ) {
		Webserver_DocumentRoot( webserver, cPattern, cDocumentRoot );
	}
	if ( cleanUp.good ) {
		args.rval( ).setBoolean( true );
	} else {
		args.rval( ).setBoolean( false );
	}
	if ( cleanUp.documentRoot ) {
		JS_free( cx, cDocumentRoot ); cDocumentRoot = NULL;
	}
	if ( cleanUp.pattern ) {
		JS_free( cx, cPattern ); cPattern = NULL;
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
 * @name	Urbin.Webserver
 * @constructor
 * @public
 * @since	0.0.5b
 * @returns	{object}							The web server javascript
 * @param	{object}		params				The connection string.
 * @param	{string}		params.ip			The Ip address that the server will listen to.<p>default: The value for 'ip' in the webserver  section of the configurationFile.</p>
 * @param	{int}			params.port			The port that the server should bind to.<p>default: The value for 'port' in the webserver section of the configurationFile.</p>
 * @param	{integer}		[timeout]			The timeout for valid connections.<p>default: The value for 'timeout' in the webserver section of the configurationFile.</p>
 *
 * @example
 * var ws = Urbin.Webserver( { ip : '10.0.0.25', port : 8888 }, 60 );
 * ws.addRoute( '^/a$', function( client ) {
 * 	console.log( 'got ' + client.url );
 * 	client.response.setContent = '<html><h1>response</h1><html>';
 * 	} );
 * ws.addDocumentRoot( '^/static/(.*)', '/var/www/static/' );
 *
 * @see	Urbin.Webserver.addRoute
 * @see	Urbin.Webserver.addDocumentRoot
 * @see	Urbin.Webserverclient
 */
static bool JsnWebserver_Constructor( JSContext * cx, unsigned argc, jsval * vp ) {
	struct webserver_t * webserver;
	struct javascript_t * javascript;
	JSObject * connObj, * webserverObj;
	JS::CallArgs args;
	char * cServerIp;
	int port, timeoutSec;
	jsval value;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	value = JSVAL_NULL;
	timeoutSec = 0;
	port = 0;
	cServerIp = NULL;
	webserverObj = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "o/i", &connObj, &timeoutSec ) == true );
	JS::RootedObject connObjRoot( cx, connObj );
	JS::HandleObject connObjHandle( connObjRoot );
	JS::RootedValue valueRoot( cx, value );
	JS::MutableHandleValue valueMut( &valueRoot );
	if ( cleanUp.good ) {
		CONNOBJ_GET_PROP_STRING( "ip", cServerIp );
		CONNOBJ_GET_PROP_NR( "port", port );
	}
	JS::RootedObject thisObj( cx, JS_THIS_OBJECT( cx, vp ) );
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
		cleanUp.good = ( ( webserverObj = 	JS_NewObject( cx, &jscWebserver, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
	}
	JS::RootedObject webserverObjRoot( cx, webserverObj );
	JS::HandleObject webserverObjHandle( webserverObjRoot );
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_DefineFunctions( cx, webserverObjHandle, jsmWebserver ) == true );
	}
	if ( cleanUp.good ) {
		JS_SetPrivate( webserverObj, ( void * ) webserver );
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
 * Urbin javascript object.
 *
 * @name Urbin
 * @public
 * @namespace
 */
static bool JsnFunction_Stub( JSContext * cx, unsigned argc, jsval * vp );
static bool JsnFunction_Stub( JSContext * cx, unsigned argc, jsval * vp ) {
	return true;
}

/**
 * Shutdown the engine after a timeout.
 *:
 * This will stop the engine.
 *
 * @name	Urbin.shutdown
 * @function
 * @public
 * @since	0.0.5b
 * @returns	{null}
 * @param	{integer}		[timeout]	Time to wait before the shut down should start. Defaults to 1 second.
 *
 * @example
 *Urbin.shutdown( 10 );
 */
static bool JsnUrbin_Shutdown( JSContext * cx, unsigned argc, jsval * vp ) {
	struct javascript_t * javascript;
	JS::CallArgs args;
	int timeout;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	JS::RootedObject thisObj( cx, JS_THIS_OBJECT( cx, vp ) );
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

static const JSClass jscUrbin = {
	MAIN_OBJ_NAME,
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmUrbin[ ] = {
	JS_FS( "shutdown", 			JsnUrbin_Shutdown, 1, 0 ),
	JS_FS( "onLoad", 			JsnFunction_Stub, 0, 0 ),
	JS_FS( "onReady", 			JsnFunction_Stub, 0, 0 ),
	JS_FS( "onUnload", 			JsnFunction_Stub, 0, 0 ),
	JS_FS_END
};

/**
 * Os object.
 *
 * namespace for the operating system
 *
 * @name os
 * @public
 * @namespace
 */

/**
 * get an environment varibale.
 *
 * @name	os.getEnv
 * @function
 * @public
 * @since	0.0.8b
 * @returns	{string}
 * @param	{string}		name	variable name to lookup
 *
 * @example
 * os.getEnv( 'SHELL' );
 */
static bool JsnOs_GetEnv( JSContext * cx, unsigned argc, jsval * vp ) {
	JSString * jString, *jValueString;
	JS::CallArgs args;
	const char * cValue;
	char * cString;
	struct {unsigned char good:1;
			unsigned char cString:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	args.rval( ).setUndefined( );
	cString = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jString ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cString = JS_EncodeString( cx, jString ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.cString = 1;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cValue = getenv( cString ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( jValueString = JS_NewStringCopyZ( cx, cValue ) ) != NULL );
	}
	if ( cleanUp.good ) {
		args.rval( ).setString( jValueString );
	}
	if ( cleanUp.good ) {
		cleanUp.cString = 1;

	}
	if ( cleanUp.cString ) {
		JS_free( cx, cString ); cString = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Write a string to a file.
 *
 * @name os.writefile
 * @public
 * @static
 * @function
 * @since	0.0.8a
 *
 * @param {string} 		filename 	Filename to write to. If the filename is null, then a temporary file will be created. e.g /tmp/urbinXXXXX
 * @param {string} 		content 	The content that will be written to the file
 * @param {boolean} 	append 	Append to the file: False: create a new file. True: appends if the file exists, else it creates a new one:
 * @returns {string} 	The filename on success, false on failure, null on incorrect parameters
 * @see os
 * @see os.readfile
 *
 * @example
 * var content = os.writefile( '/tmp/dummy.txt', 'blabla', true );
 */
static bool JsnOs_WriteFile( JSContext * cx, unsigned argc, jsval * vp ) {
	struct javascript_t * javascript;
	JSObject * thisObj;
	JS::CallArgs args;
	JSString *jFileName, * jContent;
	bool append;
	char *cContent;
	char *cFileName;
	char mode[3] = {'w', 'b', '+'};
	struct stat sb;
	FILE *fOut;
	char * tFileName;
	int rc = 0, temp;
	struct {unsigned char good:1;
			unsigned char cFileName:1;
			unsigned char tFileName:1;
			unsigned char content:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	temp = 0;
	cContent = NULL;
	cFileName = NULL;
	tFileName = NULL;
	args = CallArgsFromVp( argc, vp );
	args.rval( ).setUndefined( );
	thisObj = JS_THIS_OBJECT( cx, vp );
	javascript = (struct javascript_t *) JS_GetPrivate( thisObj );
	cleanUp.good = ( argc >= 2 );
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertArguments( cx, args, "SS/b", &jFileName, &jContent, &append ) == true );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cContent = JS_EncodeString( cx, jContent ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.content = 1;
		if ( args[0].isUndefined( ) ) {
			temp = 1;
		} else if ( args[0].isString( ) ) {
			if ( JS_GetStringLength( jFileName ) == 0 ) {
				temp = 1;
			} else {
				cleanUp.good = ( ( cFileName = JS_EncodeString( cx, jFileName ) ) != NULL );
				if ( cleanUp.good ) {
					cleanUp.cFileName = 1;
				}
			}
		}
		if ( temp ) {
			cleanUp.good = ( ( tFileName = (char *) malloc( strlen( MAIN_OBJ_NAME ) + 5 + 1 + 6 ) ) != NULL );
			if ( cleanUp.good ) {
				cleanUp.tFileName = 1;
				snprintf( tFileName, 15, "/tmp/%-5s/XXXXX", MAIN_OBJ_NAME );
				cFileName = tFileName;
			}
		}
	}
	if ( cleanUp.good ) {
		if ( append == true && ( stat( cFileName, &sb ) == 0 && S_ISREG( sb.st_mode ) ) ) {
			mode[0] = 'a';
		}
		cleanUp.good = ( ( fOut = fopen( cFileName, mode ) ) != NULL );
	}
	if ( cleanUp.good ) {
		Core_Log( javascript->core, LOG_INFO, __FILE__, __LINE__, "Writing file" );
		if ( fputs ( cContent, fOut ) != EOF ) {
			rc = 1;
		}
		if ( fclose( fOut ) == EOF ) {
			rc = 0;
		}
	}
	if ( cleanUp.good ) {
		if ( rc > 0 || ( rc == 0 && *cContent == '\0' ) ) {
			if ( temp ) {
				cleanUp.good = ( ( jFileName = JS_NewStringCopyZ( cx, cFileName ) ) != NULL );
			}
		}
	}
	if ( cleanUp.good ) {
		args.rval( ).setString( jFileName );
	} else {
		args.rval( ).setBoolean( false );
	}
	if ( cleanUp.tFileName ) {
		free( cFileName ); cFileName = NULL;
	}
	if ( cleanUp.cFileName ) {
		JS_free( cx, cFileName ); cFileName = NULL;
	}
	if ( cleanUp.content ) {
		JS_free( cx, cContent ); cContent = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Get the content of a file.
 *
 * @name os.readfile
 * @function
 * @public
 * @static
 * @ignore
 * @since	0.0.8a
 *
 * @param {string}		filename 	The filename to read
 * @returns {string}		content	The content of the file or NULL
 * @see os
 * @see os.writefile
 *
 * @example
 * var content = os.readfile( '/etc/hosts' );
 */
static bool JsnOs_ReadFile( JSContext * cx, unsigned argc, jsval * vp ) {
	struct javascript_t * javascript;
	JSObject * thisObj;
	JSString *jFileName, * jContent;
	char *cFileName;
	char *content;
	JS::CallArgs args;
	struct {unsigned char good:1;
			unsigned char fileName:1;
			unsigned char contents:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	args.rval( ).setUndefined( );
	cFileName = NULL;
	content = NULL;
	thisObj = JS_THIS_OBJECT( cx, vp );
	javascript = (struct javascript_t *) JS_GetPrivate( thisObj );
	cleanUp.good = ( argc == 1 );
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jFileName ) == true );
	}
	if ( cleanUp.good )  {
		cleanUp.good = ( ( cFileName = JS_EncodeString( cx, jFileName ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.fileName = 1;
		cleanUp.good = ( fopen( cFileName, "r" ) != NULL );
	}
	if ( cleanUp.good ) {
		Core_Log( javascript->core, LOG_INFO, __FILE__, __LINE__, "Reading file" );
		cleanUp.good = ( ( content = FileGetContents( cFileName ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.contents = 1;
		cleanUp.good = ( ( jContent = JS_NewStringCopyZ( cx, content ) ) != NULL );
	}
	if ( cleanUp.good ) {
		args.rval( ).setString( jContent );
	}
	if ( cleanUp.contents ) {
		free( content ); content = NULL;
	}
	if ( cleanUp.fileName ) {
		JS_free( cx, cFileName ); cFileName = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Create a directory, the permissions are set to 0770
 *

 * @name: os.mkdir
 * @function
 * @static
 * @public
 * @since 0.0.10
 *
 * @param {string} directory The directory to create
 * @returns {null|boolean}  undefined: if the parameters were incorrect, false if the mkdir failed or true if the directory was created
 *
 * @example:
 * var r = os.mkdir( '/tmp/tst' );
 * @see os.rm
 */
static bool JsnOs_Mkdir( JSContext *cx, unsigned argc, jsval * vp ) {
	JSString * jDir;
	JS::CallArgs args;
	char * cDir;
	const mode_t mode = 0770;
	struct {unsigned char good:1;
			unsigned char dir:1;
			} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	args.rval( ).setUndefined( );
	cDir = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jDir ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cDir = JS_EncodeString( cx, jDir ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.dir = 1;
		if ( mkdir( cDir, mode ) == 0 ) {
			args.rval( ).setBoolean( true );
		} else {
			args.rval( ).setBoolean( false );
		}
	}
	if ( cleanUp.dir ) {
		free( cDir ); cDir = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Remove a file or directory
 *

 * @name: os.rm
 * @function
 * @static
 * @public
 * @since 0.0.10
 *
 * @param {string} filename The filename or directory name
 * @returns {null|boolean}  undefined: if the parameters were incorrect, false if the delete failed or true if the delete was successfull
 *
 * @example:
 * var r = os.rm( '/tmp/dummy.txt' );
 * @see os.mkdir
 */
static bool JsnOs_Rm( JSContext *cx, unsigned argc, jsval * vp ) {
	JSString * jFileName;
	JS::CallArgs args;
	char * cFileName;
	struct {unsigned char good:1;
			unsigned char name:1;
			} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	args.rval( ).setUndefined( );
	cFileName = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jFileName ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cFileName = JS_EncodeString( cx, jFileName ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.name = 1;
		if ( remove( cFileName ) == 0 ) {
			args.rval( ).setBoolean( true );
		} else {
			args.rval( ).setBoolean( false );
		}
	}
	if ( cleanUp.name ) {
		free( cFileName ); cFileName = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Creates a symbolic link named newpath which contains the string oldpath.
 *

 * @name: os.symlink
 * @function
 * @static
 * @public
 * @since 0.0.10
 *
 * @param {string} oldpath The filename or directory name
 * @param {string} newpath The symlink name. If this existed, it will be overwritten
 * @returns {null|boolean}  undefined: if the parameters were incorrect, false if the symlink system call failed or true if the call was successfull
 *
 * @example:
 * var r = os.symlink( '/tmp/org.txt', '/tmp/link.txt' );
 * @see os.mkdir
 * @see os.rmk
 */
static bool JsnOs_Symlink( JSContext *cx, unsigned argc, jsval * vp ) {
	JSString * jFileNameNew, * jFileNameOld;
	JS::CallArgs args;
	char * cFileNameNew, * cFileNameOld;
	struct {unsigned char good:1;
			unsigned char nameOld:1;
			unsigned char nameNew:1;
			} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	args.rval( ).setUndefined( );
	cFileNameNew = cFileNameOld = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "SS", &jFileNameOld, &jFileNameNew ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cFileNameOld = JS_EncodeString( cx, jFileNameOld ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.nameOld = 1;
		cleanUp.good = ( ( cFileNameNew = JS_EncodeString( cx, jFileNameNew ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.nameNew = 1;
		if ( symlink( cFileNameOld, cFileNameNew ) == 0 ) {
			args.rval( ).setBoolean( true );
		} else {
			args.rval( ).setBoolean( false );
		}
	}
	if ( cleanUp.nameOld ) {
		free( cFileNameOld ); cFileNameOld = NULL;
	}
	if ( cleanUp.nameNew ) {
		free( cFileNameNew ); cFileNameNew = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

/**
 * Get a directory listing, with out the '.' and '..'
 *

 * @name: os.dir
 * @function
 * @static
 * @public
 * @since 0.0.10
 *
 * @param {string} pathname The directory name
 * @returns {null|boolean|Array}  undefined: if the parameters were incorrect, false if the directory could not be read or an array with files
 *
 * @example:
 * var dir = os.dir( '/tmp' );
 * for ( var i = 0; i < dirEntries.length; i++ {}
 * 	console.log( "file: " + dir[i].name + " size: " + dir[i].size + " type: " + dir[i].type );
 * }
 * @see os.mkdir
 */
static bool JsnOs_Dir( JSContext *cx, unsigned argc, jsval * vp ) {
	JSString * jDirName, * jstr;
	JS::CallArgs args;
	JSObject * recordObj;
	jsval value, currentVal, jValue;
	char * cDirName, * cFullFileName;
	int size;
	const char * cType;
	unsigned int rowId;
	size_t pathLen, fullLen;
	DIR * dp;
	struct stat sb;
	struct dirent * dptr;
	struct {unsigned char good:1;
			unsigned char dirName:1;
			unsigned char stat:1;
			unsigned char fileName:1;
			} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
	args.rval( ).setUndefined( );
	dp = NULL;
	dptr = NULL;
	cDirName = cFullFileName = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jDirName ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cDirName = JS_EncodeString( cx, jDirName ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.dirName = 1;
		pathLen = strlen( cDirName );
		if ( ( dp = opendir( cDirName ) ) != NULL ) {
			JSObject * resultArray;
			cleanUp.good = ( ( resultArray = JS_NewArrayObject( cx, 0 ) ) != NULL );
			JS::RootedObject resultArrayRoot( cx, resultArray );
			JS::HandleObject resultArrayHandle( resultArrayRoot );
			rowId = 0;
			while ( cleanUp.good &&  NULL != ( dptr = readdir( dp ) ) ) {
				cFullFileName = NULL;
				cleanUp.good = ( ( recordObj = JS_NewObject( cx, NULL, JS::NullPtr( ), JS::NullPtr( ) ) ) != NULL );
				JS::RootedObject 		recordObjRoot( cx, recordObj );
				JS::HandleObject 		recordObjHandle( recordObjRoot );
				if ( cleanUp.good ) {
					currentVal = OBJECT_TO_JSVAL( recordObj );
					JS::RootedValue 	currentValRoot( cx, currentVal );
					JS::HandleValue 	currentValHandle( currentValRoot );
					JS_SetElement( cx, resultArrayHandle, rowId, currentValHandle );
					cleanUp.good = ( ( jstr = JS_NewStringCopyZ( cx, dptr->d_name ) ) != NULL );
					if ( cleanUp.good ) {
						jValue = STRING_TO_JSVAL( jstr );
						JS::RootedValue	jValueRoot( cx, jValue );
						JS::HandleValue	jValueHandle( jValueRoot );
						JS_SetProperty( cx, recordObjHandle, "name", jValueHandle );
						fullLen = pathLen + strlen( dptr->d_name );
						cleanUp.good = ( ( cFullFileName = (char *) malloc( fullLen + 2) ) != NULL );
					}
					if ( cleanUp.good )  {
						cleanUp.fileName = 1;
						if ( cDirName[ pathLen - 1 ] == PATHSEP ) {
							snprintf( cFullFileName, fullLen + 1, "%s%s", cDirName, dptr->d_name );
						} else {
							snprintf( cFullFileName, fullLen + 2, "%s%c%s", cDirName, PATHSEP, dptr->d_name );
						}
						cleanUp.stat = ( stat( cFullFileName, &sb ) == 0 );
						if ( ! cleanUp.stat )  {
							size = 0;
							cType = "dangling symlink";
						} else {
							size = sb.st_size;
							switch ( sb.st_mode & S_IFMT ) {
								case S_IFBLK:  cType = "block device";		break;
								case S_IFCHR:  cType = "character device";	break;
								case S_IFDIR:  cType = "directory";			break;
								case S_IFIFO:  cType = "FIFO/pipe";			break;
								case S_IFLNK:  cType = "symlink";			break;
								case S_IFREG:  cType = "regular file";		break;
								case S_IFSOCK: cType = "socket";			break;
								default:       cType = "unknown";			break;
							}
						}
					}
					if ( cleanUp.good ) {
						jValue = INT_TO_JSVAL( size );
						JS::RootedValue	jValueRoot( cx, jValue );
						JS::HandleValue	jValueHandle( jValueRoot );
						JS_SetProperty( cx, recordObjHandle, "size", jValueHandle );
						cleanUp.good = ( ( jstr = JS_NewStringCopyZ( cx, cType ) ) != NULL );
					}
					if ( cleanUp.good ) {
						jValue = STRING_TO_JSVAL( jstr );
						JS::RootedValue	jValueRoot( cx, jValue );
						JS::HandleValue	jValueHandle( jValueRoot );
						JS_SetProperty( cx, recordObjHandle, "type", jValueHandle );
					}
					if ( cleanUp.fileName ) {
						cleanUp.fileName = 0;
						free( cFullFileName ); cFullFileName = NULL;
					}
				}
				rowId++;
			}
			closedir( dp );
			value = OBJECT_TO_JSVAL( resultArray );
			JS::RootedValue valueRoot( cx, value );
			args.rval( ).set( value );
		} else {
			args.rval( ).setBoolean( false );
		}
	}
	if ( cleanUp.dirName ) {
		free( cDirName ); cDirName = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}


/**
 * execute a command via system call
 * Please note that this function call is blocking!
 *

 * @name: os.system
 * @function
 * @static
 * @public
 * @deprecated: // @TODO:  future versions will probably run in a seperate thread
 * @since 0.0.8a
 *
 * @param {string} exec The full path to the executable. This must exist and executable
 * @param {string} paramstring Parameters
 * @returns {null|boolean}  undefined: if the parameters were incorrect, false if execute could not start or true if if launched successfull
 *
 * @example:var r = os.exec( '/usr/bin/wget', 'http://www.verpeteren.nl -o /tmp/www.verpeteren.nl.html' );
 * console.log( r );
 */
static bool JsnOs_System( JSContext *cx, unsigned argc, jsval * vp ) {
	struct javascript_t * javascript;
	JSObject * thisObj;
	JSString * jParams, * jExec;
	JS::CallArgs args = CallArgsFromVp( argc, vp );
	char * cParams, * cExec, * cmd;
	int rExec;
	size_t execLen, paramLen, len;
	struct stat sb;
	struct {unsigned char good:1;
			unsigned char params:1;
			unsigned char cmd:1;
			unsigned char exec:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	len = 0;
	execLen = 0;
	rExec = 0;
	cParams = NULL;
	cExec = NULL;
	cmd = NULL;
	args.rval( ).setUndefined( );
	thisObj = JS_THIS_OBJECT( cx, vp );
	javascript = (struct javascript_t *) JS_GetPrivate( thisObj );
	cleanUp.good = ( JS_ConvertArguments( cx, args, "SS", &jExec, &jParams ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cExec = JS_EncodeString( cx, jExec ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.exec = 1;
		cleanUp.good = ( ( cParams = JS_EncodeString( cx, jParams ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.params = 1;
		cleanUp.good = ( getuid( ) != 0 );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( stat( cExec, &sb ) >= 0 && ( sb.st_mode & S_IXUSR ) );
	}
	if ( cleanUp.good ) {
		paramLen = strlen( cParams );
		execLen = strlen( cExec );
		len = 1 + 1 + execLen + paramLen;
		cleanUp.good = ( ( cmd = (char *) malloc( len ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.cmd = 1;
		snprintf( cmd, len, "%s %s", cExec, cParams );
		Core_Log( javascript->core, LOG_INFO, __FILE__, __LINE__, "Executing command" );
		rExec = system( cmd );
		if ( rExec == -1 ) {
			args.rval( ).setBoolean( false );
		} else {
			args.rval( ).set( INT_TO_JSVAL( rExec ) );
		}
	}
	if ( cleanUp.cmd ) {
		free( cmd ); cmd = NULL;
	}
	if ( cleanUp.params ) {
		JS_free( cx, cParams ); cParams = NULL;
	}
	if ( cleanUp.exec ) {
		JS_free( cx, cExec ); cExec = NULL;
	}
	return ( cleanUp.good ) ? true : false;
}

static void Payload_Dns_ResultHandler_cb( struct dns_cb_data * dnsData ) {
	struct javascript_t * javascript;
	struct payload_t * payload;
	jsval ipValue;
	JSString * jIp;
	char * cIp;
	struct {unsigned char good:1;
			unsigned char ip:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	payload = ( struct payload_t * ) dnsData->context;
	cleanUp.good = ( ( cIp = DnsData_ToString( dnsData ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.ip = 1;
	}
	JSAutoRequest 			ar( payload->context );
	JSAutoCompartment 		ac( payload->context, payload->scopeObj );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( jIp = JS_NewStringCopyZ( payload->context, cIp ) ) != NULL );
	}
	if ( cleanUp.good ) {
		ipValue = STRING_TO_JSVAL( jIp );
	} else {
		ipValue = JSVAL_VOID;
	}
	JS::RootedValue ipRoot( payload->context, ipValue );
	payload->fnVal_cbArg = JS::Heap<JS::Value>( ipValue );
	Payload_Call( payload );
	// Now that we're done me must stop the callback loop also
	javascript = (struct javascript_t *) JS_GetPrivate( payload->scopeObj );
	javascript->core->dns.actives--;
	//  always cleanup
	Payload_Delete( payload ); payload = NULL;
	free( cIp ); cIp = NULL;
}

/**
 * Get the ip address of a host.
 *
 * @name os.getHostByName
 * @function
 * @public
 * @since 0.0.9
 * @static
 *
 * @param {string} hostname 		The hostname
 * @param	{function}	fn			A Javascript function that will be called after a delay.
 * @param	{string}	fn.ip		The found ip address or null.
 * @returns {null|boolean} 			undefined: if the parameters were incorrect, true if the dns lookup started
 *
 * @example
 * os.getHostByName( 'www.urbin.info', function( ip ) {
 * if ( ip ) {
 * 	console.log( 'Resolved: ' + ip );
 * } else {
 * 	console.log( 'Could not resolve host' );
 * 	}
 * } );
 *
 * @see os
 */
static bool JsnOs_GetHostByName( JSContext *cx, unsigned argc, jsval * vp ) {
	struct javascript_t * javascript;
	struct payload_t * payload;
	JSString * jHostName;
	JSObject * globalObj, * thisObj;
	JS::CallArgs args;
	jsval fnVal, dummyVal;
	char * cHostName;
	struct {	unsigned char payload:1;
				unsigned char dns:1;
				unsigned char hostName:1;
				unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, (jsval *) vp );
	dummyVal = JSVAL_NULL;
	fnVal = JSVAL_NULL;
	payload = NULL;
	cHostName = NULL;
	cleanUp.good = ( argc == 2 );
	if ( cleanUp.good ) {
		fnVal = args[1];
	}
	JS::RootedValue 		fnValRoot( cx, fnVal );
	JS::HandleValue 		fnValHandle( fnValRoot );
	JS::MutableHandleValue	fnValMut( &fnValRoot );
	thisObj = JS_THIS_OBJECT( cx, (jsval *) vp );
	JS::RootedObject thisObjRoot( cx, thisObj );
	globalObj = JS_GetGlobalForObject( cx, &args.callee( ) );
	javascript = (struct javascript_t *) JS_GetPrivate( globalObj );
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertArguments( cx, args, "Sf", &jHostName, &dummyVal ) == true );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cHostName = JS_EncodeString( cx, jHostName ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.hostName = 1;
		cleanUp.good = ( JS_ConvertValue( cx, fnValHandle, JSTYPE_FUNCTION, fnValMut ) == true );
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( payload = Payload_New( cx, thisObj, fnValMut.get( ), JSVAL_NULL, false ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		Core_GetHostByName( javascript->core, cHostName, Payload_Dns_ResultHandler_cb, (void *) payload );
	}
	if ( cleanUp.good ) {
		args.rval( ).setBoolean( true );
	} else {
		if ( cleanUp.payload ) {
			Payload_Delete( payload ); payload = NULL;
		}
		args.rval( ).setUndefined( );
	}
	//  always cleanup
	js_free( cHostName ); cHostName = NULL;
	return ( cleanUp.good ) ? true : false;

}


static const JSClass jscOs = {
	"os",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsmOs[ ] = {
	JS_FS( "getEnv", 			JsnOs_GetEnv, 1, 0 ),
	JS_FS( "readFile", 			JsnOs_ReadFile, 1, 0 ),
	JS_FS( "writeFile",			JsnOs_WriteFile, 2, 0 ),
	JS_FS( "mkdir",				JsnOs_Mkdir, 1, 0 ),
	JS_FS( "rmdir",				JsnOs_Rm, 1, 0 ),
	JS_FS( "symlink",			JsnOs_Symlink, 2, 0 ),
	JS_FS( "dir",				JsnOs_Dir, 1, 0 ),
	JS_FS( "system",			JsnOs_System, 1, 0 ),
	JS_FS( "getHostByName",		JsnOs_GetHostByName, 2, 0 ),
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
static bool JsnConsole_Log( JSContext * cx, unsigned argc, jsval * vp ) {
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
	args = CallArgsFromVp( argc, vp );
	args.rval( ).setUndefined( );
	cString = NULL;
	fileName = NULL;
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jString ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cString = JS_EncodeString( cx, jString ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.cstring = 1;
		consoleObj = JS_THIS_OBJECT( cx, vp );
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

/**
 * Load an other javascript file.
 *
 * Files will be interpreted only upon loading. Once the file has been loaded, any changes to the file will be ignored. The files will be relative to the 'javascript/scripts_path' configuration settings.
 * Please note that the files must be marked executable by the user. See also the loop_run_as_user and loop_run_as_group configuration settings.
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

static bool JsnGlobal_Include( JSContext * cx, unsigned argc, jsval * vp ) {
	struct javascript_t * javascript;
	JSString * jFile;
	JSObject * globalObj;
	JS::CallArgs args;
	char * cFile;
	bool success;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cFile = NULL;
	args = CallArgsFromVp( argc, vp );
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


static struct payload_t * Payload_New( JSContext * cx, JSObject * object, jsval fnVal_cb, jsval fnVal_cbArg, const bool repeat ) {
	struct payload_t * payload;
	struct {unsigned char good:1;
		unsigned char payload:1;
		unsigned char args:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	payload = NULL;
	cleanUp.good = ( ( payload = (struct payload_t *) malloc( sizeof( *payload ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		payload->context = cx;
		payload->scopeObj = JS::Heap<JSObject *>( object );
		payload->fnVal_cb = JS::Heap<JS::Value>( fnVal_cb );
		payload->fnVal_cbArg =  JS::Heap<JS::Value>( fnVal_cbArg );
		payload->repeat = repeat;
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.payload ) {
			payload->context = NULL;
			payload->scopeObj = NULL;
			payload->fnVal_cb = JSVAL_NULL;
			payload->fnVal_cbArg = JSVAL_NULL;
			payload->repeat = 0;
			free( payload ); payload = NULL;
		}
	}
	return payload;
}

static int Payload_Call( const struct payload_t * payload ) {
	if ( payload != NULL ) {
		jsval retVal;
		retVal = JSVAL_NULL;
		JSAutoRequest 			ar( payload->context );
		JSAutoCompartment 		ac( payload->context, payload->scopeObj );
		JS::RootedObject objRoot( payload->context, payload->scopeObj );
		JS::HandleObject objHandle( objRoot );
		JS::RootedValue fnValCbRoot( payload->context, payload->fnVal_cb );
		JS::HandleValue fnValHandle( fnValCbRoot );
		JS::RootedValue fnValCbArgRoot( payload->context, payload->fnVal_cbArg );
		JS::HandleValueArray fnValArrayHandle( fnValCbArgRoot );
		JS::RootedValue retValRoot( payload->context, retVal );
		JS::MutableHandleValue retValMut( &retValRoot );
		JS_CallFunctionValue( payload->context, objHandle, fnValHandle, fnValArrayHandle, retValMut );
		// the payload is cleaned-up by automatically, spawned by Core_ProcessTick and clearFuncCb
		// We don't need to clear the payload, because that is needed also the next time that this route will be called
	}
	return ( payload->repeat ) ? 1 : 0;
}

static int Payload_Timing_ResultHandler_cb( void * cbArgs ) {
	struct payload_t * payload;

	payload = ( struct payload_t * ) cbArgs;

	return Payload_Call( payload );
}

static void Payload_Delete_Anon( void * cbArgs ) {
	Payload_Delete( (struct payload_t *) cbArgs );
}

static void Payload_Delete( struct payload_t * payload ) {
	payload->context = NULL;
	payload->scopeObj = NULL;
	payload->fnVal_cb = JSVAL_NULL;
	payload->fnVal_cbArg = JSVAL_NULL;
	payload->repeat = 0;
	free( payload ); payload = NULL;
}

#define SET_TIMER( repeat ) do { \
	struct javascript_t * javascript; \
	struct payload_t * payload; \
	struct timing_t * timing; \
	JSObject * globalObj, * thisObj; \
	JS::CallArgs args; \
	jsval fnVal, * argsAt2, dummyVal; \
	int ms; \
	struct {	unsigned char payload:1; \
				unsigned char timer:1; \
				unsigned char good:1;} cleanUp; \
	 \
	memset( &cleanUp, 0, sizeof( cleanUp ) ); \
	args = CallArgsFromVp( argc, (jsval *) vp ); \
	dummyVal = JSVAL_NULL; \
	fnVal = args[0]; \
	JS::RootedValue 		fnValRoot( cx, fnVal ); \
	JS::HandleValue 		fnValHandle( fnValRoot ); \
	JS::MutableHandleValue	fnValMut( &fnValRoot ); \
	payload = NULL; \
	timing = NULL; \
	thisObj = JS_THIS_OBJECT( cx, (jsval *) vp ); \
	JS::RootedObject thisObjRoot( cx, thisObj ); \
	globalObj = JS_GetGlobalForObject( cx, &args.callee( ) ); \
	javascript = (struct javascript_t *) JS_GetPrivate( globalObj ); \
	cleanUp.good = ( JS_ConvertArguments( cx, args, "fi", &dummyVal, &ms ) == true ); \
	if ( cleanUp.good ) { \
		cleanUp.good = ( JS_ConvertValue( cx, fnValHandle, JSTYPE_FUNCTION, fnValMut ) == true ); \
	} \
	if ( cleanUp.good ) { \
		if ( args.length( ) > 2 ) { \
			argsAt2 = ( (jsval *) vp ) + 2; \
			cleanUp.good = ( ( payload = Payload_New( cx, thisObj, fnValMut.get( ), *argsAt2, false ) ) != NULL ); \
		} else { \
			cleanUp.good = ( ( payload = Payload_New( cx, thisObj, fnValMut.get( ), JSVAL_NULL, false ) ) != NULL ); \
		} \
	} \
	if ( cleanUp.good ) { \
		cleanUp.payload = 1; \
		cleanUp.good = ( ( timing = Core_AddTiming( javascript->core, (unsigned int) ms, repeat, Payload_Timing_ResultHandler_cb, (void * ) payload, Payload_Delete_Anon ) ) != NULL ); \
	} \
	if ( cleanUp.good ) { \
		cleanUp.timer = 1; \
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
 * 	console.log( 'Foo : ' + a + ' Bar : ' + b );
 * }, 3000, 'foo', 'bar' );
 * ClearTimeout( timeoutId );
 *
 * @see	setInterval
 * @see	clearTimeout
 * @see	clearInterval
 */

static bool JsnGlobal_SetTimeout( JSContext * cx, unsigned argc, jsval * vp ) {
	SET_TIMER( 0 );
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
 * 	console.log( 'Foo : ' + a + ' Bar : ' + b );
 * }, 3000, 'foo', 'bar' );
 * clearInterval( timeoutId );
 *
 * @see	setTimeout
 * @see	clearTimeout
 * @see	clearInterval
 */

static bool JsnGlobal_SetInterval( JSContext * cx, unsigned argc, jsval * vp ) {
	SET_TIMER( 1 );
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
 * 	console.log( 'Foo : ' + a + ' Bar : ' + b );
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
 * 	console.log( 'Foo : ' + a + ' Bar : ' + b );
 * }, 3000, 'foo', 'bar' );
 * clearInterval( timeoutId );
 *
 * @see	setInterval
 * @see	setTimeout
 * @see	clearTimeout
 */
static bool JsnGlobal_ClearTimeout( JSContext * cx, unsigned argc, jsval * vp ) {
	struct javascript_t * javascript;
	unsigned int identifier;
	JSObject * globalObj;
	JS::CallArgs args;
	struct {unsigned char good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vp );
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
	struct stat sb;
	size_t len;
	struct {unsigned char good:1;
			unsigned char script:1;
			unsigned char bytecode:1;
			unsigned char fn:1; } cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( script = (struct script_t *) malloc( sizeof( *script ) ) ) != NULL );
	len = strlen( cFile ) + strlen( javascript->path ) + 2;
	if ( cleanUp.good ) {
		cleanUp.script = 1;
		script->bytecode = NULL;
		PR_INIT_CLIST( &script->mLink );
		cleanUp.good = ( ( script->fileNameWithPath = (char *) malloc( len ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.fn = 1;
		snprintf( script->fileNameWithPath, len, "%s%c%s", javascript->path, PATHSEP, cFile );
		cleanUp.good = ( stat( script->fileNameWithPath, &sb ) >= 0 && S_ISREG( sb.st_mode ) && ( sb.st_mode & S_IXUSR ) );
	}
	if ( cleanUp.good ) {
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
		Core_Log( javascript->core, LOG_INFO, cFile ,0, "New script loaded" );
	} else {
		Core_Log( javascript->core, LOG_INFO, cFile, 0, "New script could not be loaded" );
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

	JSObject * osObj;
	osObj = 	JS_InitClass( javascript->context, globalObjHandle, JS::NullPtr( ), &jscOs, nullptr, 0, nullptr, jsmOs, nullptr, nullptr );
	JS::RootedObject 			osObjRoot( javascript->context, osObj );
	JS_SetPrivate( osObj, (void * ) javascript );


	JSObject * urbinObj;
	urbinObj = 	JS_InitClass( javascript->context, globalObjHandle, JS::NullPtr( ), &jscUrbin, nullptr, 0, nullptr, jsmUrbin, nullptr, nullptr );
	JS::RootedObject			urbinObjRoot( javascript->context, urbinObj );
	JS::HandleObject			urbinObjHandle( urbinObjRoot );
	JS_SetPrivate( urbinObj, (void * ) javascript );

	JSObject * webclientObj;
	webclientObj = 	JS_InitClass( javascript->context, urbinObjHandle, JS::NullPtr( ), &jscWebclient, JsnWebclient_Constructor, 1, nullptr, jsmWebclient, nullptr, nullptr );
	JS::RootedObject webclientObjRoot( javascript->context, webclientObj );

	JSObject * webserverObj;
	webserverObj = 	JS_InitClass( javascript->context, urbinObjHandle, JS::NullPtr( ), &jscWebserver, JsnWebserver_Constructor, 1, nullptr, jsmWebserver, nullptr, nullptr );
	JS::RootedObject webserverObjRoot( javascript->context, webserverObj );


#if HAVE_MYSQL == 1
	JSObject * mysqlObj;
	mysqlObj = JS_InitClass( javascript->context, urbinObjHandle, JS::NullPtr( ), &jscMysqlclient, JsnMysqlclient_Constructor, 1, nullptr, jsmMysqlclient, nullptr, nullptr );
	JS::RootedObject mysqlObjRoot( javascript->context, mysqlObj );
#endif
	JSObject * postgresqlObj;
	postgresqlObj =  JS_InitClass( javascript->context, urbinObjHandle, JS::NullPtr( ), &jscPostgresqlclient, JsnPostgresqlclient_Constructor, 1, nullptr, jsmPostgresqlclient, nullptr, nullptr );
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
		if ( PATHSEP == *lastChar ) {
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
		Core_Log( javascript->core, LOG_INFO, __FILE__ , __LINE__, "New Javascript allocated" );
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
			PR_INSERT_BEFORE( &script->mLink, &javascript->scripts->mLink );
		}
		Core_Log( javascript->core, LOG_INFO, __FILE__ , __LINE__, "New Script allocated" );
	}
	if ( ! cleanUp.good ) {
		if ( cleanUp.script ) {
				Script_Delete( script ); script = NULL;
		}
	}

	return script;
}

static void Javascript_DelScript( struct javascript_t * javascript, struct script_t * script ) {
	struct script_t * scriptNext;
	PRCList * next;

	if ( PR_CLIST_IS_EMPTY( &script->mLink ) ) {
		javascript->scripts = NULL;
	} else {
		next = PR_NEXT_LINK( &script->mLink );
		scriptNext = FROM_NEXT_TO_ITEM( struct script_t );
		javascript->scripts = scriptNext;
	}
	PR_REMOVE_AND_INIT_LINK( &script->mLink );
	Script_Delete( script ); script = NULL;
	Core_Log( javascript->core, LOG_INFO, __FILE__ , __LINE__, "Delete Script free-ed" );
}

void Javascript_Delete( struct javascript_t * javascript ) {
	struct script_t * firstScript;

	firstScript = javascript->scripts;
	while ( firstScript != NULL ) {
		Javascript_DelScript( javascript, firstScript );
		firstScript = javascript->scripts;
	}
	JS_DestroyContext( javascript->context );
	JS_DestroyRuntime( javascript->runtime );
	if ( jsInterpretersAlive > 0 ) {
		JS_ShutDown( );
		jsInterpretersAlive--;
	}
	free( (char *) javascript->path ); javascript->path = NULL;
	free( (char *) javascript->fileName ); javascript->fileName = NULL;
	Core_Log( javascript->core, LOG_INFO, __FILE__ , __LINE__, "Delete Javascript free-ed" );
	javascript->core = NULL;
	free( javascript ); javascript = NULL;
}
