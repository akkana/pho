# Makefile for pho

VERSION = 0.9.7-pre4

# Locate the gtk/gdk libraries (thanks to nev for this!)
CFLAGS += -g -Wall -pedantic -DVERSION='"$(VERSION)"'
G1FLAGS := $(shell gdk-pixbuf-config --cflags 2> /dev/null)
G2FLAGS := $(shell pkg-config --cflags gtk+-2.0 gdk-2.0 2> /dev/null)
CFLAGS := $(CFLAGS) $(shell if test -n "${G2FLAGS}"; then echo "${G2FLAGS}"; else echo "${G1FLAGS}"; fi)

XLIBS := $(shell pkg-config --libs gtk+-2.0 > /dev/null)
# GLIBS := $(shell gdk-pixbuf-config --libs)
GLIBS := $(shell if test "${G2FLAGS}"; then pkg-config --libs gtk+-2.0 gdk-2.0; else gdk-pixbuf-config --libs; fi)

CWD = $(shell pwd)
CWDBASE = $(shell basename `pwd`)

INSTALL = /usr/bin/install -D

INSTALLPREFIX = ${DESTDIR}/usr/local

TARFILE = pho-$(VERSION).tar.gz

EXIFLIB = exif/libphoexif.a

SRCS = pho.c gmain.c gwin.c imagenote.c gdialogs.c keydialog.c

# winman.c

OBJS = $(subst .c,.o,$(SRCS))

pho: $(EXIFLIB) $(OBJS)
	$(CC) -o $@ $(OBJS) $(EXIFLIB) $(GLIBS) $(LDFLAGS)

cflags:
	echo $(CFLAGS)

all: pho xpho

$(EXIFLIB): exif/*.c
	(cd exif && $(MAKE))

xpho: $(EXIFLIB) pho.o imagenote.o xmain.o
	$(CC) -o $@ $< $(EXIFLIB) $(XLIBS) $(LDFLAGS)

tar: clean $(TARFILE)

$(TARFILE): 
	( make clean && \
	  cd .. && \
	  tar czvf $(TARFILE) --exclude=.svn --owner=root $(CWDBASE) && \
	  mv $(TARFILE) $(CWD) && \
	  echo Created $(TARFILE) \
	)

rpm: $(TARFILE)
	cp $(TARFILE) /usr/src/redhat/SOURCES/$(TARFILE)
	rpm -ba pho.spec
	cp /usr/src/redhat/RPMS/i386/pho-$(VERSION)*.rpm .
	cp /usr/src/redhat/SRPMS/pho-$(VERSION)*.rpm .

deb: pho
	dpkg-buildpackage -rfakeroot

install: pho
	$(INSTALL) pho $(INSTALLPREFIX)/bin/pho
	$(INSTALL) pho.1 $(INSTALLPREFIX)/man/man1/pho.1

clean:
	rm -f *.[oas] *.ld core* pho xpho pho-*.tar.gz *.rpm
	rm -f build-stamp configure-stamp
	rm -rf debian/pho
	cd exif; make clean

