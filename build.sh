#!/bin/bash

DEBUG=yes

PLATFORM=./src/config.mk
CONFIG=./src/config.h
rm -f ${PLATFORM} ${CONFIG}


if [ -n "$1" ] && [ "$1" = "clean" ]; then
	cd ./src
	make -f Makefile.dependencies clean
	make clean
else
	OS_TARGET=`uname -s`
	case "$OS_TARGET" in
		linux* | Linux*)
			HOST_OS=Linux
			echo "LINUX_BUILD = 1" > ${PLATFORM}
			;;
		Darwin*)
			HOST_OS=Darwin
			echo "DARWIN_BUILD = 1" > ${PLATFORM}
			;;
		*)
			HOST_OS=Linux
			echo "GENERIC_BUILD = 1" > ${PLATFORM}
			;;
	esac
	if [ "$DEBUG" == "yes" ]; then
		echo -e "STAGING_DEBUG = 1" >> ${PLATFORM}
		echo -e "_STAGING_RELEASE = 1" >> ${PLATFORM}
		echo -e "#ifndef SRC_CONFIG_H_\n#define SRC_CONFIG_H_\n\n\n" > ${CONFIG}
		echo -e "#define DEBUG 1" >> ${CONFIG}
		echo -e "//  #define NDEBUG 1" >> ${CONFIG}
		echo -e "\n\n#endif  // SRC_COMMON_H_\n" >> ${CONFIG}
	else
		echo -e "_STAGING_DEBUG = 1" >> ${PLATFORM}
		echo -e "STAGING_RELEASE = 1" >> ${PLATFORM}
		echo -e "#ifndef SRC_CONFIG_H_\n#define SRC_CONFIG_H_\n\n\n" > ${CONFIG}
		echo -e "//  #define DEBUG 1" >> ${CONFIG}
		echo -e "#define NDEBUG 1" >> ${CONFIG}
		echo -e "\n\n#endif  // SRC_CONFIG_H_\n" >> ${CONFIG}
	fi
	cd ./src
	make -f Makefile.dependencies && mmake
fi

# vim: ts=4 sts=4 sw=4 noet nu

