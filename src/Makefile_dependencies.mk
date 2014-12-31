include Makefile_common.mk
###############################################################################
# For the master of the Makefiles
###############################################################################
NSPR_PATH = $(shell pwd)/$(DIR_NSPR)
ZLIB_PATH = $(shell pwd)/$(DIR_Z)

ifdef LINUX_BUILD
PICOEV_SOURCE = picoev_epoll.c
endif
ifdef DARWING_BUILD
PICOEV_SOURCE = picoev_kqueue.c
endif
ifdef GENERIC_BUILD
PICOEV_SOURCE = picoev_selectl.c
endif

deps: $(DEPS)

###############################################################################
# zlib: A Massively Spiffy Yet Delicately Unobtrusive Compression Library
###############################################################################
# wow there are 4 versions in use here: `find . -type f|grep /zlib.h`|grep define
# it appears that firefox is most recent (1.2.8)
$(LIB_Z_STATIC): $(DIR_Z) 
$(LIB_Z_SHARED): $(DIR_Z) 
	@echo $@
	@cd $(DIR_Z) &&\
	./configure && \
	make
	@touch $@
		
$(DIR_Z): 
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q http://zlib.net/zlib-$(VER_Z).tar.gz && \
	tar -xaf zlib-$(VER_Z).tar.gz
###############################################################################
# picoev: a tiny event loop for network applications, faster than libevent or libev
###############################################################################
$(LIB_PICOEV_STATIC): $(DIR_PICOEV)
	@echo $@
	@cd $(DIR_PICOEV) && \
	$(CC) $(CC_RELEASE_FLAGS) $(CC_DEBUG_FLAGS) -c -o picoev_static.o $(PICOEV_SOURCE) && \
	$(AR) cr libpicoev.a picoev_static.o && \
	$(RANLIB) libpicoev.a
	@touch $@

$(LIB_PICOEV_SHARED): $(DIR_PICOEV)
	@echo $@
	@cd $(DIR_PICOEV) && \
	$(CC) $(CC_RELEASE_FLAGS) $(CC_DEBUG_FLAGS) -fPIC -c -o picoev_shared.o $(PICOEV_SOURCE) && \
	$(CC) $(LD_DEBUG_FLAGS) $(LD_RELEASE_FLAGS) -shared -Wl,-soname,libpicoev.so -o libpicoev.so picoev_shared.o
	@touch $@

$(DIR_PICOEV):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q https://github.com/kazuho/picoev/archive/master.zip -O picoev.zip && \
	unzip -qq picoev.zip && \
	mv picoev-master picoev && \
	cd ./picoev && \
	cp picoev_epoll.c picoev_epoll.c.org && \
	sed -e"s/^  assert(PICOEV_FD_BELONGS_TO_LOOP/  memset( \&ev, 0, sizeof( ev ) );\n  assert(PICOEV_FD_BELONGS_TO_LOOP/" picoev_epoll.c.org > picoev_epoll.c

###############################################################################
# clog: A syslog-compatible stderr logging mechanism
###############################################################################
$(LIB_CLOG_STATIC): $(DIR_CLOG)
	@echo $@
	@cd $(DIR_CLOG) && \
	$(CC) $(CC_RELEASE_FLAGS) $(CC_DEBUG_FLAGS) -c -o logging_static.o logging.c && \
	$(AR) cr libclog.a logging_static.o && \
	$(RANLIB) libclog.a
	@touch $@

$(LIB_CLOG_SHARED): $(DIR_CLOG)
	@echo $@
	@cd $(DIR_CLOG) && \
	$(CC) $(CC_DEBUG_FLAGS) $(CC_RELEASE_FLAGS) -fPIC -c -o logging_shared.o logging.c && \
	$(CC) $(LD_DEBUG_FLAGS) $(LD_RELEASE_FLAGS) -shared -Wl,-soname,libclog.so -o libclog.so logging_shared.o
	@touch $@

$(DIR_CLOG):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q https://github.com/dhess/c-logging/archive/master.zip -O clog.zip && \
	unzip -qq clog.zip && \
	mv c-logging-master clog 

###############################################################################
# H3 - The Fast HTTP header parser library  
###############################################################################
$(LIB_H3_STATIC): $(DIR_H3)
$(LIB_H3_SHARED): $(DIR_H3)
	@echo $@
	@cd $(DIR_H3) && \
	make -f Makefile.dist libh3.a libh3.so 
	@touch $@

$(DIR_H3):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q https://github.com/verpeteren/h3/archive/master.zip -O h3.zip && \
	unzip -qq h3.zip && \
	mv h3-master h3 

