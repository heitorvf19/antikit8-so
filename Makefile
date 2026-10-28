CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS = -Iinclude -Ihardware
LDLIBS = -lm
OBJ = src/main.o src/memory.o src/replacement.o src/audit.o src/parser.o hardware/hardware_specs.o
.PHONY: all clean test sanitize results
all: antikit8
antikit8: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDLIBS) -o $@
$(OBJ): include/simulator.h hardware/hardware_specs.h
hardware/hardware_specs.o: hardware/hardware_specs.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Drand=antikit_observed_rand -c $< -o $@
test: antikit8
	python3 tests/test_simulator.py
results: antikit8
	python3 tests/run_experiments.py
sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie' test
clean:
	rm -f $(OBJ) antikit8
