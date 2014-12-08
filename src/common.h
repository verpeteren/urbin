#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include "../platform.h"

#define __RONJA_COMMON_H_

#define MAX_FDS 									1024
#define LOOP_TIMEOUT								60
#define LISTEN_BACKLOG								100

//  @TODO: get rid of these 2 configs
#define SQLCLIENT_TIMEOUT_SEC						10
#define WEBSERVER_TIMEOUT_SEC						10

#define PR_NAME										"ronja"
#define PR_VERSION									"0.0.8a"
#define PR_MAINCONFIG_FILENAME						"./etc/" PR_NAME ".conf"
#define PR_CFG_MAX_FDS								MAX_FDS						//  cfg:  main/max_file_descriptors
#define PR_CFG_GLOT_PATH							"../var/scripts/"			//  cfg:  glot/../path
#define PR_CFG_GLOT_MAIN							PR_NAME						//  cfg:  glot/../main
#define PR_CFG_MODULES_WEBSERVER_ROOT				"../var/www/"				//  cfg:  modules/WEBSERVER/documentroot
#define PR_CFG_MODULES_WEBSERVER_PATH				"/static/(.*)"				//  cfg:  modules/WEBSERVER/path
#define PR_CFG_MODULES_WEBSERVER_IP					"localhost"					//  cfg:  modules/WEBSERVER/ip
#define PR_CFG_MODULES_WEBSERVER_PORT				8080						//  cfg:  modules/WEBSERVER/port
#define PR_CFG_MODULES_WEBSERVER_TIMEOUT_SEC		WEBSERVER_TIMEOUT_SEC		//  cfg:  modules/WEBSERVER/timeout_sec
#endif // __SRC_COMMON_H_
