CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

# Use wildcard to find all .c files in src directory
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)

# Target to compile the program
vcode: $(OBJS)
	$(CC) $(CFLAGS) -o vcode $(OBJS)

# Target to clean up object files and the executable
clean:
	rm -f vcode $(OBJS)
