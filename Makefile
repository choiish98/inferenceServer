.PHONY: clean

NVCC := nvcc  # CUDA compiler
CC := gcc
MSG := @echo
HIDE := @

CFLAGS := -g -O3 -Werror -Wall -I/usr/include/python3.8 -I/usr/include/python3.8
LDLIBS := ${LDLIBS} -lrdmacm -libverbs -lpthread -lpython3.8 -lcrypt -ldl -lutil -lm

NVCCFLAGS := -I/usr/local/cuda/include -I/usr/include/python3.8
NVCCOBJS := -L/usr/local/cuda/lib64 -lcuda -lcudart -L/usr/lib/python3.8/config-3.8-x86_64-linux-gnu -lpython3.8

default: main

%.o: %.cu
	$(MSG) "    NVCC $<"
	$(HIDE) $(NVCC) -c $< $(NVCCFLAGS) -o $@

%.o: %.c
	$(MSG) "    CC $<"
	$(HIDE) $(CC) -c $< $(CFLAGS) -o $@

main: main.o gpu_mem.o gpu_infer.o rdma.o
	$(MSG) "    LD $^"
	$(HIDE) $(NVCC) $(NVCCOBJS) $(LDLIBS) $^ -o $@

clean:
	$(MSG) "    CLEAN main"
	$(HIDE) rm -rf main *.o *.d __pycache__

