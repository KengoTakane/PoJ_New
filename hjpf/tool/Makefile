CC=gcc
CFLAGS=-Wall -g 
TARGET=mem_read
SRC=mem_read.c
OBJS=$(patsubst %.c,%.o,$(SRC))
DEPEND=Makefile.depend
LIBS=../lib/libcommon.a -lglib-2.0 -lpthread -lrt -lm
INCLUDE=-I../include

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $(OBJS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

depend:
	@$(CC) -MM $(CFLAGS) $(INCLUDE) $(SRC) > $(DEPEND)

clean:
	-$(RM) $(TARGET) $(OBJS)

-include $(DEPEND)
