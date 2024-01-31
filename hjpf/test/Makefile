CC=gcc
CFLAGS=-Wall -g 
TARGET=aaa
SRC=test.c
OBJS=$(patsubst %.c,%.o,$(SRC))
DEPEND=Makefile.depend
LIBS=../lib/libcommon.a -lrt -lm
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
