# Makefile for pho

VERSION = 0.9.2

# Locate the gtk/gdk libraries (thanks to nev for this!)
CFLAGS = -g -O2 -Wall -DVERSION='"$(VERSION)"'
CFLAGS := $(CFLAGS) $(shell gdk-pixbuf-config --cflags)

XLIBS := $(shell gdk-pixbuf-config --libs) -lgdk_pixbuf_xlib -lX11
GLIBS := $(shell gdk-pixbuf-config --libs)

CWD = $(shell pwd)
CWDBASE = $(shell basename `pwd`)

INSTALL = /usr/bin/install -D

INSTALLPREFIX = ${DESTDIR}/usr/local

TARFILE = pho-$(VERSION).tar.gz

EXIFLIB = exif/libphoexif.a

SRCS = pho.c imagenote.c gmain.c dialogs.c

OBJS = $(subst .c,.o,$(SRCS))

all: pho xpho

pho: $(EXIFLIB) $(OBJS)
	$(CC) -o pho pho.o imagenote.o gmain.o dialogs.o \
              $(EXIFLIB) $(GLIBS) $(LDFLAGS)

$(EXIFLIB): exif/*.c
	(cd exif; make)

xpho: $(EXIFLIB) pho.o xmain.o
	$(CC) -o xpho pho.o imagenote.o xmain.o $(EXIFLIB) $(XLIBS) $(LDFLAGS)

tar: clean $(TARFILE)

$(TARFILE): 
	( make clean && \
	  cd .. && \
	  tar czvf $(TARFILE) --owner=root $(CWDBASE) && \
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
	rm -f *.[oas] *.ld core* pho xpho pho-*.tar.gz *.rpm *-stamp
	rm -rf debian/pho
	cd exif; make clean

