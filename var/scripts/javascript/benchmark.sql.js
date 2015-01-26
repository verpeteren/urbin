'use strict'
/*
for postgresql
	CREATE DATABASE urbinbenchmark;
	CREATE USER bench WITH PASSWORD 'mark';
	GRANT ALL PRIVILEGES ON DATABASE urbinbenchmark TO bench;
INFO: [../var/scripts/javascript/benchmark.sql.js:   56] connect	0
INFO: [../var/scripts/javascript/benchmark.sql.js:   56] drop	0.105
INFO: [../var/scripts/javascript/benchmark.sql.js:   56] create	0.397
INFO: [../var/scripts/javascript/benchmark.sql.js:   56] fill	1.555
INFO: [../var/scripts/javascript/benchmark.sql.js:   56] selectfill	35.346


for mysql
	CREATE DATABASE if not exists URBINBENCHMARK;
	CREATE USER 'bench'@'localhost' IDENTIFIED BY 'mark';
	GRANT ALL ON URBINBENCHMARK.* TO 'bench'@'localhost';
	flush privileges;
INFO: [../var/scripts/javascript/benchmark.sql.js:   50] connect	0
INFO: [../var/scripts/javascript/benchmark.sql.js:   50] drop	0.164
INFO: [../var/scripts/javascript/benchmark.sql.js:   50] create	0.4
INFO: [../var/scripts/javascript/benchmark.sql.js:   50] fill	3.974
INFO: [../var/scripts/javascript/benchmark.sql.js:   50] selectfill	94.864

*/
var pgsql = {host : 'localhost', db : 'urbinbenchmark', user : 'bench', password : 'mark', port : 5432 };
var mysql = {host : 'localhost', db : 'URBINBENCHMARK', user : 'bench', password : 'mark', port : 3306 };

var CUSTOMERS = ['lisa', 'bart', 'maggie', 'homer', 'marge', 'millhouse', 'martin', 'ralph', 'burns', 'smithers', 'barney', 'grampa', 'flanders', 'wiggum', 'lovejoy', 'willie', 'apu', 'bob', 'skinner', 'edna ', 'krusty', 'nelson', 'quimby', 'brockman', 'riviera', 'otto', 'patty', 'selma', 'frink'];
var PARTS = [ 'Akiapolaau', 'Anhinga', 'Apapane', 'Auklet', 'Avocets', 'Blue Jay', 'Bobwhite', 'Brown Thrasher', 'Bunting', 'Canary', 'Cardinal', 'Chat', 'Chickadee', 'Chicken', 'Cockatiel', 'Condor', 'Cormorant', 'Crossbill', 'Crow', 'Curlew', 'Currawong', 'Dowitchers', 'Duck', 'Eagle', 'Egret', 'Eider', 'Elepaio', 'Falcon', 'Flycatcher', 'Gallinule', 'Goldfinch', 'Goose', 'Grosbeak', 'Grouse', 'Gull', 'Harrier', 'Heron', 'Hummingbird', "I-iwi", 'Ibis', 'Jaeger', 'Kite', 'Limpkin', 'Loon', 'Macaw', 'Magpie', 'Murrelet', 'Muscovy', 'Nene', 'Omao', 'Oriole', 'Owl', 'Palila', 'Parakeet', 'Parrot', 'Peep (Stint)', 'Pelican', 'Penguine', 'Peregrine', 'Phalarope', 'Pigeon', 'Plover', 'Puffins', 'Quail', 'Raven', 'Robin', 'Rosella', 'Sanderling', 'Sandpipyer', 'Sapsucker', 'Scoter', 'Shrike', 'Solitaire', 'Sora', 'Sparrow', 'Stint (Peep)', 'Stork', 'Swan', 'Swift', 'Tanagers', 'Tern', 'Thrush', 'Thornbill', 'Turkey', 'Vagrants', 'Vulture', 'Warbler', 'Waxwing', 'Whimbrel', 'Whippoorwill', 'Woodpecker'];

