#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"


char * Xstrdup( const char* str ) {
#ifdef POSIX
	//  http://stackoverflow.com/questions/482375/strdup-function
	return strdup( str );
#else
	char * tmp;

	tmp = malloc( strlen( str ) + 1 );
	if ( tmp == NULL )  {
		return NULL;
	}
	return strcpy( tmp, str );
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
		size = (size_t) ftell(fp );
		if ( ( contents = malloc( size + 1 ) ) != NULL ) {
			contents[size] = '\0';
			rewind( fp );
			fread( &contents[0], 1, size, fp );
		}
		fclose( fp );
	}

	return contents;
}

