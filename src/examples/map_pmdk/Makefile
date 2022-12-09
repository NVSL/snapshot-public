SOURCES := $(wildcard *.c)
LDLIBS := -lpmemobj

mapcli: $(SOURCES:%.c=%.o)
	$(PUDDLES_CC) $^ $(LDLIBS) -o $@ -ldl

.PHONY: clean
clean:
	-rm -rf *.o mapcli

%.o: %.c
	$(PUDDLES_CC) -c $^ -o $@ -ldl

include ../../common.make
