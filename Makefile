# Makefile for pho

CFLAGS = -g -O2 -Wall -I/usr/include/gdk-pixbuf-1.0 -I/usr/include/gtk-1.2 -I/usr/include/glib-1.2 -I/usr/lib/glib/include -I/usr/X11R6/include

XLIBS = -L/usr/X11R6/lib -lgdk_pixbuf -lgdk_pixbuf_xlib -lX11

GLIBS = -L/usr/X11R6/lib -lgdk_pixbuf -lgtk -lgdk -lX11

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

