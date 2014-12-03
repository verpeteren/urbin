include ./common.mk

HEADERS = common.h config.h
DEBUG_FLAGS = -p -pg -g -g3 -ggdb -fno-builtin-strlen
RELEASE_FLAGS = -O2
CFLAGS = $(RELEASE_FLAGS) $(DEBUG_FLAGS) $(GET_OFF_MY_LAWN)
HAVES = 
LDFLAGS=-Wl,-z,relro -Wl,-z,now

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

