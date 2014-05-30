CC :=gcc
LDLIBS = `sdl-config --cflags --libs`
SOURCE :=main.c \
		 6502.c \
		 PPU.c \
		 APU.c \
		 mappers.c \
		 font.c
EXECUTE :=main

all:
	$(CC) -o $(EXECUTE) $(SOURCE) $(LDLIBS)

clean:
	rm -r $(EXECUTE)