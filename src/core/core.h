#ifndef SRC_CORE_H_
#define SRC_CORE_H_

#include <picoev.h>

#include "../common.h"

struct Core {
	picoev_loop *				loop;
	int							keepOnRunning;
};

void							Boot				( );
void							Shutdown			( );

struct Core *					Core_New			( );
void							Core_Loop			( struct Core * core );
void							Core_Delete			( struct Core * core );

#endif  // SRC_CORE_H_

