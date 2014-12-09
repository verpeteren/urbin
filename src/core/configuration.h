#ifndef SRC_CORE_CONFIGURATION_H_
#define SRC_CORE_CONFIGURATION_H_

#include <confuse.h>

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

void								Usage					( const char * prog_name, int code ) __attribute__( ( noreturn ) );
cfg_t *								ProcessCommandline			( int argc, const char ** argv );

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_CONFIGURATION_H_

