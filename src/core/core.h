#ifndef SRC_CORE_CORE_H_
#define SRC_CORE_CORE_H_

#include <picoev.h>

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FEATURE_JOINCORE( Feat, feature ) \
void Feat##_JoinCore( struct feature##_t * feature ) { \
	picoev_add( feature->core->loop, feature->socketFd, PICOEV_READ, 0, Feat##_HandleAccept_cb, (void * ) feature ); \
}

struct core_t {
	picoev_loop *				loop;
	unsigned int				keepOnRunning:1;
};

void							Boot					( );
void							Shutdown				( );
void							SetupSocket				( int fd );

struct core_t *					Core_New				( );
void							Core_Loop				( struct core_t * core );
void							Core_Delete				( struct core_t * core );

#ifdef __cplusplus
}
#endif

#endif  // SRC_CORE_CORE_H_

