CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O2
CPPFLAGS = -Iinclude

BIN     = micru
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/obj/%.o)

.PHONY: all test exemplos clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

build/obj/%.o: src/%.c $(wildcard include/*.h) | build/obj
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/obj:
	mkdir -p $@

test: $(BIN)
	sh testes/executar_testes.sh

exemplos: $(BIN)
	@mkdir -p build/exemplos
	@for f in Regras_PROFESSOR/*.asm; do \
		printf '%-12s ' "$$(basename $$f .asm)"; \
		./$(BIN) "$$f" "build/exemplos/$$(basename $$f .asm)"; \
	done

clean:
	rm -rf build $(BIN) $(BIN).exe
