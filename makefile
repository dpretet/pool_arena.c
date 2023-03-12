# distributed under the mit license
# https://opensource.org/licenses/mit-license.php

# http://nuclear.mutantstargoat.com/articles/make/

src = $(wildcard src/*.c test/*.c)
obj = $(src:.c=.o)

CFLAGS = -Wall -Wextra -pedantic -I ./src
LDFLAGS =

testsuite: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) testsuite
