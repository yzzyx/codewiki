CFLAGS+=-Wall -ggdb3 -DDEBUG
LDFLAGS+=-lcrypt
SRCS=$(shell ls *.c)
OBJS=file.o tags.o codewiki.o mime.o
DEPS= $(addsuffix .depend, $(OBJS))

CC?=gcc

.Make.config:
	@echo Running configure
	./configure

all: codewiki-fcgi codewiki-test .Make.config

include .Make.config

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
	rm -f codewiki-scgi $(OBJS) $(DEPS) .Make.config

-include $(DEPS)

.PHONY: all clean
