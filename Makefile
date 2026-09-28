CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS =

SRCS    = src/main.c src/graph.c src/dijkstra.c src/routing_table.c src/ospf.c src/cli.c
OBJS    = $(SRCS:.c=.o)
TARGET  = ospf-sim

.PHONY: all clean demo run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

demo: $(TARGET)
	./$(TARGET) --demo

run: $(TARGET)
	./$(TARGET)

# Non-interactive smoke test: load topology, demo path, exit
test: $(TARGET)
	@echo "=== Smoke test: load enterprise topology ==="
	@printf 'show topology\nshow path R1 R6\nshow ip route R1\nfail R1 R4\nshow path R1 R6\nrestore R1 R4\nshow path R1 R6\nquit\n' | ./$(TARGET) --load topologies/enterprise.txt
	@echo ""
	@echo "=== Demo mode ==="
	./$(TARGET) --demo | head -n 80
	@echo "... (truncated)"
	@echo "OK"

clean:
	rm -f $(OBJS) $(TARGET)
