#!/bin/bash

DEBUG=yes

MK_PLATFORM=./src/Makefile_platform.mk
C_PLATFORM=./src/platform.h
rm -f ${MK_PLATFORM} ${C_PLATFORM}


if [ -n "$1" ] && [ "$1" = "clean" ]; then
	cd ./src
	make -f Makefile.dependencies clean
	make clean
else
	OS_TARGET=`uname -s`
	case "$OS_TARGET" in
		linux* | Linux*)
			HOST_OS=Linux
			echo "LINUX_BUILD = 1" > ${MK_PLATFORM}
			;;
		Darwin*)
			HOST_OS=Darwin
			echo "DARWIN_BUILD = 1" > ${MK_PLATFORM}
			;;
		*)
			HOST_OS=Linux
			echo "GENERIC_BUILD = 1" > ${MK_PLATFORM}
			;;
	esac
	if [ "$DEBUG" == "yes" ]; then
		echo -e "STAGING_DEBUG = 1" >> ${MK_PLATFORM}
		echo -e "_STAGING_RELEASE = 1" >> ${MK_PLATFORM}
		echo -e "#ifndef SRC_PLATFORM_H_\n#define SRC_PLATFORM_H_\n\n\n" > ${C_PLATFORM}
		echo -e "#define DEBUG 1" >> ${C_PLATFORM}
		echo -e "//  #define NDEBUG 1" >> ${C_PLATFORM}
		echo -e "\n\n#endif  // SRC_PLATFORM_H_\n" >> ${C_PLATFORM}
	else
		echo -e "_STAGING_DEBUG = 1" >> ${MK_PLATFORM}
		echo -e "STAGING_RELEASE = 1" >> ${MK_PLATFORM}
		echo -e "#ifndef SRC_PLATFORM_H_\n#define SRC_PLATFORM_H_\n\n\n" > ${C_PLATFORM}
		echo -e "//  #define DEBUG 1" >> ${C_PLATFORM}
		echo -e "#define NDEBUG 1" >> ${C_PLATFORM}
		echo -e "\n\n#endif  // SRC_PLATFORM_H_\n" >> ${C_PLATFORM}
	fi
	cd ./src
	make -f Makefile.dependencies && mmake
fi

# vim: ts=4 sts=4 sw=4 noet nu

