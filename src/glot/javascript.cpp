#include <stdlib.h>
#include <string.h>

#include "javascript.h"
#include "../feature/sqlclient.h"
#include "../feature/webserver.h"
#include "../core/utils.h"

static int JS_INTERPRETERS_alive = 0;
static const char * sectionNameMain = "main";

static bool Javascript_IncludeScript( struct javascript_t * javascript, const char * cfile );

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
	struct {unsigned int good:1;
			unsigned int javascript:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	glotSection = cfg_getnsec( core->config, "glot", 0 );
	javascriptSection = cfg_getnsec( glotSection, "Javascript", 0 );
	path = cfg_getstr( javascriptSection, (char *) "path" );
	name = cfg_getstr( javascriptSection, (char *) "main" );
	cleanUp.good = ( ( javascript = Javascript_New( core, path, name ) ) != NULL );
	JS::RootedValue hardVal( javascript->context ) ;
	JS::RootedObject hardObj( javascript->context );
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
		JSAutoRequest ar( javascript->context );
		JSAutoCompartment ac( javascript->context, javascript->globalObj );
		JS::RootedValue rval( javascript->context ) ;
		JS_GetProperty( javascript->context, javascript->globalObj, "Hard", &hardVal );
		cleanUp.good = ( ( JS_ValueToObject( javascript->context, hardVal, &hardObj ) ) == true );
		JS_CallFunctionName( javascript->context, hardObj, "onLoad", JS::HandleValueArray::empty(), &rval );
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
	struct {unsigned int good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	javascript = (struct javascript_t * ) args;
	JS::RootedValue hardVal( javascript->context ) ;
	JS::RootedObject hardObj( javascript->context );
	JSAutoRequest ar( javascript->context );
	JSAutoCompartment ac( javascript->context, javascript->globalObj );
	JS::RootedValue rval( javascript->context ) ;
	JS_GetProperty( javascript->context, javascript->globalObj, "Hard", &hardVal );
	cleanUp.good = ( ( JS_ValueToObject( javascript->context, hardVal, &hardObj ) ) == true );
	JS_CallFunctionName( javascript->context, hardObj, "onReady", JS::HandleValueArray::empty(), &rval );
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
	struct {unsigned int good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	javascript = (struct javascript_t * ) args;
	JS::RootedValue hardVal( javascript->context ) ;
	JS::RootedObject hardObj( javascript->context );
	JSAutoRequest ar( javascript->context );
	JSAutoCompartment ac( javascript->context, javascript->globalObj );
	JS::RootedValue rval( javascript->context ) ;
	JS_GetProperty( javascript->context, javascript->globalObj, "Hard", &hardVal );
	cleanUp.good = ( ( JS_ValueToObject( javascript->context, hardVal, &hardObj ) ) == true );
	JS_CallFunctionName( javascript->context, hardObj, "onUnload", JS::HandleValueArray::empty(), &rval );

	Javascript_Delete( javascript );
}

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
static bool JsnWebserverClassConstructor( JSContext * cx, unsigned argc, JS::Value * vpn ) {
	struct webserver_t * webserver;
	struct javascript_t * javascript;
	JSString * jServerIp;
	JS::CallArgs args;
	const char * cDocumentRoot, *cPath;
	char * cServerIp;
	int port, timeoutSec;
	struct {unsigned int good:1;} cleanUp;

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
	if ( ! thisObj ) {
		cleanUp.good = 0;
	}
	if ( cleanUp.good ) {
		cleanUp.good = ( ( javascript = (struct javascript_t *) JS_GetPrivate( thisObj ) ) != NULL );
	}
	if (cleanUp.good ) {

		cleanUp.good = ( ( webserver = Webserver_New( javascript->core, cServerIp, (uint16_t) port, timeoutSec ) ) != NULL );
	}
	if ( cleanUp.good ){
		cleanUp.good = ( Webserver_DocumentRoot( webserver, cPath, cDocumentRoot )== 1);
	}
	if ( cleanUp.good ) {
		JS::RootedObject webserverObj( cx, JS_NewObject( cx, &jsnWebserverClass, JS::NullPtr(), JS::NullPtr() ) );
		JS_DefineFunctions( cx, webserverObj, jsnWebserverMethods );
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
static bool JsnFunctionStub( JSContext *cx, unsigned argc, JS::Value *vpn );
static bool JsnFunctionStub( JSContext *cx, unsigned argc, JS::Value *vpn ) {
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
static bool JsnHardShutdown( JSContext * cx, unsigned argc, JS::Value * vpn ) {
	struct javascript_t * javascript;
	JS::CallArgs args;
	int timeout;
	struct {unsigned int good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) ) ;
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
	JS_FS( "shutdown", JsnHardShutdown, 1, 0 ),
	JS_FS( "onInit", JsnFunctionStub, 0, 0),
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
static bool JsnConsoleLog( JSContext * cx, unsigned argc, JS::Value * vpn ) {
	struct javascript_t * javascript;
	cfg_t * section;
	JSObject *  consoleObj;
	JSString * jString;
	JS::CallArgs args;
//	const char *fileName;
//	unsigned int lineNo;
	char *cString;
	struct {unsigned int good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	args = CallArgsFromVp( argc, vpn );
	args.rval().setUndefined();
	cleanUp.good = ( JS_ConvertArguments( cx, args, "S", &jString ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( ( cString = JS_EncodeString( cx, jString ) ) != NULL );
	}
	if ( cleanUp.good ) {
		consoleObj = JS_THIS_OBJECT( cx, vpn );
		cleanUp.good = ( ( javascript = (struct javascript_t *)( JS_GetPrivate( consoleObj ) ) ) != NULL );
	}
	if ( cleanUp.good ) {
#if DEBUG & 0
		JSScript *script;
		//  @FIXME:  act on this depending on the compiled javascript debug build
		JS_DescribeScriptedCaller( cx, &script, &lineNo );
//		fileName = JS_GetScriptFilename( cx, script );
#else
//		fileName = __FILE__;
//		lineNo = __LINE__;
#endif
		//Log( javascript->core, LOG_INFO, "[%s:%d] : %s", fileName, lineNo, cString );
		section = cfg_getnsec( javascript->core->config, sectionNameMain, 0 );
		if ( cfg_getbool( section, "daemon" ) ) {
			fprintf( stderr, "%s\n", cString );
		}
	}
	JS_free( cx, cString );

	return ( cleanUp.good ) ? true : false;
}


static const JSClass jsnConsoleClass = {
	"console",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr, nullptr, nullptr, nullptr, nullptr, {nullptr}
};

static const JSFunctionSpec jsnConsoleMethods[ ] = {
	JS_FS( "log",           JsnConsoleLog, 1, 0 ),
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
struct JsPayload{
	JSContext *                                            cx;
	JS::RootedObject                                       obj;
	JS::RootedValue *                                      fnVal;
	JS::HandleValueArray *                                 args;
	bool                                                   repeat;
};

static void JsPayload_Delete( struct JsPayload * payload ) {
	JSAutoRequest ar( payload->cx );
	payload->fnVal = NULL;
	JS_free( payload->cx, payload->args ); payload->args = NULL;
	JS_free( payload->cx, payload ); payload = NULL;
}

static struct JsPayload * JsPayload_New( JSContext * cx, JSObject * object, JS::RootedValue * fn, JS::HandleValueArray * args, bool repeat ) {
	struct JsPayload * payload;
	struct {unsigned int good:1;
			unsigned int payload:1;
			unsigned int args:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( payload = (struct JsPayload* ) JS_malloc( cx, sizeof( * payload ) ) ) != NULL );
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
static bool JsnGlobalInclude( JSContext * cx, unsigned argc, JS::Value * vpn ) {
	struct javascript_t * javascript;
	JSString * jFile;
	JSObject * globalObj;
	JS::CallArgs args;
	char * cFile;
	bool success;
	struct {unsigned int good:1;} cleanUp;

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
	args.rval().setBoolean( success) ;
	return success;
}

static int TimerHandler_cb( void * cbArgs ) {
	struct JsPayload * payload;
	JSContext *cx;
	int again;
	//JSCompartment * oldCompartment;

	payload = ( struct JsPayload * ) cbArgs;
	JS::RootedValue args( payload->cx );
	JS::RootedValue retVal( payload->cx );
	cx = payload->cx;
	JS_BeginRequest( payload->cx );

	//oldCompartment = JS_EnterCompartment( payload->cx, payload->obj );
	JS_CallFunctionValue( payload->cx, payload->obj, *payload->fnVal, JS::HandleValueArray( args ), &retVal );
	if ( ! payload->repeat ) {
		JsPayload_Delete( payload );
		again = 0;
	} else {
		again = 1;
	}

	//JS_LeaveCompartment( cx, oldCompartment );
	JS_EndRequest( cx );

	return again;
}
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
static bool JsnGlobalSetTimeout( JSContext * cx, unsigned argc, JS::Value * vpn ) {
	struct javascript_t * javascript;
	struct JsPayload * payload;
	struct timing_t * timing;
	JSObject * globalObj;
	JS::CallArgs args;
	int ms;
	struct { unsigned int payload:1;
			 unsigned int args:1;
			 unsigned int timer:1;
			unsigned int good:1;} cleanUp;
	  //  @TODO:  DRY JsnGlobalSetTimeout && JsnGlobalSetInterval
	JS::RootedValue dummy( cx );
	JS::RootedValue fnVal( cx );
	args = CallArgsFromVp( argc, vpn );
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	globalObj = JS_THIS_OBJECT( cx, vpn );
	javascript = (struct javascript_t *)( JS_GetPrivate( globalObj ) );
	cleanUp.good = ( JS_ConvertArguments( cx, args, "fi", &dummy, &ms ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertValue( cx, dummy, JSTYPE_FUNCTION, &fnVal ) == true );
	}
	if ( cleanUp.good ) {
		if (args.length() > 2 ) {
			JS::HandleValueArray argsAt2 = JS::HandleValueArray::fromMarkedLocation( argc - 2, vpn );
			cleanUp.good = ( ( payload = JsPayload_New( cx, globalObj, &fnVal, &argsAt2, false ) ) != NULL );
		} else {
			JS::HandleValueArray argsAt2 = JS::HandleValueArray::empty();
			cleanUp.good = ( ( payload = JsPayload_New( cx, globalObj, &fnVal, &argsAt2, false) ) != NULL );
		}
	}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		cleanUp.good = ( ( timing = Core_AddTiming( javascript->core, ms, 0, TimerHandler_cb, (void * ) payload ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.timer = 1;
		timing->clearFunc_cb = (timerHandler_cb_t) JsPayload_Delete;
		args.rval().setInt32( (int32_t) timing->identifier );
	} else {
		if ( cleanUp.timer ) {
			Core_DelTiming( timing );
		}
		if ( cleanUp.args ) {
			JS_free( cx, payload->args) ; payload->args = NULL;
		}
		if ( cleanUp.payload ) {
			free( payload ) ; payload = NULL;
		}
		args.rval().setUndefined();
	}

	return ( cleanUp.good ) ? true : false;
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
static bool JsnGlobalSetInterval( JSContext * cx, unsigned argc, JS::Value * vpn ) {
	struct javascript_t * javascript;
	struct JsPayload * payload;
	struct timing_t * timing;
	JSObject * globalObj;
	JS::CallArgs args;
	int ms;
	struct { unsigned int payload:1;
			 unsigned int args:1;
			 unsigned int timer:1;
			unsigned int good:1;} cleanUp;
	  //  @TODO:  DRY JsnGlobalSetTimeout && JsnGlobalSetInterval
	JS::RootedValue dummy( cx );
	JS::RootedValue fnVal( cx );
	args = CallArgsFromVp( argc, vpn );
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	globalObj = JS_THIS_OBJECT( cx, vpn );
	javascript = (struct javascript_t *)( JS_GetPrivate( globalObj ) );
	cleanUp.good = ( JS_ConvertArguments( cx, args, "fi", &dummy, &ms ) == true );
	if ( cleanUp.good ) {
		cleanUp.good = ( JS_ConvertValue( cx, dummy, JSTYPE_FUNCTION, &fnVal ) == true );
	}
	if ( cleanUp.good ) {
		if (args.length() > 2 ) {
			JS::HandleValueArray argsAt2 = JS::HandleValueArray::fromMarkedLocation( argc - 2, vpn );
			cleanUp.good = ( ( payload = JsPayload_New( cx, globalObj, &fnVal, &argsAt2, true ) ) != NULL );
			payload->repeat = true;
		} else {
			JS::HandleValueArray argsAt2 = JS::HandleValueArray::empty();
			cleanUp.good = ( ( payload = JsPayload_New( cx, globalObj, &fnVal, &argsAt2, true ) ) != NULL );
		}
	}
	if ( cleanUp.good ) {
		cleanUp.payload = 1;
		cleanUp.good = ( ( timing = Core_AddTiming( javascript->core, ms, 1, TimerHandler_cb, (void * ) payload ) ) != NULL );
	}
	if ( cleanUp.good ) {
		cleanUp.timer = 1;
		timing->clearFunc_cb = (timerHandler_cb_t) JsPayload_Delete;
		args.rval().setInt32( (int32_t) timing->identifier );
	} else {
		if ( cleanUp.timer ) {
			Core_DelTiming( timing );
		}
		if ( cleanUp.args ) {
			JS_free( cx, payload->args) ; payload->args = NULL;
		}
		if ( cleanUp.payload ) {
			free( payload ) ; payload = NULL;
		}
		args.rval().setUndefined();
	}

	return ( cleanUp.good ) ? true : false;
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
static bool JsnGlobalClearTimeout( JSContext * cx, unsigned argc, JS::Value * vpn ) {
	struct javascript_t * javascript;
	unsigned int identifier;
	JSObject * globalObj;
	JS::CallArgs args;
	struct {unsigned int good:1;} cleanUp;

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
static void JsnReportError( JSContext * cx, const char * message, JSErrorReport * report ) {
	int where;
	//  http://egachine.berlios.de/embedding-sm-best-practice/ar01s02.html#id2464522
	fprintf( stderr, "%s:%u:%s\n",
		report->filename ? report->filename : "[no filename]",
		( unsigned int ) report->lineno,
		message );
	if ( report->linebuf ) {
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
	if (JSREPORT_IS_WARNING( report->flags)  ) {
		fprintf( stderr, " WARNING" );
	}
	if (JSREPORT_IS_EXCEPTION( report->flags ) ) {
		fprintf( stderr, " EXCEPTION" );
	}
	if (JSREPORT_IS_STRICT( report->flags ) ) {
		fprintf( stderr, " STRICT" );
	}
	fprintf( stderr, " (Error number: %d)\n", report->errorNumber );
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
	JS::RootedValue rval( javascript->context ) ;

	JS_DefineFunctions( javascript->context, javascript->globalObj, jsnGlobalClassFuncs );
	JS_SetPrivate( javascript->globalObj, ( void * ) javascript );

	JS::RootedObject consoleObj( javascript->context, JS_NewObject( javascript->context, &jsnConsoleClass, JS::NullPtr(), JS::NullPtr() ));
	JS_SetPrivate( consoleObj, (void * ) javascript );
	JS_DefineFunctions( javascript->context, consoleObj, jsnConsoleMethods );

	JS::RootedObject hardObj( javascript->context, JS_NewObject( javascript->context, &jsnHardClass, JS::NullPtr(), JS::NullPtr() ));
	JS_SetPrivate( hardObj, (void * ) javascript );
	JS_DefineFunctions( javascript->context, hardObj, jsnHardMethods );

	JS::RootedObject WebserverObj( javascript->context, JS_InitClass( javascript->context, hardObj, js::NullPtr(), &jsnWebserverClass, JsnWebserverClassConstructor, 3, NULL, NULL, NULL, NULL ) );

	JS_CallFunctionName( javascript->context, hardObj, "onInit", JS::HandleValueArray::empty(), &rval );

	return 0;
}

static int Javascript_Init( struct javascript_t * javascript, struct core_t * core ) {
	struct {unsigned int good:1;
			unsigned int global:1;
			unsigned int standard:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	JSAutoRequest ar( javascript->context );
	JS::CompartmentOptions compartmentOptions;
	compartmentOptions.setVersion( JSVERSION_LATEST );
	JS::RootedObject global( javascript->context, JS_NewGlobalObject( javascript->context, &jsnGlobalClass, nullptr, JS::DontFireOnNewGlobalHook, compartmentOptions ));
	cleanUp.good = ( global != NULL );
	if ( cleanUp.good ) {
		cleanUp.global = 1;
		javascript->globalObj = global;
	}
	JSAutoCompartment ac( javascript->context, javascript->globalObj );
	cleanUp.good = ( ( JS_InitStandardClasses( javascript->context,  javascript->globalObj ) ) == true );
	if ( cleanUp.good ) {
		cleanUp.standard = 1;
		JS_FireOnNewGlobalObject( javascript->context, javascript->globalObj );
		cleanUp.good = ( Javascript_Run( javascript ) != 0 ) ;
	}

	return cleanUp.good;
}

static bool Javascript_IncludeScript( struct javascript_t * javascript, const char * cfile ) {
	char fileNameWithPath[512];
	struct { unsigned int good:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	JSAutoRequest ar( javascript->context );
	memset( fileNameWithPath, '\0', sizeof( fileNameWithPath ) );
	memset( &cleanUp, 0, sizeof( cleanUp ) );
	strncpy( fileNameWithPath, javascript->path, 255 );
	strncat( fileNameWithPath, cfile, 255 );
	//Log( javascript->core, LOG_INFO, "[%14s] Loading script %s", __FILE__, fileNameWithPath );
	if (cleanUp.good ) {
		JS::RootedScript script( javascript->context );
		JS::CompileOptions options( javascript->context );
		options.setIntroductionType( "js include" )
			.setUTF8(true)
			.setFileAndLine( cfile, 1)
			.setCompileAndGo( true )
			.setNoScriptRval( true );
		cleanUp.good = ( JS::Compile( javascript->context, javascript->globalObj, options, fileNameWithPath, &script ) == true );
	}

	if ( cleanUp.good ) {
		//Log( core, LOG_INFO, "[%14s] Failed loading script %s", __FILE__, fileNameWithPath );
	}
	if ( ! cleanUp.good ){
	}
	return ( cleanUp.good ) ? true : false;
}

struct javascript_t * Javascript_New( struct core_t * core, const char * path, const char * fileName ) {
	struct javascript_t * javascript;
#define MAX_PATH_LENGTH 512
	char fullPath[MAX_PATH_LENGTH];
	struct { unsigned int good:1;
			 unsigned int path:1;
			 unsigned int jsinit:1;
			 unsigned int runtime:1;
			 unsigned int context:1;
			 unsigned int init:1;
			 unsigned int javascript:1;} cleanUp;

	memset( &cleanUp, 0, sizeof( cleanUp ) );
	cleanUp.good = ( ( javascript = (struct javascript_t *) malloc( sizeof( * javascript ) ) ) != NULL );
	if ( cleanUp.good ) {
		cleanUp.javascript = 1;
		cleanUp.good = ( ( javascript->path = strdup( path ) ) != NULL );
	}
	if 	( cleanUp.good ) {
		cleanUp.path = 1;
		if ( JS_INTERPRETERS_alive == 0 ) {
			cleanUp.good = ( JS_Init( ) == true );
			JS_INTERPRETERS_alive++;
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
		/*unsigned int options;

		options = JSOPTION_VAROBJFIX| JSOPTION_EXTRA_WARNINGS | JSOPTION_NO_SCRIPT_RVAL | JSOPTION_UNROOTED_GLOBAL | JSOPTION_COMPILE_N_GO;
#ifdef DEBUG
		JS_SetGCZeal( javascript->context, 2 , 1 );
		options |= JSOPTION_WERROR;
#endif
		JS_SetOptions( javascript->context, options );
		*/
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
			if ( JS_INTERPRETERS_alive > 0 ) {
				JS_ShutDown( ) ;
				JS_INTERPRETERS_alive--;
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
	if ( JS_INTERPRETERS_alive > 0 ) {
		JS_ShutDown( ) ;
		JS_INTERPRETERS_alive--;
	}
}
