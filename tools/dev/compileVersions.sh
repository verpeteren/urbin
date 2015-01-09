#!/bin/bash

function compileVersions() {
	CCs=(gcc clang)
	HAS_MYSQLs=(no, yes)
	MAKE_STATIC_LIBSs=(no yes)
	OPTIMIZATION_LEVELs=('-O0' '-O2')

	for make_static_lib in ${MAKE_STATIC_LIBSs[@]}; do
		for has_mysql in ${HAS_MYSQLs[@]}; do
			for cc in ${CCs[@]}; do
				for optimization_level in ${OPTIMIZATION_LEVELs[@]}; do
					make clean
					echo make -j2 CC=${cc} HAS_MYSQL=${has_mysql} MAKE_STATIC_LIBS=${make_static_lib} OPTIMIZATION_LEVEL=${optimization_level}
					make -j2 CC=${cc} HAS_MYSQL=${has_mysql} MAKE_STATIC_LIBS=${make_static_lib} OPTIMIZATION_LEVEL=${optimization_level}
					if (($? > 0)); then
						echo stopping now
						exit 1;
					fi
				done
			done
		done
	done
}

function whitespace {
	grep "if(" */*
	grep "for(" */*
	grep -n "while(" */*
	grep -n "catch(" */*
	grep -n "do(" */*
	grep -n "switch(" */*
	grep -n "((" */*
	grep -n "){" */*
	grep -n " ;" */*
	grep -n ")[a-zA-Z0-9]" */*
	grep -n "[^ ]?[^ ]" */*
	grep -n "([^\t ]" */*|grep -v "\(char *\)"|grep -v "\(uint32_t\)"|grep -v "\(struct javascript_t *\)"|grep -v "\(struct core_t *\)"|grep -v "\(struct script_t *\)"|grep -v "\(void *\)"|grep -v "\(struct sqlclient_t *\)"|grep -v "\(struct webserver_t *\)"|grep -v "\(struct payload_t *\)"|grep -v "\(PGresult *\)"|grep -v "\(MYSAC_RES *\)"|grep -v "\(int\)"|grep -v "\(int32_t\)"|grep -v "\(cfg_t *\)"|grep -v "\(\.\*\)"|grep -v "\(size_t\)"|grep -v "\(cfg_opt_t *\)"|grep -v "\(unsigned char\)"| grep -v "\(MYSAC_BIND *\)"|grep -v "\(dynamicHandler_cb_t\)"|grep -v "\(struct sockaddr *\)"|grep -v "\(struct sockaddr_in *\)"|grep -v "\(struct webserverclient_t *\)"|grep -v "\(struct webserverclientresponse_t *\)"|grep -v "\(struct namedRegex_t *\)"|grep -v "\(JSObject *\)"| grep -v "\(PRCList *\)"|grep -v "\(jsval *\)"
	grep -n "[^\t ])" */*|grep -v "\(char *\)"|grep -v "\(uint32_t\)"|grep -v "\(struct javascript_t *\)"|grep -v "\(struct core_t *\)"|grep -v "\(struct script_t *\)"|grep -v "\(void *\)"|grep -v "\(struct sqlclient_t *\)"|grep -v "\(struct webserver_t *\)"|grep -v "\(struct payload_t *\)"|grep -v "\(PGresult *\)"|grep -v "\(MYSAC_RES *\)"|grep -v "\(int\)"|grep -v "\(int32_t\)"|grep -v "\(cfg_t *\)"|grep -v "\(\.\*\)"|grep -v "\(size_t\)"|grep -v "\(cfg_opt_t *\)"|grep -v "\(unsigned char\)"| grep -v "\(MYSAC_BIND *\)"|grep -v "\(dynamicHandler_cb_t\)"|grep -v "\(struct sockaddr *\)"|grep -v "\(struct sockaddr_in *\)"|grep -v "\(struct webserverclient_t *\)"|grep -v "\(struct webserverclientresponse_t *\)"|grep -v "\(struct namedRegex_t *\)"|grep -v "\(JSObject *\)"| grep -v "\(PRCList *\)"|grep -v "\(jsval *\)"

	grep "[^ \t]:[^1: \t]" */*|grep -v "::"|grep -v "case "|grep -vi "%s"|grep -v "%2d"|grep -v "http://"|grep -v "https://"
	grep -n "[a-zA-Z0-9] (" */*|grep -v "if"|grep -v "while"|grep -v return |grep -v for|grep -v catch|grep -v switch
	grep -n " ;" */*
	grep -n "[^ \t]=" */*|grep -v "=="|grep -v "!="|grep -v "+="|grep -v "=%"|grep -v ">="
	grep -n "=[^ \t]" */*|grep -v "=="|grep -v "!="|grep -v "+="|grep -v "=%"|grep -v ">="
	grep -n free */*|grep -v NULL|grep -v Core_Log
}

function valgr {
	cd ../bin
	valgrind --leak-check=full --show-reachable=yes --track-origins=yes --show-possibly-lost=yes --suppressions=../tools/dev/valgrind.sup --malloc-fill=0xFF --free-fill=0xFF ./urbin
}

make check
whitespace 
compileVersions
valgr

