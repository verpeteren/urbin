#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

int FullPath( char target[], const size_t size, const char * path, const char * filename ) {
	char * first;

	first = &target[0];
	memset( first, '\0', size );
	return snprintf( first, size, "%s%s%s", path, "/", filename );
}

char * Xstrdup( const char* str ) {
#ifdef POSIX
	//  http://stackoverflow.com/questions/482375/strdup-function
	return strdup( str );
#else
	return strcpy( malloc( strlen( str ) + 1), str );
#endif
}

char * FileGetContents( const char * fileName ) {
	FILE * fp;
	char * contents;
	size_t size;

	contents = NULL;
	fp = fopen( fileName, "rb" );
	if ( fp != NULL ) {
		fseek( fp, 0, SEEK_END );
		size = (size_t) ftell( fp );
		if ( ( contents = malloc( size + 1 ) ) != NULL ) {
			contents[size] = '\0';
			rewind( fp );
			fread( &contents[0], 1, size, fp );
		}
		fclose( fp );
	}

	return contents;
}

