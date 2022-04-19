RMF = rm -f

CFLAGS = -std=c89
# CFLAGS += -Ofast
CFLAGS += -O0
CFLAGS += -g
CFLAGS += -fstrict-aliasing
CFLAGS += -fsanitize=address
CFLAGS += -fsanitize=undefined
CFLAGS += -fsanitize=bounds
# CFLAGS += -flto
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wextra
CFLAGS += -Wshadow
CFLAGS += -Wvla
CFLAGS += -Walloca
CFLAGS += -Wstrict-prototypes
CFLAGS += -pedantic
CFLAGS += -pedantic-errors
# CFLAGS += -DNDEBUG

# LDFLAGS +=-flto
LDFLAGS +=-fsanitize=address
LDFLAGS +=-fsanitize=undefined
LDFLAGS +=-fsanitize=bounds

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

all: site

.PHONY: site
-include $(DEPS)

site: $(OBJS)
	$(CC) $(LDFLAGS) -o site $^

%.o: %.c
	$(CC) -MMD $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	$(RMF) $(OBJS)
	$(RMF) $(DEPS)
	$(RMF) site

