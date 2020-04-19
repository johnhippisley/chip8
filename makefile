CC=gcc
CFLAGS=-g -lSDL2
FILES=*.c
EXEC=chip8

raycaster: $(FILES)
	$(CC) $(FILES) $(CFLAGS) -o $(EXEC)
