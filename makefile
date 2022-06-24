LIBSO = my_args my_log my_daemon sock_inet http_msg
LIBA =
OBJ = server.o
EXEC = server
CXXFLAGS = -I ./include $(shell mysql_config --cflags) -g
LDLIBS = -Wl,-Bstatic $(addprefix -l, $(LIBA)) \
	-Wl,-Bdynamic $(addprefix -l, $(LIBSO)) \
	$(shell mysql_config --libs)
LDFLAGS = -L ./lib -Wl,-rpath=$(CURDIR)/lib
CC = g++

.PHONY : all clean

all : $(EXEC) $(OBJ)

server.o : $(patsubst %, ./include/%.h, $(LIBSO))

clean :
	$(RM) $(OBJ) $(EXEC)
