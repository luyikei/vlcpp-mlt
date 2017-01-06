
CXXFLAGS += -I../.. -std=c++11 -fPIC

CFLAGS += -I../.. 

LDFLAGS += -L../../framework -lmlt -lmlt++ -lpthread -fPIC

include ../../../config.mak

TARGET = ../libmltvlc$(LIBSUF)

OBJS = factory.o \
	consumer_vlc.o \
	producer_vlc.o \
	VLCConsumer.o\
	VLCProducer.o

CXXFLAGS += $(shell pkg-config libvlc --cflags)

CFLAGS += $(shell pkg-config libvlc --cflags)

LDFLAGS += $(shell pkg-config libvlc --libs)

SRCS := VLCConsumer.hpp\
	VLCConsumer.cpp\
	VLCProducer.hpp\
	VLCProducer.cpp\
	factory.c\
	consumer_vlc.c

.cpp:
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

all: 	$(TARGET)


$(TARGET): $(OBJS)
	$(CXX) $(SHFLAGS) -fPIC -o $@ $(OBJS) $(LDFLAGS)

depend: $(SRCS)
	$(CXX) -MM $(CXXFLAGS) $^ 1>.depend

distclean:	clean
		rm -f .depend

clean:
		rm -f $(OBJS) $(TARGET)

install: all
	install -m 755 $(TARGET) "$(DESTDIR)$(moduledir)"
	install -d "$(DESTDIR)$(mltdatadir)/libvlc"

ifneq ($(wildcard .depend),)
include .depend
endif
