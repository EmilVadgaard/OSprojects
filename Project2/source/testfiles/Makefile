CC     = gcc
SRCS   = moduletest.c buffersize.c maxReaders.c full_buffer.c read_empty.c 

# Create a list of programs by substituting .c with (no extension)
# e.g. file1.c => file1
PROGS  = $(SRCS:.c=)

# Default target: build all programs
all: $(PROGS)

# Pattern rule:
# For each program in PROGS, build from its corresponding .c file
%: %.c
	$(CC) -o $@ $<

# "clean" target to remove generated executables
clean:
	rm -f $(PROGS)