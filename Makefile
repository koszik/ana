#
#  ana (C) Matyas Koszik <koszik@atw.hu>, 2001-2004, 2016.
#

DEFS = 
#-D__STATIC__
CC = gcc
#CC = splint
#CC = /opt/intel/compiler70/ia32/bin/icc

# -D__BSD__		: BSD-n par define mas...
# -D__OPENBSD__		: OpenBSD-n is menni fog a HOME es END

LIBS = -ldl
#LIBS = -lcrypt
# csak linuxon kell (kene mar az a ./configure...)

#LDFLAGS = -pg
LDFLAGS = -rdynamic -Wl,-export-dynamic
LDFLAGS_SOLARIS = -lsocket -lnsl -L/export/home/koszik/SUNWspro/prod/lib
# -no_cpprt


CFLAGS_GCC = -Wstrict-prototypes -Wall -Wmissing-prototypes \
            -Wmissing-declarations -ansi -DHAVE_DLOPEN -std=c99 -pedantic
#	  -Wshadow -Wpointer-arith \
#         -Wwrite-strings \
#         -Wmissing-noreturn -Wmissing-format-attribute  \
#         -Wdisabled-optimization
# -Wcast-qual -Wunreachable-code -Wconversion -Wbad-function-cast
# -Wredundant-decls

CFLAGS_OPT = $(CFLAGS_GCC) -O3 -funroll-loops -fno-strength-reduce \
         -fomit-frame-pointer -fexpensive-optimizations
# -march=i686 -mcpu=i686

CFLAGS_ICC = -Wall -O3 -ansi -wd1418,1419,279,310,981,193,810,869,171

CFLAGS_DBG = $(CFLAGS_GCC) -ggdb -O0
# -pg -fprofile-arcs

CFLAGS_SPLINT = -posix-lib

CFLAGS = $(CFLAGS_DBG)
#CFLAGS = $(CFLAGS_OPT)

INCLUDES = -I $(shell pwd)
SRCS = ana.c readuser.c conio.c log.c variable.c socket.c mem.c user.c
OBJS = $(SRCS:.c=.o)

all: ana

ana: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o ana
# objs/*.o
#	strip -R .note -R .comment ana

ana.o: ana.c main.h readuser.h conio.h log.h module.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c ana.c

socket.o: socket.c socket.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c socket.c

readuser.o: readuser.c readuser.h conio.h main.h module.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c readuser.c

conio.o: conio.c conio.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c conio.c

log.o: log.c log.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c log.c

module.o: module.c module.h log.h main.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c module.c

variable.o: variable.c variable.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c variable.c

mem.o: mem.c mem.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c mem.c

user.o: user.c user.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFS) -c user.c

clean:
	rm -f ana $(OBJS) $(EXTRA_DELS)
