.POSIX:

INCS    = 

DEFS    = 

CFLAGS  = -std=c90                       \
          -O0                            \
          -g                             \
          -Wall                          \
          -Wextra                        \
          -Wshadow                       \
          -Wvla                          \
          -Walloca                       \
          -pedantic                      \
          -pedantic-errors               \
          -fstrict-aliasing              \
          -fsanitize=address             \
          -fsanitize=undefined           \
          -fsanitize=bounds              \
          $(INCS)                        \
          $(DEFS)

LIBS    = 

LDFLAGS = -fsanitize=address                    \
          -fsanitize=undefined                  \
          -fsanitize=bounds                     \
          $(LIBS)

SRCS = site.c
OBJS = $(SRCS:.c=.o) 
DEPS = $(SRCS:.c=.d)

HTMLS = test.html
HEXDMPS = $(HTMLS:.html=.inl)

all: site

-include $(DEPS)

.PHONY: site
site: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ 

$(OBJS): $(HEXDMPS)

.SUFFIXES: .c .o
.c.o:
	$(CC) -MMD $(CFLAGS) -o $@ -c $<

.PHONY: hexdumps
hexdumps: $(HEXDMPS)

.SUFFIXES: .html .inl
.html.inl:
	xxd -i $< > $@

.PHONY: clean
clean:
	rm -f $(OBJS)
	rm -f $(DEPS)
	rm -f $(HEXDMPS)
	rm -f site

