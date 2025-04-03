CC = gcc
CFLAGS = -Wall -pedantic -Wextra -fPIC -g
OUTPUT_DIR = ./output
CLIBS =

all: 1.out 2.out 3.out 5.out 7.out find.out

%.out: %.c
	$(CC) $(CFLAGS) $(CLIBS) $< -o $(OUTPUT_DIR)/$@

clean:
	rm -f $(OUTPUT_DIR)/*.out