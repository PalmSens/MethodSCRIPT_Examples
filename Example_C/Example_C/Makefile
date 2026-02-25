SOURCES  = example.c
SOURCES += palmsens/mscript.c
SOURCES += palmsens/mscript_serial_port_linux.c

SRCS = $(SOURCES:%.c=src/%.c)
OBJS = $(SOURCES:%.c=build_linux/%.o)
DEPS = $(SOURCES:%.c=build_linux/%.d)

example: $(OBJS) Makefile
	gcc -o $@ $(OBJS)

build_linux/%.o: src/%.c build_linux/palmsens Makefile
	gcc -c -Wall -Wextra -Werror -MMD -o $@ $<

build_linux/palmsens:
	mkdir -p build_linux/palmsens

.PHONY: clean
clean:
	rm -rf build_linux example

-include $(DEPS)
