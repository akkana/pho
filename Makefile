# Makefile for pho

VERSION = 0.8

# Locate the gtk/gdk libraries (thanks to nev for this!)
CFLAGS = -O2 -Wall 
CFLAGS := $(CFLAGS) $(shell gdk-pixbuf-config --cflags)

XLIBS := $(shell gdk-pixbuf-config --libs) -lgdk_pixbuf_xlib -lX11
GLIBS := $(shell gdk-pixbuf-config --libs)

CWD = $(shell pwd)
CWDBASE = $(shell basename `pwd`)

INSTALL = /usr/bin/install -D

INSTALLPREFIX = ${DESTDIR}/usr/local

TARFILE = pho-$(VERSION).tar.gz

all: pho xpho

pho: pho.o imagenote.o gmain.o dialogs.o
	$(CC) -o pho pho.o imagenote.o gmain.o dialogs.o $(GLIBS) $(LDFLAGS)

xpho: pho.o xmain.o
	$(CC) -o xpho pho.o imagenote.o xmain.o $(XLIBS) $(LDFLAGS)

tar: clean $(TARFILE)

$(TARFILE): 
	( make clean && \
	  cd .. && \
	  tar czvf pho-$(VERSION).tar.gz $(CWDBASE) && \
	  mv $(TARFILE) $(CWD) \
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
	rm -rf debian/pho

