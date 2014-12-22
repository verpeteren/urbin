"use strict"
try {
	console.log( "hee" );
} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}
Hard.onLoad = function( ) {
	console.log( "load" );
}
Hard.onReady = function( ) {
	console.log( "ready" );
	var ws = Hard.Webserver( {ip: '127.0.0.1', port: 8888}, 60 );
	ws.addDocumentRoot( '^/static/(.*)', '../var/www/static/' );
}
Hard.onUnload = function( ) {
	console.log( "unload" );
}
