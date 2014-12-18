include ./Makefile_platform.mk
###############################################################################
# For the tweaker
###############################################################################
#HAS_MYSQL = yes
HAS_MYSQL = no

#MAKE_STATIC_LIBS =yes
MAKE_STATIC_LIBS = no

CC_FLAGS =
LD_FLAGS = 

###############################################################################
# For the master of the Makefiles
###############################################################################
AR = ar
RANLIB = ranlib
CTAGS = ctags
STRIP = strip
CC = gcc
#CC = clang

#sharedLibFileToParam = $(shell echo $1 )
sharedLibFileToParam = -L$(shell dirname $1) -l$(shell basename $1 .so|cut -b 4-)

OBJ_DIR = .objects
DEP_DIR = ../deps

VER_Z = 1.2.8
DIR_Z = $(DEP_DIR)/zlib-$(VER_Z)
INC_Z = -isystem$(DIR_Z)
LIB_Z_STATIC = $(DIR_Z)/libz.a
LIB_Z_SHARED = $(DIR_Z)/libz.so
DIR_PICOEV = $(DEP_DIR)/picoev
INC_PICOEV = -isystem$(DIR_PICOEV)/
LIB_PICOEV_STATIC = $(DIR_PICOEV)/libpicoev.a
LIB_PICOEV_SHARED = $(DIR_PICOEV)/libpicoev.so
DIR_H3 = $(DEP_DIR)/h3
INC_H3 = -isystem$(DIR_H3)/include
LIB_H3_STATIC = $(DIR_H3)/libh3.a
LIB_H3_SHARED = $(DIR_H3)/libh3.so
DIR_CLOG = $(DEP_DIR)/clog
INC_CLOG = -isystem$(DIR_CLOG)
LIB_CLOG_STATIC = $(DIR_CLOG)/libclog.a
LIB_CLOG_SHARED = $(DIR_CLOG)/libclog.so
VER_TADL = 1.1
DIR_TADL = $(DEP_DIR)/tadns-$(VER_TADL)
INC_TADL = -isystem$(DIR_TADL)
LIB_TADL_STATIC = $(DIR_TADL)/libtadns.a
LIB_TADL_SHARED = $(DIR_TADL)/libtadns.so
VER_ONIG = 5.9.5
DIR_ONIG = $(DEP_DIR)/onig-$(VER_ONIG)
INC_ONIG = -isystem$(DIR_ONIG)/
LIB_ONIG_STATIC = $(DIR_ONIG)/.libs/libonig.a
LIB_ONIG_SHARED = $(DIR_ONIG)/.libs/libonig.so
VER_PG = 9.4.0
DIR_PG = $(DEP_DIR)/postgresql-$(VER_PG)
INC_PG = -isystem$(DIR_PG)/src/interfaces/libpq/ -isystem$(DIR_PG)/src/include/ 
LIB_PG_STATIC = $(DIR_PG)/src/interfaces/libpq/libpq.a
LIB_PG_SHARED = $(DIR_PG)/src/interfaces/libpq/libpq.so
VER_CONF = 2.7
DIR_CONF = $(DEP_DIR)/confuse-$(VER_CONF)
INC_CONF = -isystem$(DIR_CONF)/src
LIB_CONF_STATIC = $(DIR_CONF)/src/.libs/libconfuse.a
LIB_CONF_SHARED = $(DIR_CONF)/src/.libs/libconfuse.so
VER_MOZ = 35.0b4
DIR_MOZ = $(DEP_DIR)/mozilla
INC_MOZ = -isystem$(DIR_MOZ)/js/src/build/dist/include/
LIB_MOZ_STATIC = $(DIR_MOZ)/js/src/build/dist/lib/libjs_static.a
LIB_MOZ_SHARED = $(DIR_MOZ)/js/src/build/dist/lib/libmozjs-$(shell echo $(VER_MOZ)|cut -d'.' -f1).so
VER_NSPR = 4.10.7
DIR_NSPR = $(DIR_MOZ)/nsprpub
INC_NSPR = -isystem$(DIR_NSPR)/dist/include/nspr
LIB_NSPR_STATIC = $(DIR_NSPR)/dist/lib/libnspr4.a
LIB_NSPR_SHARED = $(DIR_NSPR)/dist/lib/libnspr4.so
ifeq ($(HAS_MYSQL),yes)
HAVE_MYSQL = -DHAVE_MYSQL=1
#this stinks, 6.1.5 does not work with mysac 1.1.1 and 5.6.19 is only available in the debian archive, rather be using mysql-connector-c (Thanks oracle....):
VER_MY = 5.6.19
DIR_MY = $(DEP_DIR)/mysql-$(VER_MY)
INC_MY = -isystems$(DIR_MY)/include
LIB_MY_STATIC = $(DIR_MY)/libmysql/libmysqlclient.a 
LIB_MY_SHARED = $(DIR_MY)/libmysql/libmysqlclient.so
VER_MYS = 1.1.1
DIR_MYS = $(DEP_DIR)/mysac-$(VER_MYS)
INC_MYS = -isystem$(DIR_MYS)/
LIB_MYS_STATIC = $(DIR_MYS)/libmysac-static.a
LIB_MYS_SHARED = $(DIR_MYS)/libmysac.so
else
HAVE_MYSQL = -DHAVE_MYSQL=0
endif

