CC=gcc
CFLAGS=-Wall -g
TARGET=libcommon.a
SRC=com_timer.c com_shmem.c com_fs.c debug.c
OBJS=$(patsubst %.c,%.o,$(SRC))
DEPEND=Makefile.depend
INCLUDE=-I/usr/include/glib-2.0 -I/usr/lib/aarch64-linux-gnu/glib-2.0/include/ -I../include
TARGETDIR=../lib

all: $(TARGET)

$(TARGET): $(OBJS)
	ar r $(TARGET) $(OBJS)
	@if [ ! -d $(TARGETDIR) ]; then \
		mkdir $(TARGETDIR); \
	fi
	cp -p $(TARGET) $(TARGETDIR)/.

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

depend:
	@$(CC) -MM $(CFLAGS) $(INCLUDE) $(SRC) > $(DEPEND)

clean:
	rm -f $(TARGET) $(OBJS) $(TARGETDIR)/$(TARGET)

-include $(DEPEND)
