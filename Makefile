.PHONY: all clean install

PREFIX ?= /usr

name = cdthermalcw

USBLIBS = $(shell pkg-config --libs libusb-1.0)
ifeq ($(USBLIBS),)
  $(error libusb-1.0 required)
endif
LDFLAGS += $(USBLIBS) -lrt
LDFLAGS += -lpng
CFLAGS += $(shell pkg-config --cflags libusb-1.0)

CFLAGS += -Wall -Wextra -g
LDFLAGS += -Wl,-gc-sections

src = $(wildcard *.c)
obj = $(src:.c=.o)

all: $(name)

$(name): $(obj)
	$(CC) -o $(name) $(obj) $(CFLAGS) $(LDFLAGS)

$(obj): $(wildcard *.h)

clean:
	rm -f $(name) *.o

install:
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	install -m755 $(name) $(DESTDIR)/$(PREFIX)/bin
