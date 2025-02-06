.PHONY: clean

NVCC := nvcc  # CUDA compiler
CC := gcc
MSG := @echo
HIDE := @

CFLAGS := -g -O3 -Werror -Wall -I/usr/include/python3.8 -I/usr/include/python3.8
LDLIBS := ${LDLIBS} -libverbs -lpthread -lpython3.8 -lcrypt -ldl -lutil

NVCCFLAGS := -I/usr/local/cuda/include -I/usr/include/python3.8
NVCCOBJS := -L/usr/local/cuda/lib64 -lcuda -lcudart -L/usr/lib/python3.8/config-3.8-x86_64-linux-gnu -lpython3.8

default: server #client

%.o: %.cu
	$(MSG) "    NVCC $<"
	$(HIDE) $(NVCC) -c $< $(NVCCFLAGS)

%.o: %.c
	$(MSG) "    CC $<"
	$(HIDE) $(CC) -c $< $(CFLAGS) ${INC}

server: server.o gpu.o 
	$(MSG) "    LD $^"
	$(HIDE) $(NVCC) $(NVCCOBJS) $(LDLIBS) $^ -o $@

#client: client.o gpu.o mtcp.o
#	$(MSG) "    LD $^"
#	$(HIDE) $(NVCC) $(NVCCOBJS) $(LDLIBS) $^ -o $@

clean:
	$(MSG) "    CLEAN server client"
	$(HIDE) rm -rf server client *.o *.d __pycache__

