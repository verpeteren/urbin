#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include "../platform.h"

#define __RONJA_COMMON_H_


  //  @TODO:  get rid of these 3 fixed items
#define MAX_FDS 									1024
#define LOOP_TIMEOUT								60
#define LISTEN_BACKLOG								100

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
#define PR_CFG_MODULES_WEBSERVER_TIMEOUT_SEC		10 							//  cfg:  modules/WEBSERVER/timeout_sec
#define PR_CFG_MODULES_MYSQLCLIENT_DATABASE			"mysql"						//  cfg:  modules/MYSQLCLIENT/database
#define PR_CFG_MODULES_MYSQLCLIENT_IP				"127.0.0.1"					//  cfg:  modules/MYSQLCLIENT/ip
#define PR_CFG_MODULES_MYSQLCLIENT_PORT				3306						//  cfg:  modules/MYSQLCLIENT/port
#define PR_CFG_MODULES_MYSQLCLIENT_TIMEOUT_SEC		10							//  cfg:  modules/MYSQLCLIENT/timeout_sec
#define PR_CFG_MODULES_PGSQLCLIENT_DATABASE			"postgresql"				//  cfg:  modules/PGSQLCLIENT/database
#define PR_CFG_MODULES_PGSQLCLIENT_IP				"127.0.0.1"					//  cfg:  modules/PGSQLCLIENT/ip
#define PR_CFG_MODULES_PGSQLCLIENT_PORT				5432						//  cfg:  modules/PGSQLCLIENT/port
#define PR_CFG_MODULES_PGSQLCLIENT_TIMEOUT_SEC		10							//  cfg:  modules/PGSQLCLIENT/timeout_sec
#endif // __SRC_COMMON_H_
