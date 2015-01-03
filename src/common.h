#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include <logging.h>

#include "./platform.h"

#define PR_NAME										"urbin"
#define PR_VERSION									"0.0.9"
#define PR_MAINCONFIG_FILENAME						"./etc/" PR_NAME ".conf"
#define PR_CFG_LOOP_MAX_FDS							1024						//  cfg:  main/loop_max_fds
#define PR_CFG_LOOP_RUN_AS_USER						"daemon"					//  cfg:  main/loop_run_as_user
#define PR_CFG_LOOP_RUN_AS_GROUP					"daemon"					//  cfg:  main/loop_run_as_group
#define PR_CFG_LOOP_TIMEOUT_SEC						60							//  cfg:  main/loop_timeout_sec
#define PR_CFG_LOOP_TICKS_MS						50							//  cfg:  main/loop_ticks_ms
#define PR_CFG_LOOP_MAX_WAIT						10							//  cfg:  main/loop_max_wait
#define PR_CFG_LOOP_DAEMON							cfg_false					//  cfg:  main/loop_daemon
#define PR_CFG_LOOP_LOG_LEVEL_TEXT					"info"						//	cfg:  main/loop_log_level
#define PR_LOG_LEVEL_VALUE							LOG_INFO
#define PR_CFG_GLOT_PATH							"../var/scripts/"			//  cfg:  glot/../path
#define PR_CFG_GLOT_MAIN							PR_NAME						//  cfg:  glot/../main
#define PR_CFG_MODULES_WEBSERVER_ROOT				"../var/www/"				//  cfg:  modules/WEBSERVER/documentroot
#define PR_CFG_MODULES_WEBSERVER_PATH				"/static/(.*)"				//  cfg:  modules/WEBSERVER/path
#define PR_CFG_MODULES_WEBSERVER_IP					"localhost"					//  cfg:  modules/WEBSERVER/ip
#define PR_CFG_MODULES_WEBSERVER_PORT				8080						//  cfg:  modules/WEBSERVER/port
#define PR_CFG_MODULES_WEBSERVER_TIMEOUT_SEC		10 							//  cfg:  modules/WEBSERVER/timeout_sec
#define PR_CFG_MODULES_WEBSERVER_LISTEN_BACKLOG		PR_CFG_LOOP_MAX_FDS			//  cfg:  modules/WEBSERVER/listen_backlog
#define PR_CFG_MODULES_MYSQLCLIENT_DATABASE			"mysql"						//  cfg:  modules/MYSQLCLIENT/database
#define PR_CFG_MODULES_MYSQLCLIENT_IP				"127.0.0.1"					//  cfg:  modules/MYSQLCLIENT/ip
#define PR_CFG_MODULES_MYSQLCLIENT_PORT				3306						//  cfg:  modules/MYSQLCLIENT/port
#define PR_CFG_MODULES_MYSQLCLIENT_TIMEOUT_SEC		10							//  cfg:  modules/MYSQLCLIENT/timeout_sec
#define PR_CFG_MODULES_PGSQLCLIENT_DATABASE			"postgresql"				//  cfg:  modules/PGSQLCLIENT/database
#define PR_CFG_MODULES_PGSQLCLIENT_IP				"127.0.0.1"					//  cfg:  modules/PGSQLCLIENT/ip
#define PR_CFG_MODULES_PGSQLCLIENT_PORT				5432						//  cfg:  modules/PGSQLCLIENT/port
#define PR_CFG_MODULES_PGSQLCLIENT_TIMEOUT_SEC		10							//  cfg:  modules/PGSQLCLIENT/timeout_sec

#endif // __SRC_COMMON_H_
