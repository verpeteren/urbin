#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include <logging.h>

#include "./platform.h"

#define PR_NAME										"urbin"
#define PR_VERSION									"0.0.10"
#define PR_MAINCONFIG_FILENAME						"./etc/" PR_NAME ".conf"
#define PR_CFG_CORE_MAX_FDS							1024
#define PR_CFG_CORE_RUN_AS_USER						"daemon"
#define PR_CFG_CORE_RUN_AS_GROUP					"daemon"
#define PR_CFG_CORE_TIMEOUT_SEC						60
#define PR_CFG_CORE_TICKS_MS						50
#define PR_CFG_CORE_MAX_WAIT						10
#define PR_CFG_CORE_DAEMON							PR_TRUE
#define PR_CFG_CORE_LOG_LEVEL_TEXT					"info"
#define PR_LOG_LEVEL_VALUE							LOG_INFO
#define PR_CFG_GLOT_PATH							"../var/scripts/"
#define PR_CFG_GLOT_MAIN							PR_NAME
#define PR_CFG_MODULES_WEBSERVER_ROOT				"../var/www/"
#define PR_CFG_MODULES_WEBSERVER_PATH				"/static/(.*)"
#define PR_CFG_MODULES_WEBSERVER_HOSTNAME			"localhost"
#define PR_CFG_MODULES_WEBSERVER_PORT				8080
#define PR_CFG_MODULES_WEBSERVER_TIMEOUT_SEC		10
#define PR_CFG_MODULES_WEBSERVER_LISTEN_BACKLOG		PR_CFG_CORE_MAX_FDS
#define PR_CFG_MODULES_WEBCLIENT_TIMEOUT_SEC		10
#define PR_CFG_MODULES_MYSQLCLIENT_DATABASE			"mysql"
#define PR_CFG_MODULES_MYSQLCLIENT_HOSTNAME			"localhost"
#define PR_CFG_MODULES_MYSQLCLIENT_PORT				3306
#define PR_CFG_MODULES_MYSQLCLIENT_TIMEOUT_SEC		10
#define PR_CFG_MODULES_PGSQLCLIENT_DATABASE			"postgresql"
#define PR_CFG_MODULES_PGSQLCLIENT_HOSTNAME			"localhost"
#define PR_CFG_MODULES_PGSQLCLIENT_PORT				5432
#define PR_CFG_MODULES_PGSQLCLIENT_TIMEOUT_SEC		10

#endif // __SRC_COMMON_H_
