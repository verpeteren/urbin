#ifndef SRC_CORE_UTILS_H_
#define SRC_CORE_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif


int                            FullPath                      ( char target[], size_t size, const char * path, const char * filename );
char *                         FileGetContents               ( const char * fileName );

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_UTILS_H_
