include ./Makefile_dependencies.mk

STDCPP = c++0x
STDC = c99

CC_FLAGS += $(GET_OFF_MY_LAWN) 
LD_FLAGS += -lpthread -lstdc++ -lrt -ldl -lm -Wl,-z,relro -Wl,-z,now

ifeq ($(STAGING_DEBUG),yes)
CC_DEBUG_FLAGS += -g -g3 -gdwarf-2 -ggdb3 \
	-Wstack-protector -fstack-protector-all \
	-D_FORTIFY_SOURCE=2 $(OPTIMIZATION_LEVEL)
LD_DEBUG_FLAGS +=  
ifeq ($(CC),gcc)
CC_DEBUG_FLAGS += -pg -fno-builtin-strlen --param ssp-buffer-size=4
else
endif
CC_DEBUG_FLAGS += 
endif

ifeq ($(STAGING_RELEASE),yes)
CC_RELEASE_FLAGS += $(OPITMIZATION_LEVEL)
LD_RELEASE_FLAGS += $(OPITMIZATION_LEVEL)
ifeq ($(CC),gcc)
else 
endif

endif
CC_FLAGS += $(CC_RELEASE_FLAGS) $(CC_DEBUG_FLAGS)
LD_FLAGS += $(LD_RELEASE_FLAGS) $(LD_DEBUG_FLAGS)
HAVES = $(HAVE_MYSQL)

CORE_DIR = ./core
FEAT_DIR = ./feature
GLOT_DIR = ./glot
MAIN_DIR = ./main
BIN_DIR = ../bin
DOC_DIR = ../doc

COMMON_HEADERS = common.h platform.h $(DIRS) 
FEATURE_HEADERS = $(FEAT_DIR)/webcommon.h $(FEAT_DIR)/webserver.h  $(FEAT_DIR)/webclient.h  $(FEAT_DIR)/sqlclient.h
GLOT_HEADERS = $(GLOT_DIR)/javascript.h

CORE_OBJECTS = $(OBJ_DIR)/utils.o $(OBJ_DIR)/core.o
FEAT_OBJECTS = $(OBJ_DIR)/webserver.o $(OBJ_DIR)/webclient.o $(OBJ_DIR)/sqlclient.o
GLOT_OBJECTS = $(OBJ_DIR)/javascript.o
MAIN_OBJECTS = $(OBJ_DIR)/main_server.o
DOC_API_OBJECT = $(DOC_DIR)/Api.txt
HTMLDOC_DIR = $(DOC_DIR)/html

DOC_OBJECTS = $(DOC_API_OBJECT) $(HTMLDOC_DIR)
CODE_OBJECTS = $(CORE_OBJECTS) $(FEAT_OBJECTS) $(GLOT_OBJECTS) $(MAIN_OBJECTS)

THE_EXECUTABLE = $(BIN_DIR)/urbin

all: 	$(THE_EXECUTABLE) 
deps:	$(DEPS)
docs:   $(DOC_OBJECTS)

$(OBJ_DIR)/main_server.o:			EXCEPTION =
$(OBJ_DIR)/webserver.o	:			EXCEPTION = -Wno-strict-aliasing -Wno-sign-conversion -Wno-conversion -Wno-array-bounds
$(OBJ_DIR)/webclient.o	:			EXCEPTION = -Wno-strict-aliasing -Wno-sign-conversion -Wno-conversion
$(OBJ_DIR)/sqlclient.o:				EXCEPTION = -Wno-conversion
$(OBJ_DIR)/javascript.o:			EXCEPTION = -Wno-aggregate-return
$(OBJ_DIR)/core.o:					EXCEPTION = -Wno-unused-result
$(OBJ_DIR)/utils.o:					EXCEPTION = -Wno-unused-result
ifeq ($(CC),gcc)
$(OBJ_DIR)/main_server.o:			EXCEPTION +=
$(OBJ_DIR)/webserver.o:				EXCEPTION += 
$(OBJ_DIR)/webclient.o:				EXCEPTION +=
$(OBJ_DIR)/sqlclient.o:				EXCEPTION += -Wno-suggest-attribute=noreturn
$(OBJ_DIR)/javascript.o:			EXCEPTION +=
$(OBJ_DIR)/core.o:					EXCEPTION += -Wno-suggest-attribute=pure
$(OBJ_DIR)/utils.o:					EXCEPTION += 
else
$(OBJ_DIR)/main_server.o:			EXCEPTION +=
$(OBJ_DIR)/webserver.o:				EXCEPTION += -Wno-unreachable-code -Wno-cast-align
$(OBJ_DIR)/webclient.o:				EXCEPTION += -Wno-unreachable-code -Wno-cast-align
$(OBJ_DIR)/sqlclient.o:				EXCEPTION += -Wno-unreachable-code -Wno-cast-align
$(OBJ_DIR)/javascript.o:			EXCEPTION += -Wno-cast-align
$(OBJ_DIR)/core.o:					EXCEPTION += -Wno-unreachable-code -Wno-cast-align -Wno-format-nonliteral
$(OBJ_DIR)/utils.o:					EXCEPTION += 
endif

