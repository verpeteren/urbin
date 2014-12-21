#!/bin/bash

DEBUG=yes
#DEBUG=no

MK_PLATFORM=./src/Makefile_platform.mk
C_PLATFORM=./src/platform.h
rm -f ${MK_PLATFORM} ${C_PLATFORM}


if [ -n "$1" ] && [ "$1" = "clean" ]; then
	cd ./src
	make -f Makefile_dependencies.mk clean
	make clean
else
	OS_TARGET=`uname -s`
	case "$OS_TARGET" in
		linux* | Linux*)
			HOST_MK="LINUX_BUILD = 1"
			;;
		Darwin*)
			HOST_MK="DARWIN_BUILD = 1"
			;;
		*)
			HOST_MK="GENERIC_BUILD = 1"
			;;
	esac
	
	if [ "$DEBUG" == "yes" ]; then
		DEBUG_MK="STAGING_DEBUG = 1"
		RELEASE_MK="STAGING_RELEASE = 0"
		DEBUG_C="#define DEBUG 1"
		NDEBUG_C="//	#define NDEBUG 1"
	else
		DEBUG_MK="STAGING_DEBUG = 0"
		RELEASE_MK="STAGING_RELEASE = 1"
		DEBUG_C="//  #define DEBUG 0"
		NDEBUG_C="#define NDEBUG 1"
	fi	

	cat <<-ENDING > ${MK_PLATFORM}
		${HOST_MK}
		${DEBUG_MK}
		${RELEASE_MK}

ENDING

	cat <<-ENDING > ${C_PLATFORM}
		#ifndef SRC_PLATFORM_H_
		#define SRC_PLATFORM_H_
		
		${DEBUG_C}
		${NDEBUG_C}
		
		#endif  // SRC_PLATFORM_H_
	
ENDING

fi

	cat <<-ENDING
		#Get and compile the dependencies (may take some time)
		cd ./src\n
		make deps 		
		#Then compile
		make all
		#Then configure
		cd ../bin
		vi ./etc/ronja.conf
		#then hack
		vi ../var/scripts/javascript/main.hard.js
		#Then run
		./ronja
		
ENDING

