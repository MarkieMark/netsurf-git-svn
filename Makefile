#
# Makefile for NetSurf
#
# Copyright 2007 Daniel Silverstone <dsilvers@netsurf-browser.org>
#
#

# Trivially, invoke as:
#   make
# to build native, or:
#   make TARGET=riscos
# to cross-build for RO.
#
# Tested on unix platforms (building for GTK and cross-compiling for RO) and
# on RO (building for RO).
#
# To clean, invoke as above, with the 'clean' target
#
# To build developer Doxygen generated documentation, invoke as above,
# with the 'docs' target:
#   make docs
#

all: all-program

# Determine host type
# NOTE: HOST determination on RISC OS could fail because of missing bug fixes
#	in UnixLib which only got addressed in UnixLib 5 / GCCSDK 4.
#	When you don't have 'uname' available, you will see:
#	  File 'uname' not found
#	When you do and using a 'uname' compiled with a buggy UnixLib, you
#	will see the following printed on screen:
#	  RISC OS
#	In both cases HOST make variable is empty and we recover from that by
#	assuming we're building on RISC OS.
#	In case you don't see anything printed (including the warning), you
#	have an update to date RISC OS build sytem. ;-)
HOST := $(shell uname -s)
ifeq ($(HOST),)
HOST := riscos
$(warning Build platform determination failed but that's a known problem for RISC OS so we're assuming a native RISC OS build.)
else
ifeq ($(HOST),RISC OS)
# Fixup uname -s returning "RISC OS"
HOST := riscos
endif
endif

ifeq ($(HOST),riscos)
# Build happening on RO platform, default target is RO backend
ifeq ($(TARGET),)
TARGET := riscos
endif
else
# Build happening on non-RO platform, default target is GTK backend
ifeq ($(TARGET),)
TARGET := gtk
endif
endif
SUBTARGET =

ifneq ($(TARGET),riscos)
ifneq ($(TARGET),gtk)
ifneq ($(TARGET),debug)
$(error Unknown TARGET "$(TARGET)", should either be "riscos", "gtk", or "debug")
endif
endif
endif

Q=@
VQ=@
PERL=perl
MKDIR=mkdir
TOUCH=touch

ifeq ($(TARGET),riscos)
ifeq ($(HOST),riscos)
# Build for RO on RO
GCCSDK_INSTALL_ENV := <NSLibs$$Dir>
CC := gcc
EXEEXT :=
PKG_CONFIG :=
else
# Cross-build for RO (either using GCCSDK 3.4.6 - AOF,
# either using GCCSDK 4 - ELF)
GCCSDK_INSTALL_ENV ?= /home/riscos/env
GCCSDK_INSTALL_CROSSBIN ?= /home/riscos/cross/bin
CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
ifneq (,$(findstring arm-unknown-riscos-gcc,$(CC)))
SUBTARGET := -elf
EXEEXT := ,e1f
ELF2AIF := $(GCCSDK_INSTALL_CROSSBIN)/elf2aif
else
SUBTARGET := -aof
EXEEXT := ,ff8
endif
PKG_CONFIG := $(GCCSDK_INSTALL_ENV)/ro-pkg-config
endif
else
# Building for GTK or debug
PKG_CONFIG := pkg-config
endif

OBJROOT := build-$(HOST)-$(TARGET)$(SUBTARGET)

ifeq ($(HOST),riscos)
LDFLAGS := -Xlinker -symbols=$(OBJROOT)/sym -lxml2 -lz -lm -lcurl -lssl -lcrypto -lmng -ljpeg
else
LDFLAGS := $(shell $(PKG_CONFIG) --libs libxml-2.0 libcurl openssl)
LDFLAGS += -lz -lm -lmng -ljpeg

CCACHE := $(shell which ccache)

ifneq ($(CCACHE),)
CC := $(CCACHE) $(CC)
endif

endif

ifeq ($(TARGET),gtk)
# Building for GTK, we need the GTK flags

GTKCFLAGS := -std=c99 -Dgtk -Dnsgtk \
	-DGTK_DISABLE_DEPRECATED \
	-D_BSD_SOURCE \
	-D_XOPEN_SOURCE=600 \
	-D_POSIX_C_SOURCE=200112L \
	-D_NETBSD_SOURCE \
	$(WARNFLAGS) -I. -I../../libsprite/trunk/ -g $(OPT2FLAGS) \
	$(shell $(PKG_CONFIG) --cflags libglade-2.0 gtk+-2.0 librsvg-2.0) \
	$(shell $(PKG_CONFIG) --cflags librosprite) \
	$(shell xml2-config --cflags)

GTKLDFLAGS := $(shell $(PKG_CONFIG) --cflags --libs libglade-2.0 gtk+-2.0 gthread-2.0 gmodule-2.0 librsvg-2.0 librosprite)
CFLAGS += $(GTKCFLAGS)
LDFLAGS += $(GTKLDFLAGS) $(shell $(PKG_CONFIG) --libs lcms)

ifeq ($(HOST),Windows_NT)
CFLAGS += -U__STRICT_ANSI__
endif

endif

ifeq ($(TARGET),riscos)
CFLAGS += -I. $(OPTFLAGS) $(WARNFLAGS) -Driscos		\
	-std=c99 -D_BSD_SOURCE -D_POSIX_C_SOURCE	\
	-mpoke-function-name

