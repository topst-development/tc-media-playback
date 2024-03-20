ACLOCAL_AMFLAGS = -I m4
CC = @CC@ -Wall
CPP = @CPP@
CFLAGS = @CFLAGS@ $(TCMP_CFLAGS) -I$(top_srcdir)/include
CPPLAFGS = @CFLAGS@
LIBS = @LIBS@ $(TCMP_LIBS)
DEFS = @DEFS@ $(MEDIAPLAYBACKDEF)

##########################################
#			Main Program				 #
##########################################
bin_PROGRAMS = TCMediaPlayback

TCMediaPlayback_SOURCES = DBusMsgDefNames.c\
						 main.c \
						 MediaPlaybackDBus.c \
						 MultiMediaManager.c \
						 TCTime.c
clean :
	rm -rf *.o TCMediaPlayback
