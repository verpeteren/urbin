"use strict"

var apedev = {host : '10.0.0.25', db : 'apedevdb', user : 'apedev', password : 'vedepa', port : 5432 };
var mysqlDetective = {host : 'localhost', db : 'SQLDETECTIVE', user : 'sqldetective', password : 'sherlock', port : 3306 };
var pgsqlDetective = {host : 'localhost', db : 'sqldetective', user : 'sqldetective', password : 'sherlock', port : 5432 };

var showResults = function( rows ) {
	if ( Array.isArray( rows ) ) {
		for ( var rowId = 0; rowId < rows.length; rowId++ ) {
			var row = rows[rowId];
		console.log( "\t" + row.employee_id +":" + row.last_name + "," + row.first_name );
		}
	} else {
		console.log( " no data " );
	}
	console.log( "--------------------------------------------------------------------------" );
}

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
		sql.query( "SELECT * FROM employee WHERE hair_colour = $1", ['black' ], showResults );
		sql.query( "SELECT * FROM employee WHERE hair_colour = 'black'", null, showResults );
		sql.query( "SELECT * FROM employee", null, showResults ); 
		sql.query( "SELECT * FROM employee", [], showResults ) ;

		var sql = Hard.MysqlClient( mysqlDetective , 60 );
		sql.query( "SELECT * FROM employee WHERE hair_colour = ?", ['black' ], showResults );
		sql.query( "SELECT * FROM employee WHERE hair_colour = 'black'", null,  showResults );
		sql.query( "SELECT * FROM employee", null, showResults );
	}
	Hard.onUnload = function( ) {
		console.log( "unload" );
	}

} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}

