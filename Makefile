# Makefile for pho

VERSION = 1.0pre1

# Locate the gtk/gdk libraries (thanks to nev for this!)
GTKFLAGS := $(shell pkg-config --cflags gtk+-2.0 gdk-2.0 2> /dev/null)
CFLAGS += -g -Wall -pedantic -DVERSION='"$(VERSION)"' $(GTKFLAGS)

XLIBS := $(shell pkg-config --libs gtk+-2.0 > /dev/null)
GLIBS := $(shell pkg-config --libs gtk+-2.0 gdk-2.0)

CWD = $(shell pwd)
CWDBASE = $(shell basename `pwd`)

INSTALL = /usr/bin/install -D

INSTALLPREFIX = ${DESTDIR}/usr/local

TARFILE = pho-$(VERSION).tar.gz

EXIFLIB = exif/libphoexif.a -lm

SRCS = pho.c gmain.c phoimglist.c gwin.c imagenote.c gdialogs.c keydialog.c

# winman.c

OBJS = $(subst .c,.o,$(SRCS))

pho: $(EXIFLIB) $(OBJS)
	$(CC) -o $@ $(OBJS) $(EXIFLIB) $(GLIBS) $(LDFLAGS) -lm

cflags:
	echo $(CFLAGS)

all: pho

$(EXIFLIB): exif/*.c
	(cd exif && $(MAKE))

tar: clean $(TARFILE)

$(TARFILE): 
	( make clean && \
	  cd .. && \
	  tar czvf $(TARFILE) --exclude=.svn --exclude=.git --owner=root $(CWDBASE) && \
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
	$(INSTALL) doc/pho.1 $(INSTALLPREFIX)/man/man1/pho.1

clean:
	rm -f *.[oas] *.ld core* pho pho-*.tar.gz *.rpm
	rm -f build-stamp configure-stamp
	rm -rf debian/pho
	cd exif; make clean

