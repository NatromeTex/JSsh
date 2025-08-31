CC = cc
CFLAGS = -I./src/quickjs -Wall -O2 -DCONFIG_VERSION=\"2020-11-08\" -D_GNU_SOURCE
LDFLAGS = -lm -ldl -lreadline -lncurses

SRC = src/main.c \
      src/utils.c \
      src/func.c \
      src/sys.c \
      src/quickjs/quickjs.c \
      src/quickjs/quickjs-libc.c \
      src/quickjs/cutils.c \
      src/quickjs/libregexp.c \
      src/quickjs/libunicode.c \
      src/quickjs/dtoa.c

OBJ = $(SRC:.c=.o)
JSFILES := $(wildcard js/lib/*.js)

bin/jssh: $(OBJ) $(JSFILES)
	@mkdir -p bin
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ) bin/jssh
