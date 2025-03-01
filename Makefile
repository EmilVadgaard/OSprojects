CC     = gcc
SRCS   = test1.c test2.c test3.c emptyMsgBox.c incorrectMessageLength.c invalidMessageLength.c nullBufferGet.c nullBufferPut.c

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