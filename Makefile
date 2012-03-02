CFLAGS+=-Wall -ggdb3 -DDEBUG
LDFLAGS+=-lcrypt
SRCS=$(shell ls *.c)
OBJS=file.o tags.o codewiki.o
DEPS= $(addsuffix .depend, $(OBJS))

CC?=gcc

all: codewiki-fcgi codewiki-test

codewiki-test: $(OBJS) codewiki-test.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

codewiki-scgi: $(OBJS) codewiki-scgi.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+ deps/libscgi/libscgi.a deps/libasn/libasn.a

codewiki-fcgi: $(OBJS) codewiki-fcgi.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+ -lfcgi
%.o: %.c
	@echo "Generating $@.depend"
	@$(CC) -MM $(CFLAGS) $< | \
	sed 's,^.*\.o[ :]*,$@ $@.depend : ,g' > $@.depend
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f codewiki-scgi $(OBJS) $(DEPS)

-include $(DEPS)

.PHONY: all clean
