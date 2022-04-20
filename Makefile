RMF = rm -f

CC = clang

XXD = xxd

CFLAGS = -std=c89
CFLAGS += -Ofast
# CFLAGS += -O0
# CFLAGS += -g
# CFLAGS += -fstrict-aliasing
# CFLAGS += -fsanitize=address
# CFLAGS += -fsanitize=undefined
# CFLAGS += -fsanitize=bounds
CFLAGS += -flto
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

LDFLAGS +=-flto
# LDFLAGS +=-fsanitize=address
# LDFLAGS +=-fsanitize=undefined
# LDFLAGS +=-fsanitize=bounds

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

HTMLS = $(wildcard *.html)
HEXDMPS = $(HTMLS:.html=.inl)

all: site

.PHONY: site
-include $(DEPS)

site: $(OBJS)
	$(CC) $(LDFLAGS) -o site $^

$(OBJS): $(HEXDMPS)

%.o: %.c
	$(CC) -MMD $(CFLAGS) -o $@ -c $<

%.inl: %.html
	$(XXD) -i $< > $@

.PHONY: clean
clean:
	$(RMF) $(OBJS)
	$(RMF) $(DEPS)
	$(RMF) $(HEXDMPS)
	$(RMF) site

