#ifndef SRC_GLOT_JAVASCRIPT_H_
#define SRC_GLOT_JAVASCRIPT_H_

/*  kludge to avoid compile errors  */
#ifndef SIZE_MAX
#define UINT32_MAX 			( (uint32_t) - 1 )
#define SIZE_MAX 				UINT32_MAX
#endif
#include <jsapi.h>
#include <prclist.h>

#include "../core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _Script_t Script_t;

typedef struct _Javascript_t {
	Core_t *					core;
	const char *				path;
	const char *				fileName;
	JSRuntime *					runtime;
	JSContext *					context;
	Script_t *					scripts;
	JSObject * 					globalObj;
} Javascript_t;

Javascript_t *					Javascript_New						( const Core_t * core, const char * path, const char * fileName );
void							Javascript_Delete					( Javascript_t * javascript );

PRStatus		 				JavascriptModule_Load				( const Core_t * core, Module_t * module, void * args );
PRStatus						JavascriptModule_Ready				( const Core_t * core, Module_t * module, void * args );
PRStatus						JavascriptModule_Unload				( const Core_t * core, Module_t * module, void * args );

#ifdef __cplusplus
}
#endif

#endif  // SRC_GLOT_JAVASCRIPT_H_
