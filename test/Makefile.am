
AM_CPPFLAGS   = -I..
AM_C_CPPFLAGS = -I..
AM_LDFLAGS = ../mpsrc/libmpio.la

check_PROGRAMS = \
		listen_connect \
		handler \
		signal \
		timer \
		sync

TESTS = $(check_PROGRAMS)

listen_connect_SOURCES = listen_connect.cc

handler_SOURCES = handler.cc

signal_SOURCES = signal.cc

timer_SOURCES = timer.cc

sync_SOURCES = sync.cc

