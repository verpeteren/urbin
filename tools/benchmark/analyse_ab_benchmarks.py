#!/usr/bin/python

import glob
from pprint import pprint

OUTPUT_PATH = "./output/"

PARAMETERS = [
	'Time taken for tests',
	'Complete requests',
	'Failed requests',
	'write errors',
	'Total transferred',
	'HTML transferred',
	'Requests per second',
	'Transfer rate'
    ]


def main( ):
	"""
	Take a bunch of files, with the output of ab (apache bench), 
		and a specific naming convention, 
		and create a string that can be used to display a graph
	"""
	tests = []
	keep_alive = OUTPUT_PATH + '-k'
	for file_name in glob.glob( OUTPUT_PATH + '*.ab' ):
		connection, requests, concurrent, server_name, dummy1, dummy2, rest = file_name.split( '_' )
		del( dummy1 )
		del( dummy2 )
		if connection == keep_alive :
			connection = 'Keep-Alive'
		else:
			connection = 'Close'
		file_size = rest[4:rest.find( "." )]
		fileH = open( file_name, 'r' )
		ab_content = fileH.read( )
		fileH.close( )
		test = {	'connection': connection, 
					'requests': requests, 
					'concurrent': concurrent, 
					'serverName': server_name, 
					'fileSize': file_size, 
					'params': {} }
		for parameter in PARAMETERS:
			test['params'][parameter] = 0
		for line in ab_content.splitlines( ):
			for parameter in PARAMETERS:
				plen = len( parameter )
				if line[:plen] == parameter:
					rest_line = line[plen + 1:].strip( ) + " " 
					value = rest_line[:rest_line.find( " " )]  
					test['params'][parameter] = value
					break
		tests.append( test ) 
	print "var BenchmarkDataSet ="
	pprint( tests )
	print( ";" )

main()

