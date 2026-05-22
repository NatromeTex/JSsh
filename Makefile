CC = cc
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
BREW_READLINE := $(shell brew --prefix readline 2>/dev/null)
BREW_LIBSSH   := $(shell brew --prefix libssh 2>/dev/null)
BREW_LIBGIT2  := $(shell brew --prefix libgit2 2>/dev/null)
CFLAGS = -I./src/quickjs -Wall -O2 -DCONFIG_VERSION=\"2020-11-08\" -DMACOS -DJSSH_VERSION=\"0.6.1\" -DJSVIM_VERSION=\"0.3.0\" \
         $(if $(BREW_READLINE),-I$(BREW_READLINE)/include,)
LDFLAGS_BASE = -lm -lncurses $(if $(BREW_READLINE),-L$(BREW_READLINE)/lib -lreadline,-lreadline)
LDFLAGS_SSH  = $(if $(BREW_LIBSSH),-L$(BREW_LIBSSH)/lib,) -lssh
LDFLAGS_GIT  = $(if $(BREW_LIBGIT2),-L$(BREW_LIBGIT2)/lib,) -lgit2
else
CFLAGS = -I./src/quickjs -Wall -O2 -DCONFIG_VERSION=\"2020-11-08\" -D_GNU_SOURCE -DLINUX -DJSSH_VERSION=\"0.6.1\" -DJSVIM_VERSION=\"0.3.0\"
LDFLAGS_BASE = -lm -ldl -lreadline -lncurses
LDFLAGS_SSH  = -lssh
LDFLAGS_GIT  = -lgit2
endif

LDFLAGS = $(LDFLAGS_BASE)

# core sources
SRC = src/main.c \
      src/utils.c \
      src/func.c \
      src/sys.c \
      src/env.c \
      src/quickjs/quickjs.c \
      src/quickjs/quickjs-libc.c \
      src/quickjs/cutils.c \
      src/quickjs/libregexp.c \
      src/quickjs/libunicode.c \
      src/quickjs/dtoa.c

# optional modules
MODULES ?=
NEED_SETCAP =

# network module
ifneq ($(findstring network,$(MODULES)),)
SRC += lib/network/module.c \
       lib/network/net_utils.c
CFLAGS += -DENABLE_NETWORK $(if $(BREW_LIBSSH),-I$(BREW_LIBSSH)/include,)
LDFLAGS += $(LDFLAGS_SSH)
NEED_SETCAP = yes
endif

# compiler module
ifneq ($(findstring compiler,$(MODULES)),)
SRC += lib/compiler/module.c \
       lib/compiler/cmp_utils.c 
CFLAGS += -DENABLE_COMPILER
endif

# fs module
ifneq ($(findstring fs,$(MODULES)),)
SRC += lib/fs/module.c \
       lib/fs/fs_utils.c
CFLAGS += -DENABLE_FS
endif

# git module
ifneq ($(findstring git,$(MODULES)),)
SRC += lib/git/module.c \
       lib/git/git_utils.c
CFLAGS += -DENABLE_GIT $(if $(BREW_LIBGIT2),-I$(BREW_LIBGIT2)/include,)
LDFLAGS += $(LDFLAGS_GIT)
endif

# apps module (jsvim)
ifneq ($(findstring apps,$(MODULES)),)
SRC += lib/apps/module.c \
       lib/apps/app_utils.c
APPS_ENABLED = yes
CFLAGS += -DENABLE_APPS
endif

# enable all modules
ifeq ($(MODULES),all)
SRC += lib/network/module.c \
       lib/network/net_utils.c \
       lib/compiler/module.c \
       lib/compiler/cmp_utils.c \
       lib/fs/module.c \
       lib/fs/fs_utils.c \
       lib/git/module.c \
       lib/git/git_utils.c \
       lib/apps/module.c \
       lib/apps/app_utils.c
CFLAGS += -DENABLE_NETWORK -DENABLE_COMPILER -DENABLE_FS -DENABLE_GIT -DENABLE_APPS \
          $(if $(BREW_LIBSSH),-I$(BREW_LIBSSH)/include,) \
          $(if $(BREW_LIBGIT2),-I$(BREW_LIBGIT2)/include,)
LDFLAGS += $(LDFLAGS_SSH) $(LDFLAGS_GIT)
APPS_ENABLED = yes
NEED_SETCAP = yes
endif

OBJ = $(SRC:.c=.o)


# Default build target
ifeq ($(APPS_ENABLED),yes)
all: bin/jssh bin/jsvim
else
all: bin/jssh
endif


# Build main jssh executable
bin/jssh: $(OBJ)
	@mkdir -p bin
	$(CC) -o $@ $(OBJ) $(LDFLAGS)
ifeq ($(NEED_SETCAP),yes)
ifneq ($(UNAME), Darwin)
	@echo "Setting cap_net_raw on $@"
	@sudo setcap cap_net_raw=ep $@
else
	@echo "Note: setcap not available on macOS. Run as root for raw socket access."
endif
endif


# Build standalone jsvim only when apps module is enabled
JSVIM_SRC = lib/apps/JSVIM/main.c \
            lib/apps/JSVIM/editor.c \
            lib/apps/JSVIM/buffer.c \
            lib/apps/JSVIM/render.c \
            lib/apps/JSVIM/highlight.c \
            lib/apps/JSVIM/lsp.c \
            lib/apps/JSVIM/language.c \
            lib/apps/JSVIM/util.c \
            lib/apps/JSVIM/semantic.c \
            lib/apps/JSVIM/cJSON.c

ifeq ($(APPS_ENABLED),yes)
bin/jsvim: $(JSVIM_SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) -I./lib/apps/JSVIM $(JSVIM_SRC) \
	  $(if $(filter Darwin,$(UNAME)),-lncurses,-lncursesw) -o bin/jsvim
endif


# Installation
ifeq ($(APPS_ENABLED),yes)
install: bin/jsvim
	sudo install -m 755 bin/jsvim /usr/local/bin/jsvim
else
install:
	@echo "No apps enabled. Nothing to install."
endif


clean:
	rm -f $(OBJ)