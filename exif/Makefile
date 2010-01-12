# Makefile for pho/exif

EXIFLIB = libphoexif.a

CFLAGS += -Wall -g -O2 -I../include

SRCS = jhead.c jpgfile.c exif.c phoexif.c

# Next line is gmake-specific:
#OBJS = $(subst .c,.o,$(SRCS))
# so here's a simpler line that doesn't break the FreeBSD build:
OBJS = jhead.o jpgfile.o exif.o phoexif.o

$(EXIFLIB): $(OBJS)
	ar cr $(EXIFLIB) $(OBJS)
	ranlib $(EXIFLIB)

clean:
	rm -f *.[oas] *.ld core* $(EXIFLIB)

