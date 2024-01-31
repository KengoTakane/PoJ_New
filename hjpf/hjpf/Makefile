CC=gcc
CFLAGS=-Wall -g 
TARGET=hjpf
SRC=hjpf.c process.c resource.c gnss.c ins.c imu.c altmt.c i2c.c bme680.c ping.c camera.c failsafe.c mavlink.c
OBJS=$(patsubst %.c,%.o,$(SRC))
DEPEND=Makefile.depend
LIBS=../lib/libcommon.a -lglib-2.0 -lpthread -lrt -lm
INCLUDE=-I/usr/include/glib-2.0 -I/usr/lib/aarch64-linux-gnu/glib-2.0/include/ -I../include

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
