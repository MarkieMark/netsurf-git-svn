#
# Makefile for NetSurf
#
# Copyright 2007 Daniel Silverstone <dsilvers@netsurf-browser.org>
# Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
#
# Trivially, invoke as:
#   make
# to build native, or:
#   make TARGET=riscos
# to cross-build for RO.
#
# Look at Makefile.config for configuration options.
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
#	have an up-to-date RISC OS build system. ;-)
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
  ifeq ($(HOST),BeOS)
    HOST := beos
  endif
  ifeq ($(HOST),Haiku)
    # Haiku implements the BeOS API
    HOST := beos
  endif
  ifeq ($(HOST),beos)
    # Build happening on BeOS platform, default target is BeOS backend
    ifeq ($(TARGET),)
      TARGET := beos
    endif
    # BeOS still uses gcc2
    GCCVER := 2
  else
    ifeq ($(HOST),AmigaOS)
      HOST := amiga
      ifeq ($(TARGET),)
        TARGET := amiga
      endif
    else
      ifeq ($(HOST),Darwin)
        HOST := macosx
      endif

      # Default target is GTK backend
      ifeq ($(TARGET),)
        TARGET := gtk
      endif
    endif
  endif
endif
SUBTARGET =
RESOURCES =

ifneq ($(TARGET),riscos)
  ifneq ($(TARGET),gtk)
    ifneq ($(TARGET),beos)
      ifneq ($(TARGET),amiga)
        ifneq ($(TARGET),framebuffer)
          ifneq ($(TARGET),windows)
            $(error Unknown TARGET "$(TARGET)", should either be "riscos", "gtk", "beos", "amiga", "framebuffer" or "windows")
          endif
        endif
      endif
    endif
  endif
endif

Q=@
VQ=@
PERL=perl
MKDIR=mkdir
TOUCH=touch
STRIP=strip

# Override this only if the host compiler is called something different
HOST_CC := gcc
ifeq ($(TARGET),amiga)
  ifneq ($(HOST),amiga)
    CC := ppc-amigaos-gcc
  endif
endif

ifeq ($(TARGET),riscos)
  ifeq ($(HOST),riscos)
    # Build for RO on RO
    GCCSDK_INSTALL_ENV := <NSLibs$$Dir>
    CCRES := ccres
    TPLEXT :=
    MAKERUN := makerun
    RUNEXT :=
    CC := gcc
    EXEEXT :=
    PKG_CONFIG :=
  else
    # Cross-build for RO (either using GCCSDK 3.4.6 - AOF,
    # either using GCCSDK 4 - ELF)
    GCCSDK_INSTALL_ENV ?= /home/riscos/env
    GCCSDK_INSTALL_CROSSBIN ?= /home/riscos/cross/bin
    CCRES := $(GCCSDK_INSTALL_CROSSBIN)/ccres
    TPLEXT := ,fec
    MAKERUN := $(GCCSDK_INSTALL_CROSSBIN)/makerun
    RUNEXT := ,feb
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
    CCACHE := $(shell which ccache)
    ifneq ($(CCACHE),)
      CC := $(CCACHE) $(CC)
    endif
  endif
else
  ifeq ($(TARGET),beos)
    # Building for BeOS/Haiku
    #ifeq ($(HOST),beos)
      # Build for BeOS on BeOS
      GCCSDK_INSTALL_ENV := /boot/develop
      CC := gcc
      CXX := g++
      EXEEXT :=
      PKG_CONFIG :=
    #endif
  else
    ifeq ($(TARGET),windows)
      ifneq ($(HOST),windows)
        # Set Mingw defaults
	MINGW_PREFIX ?= i586-mingw32msvc-
	MINGW_INSTALL_ENV ?= /usr/i586-mingw32msvc/

        # mingw cross-compile
        CC := $(MINGW_PREFIX)gcc
        PKG_CONFIG := $(MINGW_INSTALL_ENV)/bin/pkg-config
      endif
    else
      # Building for GTK, Amiga, Framebuffer
      PKG_CONFIG := pkg-config
    endif
  endif
endif

OBJROOT = build-$(HOST)-$(TARGET)$(SUBTARGET)

# ----------------------------------------------------------------------------
# General flag setup
# ----------------------------------------------------------------------------

