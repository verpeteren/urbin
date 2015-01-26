'use strict'

try {

	Urbin.onLoad = function( ) {
		console.log( 'load' );
	}
	Urbin.onReady = function( ) {
		var ws = Urbin.Webserver( {ip: 'localhost', port: 8888}, 120 );
		ws.addDocumentRoot( '^/bench/(?<path>.*)', '/var/www/www.urbin.info/bench/' );
		/*ws.addRoute( '^/blog/(?<year>[0-9]{4})/(?<month>[0-9]{1,2})/(?<day>[0-9]{1,2})', function( client ) {
			var params = client.getNamedGroups( );
			var blog = 'Well,  on ' + params.year + '-' + params.month + '-' + params.day + ' nothing happened';
			client.response.setContent( blog ).setMime( 'html' ).setCode( 200 );
		 } );
		*/
	}
	Urbin.onUnload = function( ) {
		console.log( 'unload' );
	}

} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}

