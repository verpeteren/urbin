include ./Makefile_platform.mk

AR = ar
RANLIB = ranlib
CTAGS = ctags
#CC = clang
CC = gcc

OBJ_DIR = .objects
DEP_DIR = ../deps

#HAS_MYSQL = "yes"
HAS_MYSQL = "no"

DIR_PICOEV = $(DEP_DIR)/picoev
INC_PICOEV = -isystem$(DIR_PICOEV)/
LIB_PICOEV = $(DIR_PICOEV)/libpicoev.a
DIR_H3 = $(DEP_DIR)/h3
INC_H3 = -isystem$(DIR_H3)/include
LIB_H3 = $(DIR_H3)/libh3.a
DIR_CLOG = $(DEP_DIR)/clog
INC_CLOG = -isystem$(DIR_CLOG)
LIB_CLOG = $(DIR_CLOG)/libclog.a
VER_ONIG = 5.9.5
DIR_ONIG = $(DEP_DIR)/onig-$(VER_ONIG)
INC_ONIG = -isystem$(DIR_ONIG)/
LIB_ONIG = $(DIR_ONIG)/.libs/libonig.a
VER_PG = 9.4rc1
DIR_PG = $(DEP_DIR)/postgresql-$(VER_PG)
INC_PG = -isystem$(DIR_PG)/src/interfaces/libpq/ -isystem$(DIR_PG)/src/include/ 
LIB_PG = $(DIR_PG)/src/interfaces/libpq/libpq.a
VER_CONF = 2.7
DIR_CONF = $(DEP_DIR)/confuse-$(VER_CONF)
INC_CONF = -isystem$(DIR_CONF)/src
LIB_CONF = $(DIR_CONF)/src/.libs/libconfuse.a
ifeq ($(HAS_MYSQL), yes)
HAVE_MYSQL = -DHAVE_MYSQL=1
#this stinks, 6.1.5 does not work with mysac 1.1.1 and 5.6.19 is only available in the debian archive, rather be using mysql-connector-c (Thanks oracle....):
VER_MY = 5.6.19
DIR_MY = $(DEP_DIR)/mysql-$(VER_MY)
INC_MY = -isystems$(DIR_MY)/include
LIB_MY = $(DIR_MY)/libmysql/libmysqlclient.a 
VER_MYS = 1.1.1
DIR_MYS = $(DEP_DIR)/mysac-$(VER_MYS)
INC_MYS = -isystem$(DIR_MYS)/
LIB_MYS = $(DIR_MYS)/libmysac-static.a
else
HAVE_MYSQL = -DHAVE_MYSQL=0
endif
VER_MOZ = 35.0b1
DIR_MOZ = $(DEP_DIR)/mozilla
INC_MOZ = -isystem$(DIR_MOZ)/js/src/build/dist/include/
LIB_MOZ = $(DIR_MOZ)/js/src/build/dist/lib/libjs_static.a
VER_NSPR = 4.10.7
DIR_NSPR = $(DIR_MOZ)/nsprpub
INC_NSPR = -isystem$(DIR_NSPR)/dist/include/nspr
LIB_NSPR = $(DIR_NSPR)/dist/lib/libnspr4.a

DIRS = $(DIR_PICOEV) $(DIR_H3) $(DIR_CLOG) $(DIR_ONIG) $(DIR_PG) $(DIR_MYS) $(DIR_MY) $(DIR_MOZ) $(DIR_NSPR) $(DIR_CONF)
INCS = $(INC_PICOEV) $(INC_H3) $(INC_CLOG) $(INC_ONIG) $(INC_PG) $(INC_MYS) $(INC_MY) $(INC_MOZ) $(INC_NSPR) $(INC_CONF)
LIBS = $(LIB_PICOEV) $(LIB_H3) $(LIB_CLOG) $(LIB_ONIG) $(LIB_PG) $(LIB_MYS) $(LIB_MY) $(LIB_MOZ) $(LIB_NSPR) $(LIB_CONF)


GET_OFF_MY_LAWN = -Wall -Werror -Wextra -Wfatal-errors \
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

