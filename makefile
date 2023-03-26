# distributed under the mit license
# https://opensource.org/licenses/mit-license.php

# http://nuclear.mutantstargoat.com/articles/make/

src = $(wildcard src/*.c test/*.c)
obj = $(src:.c=.o)

CFLAGS = -Wall -Wextra -pedantic -I ./src -DPOOL_ARENA_DEBUG=1 -fsanitize=address -fsanitize=undefined

test/testsuite: $(obj)
	$(CC) $(CFLAGS) $(obj) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(obj) testsuite
