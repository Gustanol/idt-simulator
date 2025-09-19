CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -g
SRCDIR=src
INCDIR=include
BUILDDIR=build
TARGET=idt-simulator

SOURCES=$(wildcard $(SRCDIR)/*.c)
OBJECTS=$(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -rf $(BUILDDIR) $(TARGET)

test: $(TARGET)
	./$(TARGET)