###############################################################################
# KNOW-HOW
###############################################################################
$(OBJ_DIR)/utils.o:					$(CORE_DIR)/utils.c  $(CORE_DIR)/utils.h $(COMMON_HEADERS)
$(OBJ_DIR)/core.o:					$(CORE_DIR)/core.c $(CORE_DIR)/core.h $(CORE_DIR)/utils.h $(COMMON_HEADERS)
$(OBJ_DIR)/webserver.o:				$(FEAT_DIR)/webserver.c $(FEAT_DIR)/webserver.h $(FEAT_DIR)/webcommon.h $(CORE_DIR)/core.h $(CORE_DIR)/utils.h $(COMMON_HEADERS)
$(OBJ_DIR)/webclient.o:				$(FEAT_DIR)/webclient.c $(FEAT_DIR)/webclient.h $(FEAT_DIR)/webcommon.h $(CORE_DIR)/core.h $(CORE_DIR)/utils.h $(COMMON_HEADERS)
$(OBJ_DIR)/sqlclient.o:				$(FEAT_DIR)/sqlclient.c $(FEAT_DIR)/sqlclient.h $(CORE_DIR)/core.h $(CORE_DIR)/utils.h $(COMMON_HEADERS)
$(OBJ_DIR)/javascript.o:			$(GLOT_DIR)/javascript.cpp $(GLOT_DIR)/javascript.h $(CORE_DIR)/core.h $(CORE_DIR)/utils.h $(FEAT_DIR)/webserver.h $(FEAT_DIR)/webclient.h $(FEAT_DIR)/sqlclient.h $(COMMON_HEADERS) 
$(OBJ_DIR)/main_server.o:			$(MAIN_DIR)/main_server.cpp $(CORE_DIR)/core.h $(FEATURE_HEADERS) $(GLOT_HEADERS) $(COMMON_HEADERS)

###############################################################################
# LINKING
###############################################################################
$(THE_EXECUTABLE): 					$(DEPS) $(CODE_OBJECTS) $(DOC_OBJECT)
ifeq ($(MAKE_STATIC_LIBS),yes)
	@echo "\tLD.a :\t"$@
else
	@echo "\tLD.so:\t"$@
ifeq ($(HAS_MYSQL),yes)
	@ln -fs $(LIB_MYS_SHARED) $(BIN_DIR)/libmysac.so.0.0
endif
endif
	@$(CC) -o $@ $(CODE_OBJECTS) $(LIBS) $(LD_FLAGS) && \
	cd $(BIN_DIR) && \
	ln -fs $(DEPS) -t . && \
	ln -fs $(LIB_URL_SHARED) $(BIN_DIR)/liburiparser.so.1 && \
	chmod  +x $@ lib* && \
	export LD_LIBRARY_PATH=$(BIN_DIR)
ifeq ($(STAGING_RELEASE),yes)
	@$(STRIP) $@ $(DEPS)
endif
	@touch $@
###############################################################################
# OBJECTS
###############################################################################
$(OBJ_DIR)/main_%.o:				$(MAIN_DIR)/main_%.cpp
	@echo "\tCC\t"$@:
	@$(CC) -std=$(STDCPP) $(CC_FLAGS) 	-c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)

$(OBJ_DIR)/%.o:						$(FEAT_DIR)/%.c
	@echo "\tCC\t"$@
	@$(CC) -std=$(STDC) $(CC_FLAGS) -c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)

$(OBJ_DIR)/%.o:						$(CORE_DIR)/%.c
	@echo "\tCC\t"$@
	@$(CC) -std=$(STDC) $(CC_FLAGS) -c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)

$(OBJ_DIR)/%.o:						$(GLOT_DIR)/%.cpp
	@echo "\tCC\t"$@
	@$(CC) -std=$(STDCPP) $(CC_FLAGS) -c $< -o $@ $(EXCEPTION) -I$(CORE_DIR) $(HAVES) $(INCS)

###############################################################################
# DEPENDENCIES
###############################################################################

###############################################################################
# Documentation
###############################################################################
$(DOC_API_OBJECT): tags
	@echo $@
	@echo 'PUBLIC\n' */* > $@ && \
	grep "^URBIN_" tags|cut -f1|sort >> $@ 

include ./Makefile_documentation.mk

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

.PHONY: clean propperclean check 

clean:
	@echo cleaning
	@rm -rf $(CODE_OBJECTS) $(THE_EXECUTABLE) $(DOC_OBJECTS) $(BIN_DIR)/*.a $(BIN_DIR)/*.so*
	
propperclean: clean
	@echo propper cleaning 
	@rm -rf $(shell find $(DEP_DIR) -type d) tags
	

