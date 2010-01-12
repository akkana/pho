# Makefile for pho/exif

EXIFLIB = libphoexif.a

CFLAGS += -Wall -g -O2 -I../include

SRCS = jhead.c jpgfile.c exif.c phoexif.c

OBJS = $(subst .c,.o,$(SRCS))

$(EXIFLIB): $(OBJS)
	ar cr $(EXIFLIB) $(OBJS)
	ranlib $(EXIFLIB)

clean:
	rm -f *.[oas] *.ld core* $(EXIFLIB)

