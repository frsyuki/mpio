
ldflags = -L../mpsrc/.libs -lmpio

AM_CPPFLAGS   = -I..
AM_C_CPPFLAGS = -I..

check_PROGRAMS = \
		listen_connect \
		handler \
		signal \
		timer \
		sync

listen_connect_SOURCES = listen_connect.cc
listen_connect_LDFLAGS = $(ldflags)

handler_SOURCES = handler.cc
handler_LDFLAGS = $(ldflags)

signal_SOURCES = signal.cc
signal_LDFLAGS = $(ldflags)

timer_SOURCES = timer.cc
timer_LDFLAGS = $(ldflags)

sync_SOURCES = sync.cc
sync_LDFLAGS = $(ldflags)

TESTS = $(check_PROGRAMS)

