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

	Urbin.onLoad = function( ) {
		console.log( "load" );
	}
	Urbin.onReady = function( ) {
		var test = { webserver: 	true,
					os: {	file: 	true,
							env:	true,
							system:	true
						},
					sql: { 	pg: 	false,
							my: 	true
						},
					timeout: 		false
					};
		if ( test.webserver ) {
			var ws = Urbin.Webserver( {ip: '127.0.0.1', port: 8888}, 60 );
			ws.addDocumentRoot( '^/static/(?<path>.*)', '../var/www/static/' );
			ws.addDocumentRoot( '^/docs/(?<path>.*)', '../var/www/docs/' );
			ws.addRoute( "^/dynamic/(?<path>.*)", function( client ) {
					console.log( "requested: " + client.method + " " + client.ip + " " + client.url );
					client.response.setContent( "okidokie" ).setMime( 'html' ).setCode( 200 );
				}
			);
			ws.addRoute( '^/blog/(?<year>[0-9]{4})/(?<month>[0-9]{1,2})/(?<day>[0-9]{1,2})', function( client ) {
				/* this is buggy right now */
				var params = client.getNamedGroups( );
				console.log( 'year:\t' + params.year );
				console.log( 'month:\t' + params.month );
				console.log( 'day:\t' + params.day );
				client.response.setContent( "ok" ).setMime( 'html' ).setCode( 200 );
				console.log( "eee" );
			 } );

		}
		if ( test.os.env ) {
			var env = 'SHELL';
			console.log(" env : " + env + ": " + os.getEnv( env ) );
		}
		if ( test.os.file ) {
			var fileName = '/tmp/test.txt';
			var writeContent = "blabalbla";
			
			os.writeFile( fileName, writeContent, false );
			var readContent = os.readFile( fileName ); 
			if ( readContent == writeContent ) { 
				console.log( "ok" );
			} else {
				console.log( "os read/write function failed! got: '" + readContent + "'" + 
							 "                          expected: '" + writeContent + "'");
			}
		}
		if ( test.os.system ) {
			var r = os.system( '/usr/bin/wget', 'http://www.verpeteren.nl -o /tmp/www.verpeteren.nl.dl -O /tmp/www.verpeteren.nl.html' );
			console.log( r );
		}
		if ( test.timeout ) {
			/* This is buggy rigth now*/
			setTimeout( function( ) {
				console.log( "tttt" );
			}, 1000 );
		}
		if ( test.sql.pg ) {
			var sql = Urbin.PostgresqlClient( pgsqlDetective , 60 );
			sql.query( "SELECT * FROM employee WHERE first_name = $1", [ 'Marla' ], showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = $1", [ 'Neal' ], showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = 'Neal'", null, showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = 'Zelda'", [], showResults ) ;
			sql.query( "SELECT * FROM employee WHERE first_name = 'Dong'", [], showResults ) ;
			sql.query( "SELECT * FROM employee WHERE first_name = 'Anne'", null, showResults );
		}
		if ( test.sql.my ) {
			var sql = Urbin.MysqlClient( mysqlDetective , 60 );
			sql.query( "SELECT * FROM employee WHERE first_name = ?", [ 'Marla' ], showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = ?", [ 'Neal' ], showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = 'Neal'", null, showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = 'Zelda'", [], showResults ) ;
			sql.query( "SELECT * FROM employee WHERE first_name = 'Dong'", [], showResults ) ;
			sql.query( "SELECT * FROM employee WHERE first_name = 'Anne'", null, showResults ); 
		}
	}
	Urbin.onUnload = function( ) {
		console.log( "unload" );
	}

} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}

