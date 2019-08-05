TARGET = ./bin/lmsmonitor
# LIBS = -lasound -lpthread -L./lib -lwiringPi_static -lArduiPi_OLED_static
LIBS = -lasound -lpthread -L./lib -lwiringPi -lArduiPi_OLED
CC = g++
CFLAGS = -g -Wall -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s -I.

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
