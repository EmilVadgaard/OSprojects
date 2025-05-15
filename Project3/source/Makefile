GCC = gcc
SOURCES = dm510fs.c
OBJS := $(patsubst %.c,%.o,$(SOURCES))
CFLAGS = -O2 -Wall -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=25

.PHONY: dm510fs

##
# Libs 
##
LIBS := fuse 
LIBS := $(addprefix -l,$(LIBS))

all: dm510fs test_suite test_suite_destroy

%.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $<

dm510fs: $(OBJS)
	$(GCC) $(OBJS) $(LIBS) $(CFLAGS) -o dm510fs

test_suite: test_suite.c
	$(GCC) $(CFLAGS) -o $@ $^

test_suite_destroy: test_suite_destroy.c
	$(GCC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJS) lfs