# Set up the WARNFLAGS here so that they can be overridden in the Makefile.config
WARNFLAGS = -W -Wall -Wundef -Wpointer-arith \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs
ifneq ($(GCCVER),2)
  WARNFLAGS += -Wno-unused-parameter 
endif

# Pull in the configuration
include Makefile.defaults

# 1: Feature name (ie, NETSURF_USE_BMP -> BMP)
# 2: Parameters to add to CFLAGS
# 3: Parameters to add to LDFLAGS
# 4: Human-readable name for the feature
define feature_enabled
  ifeq ($$(NETSURF_USE_$(1)),YES)
    CFLAGS += $(2)
    LDFLAGS += $(3)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(4)	enabled       (NETSURF_USE_$(1) := YES))
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(4)	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    $$(info M.CONFIG: $(4)	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
    $$(error NETSURF_USE_$(1) must be YES or NO)
  endif
endef

# 1: Feature name (ie, NETSURF_USE_RSVG -> RSVG)
# 2: pkg-config required modules for feature
# 3: Human-readable name for the feature
define pkg_config_find_and_add
  ifeq ($$(PKG_CONFIG),)
    $$(error pkg-config is required to auto-detect feature availability)
  endif

  NETSURF_FEATURE_$(1)_AVAILABLE := $$(shell $$(PKG_CONFIG) --exists $(2) && echo yes)

  ifeq ($$(NETSURF_USE_$(1)),YES)
    ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	enabled       (NETSURF_USE_$(1) := YES))
      endif
    else
      $$(info M.CONFIG: $(3) ($(2))	failed        (NETSURF_USE_$(1) := YES))
      $$(error Unable to find library for: $(3) ($(2)))
    endif
  else ifeq ($$(NETSURF_USE_$(1)),AUTO)
    ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	auto-enabled  (NETSURF_USE_$(1) := AUTO))
      endif
    else
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	auto-disabled (NETSURF_USE_$(1) := AUTO))
      endif
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(3) ($(2))	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    $$(info M.CONFIG: $(3) ($(2))	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
    $$(error NETSURF_USE_$(1) must be YES, NO, or AUTO)
  endif
endef

$(eval $(call feature_enabled,JPEG,-DWITH_JPEG,-ljpeg,JPEG (libjpeg)))
$(eval $(call feature_enabled,MNG,-DWITH_MNG,-lmng,JNG/MNG/PNG (libmng)))

$(eval $(call feature_enabled,HARU_PDF,-DWITH_PDF_EXPORT,-lhpdf -lpng,PDF export (haru)))
$(eval $(call feature_enabled,LIBICONV_PLUG,-DLIBICONV_PLUG,,glibc internal iconv))

# common libraries without pkg-config support
LDFLAGS += -lz

CFLAGS += -DNETSURF_UA_FORMAT_STRING=\"$(NETSURF_UA_FORMAT_STRING)\"
CFLAGS += -DNETSURF_HOMEPAGE=\"$(NETSURF_HOMEPAGE)\"

# ----------------------------------------------------------------------------
# RISC OS target setup
# ----------------------------------------------------------------------------

ifeq ($(TARGET),riscos)
  ifeq ($(HOST),riscos)
    LDFLAGS += -Xlinker -symbols=$(OBJROOT)/sym -lxml2 -lz -lm -lcurl -lcares
    LDFLAGS += -lssl -lcrypto -lhubbub -lcss -lparserutils -lwapcaplet
  else
    LDFLAGS += $(shell $(PKG_CONFIG) --libs libxml-2.0 libcares libcurl openssl)
    LDFLAGS += $(shell $(PKG_CONFIG) --libs libhubbub libcss)
  endif

  $(eval $(call feature_enabled,NSSVG,-DWITH_NS_SVG,-lsvgtiny,SVG (libsvgtiny)))
  $(eval $(call feature_enabled,DRAW,-DWITH_DRAW,,Drawfile rendering))
  $(eval $(call feature_enabled,SPRITE,-DWITH_SPRITE,,Sprite rendering))
  $(eval $(call feature_enabled,ARTWORKS,-DWITH_ARTWORKS,,ArtWorks rendering))
  $(eval $(call feature_enabled,PLUGINS,-DWITH_PLUGIN,,Plugin protocol))
  $(eval $(call feature_enabled,DRAW_EXPORT,-DWITH_DRAW_EXPORT,-lpencil,Drawfile export))
  ifeq ($(HOST),riscos)
    $(eval $(call feature_enabled,BMP,-DWITH_BMP,-lnsbmp,BMP (libnsbmp)))
    $(eval $(call feature_enabled,GIF,-DWITH_GIF,-lnsgif,GIF (libnsgif)))
    $(eval $(call feature_enabled,PNG,-DWITH_PNG,-lpng,PNG (libpng)  ))
  else
    NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
    NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF
    NETSURF_FEATURE_PNG_CFLAGS := -DWITH_PNG
    $(eval $(call pkg_config_find_and_add,BMP,libnsbmp,BMP))
    $(eval $(call pkg_config_find_and_add,GIF,libnsgif,GIF))
    $(eval $(call pkg_config_find_and_add,PNG,libpng,PNG  ))
  endif

  TPD_RISCOS = $(foreach TPL,$(notdir $(TPL_RISCOS)), \
		!NetSurf/Resources/$(TPL)/Templates$(TPLEXT))

  RESOURCES = $(TPD_RISCOS)

  CFLAGS += -I. $(WARNFLAGS) -Driscos				\
		-std=c99 -D_BSD_SOURCE -D_POSIX_C_SOURCE	\
		-mpoke-function-name -fno-strict-aliasing

  CFLAGS += -I$(GCCSDK_INSTALL_ENV)/include			\
		-I$(GCCSDK_INSTALL_ENV)/include/libxml2		\
		-I$(GCCSDK_INSTALL_ENV)/include/libmng
  ifeq ($(HOST),riscos)
    CFLAGS += -I<OSLib$$Dir> -mthrowback
  endif
  ASFLAGS += -xassembler-with-cpp -I. -I$(GCCSDK_INSTALL_ENV)/include
  LDFLAGS += -L$(GCCSDK_INSTALL_ENV)/lib -lrufl
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

# ----------------------------------------------------------------------------
# BeOS target setup
# ----------------------------------------------------------------------------

ifeq ($(TARGET),beos)
  $(eval $(call feature_enabled,PNG,-DWITH_PNG,-lpng,PNG (libpng)  ))

  LDFLAGS += -L/boot/home/config/lib
  # for Haiku
  LDFLAGS += -L/boot/common/lib
  # some people do *not* have libm...
  LDFLAGS += -lxml2 -lcurl -liconv
  LDFLAGS += -lssl -lcrypto -lhubbub -lcss -lparserutils -lwapcaplet

  CFLAGS += -I. -O $(WARNFLAGS) -Dnsbeos		\
		-D_BSD_SOURCE -D_POSIX_C_SOURCE		\
		-Drestrict="" -Wno-multichar 
  # DEBUG
  CFLAGS += -g -O0
  # -DDEBUG=1

  BEOS_BERES := beres
  BEOS_RC := rc
  BEOS_XRES := xres
  BEOS_SETVER := setversion
  BEOS_MIMESET := mimeset
  VERSION_FULL := $(shell sed -n '/"/{s/.*"\(.*\)".*/\1/;p;}' desktop/version.c)
  VERSION_MAJ := $(shell sed -n '/_major/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)
  VERSION_MIN := $(shell sed -n '/_minor/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)
  RSRC_BEOS = $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.rdef,%.rsrc,$(RDEF_BEOS))))
  RESOURCES = $(RSRC_BEOS)
  ifeq ($(HOST),beos)
    CFLAGS += -I/boot/home/config/include		\
		-I/boot/home/config/include/libxml2	\
		-I/boot/home/config/include/libmng	\
		-I/boot/home/config/include/hubbub	\
		-I/boot/home/config/include/libcss	\
		-I/boot/home/config/include/parserutils
    ifneq ($(wildcard /boot/develop/lib/*/libzeta.so),)
      LDFLAGS += -lzeta
    endif
    ifneq ($(wildcard /boot/develop/lib/*/libnetwork.so),)
      # Haiku
      CFLAGS += -I/boot/common/include		\
		-I/boot/common/include/libxml2	\
		-I/boot/common/include/libmng	\
		-I/boot/common/include/hubbub	\
		-I/boot/common/include/libcss	\
		-I/boot/common/include/parserutils
      NETLDFLAGS := -lnetwork
    else
      ifneq ($(wildcard /boot/develop/lib/*/libbind.so),)
        # BONE
        NETLDFLAGS := -lsocket -lbind
      else
        # net_server, will probably never work
        NETLDFLAGS := -lnet
      endif
    endif
  else
    # cross: Haiku ?
    NETLDFLAGS := -lnetwork
  endif
  LDFLAGS += -lbe -ltranslation $(NETLDFLAGS)

  $(eval $(call feature_enabled,NSSVG,-DWITH_NS_SVG,-lsvgtiny,SVG (libsvgtiny)))
  ifeq ($(HOST),beos)
    CFLAGS += -I$(PREFIX)/include
    LDFLAGS += -L$(PREFIX)/lib
    $(eval $(call feature_enabled,BMP,-DWITH_BMP,-lnsbmp,BMP (libnsbmp)))
    $(eval $(call feature_enabled,GIF,-DWITH_GIF,-lnsgif,GIF (libnsgif)))
    $(eval $(call feature_enabled,PNG,-DWITH_PNG,-lpng,PNG (libpng)  ))
  else
    NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
    NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF
    NETSURF_FEATURE_PNG_CFLAGS := -DWITH_PNG
    $(eval $(call pkg_config_find_and_add,BMP,libnsbmp,BMP))
    $(eval $(call pkg_config_find_and_add,GIF,libnsgif,GIF))
    $(eval $(call pkg_config_find_and_add,PNG,libpng,PNG  ))
  endif
endif

# ----------------------------------------------------------------------------
# GTK flag setup (using pkg-config)
# ----------------------------------------------------------------------------

ifeq ($(TARGET),gtk)
  LDFLAGS += $(shell $(PKG_CONFIG) --libs libxml-2.0 libcurl libhubbub openssl)
  LDFLAGS += $(shell $(PKG_CONFIG) --libs libcss)

  # define additional CFLAGS and LDFLAGS requirements for pkg-configed libs here
  NETSURF_FEATURE_RSVG_CFLAGS := -DWITH_RSVG
  NETSURF_FEATURE_NSSVG_CFLAGS := -DWITH_NS_SVG
  NETSURF_FEATURE_ROSPRITE_CFLAGS := -DWITH_NSSPRITE
  NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
  NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF
  NETSURF_FEATURE_PNG_CFLAGS := -DWITH_PNG

  # add a line similar to below for each optional pkg-configed lib here
  $(eval $(call pkg_config_find_and_add,RSVG,librsvg-2.0,SVG))
  $(eval $(call pkg_config_find_and_add,NSSVG,libsvgtiny,SVG))
  $(eval $(call pkg_config_find_and_add,ROSPRITE,librosprite,Sprite))
  $(eval $(call pkg_config_find_and_add,BMP,libnsbmp,BMP))
  $(eval $(call pkg_config_find_and_add,GIF,libnsgif,GIF))
  $(eval $(call pkg_config_find_and_add,PNG,libpng,PNG  ))

  GTKCFLAGS := -std=c99 -Dgtk -Dnsgtk \
		-DGTK_DISABLE_DEPRECATED \
		-D_BSD_SOURCE \
		-D_XOPEN_SOURCE=600 \
		-D_POSIX_C_SOURCE=200112L \
		-D_NETBSD_SOURCE \
		-DGTK_RESPATH=\"$(NETSURF_GTK_RESOURCES)\" \
		$(WARNFLAGS) -I. -g \
		$(shell $(PKG_CONFIG) --cflags libglade-2.0 gtk+-2.0) \
		$(shell $(PKG_CONFIG) --cflags libhubbub libcurl openssl) \
		$(shell xml2-config --cflags)

  GTKLDFLAGS := $(shell $(PKG_CONFIG) --cflags --libs libglade-2.0 gtk+-2.0 gthread-2.0 gmodule-2.0 lcms)

  CFLAGS += $(GTKCFLAGS)
  LDFLAGS += $(GTKLDFLAGS)

  # ---------------------------------------------------------------------------
  # Windows flag setup
  # ---------------------------------------------------------------------------

  ifeq ($(HOST),Windows_NT)
    CFLAGS += -U__STRICT_ANSI__
  endif
endif

# ----------------------------------------------------------------------------
# Windows target setup
# ----------------------------------------------------------------------------
ifeq ($(TARGET),windows)
  NETSURF_FEATURE_NSSVG_CFLAGS := -DWITH_NS_SVG
  NETSURF_FEATURE_ROSPRITE_CFLAGS := -DWITH_NSSPRITE
  NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
  NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF
  NETSURF_FEATURE_PNG_CFLAGS := -DWITH_PNG
  $(eval $(call feature_enabled,BMP,-DWITH_BMP,-lnsbmp,BMP (libnsbmp)))
  $(eval $(call feature_enabled,GIF,-DWITH_GIF,-lnsgif,GIF (libnsgif)))
  $(eval $(call feature_enabled,PNG,-DWITH_PNG,-lpng,PNG (libpng)  ))
  $(eval $(call feature_enabled,NSSVG,-DWITH_NS_SVG,-lsvgtiny,SVG (libsvgtiny)))
  $(eval $(call feature_enabled,MNG,,-llcms -ljpeg,PNG/JNG/MNG (libmng)))

  LDFLAGS += -L${MINGW_INSTALL_ENV}/lib $(shell $(PKG_CONFIG) --libs zlib \
	libxml-2.0 libcurl libhubbub libparserutils libcss libwapcaplet) \
	-lparserutils -lssl -lcrypto -lregex -liconv -lcss -lwapcaplet \
	-lgdi32 -lcomctl32 -lws2_32 -lmsimg32 -mwindows
  CFLAGS += -U__STRICT_ANSI__ -mwin32
  WSCFLAGS := -std=c99 \
	$(WARNFLAGS) -I. -I/${MINGW_INSTALL_ENV}/include \
	-DCURL_STATICLIB \
	$(shell $(PKG_CONFIG) --cflags libcurl libhubbub zlib libparserutils \
	libxml-2.0) -g
  CFLAGS += $(WSCFLAGS)
  LDFLAGS += $(WSCFLAGS)
endif

# ----------------------------------------------------------------------------
# Amiga target setup
# ----------------------------------------------------------------------------

ifeq ($(TARGET),amiga)
  NETSURF_FEATURE_ROSPRITE_CFLAGS := -DWITH_NSSPRITE
  NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
  NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF
  NETSURF_FEATURE_PNG_CFLAGS := -DWITH_PNG
  NETSURF_FEATURE_NSSVG_CFLAGS := -DWITH_NS_SVG

    $(eval $(call feature_enabled,ROSPRITE,-DWITH_NSSPRITE,-lrosprite,Sprite (librosprite)))
    $(eval $(call feature_enabled,BMP,-DWITH_BMP,-lnsbmp,BMP (libnsbmp)))
    $(eval $(call feature_enabled,GIF,-DWITH_GIF,-lnsgif,GIF (libnsgif)))
    $(eval $(call feature_enabled,PNG,-DWITH_PNG,-lpng,PNG (libpng)  ))
    $(eval $(call feature_enabled,NSSVG,-DWITH_NS_SVG,-lsvgtiny,SVG (libsvgtiny)))
    $(eval $(call feature_enabled,MNG,,-llcms -ljpeg,PNG/JNG/MNG (libmng)))

  CFLAGS += -D__USE_INLINE__ -std=c99 -I . -Dnsamiga
  LDFLAGS += -lxml2 -lcurl -lpthread -lregex -lauto
  LDFLAGS += -lssl -lcrypto -lhubbub -lcss -lparserutils -lwapcaplet

  ifeq ($(NETSURF_AMIGA_USE_CAIRO),YES)
    CFLAGS += -DNS_AMIGA_CAIRO -I SDK:local/common/include/cairo
    LDFLAGS += -use-dynld -lcairo -lpixman-1 -lfreetype -lfontconfig -lpng12 -lexpat
  endif
endif

# ----------------------------------------------------------------------------
# Framebuffer target setup
# ----------------------------------------------------------------------------

ifeq ($(TARGET),framebuffer)

  $(eval $(call feature_enabled,MNG,-DWITH_MNG,-lmng,PNG/MNG/JNG (libmng)))
  $(eval $(call feature_enabled,PNG,-DWITH_PNG,-lpng,PNG (libpng)  ))


  ifeq ($(NETSURF_FB_FONTLIB),freetype)
    CFLAGS += -DFB_USE_FREETYPE $(shell freetype-config --cflags)
    LDFLAGS += $(shell freetype-config --libs)
  endif

  # define additional CFLAGS and LDFLAGS requirements for pkg-configed libs here
  NETSURF_FEATURE_RSVG_CFLAGS := -DWITH_RSVG
  NETSURF_FEATURE_ROSPRITE_CFLAGS := -DWITH_NSSPRITE
  NETSURF_FEATURE_HUBBUB_CFLAGS := -DWITH_HUBBUB
  NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
  NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF

  CFLAGS += -Dnsframebuffer '-DNETSURF_FB_RESPATH="$(NETSURF_FB_RESPATH_$(NETSURF_FB_FRONTEND))"'

  $(eval $(call pkg_config_find_and_add,ROSPRITE,librosprite,Sprite))
  $(eval $(call pkg_config_find_and_add,BMP,libnsbmp,BMP))
  $(eval $(call pkg_config_find_and_add,GIF,libnsgif,GIF))


  CFLAGS += -std=c99 -g -I. -Dsmall $(WARNFLAGS) \
		-D_BSD_SOURCE \
		-D_XOPEN_SOURCE=600 \
		-D_POSIX_C_SOURCE=200112L  \
		$(shell $(PKG_CONFIG) --cflags libnsfb) \
		$(shell $(PKG_CONFIG) --cflags libhubbub libcurl openssl) \
		$(shell $(PKG_CONFIG) --cflags libcss) \
		$(shell xml2-config --cflags)

  LDFLAGS += -Wl,--whole-archive $(shell $(PKG_CONFIG) --libs libnsfb) -Wl,--no-whole-archive 
  LDFLAGS += $(shell $(PKG_CONFIG) --libs libxml-2.0 libcurl libhubbub openssl)
  LDFLAGS += $(shell $(PKG_CONFIG) --libs libcss)

endif

# ----------------------------------------------------------------------------
# General make rules
# ----------------------------------------------------------------------------

$(OBJROOT)/created:
	$(VQ)echo "   MKDIR: $(OBJROOT)"
	$(Q)$(MKDIR) $(OBJROOT)
	$(Q)$(TOUCH) $(OBJROOT)/created

DEPROOT := $(OBJROOT)/deps
$(DEPROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(DEPROOT)"
	$(Q)$(MKDIR) $(DEPROOT)
	$(Q)$(TOUCH) $(DEPROOT)/created

TOOLROOT := $(OBJROOT)/tools
$(TOOLROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(TOOLROOT)"
	$(Q)$(MKDIR) $(TOOLROOT)
	$(Q)$(TOUCH) $(TOOLROOT)/created

CLEANS := clean-target

POSTEXES :=

include Makefile.resources
include Makefile.sources

OBJECTS := $(sort $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(patsubst %.s,%.o,$(SOURCES)))))))

$(EXETARGET): $(OBJECTS) $(RESOURCES)
	$(VQ)echo "    LINK: $(EXETARGET)"
ifneq ($(TARGET)$(SUBTARGET),riscos-elf)
	$(Q)$(CC) -o $(EXETARGET) $(OBJECTS) $(LDFLAGS)
else
	$(Q)$(CC) -o $(EXETARGET:,ff8=,e1f) $(OBJECTS) $(LDFLAGS)
	$(Q)$(ELF2AIF) $(EXETARGET:,ff8=,e1f) $(EXETARGET)
	$(Q)$(RM) $(EXETARGET:,ff8=,e1f)
endif
ifeq ($(TARGET),windows)
	$(Q)$(TOUCH) windows/res/preferences
endif
ifeq ($(TARGET),gtk)
	$(Q)$(TOUCH) gtk/res/toolbarIndices
endif
ifeq ($(NETSURF_STRIP_BINARY),YES)
	$(VQ)echo "   STRIP: $(EXETARGET)"
	$(Q)$(STRIP) $(EXETARGET)
endif
ifeq ($(TARGET),beos)
	$(VQ)echo "    XRES: $(EXETARGET)"
	$(Q)$(BEOS_XRES) -o $(EXETARGET) $(RSRC_BEOS)
	$(VQ)echo "  SETVER: $(EXETARGET)"
	$(Q)$(BEOS_SETVER) $(EXETARGET) \
                -app $(VERSION_MAJ) $(VERSION_MIN) 0 d 0 \
                -short "NetSurf $(VERSION_FULL)" \
                -long "NetSurf $(VERSION_FULL) © 2003 - 2008 The NetSurf Developers"
	$(VQ)echo " MIMESET: $(EXETARGET)"
	$(Q)$(BEOS_MIMESET) $(EXETARGET)
endif

ifeq ($(TARGET),beos)
$(RSRC_BEOS): $(RDEF_BEOS) $(RDEP_BEOS)
	$(VQ)echo "      RC: $<"
	$(Q)$(BEOS_RC) -o $@ $<
endif

ifeq ($(HOST),amiga)
	$(VQ)echo "   CLEAN: amiga/version.c"
	$(Q)$(RM) amiga/version.c
amiga/version.c: amiga/version.rexx
	$(VQ)echo "   AREXX: amiga/version.rexx"
	$(Q)rx amiga/version.rexx >amiga/version.c
endif

ifeq ($(TARGET),riscos)
  # Native RO build is different as 1) it can't do piping and 2) ccres on
  # RO does not understand Unix filespec
  ifeq ($(HOST),riscos)
    define compile_template
!NetSurf/Resources/$(1)/Templates$$(TPLEXT): $(2)
	$$(VQ)echo "TEMPLATE: $(2)"
	$$(Q)$$(CC) -x c -E -P $$(CFLAGS) -o processed_template $(2)
	$$(Q)$$(CCRES) processed_template $$(subst /,.,$$@)
	$$(Q)$(RM) processed_template
CLEAN_TEMPLATES += !NetSurf/Resources/$(1)/Templates$$(TPLEXT)

    endef
  else
    define compile_template
!NetSurf/Resources/$(1)/Templates$$(TPLEXT): $(2)
	$$(VQ)echo "TEMPLATE: $(2)"
	$$(Q)$$(CC) -x c -E -P $$(CFLAGS) $(2) | $$(CCRES) - $$@
CLEAN_TEMPLATES += !NetSurf/Resources/$(1)/Templates$$(TPLEXT)

    endef
  endif

clean-templates:
	$(VQ)echo "   CLEAN: $(CLEAN_TEMPLATES)"
	$(Q)$(RM) $(CLEAN_TEMPLATES)
CLEANS += clean-templates

$(eval $(foreach TPL,$(TPL_RISCOS), \
	$(call compile_template,$(notdir $(TPL)),$(TPL))))
endif

clean-target:
	$(VQ)echo "   CLEAN: $(EXETARGET)"
	$(Q)$(RM) $(EXETARGET)

clean-builddir:
	$(VQ)echo "   CLEAN: $(OBJROOT)"
	$(Q)$(RM) -r $(OBJROOT)
CLEANS += clean-builddir

all-program: $(EXETARGET) post-exe

post-exe: $(POSTEXES)

.SUFFIXES:

DEPFILES :=
# Now some macros which build the make system

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_c
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1) Makefile.config

endef

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_s
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
ifeq ($(GCCVER),2)
# simpler deps tracking for gcc2...
define compile_target_c
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(CFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$(3) $$(OBJROOT)/$(2):,' \
		    > $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(CC) $$(CFLAGS) -o $$(OBJROOT)/$(2) -c $(1)

endef
else
define compile_target_c
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(CFLAGS) -MMD -MT '$$(DEPROOT)/$(3) $$(OBJROOT)/$(2)' \
		    -MF $$(DEPROOT)/$(3) -o $$(OBJROOT)/$(2) -c $(1)

endef
endif

define compile_target_cpp
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(CFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$(3) $$(OBJROOT)/$(2):,' \
		    > $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(CXX) $$(CFLAGS) -o $$(OBJROOT)/$(2) -c $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
define compile_target_s
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "ASSEMBLE: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(ASFLAGS) -MMD -MT '$$(DEPROOT)/$(3) $$(OBJROOT)/$(2)' \
		    -MF $$(DEPROOT)/$(3) -o $$(OBJROOT)/$(2) -c $(1)

endef

# Rules to construct dep lines for each object...
$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.d)),$(subst /,_,$(SOURCE:.c=.o)))))

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.d)),$(subst /,_,$(SOURCE:.cpp=.o)))))

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

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call compile_target_cpp,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.o)),$(subst /,_,$(SOURCE:.cpp=.d)))))

$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
	$(call compile_target_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.o)),$(subst /,_,$(SOURCE:.s=.d)))))

.PHONY: all clean docs install install-gtk

clean: $(CLEANS)

install-gtk: nsgtk
	mkdir -p $(DESTDIR)$(NETSURF_GTK_RESOURCES)throbber
	mkdir -p $(DESTDIR)$(NETSURF_GTK_BIN)
	@cp -v nsgtk $(DESTDIR)$(NETSURF_GTK_BIN)netsurf
	@cp -vRL gtk/res/adblock.css $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/arrow_down_8x32.png $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/blankpage $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/ca-bundle.txt $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/default.css $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/favicon.png $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/gtkdefault.css $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/license $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/languages $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/netsurf.xpm $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/netsurf-16x16.xpm $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/themelist $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/throbber/*.png $(DESTDIR)$(NETSURF_GTK_RESOURCES)/throbber
	@cp -vRL gtk/res/Aliases $(DESTDIR)$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/docs $(DESTDIR)/$(NETSURF_GTK_RESOURCES)
	@cp -vRL gtk/res/SearchEngines $(DESTDIR)$(NETSURF_GTK_RESOURCES)SearchEngines
	gzip -9v < gtk/res/downloads.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)downloads.glade
	gzip -9v < gtk/res/history.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)history.glade
	gzip -9v < gtk/res/login.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)login.glade
	gzip -9v < gtk/res/messages > $(DESTDIR)$(NETSURF_GTK_RESOURCES)messages
	gzip -9v < gtk/res/netsurf.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)netsurf.glade
	gzip -9v < gtk/res/options.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)options.glade
	gzip -9v < gtk/res/password.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)password.glade
	gzip -9v < gtk/res/source.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)source.glade
	gzip -9v < gtk/res/ssl.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)ssl.glade
	gzip -9v < gtk/res/toolbar.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)toolbar.glade
	gzip -9v < gtk/res/warning.glade > $(DESTDIR)$(NETSURF_GTK_RESOURCES)warning.glade

install-beos: NetSurf
	# TODO:HAIKU -- not sure if throbber is needed.  being left out for now.
	mkdir -p $(DESTDIR)$(NETSURF_BEOS_BIN)
	mkdir -p $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
#	mkdir -p $(DESTDIR)$(NETSURF_BEOS_RESOURCES)throbber
	@copyattr -d NetSurf $(DESTDIR)$(NETSURF_BEOS_BIN)NetSurf
	@cp -vRL beos/res/adblock.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL beos/res/ca-bundle.txt $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL beos/res/default.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL beos/res/Aliases $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL beos/res/beosdefault.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL gtk/res/license $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
#	@cp -vRL beos/res/throbber/*.png $(DESTDIR)$(NETSURF_BEOS_RESOURCES)throbber
	gzip -9v < beos/res/messages > $(DESTDIR)$(NETSURF_BEOS_RESOURCES)messages 


install-framebuffer: $(EXETARGET)
	mkdir -p $(DESTDIR)$(NETSURF_FRAMEBUFFER_BIN)
	mkdir -p $(DESTDIR)$(NETSURF_FRAMEBUFFER_RESOURCES)
	@cp -v $(EXETARGET) $(DESTDIR)/$(NETSURF_FRAMEBUFFER_BIN)netsurf$(SUBTARGET)
	@for F in Aliases default.css messages; do cp -vL framebuffer/res/$$F $(DESTDIR)/$(NETSURF_FRAMEBUFFER_RESOURCES); done

install: all-program install-$(TARGET)

docs:
	doxygen Docs/Doxyfile
