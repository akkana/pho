# Makefile for pho

# Locate the gtk/gdk libraries (thanks to nev for this!)
CFLAGS = -g -O2 -Wall 
CFLAGS := $(CFLAGS) $(shell gdk-pixbuf-config --cflags)

XLIBS := $(shell gdk-pixbuf-config --libs) -lgdk_pixbuf_xlib -lX11

GLIBS := $(shell gdk-pixbuf-config --libs)

INSTALL = /usr/bin/install -D

INSTALLPREFIX = /usr/local

all: pho xpho

pho: pho.o imagenote.o gmain.o dialogs.o
	$(CC) -o pho pho.o imagenote.o gmain.o dialogs.o $(GLIBS) $(LDFLAGS)

xpho: pho.o xmain.o
	$(CC) -o xpho pho.o imagenote.o xmain.o $(XLIBS) $(LDFLAGS)

install: pho
	$(INSTALL) pho $(INSTALLPREFIX)/bin/pho
	$(INSTALL) pho.1 $(INSTALLPREFIX)/man/man1/pho.1

clean:
	rm -f *.[oas] *.ld core* pho xpho

