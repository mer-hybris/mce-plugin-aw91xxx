# ----------------------------------------------------------- -*- mode: sh -*-
# Installation directories etc
# ----------------------------------------------------------------------------

NAME    ?= mce-plugin-aw91xxx
DESTDIR ?= /tmp/test-install-$(NAME)
_LIBDIR ?= /usr/lib

# ----------------------------------------------------------------------------
# List of targets to build
# ----------------------------------------------------------------------------

TARGETS += hybris.so

# ----------------------------------------------------------------------------
# Top level targets
# ----------------------------------------------------------------------------

.PHONY: build install clean distclean mostlyclean

build:: $(TARGETS)

install:: build

clean:: mostlyclean
	$(RM) $(TARGETS)

distclean:: clean
	$(RM) *.so

mostlyclean::
	$(RM) *~ *.bak *.o
	$(RM) */*~ */*.bak */*.o

# ----------------------------------------------------------------------------
# Default flags
# ----------------------------------------------------------------------------

CPPFLAGS += -D_GNU_SOURCE
CPPFLAGS += -D_FILE_OFFSET_BITS=64
CPPFLAGS += -D_THREAD_SAFE
CPPFLAGS += -DMCE_HYBRIS_INTERNAL=2

COMMON   += -Wall
COMMON   += -Wextra
COMMON   += -Wmissing-prototypes
COMMON   += -Wno-missing-field-initializers
COMMON   += -Os
COMMON   += -g
COMMON   += -fvisibility=hidden

CFLAGS   += $(COMMON)
CFLAGS   += -std=c99

CXXFLAGS += $(COMMON)

LDFLAGS  += -g

LDLIBS   += -Wl,--as-needed
LDLIBS   += -lpthread

# ----------------------------------------------------------------------------
# Flags from pkg-config
# ----------------------------------------------------------------------------

PKG_NAMES += glib-2.0

maintenance  = normalize clean distclean mostlyclean
intersection = $(strip $(foreach w,$1, $(filter $w,$2)))
ifneq ($(call intersection,$(maintenance),$(MAKECMDGOALS)),)
PKG_CONFIG   ?= true
endif

ifneq ($(strip $(PKG_NAMES)),)
PKG_CONFIG   ?= pkg-config
PKG_CFLAGS   := $(shell $(PKG_CONFIG) --cflags $(PKG_NAMES))
PKG_LDLIBS   := $(shell $(PKG_CONFIG) --libs   $(PKG_NAMES))
PKG_CPPFLAGS := $(filter -D%,$(PKG_CFLAGS)) $(filter -I%,$(PKG_CFLAGS))
PKG_CFLAGS   := $(filter-out -I%, $(filter-out -D%, $(PKG_CFLAGS)))
endif

CPPFLAGS += $(PKG_CPPFLAGS)
CFLAGS   += $(PKG_CFLAGS)
LDLIBS   += $(PKG_LDLIBS)

# ----------------------------------------------------------------------------
# Implicit rules
# ----------------------------------------------------------------------------

.SUFFIXES: %.pic.o
.PRECIOUS: %.pic.o

%.so :
	$(CC) -o $@ -shared $^ $(LDFLAGS) $(LDLIBS)

%.pic.o : %.c
	$(CC) -c -o $@ $< -fPIC $(CPPFLAGS) $(CFLAGS)

%.q : %.c
	$(CC) -E -o $@ $(CPPFLAGS) $<

%.p : %.q
	cat $< | cproto    | prettyproto.py > $@
%.i : %.q
	cat $< | cproto -s | prettyproto.py > $@

preprocess: $(patsubst %.c,%.q,$(wildcard */*.c))
prototypes: $(patsubst %.c,%.p,$(wildcard */*.c))
locals: $(patsubst %.c,%.i,$(wildcard */*.c))

# ----------------------------------------------------------------------------
# Explicit dependencies
# ----------------------------------------------------------------------------

hybris_OBJS += plugin/plugin-api.pic.o

hybris_OBJS += plugin/plugin-config.pic.o
hybris_OBJS += plugin/plugin-logging.pic.o

hybris_OBJS += hal/hal-aw91xxx.pic.o

#hybris.so : LDLIBS += -lm
hybris.so : $(hybris_OBJS)

install:: hybris.so
	install -d -m755 $(DESTDIR)$(_LIBDIR)/mce/modules
	install -m755 hybris.so $(DESTDIR)$(_LIBDIR)/mce/modules/

# ----------------------------------------------------------------------------
# Source code normalization
# ----------------------------------------------------------------------------

.PHONY: normalize
normalize::
	normalize_whitespace -M Makefile
	normalize_whitespace -a $(wildcard */*.[ch])
	normalize_whitespace -a README.md *.py

# ----------------------------------------------------------------------------
# AUTOMATIC HEADER DEPENDENCIES
# ----------------------------------------------------------------------------

.PHONY: depend
depend::
	@echo "Updating .depend"
	$(CC) -MM $(CPPFLAGS) $(wildcard */*.c) |\
	./depend_filter.py > .depend

ifneq ($(MAKECMDGOALS),depend) # not while: make depend
ifneq (,$(wildcard .depend))   # not if .depend does not exist
include .depend
endif
endif

# ----------------------------------------------------------------------------
# Hunt for excess include statements
# ----------------------------------------------------------------------------

.PHONY: headers
.SUFFIXES: %.checked

headers:: c_headers c_sources

%.checked : %
	find_unneeded_includes.py $(CPPFLAGS) $(CFLAGS) -- $<
	@touch $@

clean::
	$(RM) */*.checked */*.order

c_headers:: $(patsubst %,%.checked,$(wildcard */*.h))
c_sources:: $(patsubst %,%.checked,$(wildcard */*.c))

order::
	find_unneeded_includes.py -- $(wildcard */*.h) $(wildcard */*.c)

# ----------------------------------------------------------------------------
# Development Time Prototype Scanning
# ----------------------------------------------------------------------------

.SUFFIXES: %.q %.p %.g

PROTO_CPPFLAGS += $(CPPFLAGS)
PROTO_CPPFLAGS += -D_Float32=float
PROTO_CPPFLAGS += -D_Float64=double
PROTO_CPPFLAGS += -D_Float128="long double"
PROTO_CPPFLAGS += -D_Float32x=float
PROTO_CPPFLAGS += -D_Float64x=double
PROTO_CPPFLAGS += -D_Float128x="long double"

%.q : %.c ; $(CC) -o $@ -E $< $(PROTO_CPPFLAGS)
%.p : %.q ; cproto -s < $< | prettyproto.py | tee $@
%.g : %.q ; cproto < $< | prettyproto.py -xg_module | tee $@

protos-q: $(patsubst %.c,%.q,$(wildcard */*.c))
protos-p: $(patsubst %.c,%.p,$(wildcard */*.c))
protos-g: $(patsubst %.c,%.g,$(wildcard */*.c))

protos:: protos-p protos-g

clean::
	$(RM) -f */*.[qpg]
