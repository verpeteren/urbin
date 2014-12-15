#ifndef SRC_GLOT_JAVASCRIPT_H_
#define SRC_GLOT_JAVASCRIPT_H_

/*  kludge to avoid compile errors  */
#ifndef SIZE_MAX
#define UINT32_MAX ( (uint32_t) - 1 )
#define SIZE_MAX UINT32_MAX
#endif
#include <jsapi.h>

#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct javascript_t {
	struct core_t *				core;
	const char *				path;
	JSRuntime *					runtime;
	JSContext *					context;
	JS::PersistentRootedObject	globalObj;
};

struct javascript_t *			Javascript_New						( const struct core_t * core, const char * path, const char * fileName );
void							Javascript_Delete					( struct javascript_t * javascript );

void *							JavascriptModule_Load				( const struct core_t * core );
void							JavascriptModule_Ready				( const struct core_t * core, void * args );
void							JavascriptModule_Unload				( const struct core_t * core, void * args );

#ifdef __cplusplus
}
#endif

#endif  // SRC_GLOT_JAVASCRIPT_H_
