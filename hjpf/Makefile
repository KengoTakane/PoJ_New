CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -g								#通常
#CFLAGS=-Wall -g -DMEASURE					#周期計測用
#CFLAGS=-Wall -g -DMEASURE -DMEASURELAN		#周期計測用(+lantrans)
LDFLAGS=-lc
TARGET=hjpf common
MAKE=make
MAKE_DIRS=common hjpf

all: 
	@for subdir in $(MAKE_DIRS) ; do \
		(cd $$subdir && $(MAKE) CC='$(CC)' CFLAGS='$(CFLAGS)') ;\
	done

clean:
	@for subdir in $(MAKE_DIRS) ; do \
		(cd $$subdir && $(MAKE) clean) ;\
	done

depend:
	@for subdir in $(MAKE_DIRS) ; do \
		(cd $$subdir && $(MAKE) depend) ;\
	done
