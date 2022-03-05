CFLAGS := -g -O0
LDLIBS := -pthread -lpmemobj
LDFLAGS := -g -O0
TARGET := hoh
SOURCES := hoh.c

all: $(TARGET)
$(TARGET): $(SOURCES:.c=.o)
clean:
	rm -f *.o $(TARGET)