CFLAGS += -I$(GCCSDK_INSTALL_ENV)/include		\
	-I$(GCCSDK_INSTALL_ENV)/include/libxml2		\
	-I$(GCCSDK_INSTALL_ENV)/include/libmng
ifeq ($(HOST),riscos)
CFLAGS += -I<OSLib$$Dir> -mthrowback
endif
ASFLAGS += -xassembler-with-cpp -I. -I$(GCCSDK_INSTALL_ENV)/include
LDFLAGS += -L$(GCCSDK_INSTALL_ENV)/lib -lrufl -lpencil \
	-lsvgtiny
ifeq ($(HOST),riscos)
LDFLAGS += -LOSLib: -lOSLib32
else
LDFLAGS += -lOSLib32
ifeq ($(SUBTARGET),-elf)
# Go for static builds & AIF binary at the moment:
CFLAGS += -static
LDFLAGS += -static
EXEEXT := ,ff8
endif
endif
endif

ifeq ($(TARGET),debug)
CFLAGS += -std=c99 -DDEBUG_BUILD \
	-D_BSD_SOURCE \
	-D_XOPEN_SOURCE=600 \
	-D_POSIX_C_SOURCE=200112L \
	-D_NETBSD_SOURCE \
	$(WARNFLAGS) -I. -I../../libsprite/trunk/ -g $(OPT0FLAGS) \
	$(shell $(PKG_CONFIG) --cflags librosprite) \
	$(shell xml2-config --cflags)
LDFLAGS += $(shell $(PKG_CONFIG) --libs librosprite)
endif

$(OBJROOT)/created:
	$(VQ)echo "   MKDIR: $(OBJROOT)"
	$(Q)$(MKDIR) $(OBJROOT)
	$(Q)$(TOUCH) $(OBJROOT)/created

DEPROOT := $(OBJROOT)/deps
$(DEPROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(DEPROOT)"
	$(Q)$(MKDIR) $(DEPROOT)
	$(Q)$(TOUCH) $(DEPROOT)/created

WARNFLAGS = -W -Wall -Wundef -Wpointer-arith \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline -Wno-unused-parameter

OPT0FLAGS = -O0
# -O and -O2 can use -Wuninitialized which gives us more static checking.
# unfortunately the optimiser is what provides the hints in the code tree
# so we cannot do it when we do -O0 (E.g. debug)
OPTFLAGS = -O -Wuninitialized
OPT2FLAGS = -O2 -Wuninitialized

CLEANS := clean-target

include Makefile.sources

OBJECTS := $(sort $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.c,%.o,$(patsubst %.s,%.o,$(SOURCES))))))

$(EXETARGET): $(OBJECTS)
	$(VQ)echo "    LINK: $(EXETARGET)"
ifneq ($(TARGET)$(SUBTARGET),riscos-elf)
	$(Q)$(CC) -o $(EXETARGET) $(OBJECTS) $(LDFLAGS)
else
	$(Q)$(CC) -o $(EXETARGET:,ff8=,e1f) $(OBJECTS) $(LDFLAGS)
	$(Q)$(ELF2AIF) $(EXETARGET:,ff8=,e1f) $(EXETARGET)
	$(Q)$(RM) $(EXETARGET:,ff8=,e1f)
endif

clean-target:
	$(VQ)echo "   CLEAN: $(EXETARGET)"
	$(Q)$(RM) $(EXETARGET)

clean-builddir:
	$(VQ)echo "   CLEAN: $(OBJROOT)"
	$(Q)$(RM) -r $(OBJROOT)
CLEANS += clean-builddir

all-program: $(EXETARGET)

.SUFFIXES:

DEPFILES :=
# Now some macros which build the make system

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_c
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1) css/css_enum.h css/parser.h
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -MM -MT '$$(DEPROOT)/$2 $$(OBJROOT)/$(3)' \
		    -MF $$(DEPROOT)/$(2) $(1)

endef

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_s
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1)
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -MM -MT '$$(DEPROOT)/$2 $$(OBJROOT)/$(3)' \
		    -MF $$(DEPROOT)/$(2) $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
define compile_target_c
$$(OBJROOT)/$(2): $$(OBJROOT)/created $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(CC) $$(CFLAGS) -o $$@ -c $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
define compile_target_s
$$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo " ASSEMBLE: $(1)"
	$$(Q)$$(CC) $$(ASFLAGS) -o $$@ -c $(1)

endef

# Rules to construct dep lines for each object...
$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.d)),$(subst /,_,$(SOURCE:.c=.o)))))

# Cannot currently generate dep files for S files because they're objasm
# when we move to gas format, we will be able to.

#$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
#	$(call dependency_generate_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.d)),$(subst /,_,$(SOURCE:.s=.o)))))

ifneq ($(MAKECMDGOALS),clean)
-include $(sort $(addprefix $(DEPROOT)/,$(DEPFILES)))
endif

# And rules to build the objects themselves...

$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call compile_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.o)),$(subst /,_,$(SOURCE:.c=.d)))))

$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
	$(call compile_target_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.o)),$(subst /,_,$(SOURCE:.s=.d)))))

.PHONY: all clean docs

clean: $(CLEANS)

docs:
	doxygen Docs/Doxyfile