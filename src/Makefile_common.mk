include ./Makefile_platform.mk

AR = ar
RANLIB = ranlib
CTAGS = ctags
CC = gcc
#CC = clang

OBJ_DIR = .objects
DEP_DIR = ./deps

DIR_PICOEV = $(DEP_DIR)/picoev
INC_PICOEV = -isystem$(DIR_PICOEV)/
LIB_PICOEV = $(DIR_PICOEV)/libpicoev.a
VER_ONIG = 5.9.5
DIR_ONIG = $(DEP_DIR)/onig-$(VER_ONIG)
INC_ONIG = -isystem$(DIR_ONIG)/
LIB_ONIG = $(DIR_ONIG)/.libs/libonig.a
DIR_H3 = $(DEP_DIR)/h3
INC_H3 = -isystem$(DIR_H3)/include
LIB_H3 = $(DIR_H3)/libh3.a
VER_PG = 9.4rc1
DIR_PG = $(DEP_DIR)/postgresql-$(VER_PG)
INC_PG = -isystem$(DIR_PG)/src/interfaces/libpq/ -isystem$(DIR_PG)/src/include/ 
LIB_PG = $(DIR_PG)/src/interfaces/libpq/libpq.a
#VER_MY = 5.5.40
#this stinks, (Thanks oracle):
#	5.5.40 does  works with mysac, but cannot be downloaded! thanks oracle!
#	6.1.5 does not work with mysac, but can be downloadedi
# The workaround: we use "sudo apt-get install libmysqlclient-dev"
VER_MY = 6.1.5
DIR_MY = $(DEP_DIR)/mysql-connector-c-$(VER_MY)-src
INC_MY = -isystems$(DIR_MY)/include
LIB_MY = $(DIR_MY)/libmysql/mysqlclient.a 
VER_MYS = 1.1.1
DIR_MYS = $(DEP_DIR)/mysac-$(VER_MYS)
INC_MYS = -isystem$(DIR_MYS)/
LIB_MYS = $(DIR_MYS)/libmysac-static.a
VER_MOZ = 34.0.5
DIR_MOZ = $(DEP_DIR)/mozilla-release
INC_MOZ = -isystem$(DIR_MOZ)/js/src/build-beta/dist/include/
LIB_MOZ = $(DIR_MOZ)/js/src/build-beta/dist/lib/libjs_static.a
VER_NSPR = 4.10.7
DIR_NSPR = $(DIR_MOZ)/nsprpub
INC_NSPR = -isystem$(DIR_NSPR)/dist/include/nspr
LIB_NSPR = $(DIR_NSPR)/dist/lib/lilbnspr4.a

INCS = $(INC_PICOEV) $(INC_H3) $(INC_ONIG) $(INC_PG) $(INC_MYS) $(INC_MY) $(INC_MOZ) $(INC_NSPR)
LIBS = $(LIB_PICOEV) $(LIB_H3) $(LIB_ONIG) $(LIB_PG) $(LIB_MYS) $(LIB_MY) $(LIB_MOZ) $(LIB_NSPR)


GET_OFF_MY_LAWN = -std=gnu99 -Wall -Werror -Wextra -Wfatal-errors \
	-Wunreachable-code \
	-Wpointer-arith \
	-Wdiv-by-zero \
	-Wconversion \
	-Wmultichar \
	-Winit-self \
	-Wdeprecated \
	-Wmultichar \
	-Wmissing-braces \
	-Wmissing-noreturn \
	-Wmissing-declarations \
	-Waggregate-return \
	-Winline \
	-Wredundant-decls \
	-Wwrite-strings \
	-Wcast-align \
	-Wshadow \
	-Wswitch-default -Wswitch-enum \
	-Wformat-nonliteral -Wformat-security -Wformat=2 \
	-Wendif-labels -Wundef -Wimport -Wunused-macros \
	-Wstrict-aliasing=1 \
	-Wno-padded \
	-Wno-unused-parameter \
	-Wpacked \
	-ftrapv
	
ifeq ($(CC),gcc)
	GET_OFF_MY_LAWN += -Wsuggest-attribute=pure -Wsuggest-attribute=const -Wsuggest-attribute=noreturn \
	-pie -fPIE
else
	
endif

# vim: ts=4 sts=4 sw=4 noet nu
