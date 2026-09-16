CC = gcc
CFLAGS = -I./src/ -static
LDFLAGS = -luserenv
SRC = ./src/mavra.c ./src/main.c ./src/string_utils.c

all: masm ml link ml64 link64

masm: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=1 $(SRC) $(CFLAGS) $(LDFLAGS) -o ./bin/$@.exe

ml: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=86 $(SRC) $(CFLAGS) $(LDFLAGS) -o ./bin/$@.exe

ml64: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=64 $(SRC) $(CFLAGS) $(LDFLAGS) -o ./bin/$@.exe

link: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=86 -DLINKER_MODE=1 $(SRC) $(CFLAGS) $(LDFLAGS) -o ./bin/$@.exe

link64: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=64 -DLINKER_MODE=1 $(SRC) $(CFLAGS) $(LDFLAGS) -o ./bin/$@.exe