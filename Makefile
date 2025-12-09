CC = cc
CFLAGS = -I./src/quickjs -Wall -O2 -DCONFIG_VERSION=\"2020-11-08\" -D_GNU_SOURCE -DJSSH_VERSION=\"0.6.1\" -DJSVIM_VERSION=\"0.1.2\"
LDFLAGS = -lm -ldl -lreadline -lncurses -lssh -lgit2

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
CFLAGS += -DENABLE_NETWORK
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
CFLAGS += -DENABLE_GIT
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
CFLAGS += -DENABLE_NETWORK -DENABLE_COMPILER -DENABLE_FS -DENABLE_GIT -DENABLE_APPS
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
	@echo "Setting cap_net_raw on $@"
	@sudo setcap cap_net_raw=ep $@
endif


# Build standalone jsvim only when apps module is enabled
ifeq ($(APPS_ENABLED),yes)
bin/jsvim: lib/apps/jsvim.c
	@mkdir -p bin
	$(CC) $(CFLAGS) lib/apps/jsvim.c -lncurses -o bin/jsvim
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