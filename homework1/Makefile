CC = gcc
CFLAGS = -Wall -I../include
SRCDIR = .
INCDIR = ../include
OBJDIR = ./bin

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

EXECUTABLE = tarsau

.PHONY: all clean

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ -I$(INCDIR)

clean:
	rm -rf $(OBJDIR)/*.o $(EXECUTABLE)

