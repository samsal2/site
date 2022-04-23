RMF = rm -f
XXD = xxd

SRCDIR  = site
OUT = site.out

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

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:.c=.o) 
DEPS = $(SRCS:.c=.d)

HTMLS = $(wildcard $(SRCDIR)/*.html)
HEXDMPS = $(HTMLS:.html=.inl)

all: $(OUT)

.PHONY: $(OUT)
-include $(DEPS)

$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ 

$(OBJS): $(HEXDMPS)

%.o: %.c
	$(CC) -MMD $(CFLAGS) -o $@ -c $<

.PHONY: hexdumps

hexdumps: $(HEXDMPS)

%.inl: %.html
	$(XXD) -i $< > $@

.PHONY: clean
clean:
	$(RMF) $(OBJS)
	$(RMF) $(DEPS)
	$(RMF) $(HEXDMPS)
	$(RMF) $(OUT)

