#ifndef SRC_CORE_UTILS_H_
#define SRC_CORE_UTILS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char * 								Xstrdup							( const char* str );
int									FullPath						( char target[], const size_t size, const char * path, const char * filename );
char *								FileGetContents					( const char * fileName );

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_UTILS_H_
