CC = cc
CFLAGS = -I./src/quickjs -Wall -O2 -DCONFIG_VERSION=\"2020-11-08\" -D_GNU_SOURCE -DJSSH_VERSION=\"0.5.7\"
LDFLAGS = -lm -ldl -lreadline -lncurses -lssh

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
ifneq ($(findstring network,$(MODULES)),)
SRC += lib/network/module.c \
       lib/network/net_utils.c 
CFLAGS += -DENABLE_NETWORK
NEED_SETCAP = yes
endif

ifneq ($(findstring compiler,$(MODULES)),)
SRC += lib/compiler/module.c \
       lib/compiler/cmp_utils.c 
CFLAGS += -DENABLE_COMPILER
endif

OBJ = $(SRC:.c=.o)

all: bin/jssh

bin/jssh: $(OBJ)
	@mkdir -p bin
	$(CC) -o $@ $(OBJ) $(LDFLAGS)
ifeq ($(NEED_SETCAP),yes)
	@echo "Setting cap_net_raw on $@"
	@sudo setcap cap_net_raw=ep $@
endif

clean:
	rm -f $(OBJ)