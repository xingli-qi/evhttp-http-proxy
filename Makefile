# Generic Makefile
# 03/17/2014
# xingqi@cisco.com

EXECUTABLE := http-proxy
LIBS := event
CFLAGS := -g -Wall -MD
CC = gcc

SOURCES := $(wildcard *.c)
OBJS := $(patsubst %.c, %.o, $(SOURCES))
DEPS := $(patsubst %.o, %.d, $(OBJS))

.PHONY : all clean deep clean rebuild 

all: $(EXECUTABLE)

-include $(DEPS)
$(EXECUTABLE): $(OBJS)
	$(CC) -o $@ $^ $(addprefix -l, $(LIBS))

clean:
	@rm -f *.o *.d

deepclean:
	@rm -f *.o *.d $(EXECUTABLE)

rebuild: deepclean all
