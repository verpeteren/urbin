include ./common.mk

HEADERS = common.h config.h
OPTMIZATIONLEVEL = -O0

CC_DEBUG_FLAGS =  $(GET_OFF_MY_LAWN)
CC_RELEASE_FLAG =  $(GET_OFF_MY_LAWN)
LD_DEBUG_FLAGS  = -Wl,-z,relro -Wl,-z,now
LD_RELEASE_FLAGS = -Wl,-z,relro -Wl,-z,now
 
ifdef STAGING_DEBUG
CC_DEBUG_FLAGS += -g -g3 -ggdb \
	-Wstack-protector -fstack-protector-all \
	-D_FORTIFY_SOURCE=2 $(OPTMIZATIONLEVEL)
LD_DEBUG_FLAGS +=  
ifeq ($(CC),gcc)
CC_DEBUG_FLAGS += -pg -fno-builtin-strlen --param ssp-buffer-size=4
else
 
endif
endif

ifdef STAGING_RELEASE
CC_RELEASE_FLAGS = $(OPTMIZATIONLEVEL)
LD_RELEASE_FLAGS = $(OPTMIZATIONLEVEL)
ifeq ($(CC),gcc)
else 
endif

endif
CFLAGS = $(CC_RELEASE_FLAGS) $(CC_DEBUG_FLAGS)
LDFLAGS = $(LD_RELEASE_FLAGS) $(LD_DEBUG_FLAGS)
HAVES = 



CORE_DIR=./core
FEAT_DIR=./feature
BIN_DIR=../bin

CODE_OBJECTS = $(OBJ_DIR)/core.o
FEAT_OBJECTS = $(OBJ_DIR)/webserver.o
MAIN_OBJECTS = $(OBJ_DIR)/main_server.o

OBJECTS = $(CODE_OBJECTS) $(MAIN_OBJECTS) $(FEAT_OBJECTS)

THE_EXECUTABLE = $(BIN_DIR)/server

all: 	$(THE_EXECUTABLE) 
tests:	$(TST_EXECUTABLE) 
docs:   ./doc/Api.txt

$(OBJ_DIR)/main_server.o:			EXCEPTION =
$(OBJ_DIR)/core.o:					EXCEPTION =
$(OBJ_DIR)/webserver.o	:			EXCEPTION = -Wno-strict-aliasing -Wno-sign-conversion
ifeq ($(CC),gcc)
else 
endif

###############################################################################
# LINKING
###############################################################################
$(THE_EXECUTABLE): 			$(OBJ_DIR)/main_server.o $(CODE_OBJECTS) $(FEAT_OBJECTS) $(LIBS)
	@echo "\tCCLD\t"$@
	@$(CC) $(INC) $(LDFLAGS) -o $@ $^ -lm

###############################################################################
# OBJECTS
###############################################################################
$(OBJ_DIR)/webserver.o:			$(FEAT_DIR)/webserver.c $(FEAT_DIR)/webserver.h $(CORE_DIR)/core.h $(HEADERS)
	@echo "\tCC\t"$@
	@$(CC) $(INC) $(CFLAGS) 	-c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)
$(OBJ_DIR)/core.o:			$(CORE_DIR)/core.c $(CORE_DIR)/core.h $(HEADERS)
	@echo "\tCC\t"$@
	@$(CC) $(INC) $(CFLAGS) 	-c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)
$(OBJ_DIR)/main_server.o:		main_server.c $(CORE_DIR)/core.h $(HEADERS)
	@echo "\tCC\t"$@
	@$(CC) $(INC) $(CFLAGS) 	-c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)


###############################################################################
# DEPENDENCIES
###############################################################################

###############################################################################
# Documentation
###############################################################################
./doc/Api.txt: tags
	@echo $@
	@echo 'PUBLIC\n' > $@ && \
	grep "^RONJA_" tags|cut -f1|sort >> $@ && \

###############################################################################
# Boilerplate
###############################################################################
tags:	$(shell ls *.[ch]) 
	@$(CTAGS) --if0=yes --languages=c -f $@ --fields=+Snf --extra=+q --exclude=$(DEPS) -R . 2>/dev/null

.PHONEY: clean propperclean

clean:
	@echo cleaning
	@rm -f $(OBJECTS) $(THE_EXECUTABLE)
	
propperclean: clean
	@echo propper cleaning 
	@rm -rf $(shell find $(DEP_DIR) -type d) tags
	
# vim: ts=4 sts=4 sw=4 noet nu