var run = {pg: true, my: false  };
var topics = [ 'connect', 'drop', 'create', 'fill', 'selectfill' ];
var data, i, timing;

var timings = {	
	pg: { 
		connect: {start: null, end: null}, 
		drop: {start: null, end: null}, 
		create: {start: null, end: null}, 
		fill: {start: null, end: null}, 
		selectfill: {start: null, end: null}, 
		},
	my: { 
		connect: {start: null, end: null}, 
		drop: {start: null, end: null}, 
		create: {start: null, end: null}, 
		fill: {start: null, end: null}, 
		selectfill: {start: null, end: null}, 
		}
	};
	function showResults( ) {
		for ( var ti = 0; ti < topics.length; ti++ ){
			var topic = topics[ti]
			var delta = timing[topic].end - timing[topic].start;
			console.log( topic + "\t" + delta / 1000 );
		} 
	};

try {
		if ( run.pg ) {
		i = 0;
		timing = timings.pg;
		data = {customers: {}, parts: {} };
		timing.connect.start = Date.now();
		var sql = Urbin.PostgresqlClient( pgsql , 60 );
		timing.connect.end = Date.now( );	
		timing.drop.start = Date.now( );
		sql.query( 'DROP TABLE IF EXISTS ORDERS', [], function( drop1 ) {
			sql.query( 'DROP TABLE IF EXISTS PARTS', [], function( drop2 ) {
				sql.query( 'DROP TABLE IF EXISTS CUSTOMERS', [], function( drop3 ) { 
					timing.drop.end = Date.now( );
					timing.create.start = Date.now( );
					sql.query( 'CREATE TABLE CUSTOMERS (' + '\n' +
					'	CUST_ID			SERIAL 				PRIMARY KEY, ' + '\n' +
					'	CUST_NAME		VARCHAR (20)		NOT NULL, ' + '\n' +
					'	CREATED			TIMESTAMP 			DEFAULT NOW( )' + '\n' +
					')', [], function( create1 ) {
						sql.query( 'CREATE UNIQUE INDEX I_CUSTOMERS_NAME ON CUSTOMERS (CUST_NAME)', [], function( create2 ) { 
							sql.query( 'CREATE TABLE PARTS( ' + '\n' +
									'	PART_ID			SERIAL				PRIMARY KEY, ' + '\n' +
									'	PART_NAME		VARCHAR(20)			NOT NULL' + '\n' +
									')', [], function( create4 ) {
								sql.query( 'CREATE UNIQUE INDEX I_PARTS_NAME ON PARTS (PART_NAME)', [], function( create4 ) {
									sql.query( 'CREATE TABLE ORDERS( ' + '\n' +
										'	ORDER_ID		SERIAL				PRIMARY KEY, ' + '\n' +
										'	CUST_ID			INT					NOT NULL REFERENCES CUSTOMERS(CUST_ID) ON DELETE RESTRICT, ' + '\n' +
										'	PART_ID 		INT					NOT NULL REFERENCES PARTS(PART_ID) ON DELETE RESTRICT, ' + '\n' +
										'	QUANTITY		INT					NOT NULL, ' + '\n' +
										'	ENTRY 			TIMESTAMP DEFAULT NOW( )'  + '\n' +
										')', [], function( create5 ) {
										sql.query( 'CREATE INDEX I_ORDERS_CUST_ID ON ORDERS (CUST_ID)', [], function( create6 ) {
											sql.query( 'CREATE INDEX I_ORDERS_PART_ID ON ORDERS (PART_ID)', [], function( create7 ) { 
												timing.create.end = Date.now( );
												timing.fill.start = Date.now( );
												for ( var c = 0; c < CUSTOMERS.length; c++ ) { 
													sql.query( 'INSERT INTO CUSTOMERS (CUST_NAME) VALUES ($1) RETURNING CUST_ID, CUST_NAME, CREATED', [CUSTOMERS[c]], function( fillCustomer ) { 
														data.customers[fillCustomer[0].cust_name] = fillCustomer[0].cust_id;
														if ( fillCustomer[0].cust_name == CUSTOMERS[CUSTOMERS.length - 1] ) {
															for ( var p = 0; p < PARTS.length; p++ ) { 
																sql.query( 'INSERT INTO PARTS (PART_NAME) VALUES ($1) RETURNING PART_ID, PART_NAME', [PARTS[p]], function( fillPart ) { 
																	data.parts[fillPart[0].part_name] = fillPart[0].part_id;
																	if ( fillPart[0].part_name == PARTS[PARTS.length - 1] ) {
																		timing.fill.end = Date.now( );
																		timing.selectfill.start = Date.now( );
																		var i = 0;
																		for( var customer in data.customers ) {
																			if ( data.customers.hasOwnProperty( customer ) ) {
																				for( var part in data.parts ) {
																					if ( data.parts.hasOwnProperty( part ) ) {
																						var customerId = data.customers[customer];
																						var partId = data.parts[part];
																						sql.query('INSERT INTO ORDERS (CUST_ID, PART_ID, QUANTITY) VALUES ($1, $2, $3) RETURNING CUST_ID, PART_ID', [customerId, partId, i ], function( res ) { 
																							if ( res[0].cust_id == data.customers[CUSTOMERS[CUSTOMERS.length - 1]] && res[0].part_id == data.parts[PARTS[PARTS.length - 1]] ) {
																								timing.selectfill.end  = Date.now( );
																								showResults( );
																							}
																						} );
																						i++;
																					}
																				}
																			}
																		}
																	} 
																} );
															}
														}
													} );
												}
											} );
										} );
									} );
								} );
							} );
						} );
					} );
				} );
			} );
		} );
	}
	if ( run.my ) {
		i = 0;
		timing = timings.my;
		data = {customers: {}, parts: {} };
		timing.connect.start = Date.now();
		var sql = Urbin.MysqlClient( mysql , 60 );
		timing.connect.end = Date.now( );	
		timing.drop.start = Date.now( );
		sql.query( 'DROP TABLE IF EXISTS ORDERS', [], function( drop1 ) {
			sql.query( 'DROP TABLE IF EXISTS PARTS', [], function( drop2 ) {
				sql.query( 'DROP TABLE IF EXISTS CUSTOMERS', [], function( drop3 ) { 
					timing.drop.end = Date.now( )
					timing.create.start = Date.now( );
					sql.query('CREATE TABLE CUSTOMERS ( ' + '\n' +
					'	CUST_ID						INT NOT NULL AUTO_INCREMENT, ' + '\n' +
					'	CUST_NAME VARCHAR (20)      NOT NULL, ' + '\n' +
					'	CREATED						TIMESTAMP DEFAULT CURRENT_TIMESTAMP,' + '\n' +
					'	PRIMARY KEY (CUST_ID),' + '\n' +
					'	UNIQUE INDEX I_CUSTOMERS_NAME (CUST_NAME)' + '\n' +
					')ENGINE=InnoDB', [], function( create1 ) { 
						sql.query( 'CREATE TABLE PARTS( ' + '\n' +
						'	PART_ID 					INT NOT NULL AUTO_INCREMENT, ' + '\n' +
						'	PART_NAME					VARCHAR(20)		NOT NULL, ' + '\n' +
						'	PRIMARY KEY (PART_ID), ' + '\n' +
						'	UNIQUE INDEX I_PARTS_NAME (PART_NAME)' + '\n' +
						')ENGINE=InnoDB', [], function( create2 ) {
							sql.query( 'CREATE TABLE ORDERS( ' + '\n' +
							'	ORDER_ID 					INT NOT NULL AUTO_INCREMENT, ' + '\n' +
							'	CUST_ID						INT		NOT NULL, ' + '\n' +
							'	PART_ID 					INT NOT NULL, ' + '\n' +
							'	QUANTITY 					INT NOT NULL, ' + '\n' +
							'	ENTRY 						TIMESTAMP DEFAULT CURRENT_TIMESTAMP, ' + '\n' +
							'	PRIMARY KEY (ORDER_ID),  ' + '\n' +
							'	INDEX i_orders_customers_cust_id (CUST_ID),' + '\n' +
							'	INDEX i_orders_parts_part_id (PART_ID),' + '\n' +
							'	FOREIGN KEY f_customers_customer_id (CUST_ID) REFERENCES CUSTOMERS(CUST_ID) ON DELETE RESTRICT,' + '\n' +
							'	FOREIGN KEY f_parts_part_id (PART_ID) REFERENCES PARTS(PART_ID) ON DELETE RESTRICT' + '\n' +
							')ENGINE=InnoDB', [], function( create3 ) { 
								timing.create.end = Date.now( );
								timing.fill.start = Date.now( );
								for ( var c = 0; c < CUSTOMERS.length; c++ ) { 
									//sql.query( 'INSERT INTO CUSTOMERS (CUST_NAME) VALUES (?)', [CUSTOMERS[c]], function( fillCustomer ) {  //  @WTF:  ??
									sql.query( 'INSERT INTO CUSTOMERS (CUST_NAME) VALUES ( \'' + CUSTOMERS[c] + '\' )', [], function( fillCustomer ) {
										var idc = sql.getInsertId( );
										sql.query( 'SELECT c.*, NULL FROM CUSTOMERS c WHERE CUST_ID = ? ', [idc], function( fillCustomer ) { 
											data.customers[fillCustomer[0].CUST_NAME] = fillCustomer[0].CUST_ID;
											if ( fillCustomer[0].CUST_NAME == CUSTOMERS[CUSTOMERS.length - 1] ) {
												for ( var p = 0; p < PARTS.length; p++ ) { 
													sql.query( 'INSERT INTO PARTS (PART_NAME) VALUES ( \'' + PARTS[p] +  '\' )', [], function( fillPart ) {  //  @WTF:  ??
														var idp = sql.getInsertId( );
														sql.query( 'SELECT * FROM PARTS WHERE PART_ID = ?', idp, function( fillPart ) { 
															data.parts[fillPart[0].PART_NAME] = fillPart[0].PART_ID;
															if ( fillPart[0].PART_NAME == PARTS[PARTS.length - 1] ) {
																timing.fill.end = Date.now( );
																timing.selectfill.start = Date.now( );
																var i = 0;
																for( var customer in data.customers ) {
																	if ( data.customers.hasOwnProperty( customer ) ) {
																		for( var part in data.parts ) {
																			if ( data.parts.hasOwnProperty( part ) ) {
																				var customerId = data.customers[customer];
																				var partId = data.parts[part];
																				sql.query('INSERT INTO ORDERS (CUST_ID, PART_ID, QUANTITY) VALUES ( \'' + customerId + '\', \'' + partId + '\', \'' + i + '\' )', [], function( res ) {  //  @WTF: ??
																					var ido = sql.getInsertId( );
																					sql.query('SELECT * FROM ORDERS WHERE ORDER_ID = ?', [ido], function( res ) {
																						if ( res[0].CUST_ID == data.customers[CUSTOMERS[CUSTOMERS.length - 1]] && res[0].PART_ID == data.parts[PARTS[PARTS.length - 1]] ) {
																							timing.selectfill.end  = Date.now( );
																							showResults( );
																						}
																					} );
																				} );
																				i++;
																			}
																		}
																	}
																}
															} 
														} );
													} );
												}
											}
										} );
									} );
								}
							} );
						} );
					} );
				} );
			} );
		} );
	}	
} catch ( e ) {
	console.log( e.message + '\n\t' + e.fileName + ':' + e.lineNumber );
}

