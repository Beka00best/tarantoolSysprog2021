CC	=  cc
CFLAGS	=  -I .
CFLAGS	+= -W -Wall -Wextra -Werror -Wfloat-equal
CFLAGS	+= -Wundef -Wpointer-arith -Wcast-align -Wshadow
CFLAGS	+= -Wwrite-strings
CFLAGS	+= -Wswitch-enum -Wunreachable-code -Winit-self
CFLAGS	+= -Wno-unused-parameter -pedantic -O3
LDFLAGS	=

BASE_SOURCES = shell.c parser.c 
SOURCES		= $(BASE_SOURCES)
OBJS		= $(SOURCES:.c=.o)
EXECUTABLE	= task

all: test

build: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

test: build
	python3 checker.py -e ./$(EXECUTABLE)

clean:
	rm -rf $(EXECUTABLE) $(OBJS)

.PHONY: clean