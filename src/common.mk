include ./config.mk

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
DIR_ONIG = $(DEP_DIR)/onig-5.9.5
INC_ONIG = -isystem$(DIR_ONIG)/
LIB_ONIG = $(DIR_ONIG)/.libs/libonig.a
DIR_H3 = $(DEP_DIR)/h3
INC_H3 = -isystem$(DIR_H3)/include
LIB_H3 = $(DIR_H3)/libh3.a

INCS = $(INC_PICOEV) $(INC_H3) $(INC_ONIG)
LIBS = $(LIB_PICOEV) $(LIB_H3) $(LIB_ONIG)


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