###############################################################################
# TADns - Tiny Asyncronous Dns Lookup library
###############################################################################
$(LIB_TADL_STATIC): $(DIR_TADL)
	@echo $@
	@cd $(DIR_TADL) && \
	$(CC) $(CC_DEBUG_FLAGS) $(CC_RELEASE_FLAGS) -c -o tadns_static.o tadns.c && \
	$(AR) cr libtadns.a tadns_static.o && \
	$(RANLIB) libtadns.a
	@touch $@

$(LIB_TADL_SHARED): $(DIR_TADL)
	@echo $@
	@cd $(DIR_TADL) && \
	$(CC) $(LD_DEBUG_FLAGS) $(LD_RELEASE_FLAGS) -fPIC -c -o tadns_shared.o tadns.c && \
	$(CC) $(LD_DEBUG_FLAGS) $(LD_RELEASE_FLAGS) -shared -Wl,-soname,libtadns.so -o libtadns.so tadns_shared.o
	@touch $@

$(DIR_TADL):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q http://sourceforge.net/projects/adns/files/tadns/$(VER_TADL)/tadns-$(VER_TADL).tar.gz/download -O tadns-$(VER_TADL).tar.gz && \
	tar -xaf tadns-$(VER_TADL).tar.gz 
	@touch $@

###############################################################################
# oniguruma: an regular expression engine
###############################################################################
$(LIB_ONIG_STATIC): $(DIR_ONIG)
$(LIB_ONIG_SHARED): $(DIR_ONIG)
	@echo $@
	@cd $(DIR_ONIG) && \
	./configure --with-pic --with-gnu-ld --enable-static --enable-shared && \
	make
	@touch $@

$(DIR_ONIG):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q http://www.geocities.jp/kosako3/oniguruma/archive/onig-$(VER_ONIG).tar.gz && \
	tar -xaf onig-$(VER_ONIG).tar.gz

###############################################################################
# libpq: native postgresql connector
###############################################################################
$(LIB_PG_STATIC): $(DIR_PG)
$(LIB_PG_SHARED): $(DIR_PG)
	@echo $@
	@cd $(DIR_PG) && \
	./configure --without-readline --without-zlib --with-gnu-ld --without-libxml --without-libxslt --without-openssl --without-bonjour --without-ldap --without-pam --without-krb5 --without-gssapi --without-python --without-perl --without-tcl&& \
	cd ./src/interfaces/libpq && \
	make
#	@touch $@

$(DIR_PG):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q http://ftp.postgresql.org/pub/source/v$(VER_PG)/postgresql-$(VER_PG).tar.bz2 && \
	tar -xaf postgresql-$(VER_PG).tar.bz2

###############################################################################
# mysac: mysql simple asynchronous client 
###############################################################################
$(LIB_MYS_STATIC): $(DIR_MYS) $(DIR_MY)
$(LIB_MYS_SHARED): $(DIR_MYS) $(DIR_MY)
	@echo $@
	@cd $(DIR_MYS) && \
	make libmysac-static.a libmysac.so
	@touch $@

$(DIR_MYS):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q http://www.arpalert.org/src/mysac-$(VER_MYS).tar.gz && \
	tar -xaf mysac-$(VER_MYS).tar.gz && \
	cd ./mysac-$(VER_MYS) && \
	cp Makefile Makefile.org && \
	sed -e"s/\/ITRIEDTOREPAIRusr\/include\/mysql/\.\.\/mysql-connector-c-$(VER_MY)-src -I\.\.\/mysql-connector-c-$(VER_MY)-src\/include/" \
		-e"s/-ITRIEDTOREPAIRWerror//" \
		Makefile.org > Makefile && \
	cp mysac.h mysac.h.org && \
	sed -e"s/^#define __MYSAC_H__/#define __MYSAC_H__\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n/" \
		mysac.h.org | \
		tac | \
		sed  -e"0,/^#endif/ s//#endif\n#endif\n}\n#ifdef __cplusplus/" | \
		tac > \
		mysac.h

###############################################################################
# mysql: mysql c connection
###############################################################################
$(LIB_MY_STATIC): $(DIR_MY) $(LIB_Z_STATIC)
$(LIB_MY_SHARED): $(DIR_MY) $(LIB_Z_SHARED)
	@echo $@
	@cd $(DIR_MY)/ && \
	cmake . -DCMAKE_INSTALL_PREFIX=/usr/local/mysql -DWITH_EMBEDDED_SERVER=0 -DWITH_LIBEDIT=0 -DISABLE_SHARED=1 -DENABLED_PROFILING=0 -DWITHOUT_SERVER=1 -DWITH_VALGRIND=0 -DWITH_UNIT_TESTS=0 -DENABLE_GCOV=0 -DENABLE_GPROF=0 -DWITH_ZLIB=system&& \
	make
	@touch $@

