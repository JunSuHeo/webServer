CC= gcc
APP=simple_webserver
CFLAGS = -D_REENTRANT
LDFLAGS = -lpthread
OBJS=simple_webserver.o

all: $(APP)
$(APP): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
clean:
	rm -f *~ $(OBJS) $(APP)
