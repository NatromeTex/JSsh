CC = cc
CFLAGS = -I./src/quickjs -Wall -O2 -DCONFIG_VERSION=\"2020-11-08\" -D_GNU_SOURCE
LDFLAGS = -lm -ldl -lreadline -lncurses
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
OBJ = $(SRC:.c=.o)
JSFILES := $(wildcard js/lib/*.js)
APP_SRC := $(wildcard lib/apps/*/entry.c)
APP_SO  := $(APP_SRC:.c=.so)
all: bin/jssh $(APP_SO)
bin/jssh: $(OBJ) $(JSFILES)
	@mkdir -p bin
	$(CC) -o $@ $(OBJ) $(LDFLAGS)
lib/apps/%/entry.so: lib/apps/%/entry.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $< $(LDFLAGS)
clean:
	rm -f $(OBJ) $(APP_SO)