$(DIR_MY):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q http://ftp.de.debian.org/debian/pool/main/m/mysql-5.6/mysql-5.6_$(VER_MY).orig.tar.gz && \
	tar -xaf mysql-5.6_$(VER_MY).orig.tar.gz && \
	ln -s ./mysql-$(VER_MY)/include ./mysql-$(VER_MY)/mysql 

###############################################################################
# NSPR
###############################################################################
$(LIB_NSPR_STATIC): $(DIR_MOZ)
$(LIB_NSPR_SHARED): $(DIR_MOZ)
	@echo $@
	@cd $(DIR_NSPR) && \
	./configure \
		--enable-optimize=-O2 \
		--disable-debug \
		--enable-strip \
		--disable-symbian-target && \
	make 
	@cp $(DIR_NSPR)/config/nspr-config $(DIR_NSPR)/dist/bin/
	@touch $@

$(DIR_NSPR): $(DIR_MOZ)
	@echo $@
	@cd $(DEP_DIR) 
	#wget -q https://ftp.mozilla.org/pub/mozilla.org/nspr/betas/v$(VER_NSPR)/src/nspr-$(VER_NSPR).tar.gz && \
	#tar xzf nspr-$(VER_NSPR).tar.gz

###############################################################################
# spidermonkey: embedded javascript interpreter
###############################################################################
$(LIB_MOZ_STATIC): $(LIB_NSPR_STATIC) $(LIB_Z_STATIC)
$(LIB_MOZ_SHARED): $(LIB_NSPR_SHARED) $(LIB_Z_SHARED)
	@echo $@
	@cd $(DIR_MOZ)/js/src && \
		autoconf2.13 && \
		mkdir -p build && \
		cd build && \
		../configure \
		--without-android-ndk \
		--disable-metro \
		--without-x \
		--disable-trace-malloc \
		--disable-trace-jscalls \
		--disable-gc-trace \
		--disable-wrap-malloc \
		--disable-arm-simulator \
		--disable-mips-simulator \
		--disable-jprof \
		--disable-shark \
		--disable-instruments \
		--disable-callgrind \
		--disable-vtune \
		--disable-perf \
		--disable-js-diagnostics \
		--disable-nspr-build \
		--disable-clang-plugin \
		--disable-oom-breakpoint \
		--disable-more-deterministic \
		--disable-strip \
		--disable-install-strip \
		--disable-tests \
		--enable-shared-js \
		--enable-valgrind \
		--enable-threadsafe \
		--enable-release \
		--without-ccache \
		--with-system-zlib=$(ZLIB_PATH) \
		--with-nspr-prefix=$(NSPR_PATH) --with-nspr-cflags="-I$(NSPR_PATH)/dist/include/nspr" --with-nspr-libs="-lrt -L$(NSPR_PATH)/dist/lib/ -lnspr4 -lplds4 -lplc4" \
		$(DEBUG)  \
	&&make 
	@touch $@

$(DIR_MOZ):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q https://ftp.mozilla.org/pub/mozilla.org/firefox/candidates/$(VER_MOZ)-candidates/build1/source/firefox-$(VER_MOZ).source.tar.bz2 && \
	tar -xaf firefox-$(VER_MOZ).source.tar.bz2 && \
	mv mozilla-beta mozilla 

#############################################################################
# libconfuse: configuration file library 
#############################################################################
$(LIB_CONF_STATIC): $(DIR_CONF)
$(LIB_CONF_SHARED): $(DIR_CONF)
	@echo $@
	@cd $(DIR_CONF) && \
	./configure --with-gnu-ld --disable-examples --disable-nls --disable-rpath --enable-shared --enable-static && \
	make 
	@touch $@

$(DIR_CONF):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q http://savannah.nongnu.org/download/confuse/confuse-$(VER_CONF).tar.gz && \
	tar -xaf confuse-$(VER_CONF).tar.gz

###############################################################################
# Docstrape: JSDOC-Toolkit template based on bootstrap, and it realy looks slik!
###############################################################################
$(DIR_DST):
	@echo $@
	@cd $(DEP_DIR) && \
	wget -q https://github.com/verpeteren/Docstrape/archive/master.zip -O Docstrape.zip && \
	unzip -qq Docstrape.zip && \
	mv Docstrape-master Docstrape 

###############################################################################
# Boilerplate
###############################################################################
.PHONY: depclean deppropperclean

depclean:
	@echo cleaning some dependencies
	@rm -rf $(DEPS)

deppropperclean: depclean
	@echo cleaning all dependencies
	@rm -rf $(DIRS) $(DEP_DIR)/*.zip $(DEP_DIR)/*.tar.gz $(DEP_DIR)/*.tar.bz2 $(DEP_DIR)/*.tar