DIRS = $(DIR_PICOEV) $(DIR_H3) $(DIR_CLOG) $(DIR_TADL) $(DIR_ONIG) $(DIR_PG) $(DIR_MOZ) $(DIR_NSPR) $(DIR_CONF) $(DIR_Z)
INCS = $(INC_PICOEV) $(INC_H3) $(INC_CLOG) $(INC_TADL) $(INC_ONIG) $(INC_PG) $(INC_MOZ) $(INC_NSPR) $(INC_CONF) $(INC_Z)
ifeq ($(MAKE_STATIC_LIBS),yes)
DEPS = $(LIB_PICOEV_STATIC) $(LIB_H3_STATIC) $(LIB_CLOG_STATIC) $(LIB_TADL_STATIC) $(LIB_ONIG_STATIC) $(LIB_PG_STATIC) $(LIB_MOZ_STATIC) $(LIB_NSPR_STATIC) $(LIB_CONF_STATIC) $(LIB_Z_STATIC)
LIBS = $(LIB_PICOEV_STATIC) $(LIB_H3_STATIC) $(LIB_CLOG_STATIC) $(LIB_TADL_STATIC) $(LIB_ONIG_STATIC) $(LIB_PG_STATIC) $(LIB_MOZ_STATIC) $(LIB_NSPR_STATIC) $(LIB_CONF_STATIC) $(LIB_Z_STATIC)
else
DEPS = $(LIB_PICOEV_SHARED) $(LIB_H3_SHARED) $(LIB_CLOG_SHARED) $(LIB_TADL_SHARED) $(LIB_ONIG_SHARED) $(LIB_PG_SHARED) $(LIB_MOZ_SHARED) $(LIB_NSPR_SHARED) $(LIB_CONF_SHARED) $(LIB_Z_SHARED)
LIBS = 	$(call sharedLibFileToParam, $(LIB_PICOEV_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_H3_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_CLOG_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_TADL_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_ONIG_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_PG_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_MOZ_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_NSPR_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_CONF_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_Z_SHARED)) 
endif

ifeq ($(HAS_MYSQL),yes)
DIRS += $(DIR_MYS) $(DIR_MY) 
INCS += $(INC_MYS) $(INC_MY) 
ifeq ($(MAKE_STATIC_LIBS),yes)
DEPS += $(LIB_MYS_STATIC) $(LIB_MY_STATIC) 
LIBS += $(LIB_MYS_STATIC) $(LIB_MY_STATIC) 
else
DEPS += $(LIB_MYS_SHARED) $(LIB_MY_SHARED) 
LIBS +=	$(call sharedLibFileToParam, $(LIB_MYS_SHARED)) \
		$(call sharedLibFileToParam, $(LIB_MY_SHARED)) 
endif
endif

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

all: 
