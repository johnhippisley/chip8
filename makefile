CC=gcc
CFLAGS=-g -lSDL2
FILES=*.c
EXEC=chip8

chip8: $(FILES)
	$(CC) $(FILES) $(CFLAGS) -o $(EXEC)
