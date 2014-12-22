"use strict"

try {

	Hard.onLoad = function( ) {
		console.log( "load" );
	}

	Hard.onReady = function( ) {
		/*
		var ws = Hard.Webserver( {ip: '127.0.0.1', port: 8888}, 60 );
		ws.addDocumentRoot( '^/static2/(.*)', '/var/www/' );
		ws.addDocumentRoot( '^/static1/(.*)', '../var/www/static/' );
		ws.addRoute( "^/dynamic/(.*)", function( arg ) {
				console.log( "handlin" );
				console.log( arg );
			}
		);
		console.log( "ready" );
		*/
		setTimeout( function( ) {
					 console.log( "tttt" );
					}, 1000 );
	}

	Hard.onUnload = function( ) {
		console.log( "unload" );
	}

} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}

