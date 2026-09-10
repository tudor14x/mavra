CC = gcc
CFLAGS = -static
LDFLAGS = -luserenv


all: x64 x86
x64: masm ml link ml64 link64
x86: masm86 ml86 link86

masm: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	@if not exist bin\x64 mkdir bin\x64 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=1 -I./src/ ./src/mavra.c $(CFLAGS) $(LDFLAGS) -o ./bin/x64/$@.exe

masm86: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	@if not exist bin\x86 mkdir bin\x86 2>nul
	$(CC) -DARCHITECTURE=86 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=1 -I./src/ ./src/mavra.c $(CFLAGS) $(LDFLAGS) -o ./bin/x86/masm.exe

ml: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	@if not exist bin\x86 mkdir bin\x86 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=86 -I./src/ ./src/mavra.c $(CFLAGS) $(LDFLAGS) -o ./bin/x64/$@.exe

ml86: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	@if not exist bin\x86 mkdir bin\x86 2>nul
	$(CC) -DARCHITECTURE=86 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=86 -I./src/ ./src/mavra.c $(CFLAGS) $(LDFLAGS) -o ./bin/x86/ml.exe

ml64: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	@if not exist bin\x64 mkdir bin\x64 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=64 -I./src/ ./src/mavra.c $(CFLAGS) $(LDFLAGS) -o ./bin/x64/$@.exe

link: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	@if not exist bin\x86 mkdir bin\x86 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=86 -DLINKER_MODE=1 -I./src/ ./src/mavra.c $(CFLAGS) $(LDFLAGS) -o ./bin/x64/$@.exe

link86: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	@if not exist bin\x86 mkdir bin\x86 2>nul
	$(CC) -DARCHITECTURE=86 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=86 -DLINKER_MODE=1 -I./src/ ./src/mavra.c $(CFLAGS) $(LDFLAGS) -o ./bin/x86/link.exe

link64: src/mavra.c src/mavra.h
	@if not exist bin mkdir bin 2>nul
	@if not exist bin\x64 mkdir bin\x64 2>nul
	$(CC) -DARCHITECTURE=64 -DMASM_ARCHITECTURE_AGNOSTIC_BUILD=0 -DTARGET_MASM_ARCHITECTURE=64 -DLINKER_MODE=1 -I./src/ ./src/mavra.c $(CFLAGS) $(LDFLAGS) -o ./bin/x64/$@.exe