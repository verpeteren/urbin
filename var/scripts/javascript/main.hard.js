"use strict"

var apedev = {host : '10.0.0.25', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432 };
var mysqlDetective = {host : 'localhost', db : 'SQLDETECTIVE', user : 'sqldetective', password : 'sherlock', port : 3306 };
var pgsqlDetective = {host : 'localhost', db : 'sqldetective', user : 'sqldetective', password : 'sherlock', port : 5432 };
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
		var sql = Hard.PostgresqlClient( pgsqlDetective , 60 );
		//var sql = Hard.MysqlClient( mysqlDetective , 60 );
			sql.query( "SELECT *  FROM employee WHERE hair_colour = '$1' );", ['black' ], function( res ) {
				if ( typeof query == array ) {
					for ( var rowId = 0; rowId < res.length; rowId++ ) {
						row = res[rowId];
						console.log( rowId + ' ' + row.name + ' ' + row.sales );
					}
				}
			} );
	}
	Hard.onUnload = function( ) {
		console.log( "unload" );
	}

} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}

