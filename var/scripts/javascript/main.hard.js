"use strict"

try {

	Hard.onLoad = function( ) {
		console.log( "load" );
	}

	Hard.onReady = function( ) {
		var ws = Hard.Webserver( {ip: '127.0.0.1', port: 8888}, 60 );
		ws.addDocumentRoot( '^/static/(.*)', '../var/www/static/' );
		ws.addDocumentRoot( '^/docs/(.*)', '../var/www/docs/' );
		ws.addRoute( "^/dynamic/(.*)", function( client ) {
				console.log( client );
				console.log( "requested" + client.method + " " + client.ip + " " + client.url );
				client.response.setContent( "okidokie" ).setMime( 'html' ).setCode( 200 );
			}
		);
		console.log( "ready" );
		/*
		setInterval( function( ) {
					 console.log( "tttt" );
					}, 1000 );
		*/
	}
	Hard.onUnload = function( ) {
		console.log( "unload" );
	}

} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}

