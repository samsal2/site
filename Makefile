.POSIX:
.SUFFIXES:

WARNFLAGS = -Wall -Wextra -Wshadow -pedantic -pedantic-errors
SANITIZEFLAGS = -fsanitize=address -fsanitize=undefined -fsanitize=bounds

PCFLAGS = $(WARNFLAGS) $(SANITIZEFLAGS) $(CFLAGS)
PLDFLAGS = $(SANITIZEFLAGS) $(LDFLAGS)

NAME = site
SRC = site.c
OBJ = $(SRC:.c=.o) 
DEP = $(SRC:.c=.d)

HTML = test.html
HEXDMP = $(HTML:.html=.inl)

all: $(NAME)

.PHONY: $(NAME)
-include $(DEP)
site: $(OBJ)
	$(CC) $(PCFLAGS) $(PLDFLAGS) $^ -o $@ 

$(OBJ): $(HEXDMP)

.SUFFIXES: .c .o
.c.o:
	$(CC) -MMD $(PCFLAGS) -o $@ -c $<

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
	rm -f $(NAME)

