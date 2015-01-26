'use strict'

var mysqlDetective = {host : 'localhost', db : 'SQLDETECTIVE', user : 'sqldetective', password : 'sherlock', port : 3306 };
var pgsqlDetective = {host : 'localhost', db : 'sqldetective', user : 'sqldetective', password : 'sherlock', port : 5432 };

var showResults = function( rows ) {
	if ( Array.isArray( rows ) ) {
		for ( var rowId = 0; rowId < rows.length; rowId++ ) {
			var row = rows[rowId];
		console.log( '\t' + row.employee_id +':' + row.last_name + ',' + row.first_name );
		}
	} else {
		console.log( ' no data ' );
	}
	console.log( '--------------------------------------------------------------------------' );
}

try {

	Urbin.onLoad = function( ) {
		console.log( 'load' );
	}
	Urbin.onReady = function( ) {
		var test = { webserver: 		true,
					webclient:			false,
					os: {	file: 		false,
							env:		false,
							system:		false,
							mkdir:		false,
							rm:			false,
							dir:		false,
							symlink:	false,
							readdir:	false,
							hostName:	false
						},
					sql: { 	pg: 		false,
							my: 		false
						},
					timeout: 			false,
					interval: 			false
					};
		if ( test.webserver ) {
			var ws = Urbin.Webserver( {ip: 'localhost', port: 8888}, 60 );
			ws.addDocumentRoot( '^/static/(?<path>.*)', '../var/www/static/' );
			ws.addDocumentRoot( '^/docs/(?<path>.*)', '../var/www/docs/' );
			ws.addRoute( '^/dynamic/(?<path>.*)', function( client ) {
					console.log( 'requested: ' + client.method + ' ' + client.ip + ' ' + client.url );
					client.response.setContent( 'okidokie' ).setMime( 'html' ).setCode( 200 );
					console.log( client.request.headers );
					console.log( client.request.content );
				}
			);
			ws.addRoute( '^/blog/(?<year>[0-9]{4})/(?<month>[0-9]{1,2})/(?<day>[0-9]{1,2})', function( client ) {
				var params = client.getNamedGroups( );
				var blog = 'Well,  on ' + params.year + '-' + params.month + '-' + params.day + ' nothing happened';
				client.response.setContent( blog ).setMime( 'html' ).setCode( 200 );
					console.log( client.request.headers );
					console.log( client.request.content );
			 } );
		}
		if ( test.webclient ) {
			var showResponse = function( response ) {
				console.log( "\n\n\ngot webpage" );
				console.log( response.code );
				console.log( response.headers );
				console.log( response.content );
			}
			var con = {method: 'GET', headers: 'Host: www.urbin.info\r\nAccept: text/html\r\n Cache-Control: max-age=0\r\n', content: '/content/'};
			var wc = Urbin.Webclient( 'http://localhost:80/benchmark.html', showResponse, con, 60 );
		//	wc.queue( '127.0.0.1:80/index.html', showResponse, con );
			wc.queue( 'http://localhost:80/index.html', showResponse, con );
		}
		if ( test.os.hostName ) {
			os.getHostByName('www.urbin.info', function( ip ) {
				if ( ip ) {
					console.log( 'Resolved: ' + ip );
				} else {
					console.log( 'Could not resolve host' );
				}
			});
			os.getHostByName('www.google.com', function( ip ) {
				if ( ip ) {
					console.log( 'Resolved: ' + ip );
				} else {
					console.log( 'Could not resolve host' );
				}
			});
			os.getHostByName('self', function( ip ) {
				if ( ip ) {
					console.log( 'Resolved: ' + ip );
				} else {
					console.log( 'Could not resolve host' );
				}
			});
		}
		if ( test.os.env ) {
			var env = 'SHELL';
			console.log(' env : ' + env + ': ' + os.getEnv( env ) );
		}		
		var fileName = '/tmp/test.txt';
		if ( test.os.file ) {
			var writeContent = 'blabalbla';
			
			os.writeFile( fileName, writeContent, false );
			var readContent = os.readFile( fileName ); 
			if ( readContent == writeContent ) { 
				console.log( 'ok' );
			} else {
				console.log( 'os read/write function failed! got: "' + readContent + '"' + 
							 '                          expected: "' + writeContent + '"');
			}
		}
		if ( test.os.symlink ) {
			os.symlink( fileName, fileName + ".lnk" );
		}
		var now = new Date( );
		var dirName = '/tmp/dir_' + now.getTime();
		if ( test.os.mkdir ) {
			console.log( 'creating dir: ' + dirName );
			var success = os.mkdir( dirName );
			console.log( 'success : ' + success );
		}
		if ( test.os.rm ) {
			console.log( 'removing dir: ' + dirName );
			var success = os.rmdir( dirName );
			console.log( 'success : ' + success );
				
			console.log( 'removing file: ' + fileName );
			var success = os.rmdir( fileName );
			console.log( 'success : ' + success );
		}
		if ( test.os.dir ) {
			var dirEntries = os.dir( '/tmp/' );
			for ( var i = 0; i < dirEntries.length; i++ ) {
				var dir = dirEntries[i];
 				console.log( "file: " + dir.name + " size: " + dir.size + " type: " + dir.type );
			}
		}
		if ( test.os.system ) {
			var r = os.system( '/usr/bin/wget', 'http://www.verpeteren.nl -o /tmp/www.verpeteren.nl.dl -O /tmp/www.verpeteren.nl.html' );
			console.log( r );
		}
		if ( test.interval ) {
			setTimeout( function( ) {
				console.log( 'timeout' );
			}, 1000 );
		}
		if ( test.timeout ) {
			var delta = 333;
			var db = new Date ();
			var b = db.getTime( );
			console.log( 'before ' + db + ' ' + b );
			setTimeout( function( ) {
				var da = new Date ();
				var a = da.getTime( );
				var dif = a - b;
				console.log( 'after  ' + da + ' ' + b );
				console.log( 'diff   ' + dif  + ' ' + dif % delta );
			}, delta );
		}
		if ( test.sql.pg ) {
			var sql = Urbin.PostgresqlClient( pgsqlDetective , 60 );
			sql.query( 'SELECT * FROM employee WHERE first_name = $1', [ 'Marla' ], showResults );
			sql.query( 'SELECT * FROM employee WHERE first_name = $1', [ 'Neal' ], showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = 'Neal'", null, showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = 'Zelda'", [], showResults ) ;
			sql.query( "SELECT * FROM employee WHERE first_name = 'Dong'", [], showResults ) ;
			sql.query( "SELECT * FROM employee WHERE first_name = 'Anne'", null, showResults );
		}
		if ( test.sql.my ) {
			var sql = Urbin.MysqlClient( mysqlDetective , 60 );
			sql.query( 'SELECT * FROM employee WHERE first_name = ?', [ 'Marla' ], showResults );
			sql.query( 'SELECT * FROM employee WHERE first_name = ?', [ 'Neal' ], showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = 'Neal'", null, showResults );
			sql.query( "SELECT * FROM employee WHERE first_name = 'Zelda'", [], showResults ) ;
			sql.query( "SELECT * FROM employee WHERE first_name = 'Dong'", [], showResults ) ;
			sql.query( "SELECT * FROM employee WHERE first_name = 'Anne'", null, showResults ); 
		}
	}
	Urbin.onUnload = function( ) {
		console.log( 'unload' );
	}

} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}

