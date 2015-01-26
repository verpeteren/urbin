###############################################################################
# For the tweaker
###############################################################################
ifndef CC
CC = gcc
#CC = clang
endif

ifndef HAS_MYSQL
HAS_MYSQL = yes
#HAS_MYSQL = no
endif

ifndef MAKE_STATIC_LIBS
#MAKE_STATIC_LIBS = yes
MAKE_STATIC_LIBS = no
endif

ifeq ($(STAGING_RELEASE),yes)
ifndef OPTIMIZATION_LEVEL
OPTIMIZATION_LEVEL = -O2
endif
CC_RELEASE_FLAGS = 
LD_RELEASE_FLAGS =  
else
ifndef OPTIMIZATION_LEVEL
OPTIMIZATION_LEVEL = -O0
endif
CC_DEBUG_FLAGS = 
LD_DEBUG_FLAGS = 
endif

CC_FLAGS =
LD_FLAGS = 

