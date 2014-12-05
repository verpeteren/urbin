#ifndef SRC_CORE_H_
#define SRC_CORE_H_

#include <picoev.h>

#include "../common.h"

struct core_t {
	picoev_loop *				loop;
	unsigned int				keepOnRunning:1;
};

void							Boot					( );
void							Shutdown				( );

struct core_t *					Core_New				( );
void							Core_Loop				( struct core_t * core );
void							Core_Delete				( struct core_t * core );

#endif  // SRC_CORE_H_

