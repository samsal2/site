.POSIX:

WCFLAGS = -Wall -Wextra -Wshadow -pedantic -pedantic-errors -fstrict-aliasing \
          -fsanitize=address -fsanitize=undefined -fsanitize=bounds 


WLDFLAGS = -fsanitize=address -fsanitize=undefined -fsanitize=bounds

SITECFLAGS = $(WCFLAGS) $(CFLAGS)
SITELDFLAGS = $(WLDFLAGS0 $(LDFLAGS)

SRC = site.c
OBJ = $(SRC:.c=.o) 
DEP = $(SRC:.c=.d)

HTML = test.html
HEXDMP = $(HTML:.html=.inl)

all: site

.PHONY: site
-include $(DEP)
site: $(OBJ)
	$(CC) $(SITECFLAGS) $(SITELDFLAGS) $^ -o $@ 

$(OBJ): $(HEXDMP)

.SUFFIXES: .c .o
.c.o:
	$(CC) -MMD $(SITECFLAGS) -o $@ -c $<

.PHONY: hexdumps
hexdumps: $(HEXDMP)

.SUFFIXES: .html .inl
.html.inl:
	xxd -i $< > $@

.PHONY: clean
clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(HEXDMP)
	rm -f site

