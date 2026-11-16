CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic

banker: banker.c
	$(CC) $(CFLAGS) banker.c -o banker

.PHONY: clean
clean:
	rm -f banker