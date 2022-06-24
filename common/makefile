LIB_NAME = my_str my_args my_log my_daemon readconfig sock_inet http_msg
LIBS = $(patsubst %, ../lib/lib%.so, $(LIB_NAME)) \
	$(patsubst %, ../lib/lib%.a, $(LIB_NAME))
OBJ_NAME = my_str my_args my_log my_daemon readconfig sock_inet http_msg
OBJS = $(patsubst %, %.o, $(OBJ_NAME))

CFLAGS = -g -fPIC -I ../include
CXXFLAGS = -g  -fPIC -I ../include
VPATH = ../include
LDFLAGS =  -L ../lib -Wl,-rpath=$(CURDIR)/../lib
.PHONY : all clean

all: $(OBJS) $(LIBS)

../lib/libmy_args.so : my_args.o my_args.h
	$(CC) $< -o $@ -shared $(LDFLAGS) -lmy_str

../lib/libsock_inet.so : sock_inet.o sock_inet.h
	$(CXX) $< -o $@ -shared $(LDFLAGS) -lmy_args

../lib/lib%.so : %.o %.h
	$(CC) $< -o $@ -shared

../lib/lib%.a : %.o %.h
	$(AR) r $@ $<

clean :
	$(RM) $(OBJS)
