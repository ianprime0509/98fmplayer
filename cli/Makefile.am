bin_PROGRAMS=98fmplayer

LIBOPNA_SRC=../libopna/opnaadpcm.c \
            ../libopna/opnadrum.c \
            ../libopna/opnafm.c \
            ../libopna/opnassg.c \
            ../libopna/opnassg-sinc-c.c \
            ../libopna/opnatimer.c \
            ../libopna/opna.c

FMDRIVER_SRC=../fmdriver/fmdriver_fmp.c \
             ../fmdriver/fmdriver_pmd.c \
             ../fmdriver/fmdriver_common.c \
             ../fmdriver/ppz8.c

98fmplayer_CPPFLAGS=-Wall -Wextra -pedantic \
                  -I$(top_srcdir)/.. \
                  $(SNDFILE_CFLAGS)
98fmplayer_LDADD=$(SNDFILE_LIBS) -lm

if ENABLE_NEON
LIBOPNA_SRC+=../libopna/opnassg-sinc-neon.s
98fmplayer_CCASFLAGS=-march=armv8-a -mfpu=crypto-neon-fp-armv8
endif

if ENABLE_SSE
noinst_LIBRARIES=libsse.a
98fmplayer_LDADD+=libsse.a
libsse_a_SOURCES=../libopna/opnassg-sinc-sse2.c
libsse_a_CPPFLAGS=$(98fmplayer_CPPFLAGS)
libsse_a_CFLAGS=-msse2
endif

98fmplayer_SOURCES=main.c \
                 ../common/fmplayer_file.c \
                 ../common/fmplayer_file_unix.c \
                 ../common/fmplayer_work_opna.c \
                 ../common/fmplayer_drumrom_unix.c \
                 ../fft/fft.c \
                 $(LIBOPNA_SRC) \
                 $(FMDRIVER_SRC)
