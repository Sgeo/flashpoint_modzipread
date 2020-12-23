# Makefile for mod_zipread

APXS=apxs
APACHECTL=apache2ctl

INCLUDES=-I/usr/include/apr-0
LIBS=-lzzip

all:
	$(APXS) -c $(INCLUDES) $(LIBS) mod_zipread.c

install:
	$(APXS) -i mod_zipread.la

clean:
	-rm -f mod_zipread.o mod_zipread.lo mod_zipread.slo mod_zipread.la
	-rm -rf .libs

reload: install restart

start:
	$(APACHECTL) start
restart:
	$(APACHECTL) restart
stop:
	$(APACHECTL) stop

