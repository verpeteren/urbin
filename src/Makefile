include ./Makefile_common.mk
include ./Makefile.dependencies

OPTMIZATIONLEVEL = -O0
CC_FLAGS = $(GET_OFF_MY_LAWN)
CC_DEBUG_FLAGS = $(CC_FLAGS)
CC_RELEASE_FLAG = $(CC_FLAGS)
LD_FLAGS  = -lpthread -lstdc++ -lrt -ldl
LD_DEBUG_FLAGS  = -Wl,-z,relro -Wl,-z,now $(LD_FLAGS)
LD_RELEASE_FLAGS = -Wl,-z,relro -Wl,-z,now $(LD_FLAGS)
 
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
HAVES = $(HAVE_MYSQL)

CORE_DIR=./core
FEAT_DIR=./feature
GLOT_DIR=./glot
BIN_DIR=../bin

COMMON_HEADERS = common.h platform.h $(DIRS)
FEATURE_HEADERS = $(FEAT_DIR)/webserver.h $(FEAT_DIR)/sqlclient.h
GLOT_HEADERS = $(GLOT_DIR)/javascript.h

CODE_OBJECTS = $(OBJ_DIR)/core.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/configuration.o
FEAT_OBJECTS = $(OBJ_DIR)/webserver.o $(OBJ_DIR)/sqlclient.o
GLOT_OBJECTS = $(OBJ_DIR)/javascript.o
MAIN_OBJECTS = $(OBJ_DIR)/main_server.o

OBJECTS = $(CODE_OBJECTS) $(MAIN_OBJECTS) $(FEAT_OBJECTS) $(GLOT_OBJECTS)

THE_EXECUTABLE = $(BIN_DIR)/ronja

all: 	$(THE_EXECUTABLE) 
tests:	$(TST_EXECUTABLE) 
docs:   ../doc/Api.txt

$(OBJ_DIR)/main_server.o:			EXCEPTION =
$(OBJ_DIR)/sqlclient.o:				EXCEPTION =
$(OBJ_DIR)/webserver.o	:			EXCEPTION = -Wno-strict-aliasing -Wno-sign-conversion
$(OBJ_DIR)/javascript.o:			EXCEPTION = -Wno-aggregate-return
$(OBJ_DIR)/core.o:					EXCEPTION =
$(OBJ_DIR)/utils.o:					EXCEPTION =
ifeq ($(CC),gcc)
else 
$(OBJ_DIR)/sqlclient.o:				EXCEPTION += -Wno-unreachable-code -Wno-cast-align
$(OBJ_DIR)/webserver.o:				EXCEPTION += -Wno-unreachable-code -Wno-cast-align
$(OBJ_DIR)/javascript.o:			EXCEPTION += 
$(OBJ_DIR)/core.o:					EXCEPTION += -Wno-unreachable-code -Wno-cast-align
$(OBJ_DIR)/utils.o:					EXCEPTION += 
$(OBJ_DIR)/configuration.o:			EXCEPTION += 
endif

###############################################################################
# LINKING
###############################################################################
$(THE_EXECUTABLE): 			$(OBJ_DIR)/main_server.o $(CODE_OBJECTS) $(FEAT_OBJECTS) $(LIBS)
	@echo "\tLD\t"$@
	@$(CC) $(INC) $(LDFLAGS) -o $@ $^ -lm

###############################################################################
# KNOW-HOW
###############################################################################
$(OBJ_DIR)/sqlclient.o:			$(FEAT_DIR)/sqlclient.c $(FEAT_DIR)/sqlclient.h $(CORE_DIR)/configuration.h $(CORE_DIR)/core.h $(COMMON_HEADERS) 
$(OBJ_DIR)/webserver.o:			$(FEAT_DIR)/webserver.c $(FEAT_DIR)/webserver.h $(CORE_DIR)/configuration.h $(CORE_DIR)/core.h $(CORE_DIR)/utils.h $(CORE_DIR)/configuration.h $(COMMON_HEADERS)
$(OBJ_DIR)/javascript.o:		$(GLOT_DIR)/javascript.cpp $(CORE_DIR)/core.h $(CORE_DIR)/configuration.h $(CORE_DIR)/utils.h $(FEAT_DIR)/webserver.h $(FEAT_DIR)/sqlclient.h $(COMMON_HEADERS) 
$(OBJ_DIR)/core.o:				$(CORE_DIR)/core.c $(CORE_DIR)/core.h $(CORE_DIR)/configuration.h $(COMMON_HEADERS)
$(OBJ_DIR)/utils.o:				$(CORE_DIR)/utils.c 
$(OBJ_DIR)/configuration.o:		$(CORE_DIR)/configuration.c $(CORE_DIR)/configuration.h $(COMMON_HEADERS)

###############################################################################
# OBJECTS
###############################################################################
$(OBJ_DIR)/main_server.o:		main_server.cpp $(CORE_DIR)/core.h $(FEATURE_HEADERS) $(GLOT_HEADERS) $(COMMON_HEADERS)
	@echo "\tCC\t"$@:
	@$(CC) -std=c++0x $(INC) $(CFLAGS) 	-c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)

$(OBJ_DIR)/%.o:					$(FEAT_DIR)/%.c
	@echo "\tCC\t"$@
	@$(CC) -std=gnu99 $(INC) $(CFLAGS) -c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)

$(OBJ_DIR)/%.o:					$(CORE_DIR)/%.c
	@echo "\tCC\t"$@
	@$(CC) -std=gnu99 $(INC) $(CFLAGS) -c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)

$(OBJ_DIR)/%.o:					$(GLOT_DIR)/%.cpp
	@echo "\tCC\t"$@
	@$(CC) -std=c++0x $(INC) $(CFLAGS) -c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)

###############################################################################
# DEPENDENCIES
###############################################################################

###############################################################################
# Documentation
###############################################################################
../doc/Api.txt: tags
	@echo $@
	@echo 'PUBLIC\n' */* > $@ && \
	grep "^RONJA_" tags|cut -f1|sort >> $@ 

###############################################################################
# Boilerplate
###############################################################################
check: 
	@echo "starting cppcheck"
	@cppcheck \
	-q --enable=all --error-exitcode=255 --check-config --platform=unix32 --suppress=unusedScopedObject \
	$(INCS) $(HAVES)\
	$(CORE_DIR) $(FEAT_DIR) $(GLOT_DIR) .


tags:	$(shell ls *.[ch]) 
	@$(CTAGS) --if0=no --languages=c,c++ -f $@ --fields=+Snf --extra=+q --exclude=$(DEP_DIR) -R . 2>/dev/null

.PHONEY: clean propperclean check

clean:
	@echo cleaning
	@rm -f $(OBJECTS) $(THE_EXECUTABLE)
	
propperclean: clean
	@echo propper cleaning 
	@rm -rf $(shell find $(DEP_DIR) -type d) tags
	

