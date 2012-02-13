CFLAGS+=-Wall -ggdb3 -Ideps/libasn -Ideps/libscgi -Ideps
SRCS=$(shell ls *.c)
OBJS=$(SRCS:.c=.o)
DEPS= $(addsuffix .depend, $(OBJS))

CC?=gcc

all: codewiki-scgi

codewiki-scgi: $(OBJS) codewiki-scgi.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+ deps/libscgi/libscgi.a deps/libasn/libasn.a

%.o: %.c
	@echo "Generating $@.depend"
	@$(CC) -MM $(CFLAGS) $< | \
	sed 's,^.*\.o[ :]*,$@ $@.depend : ,g' > $@.depend
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f codewiki-scgi $(OBJS) $(DEPS)

-include $(DEPS)

.PHONY: all clean
