#ifndef SRC_CORE_UTILS_H_
#define SRC_CORE_UTILS_H_

#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STRING_LENGTH_OF_INT( value ) (size_t) ( value == 0 ? 1 : ( log10( value ) + 1 ) )

char * 								Xstrdup							( const char* str );
int									FullPath						( char target[], const size_t size, const char * path, const char * filename );
char *								FileGetContents					( const char * fileName );

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_UTILS_H_
