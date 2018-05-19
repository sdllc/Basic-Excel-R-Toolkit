
CC=g++
SRC=src/controlr2.cc
LIBUV=../../libuv
PROTOBUF=../../protobuf-3.5.1
PB=../PB
COMMON=../Common
RINCLUDES=$(shell R_HOME=; R CMD config --cppflags)
RLIBS=$(shell R_HOME=; R CMD config --ldflags)
INCLUDE=-Iinclude -I$(COMMON) -I$(LIBUV)/include -I$(PB) -I$(PROTOBUF)/src $(RINCLUDES)
MODULES= rinterface_common rinterface_linux \
	variable.pb message_utilities \
	console_graphics_device controlr2

OBJ_FILES=$(MODULES:%=build/%.o)

all: build/controlr2

build/%.o: src/%.cc
	$(CC) -o $@ -MD -MP $(INCLUDE) -c $<

build/%.o: ../Common/%.cc
	$(CC) -o $@ -MD -MP $(INCLUDE) -c $<

build/%.o: ../PB/%.cc
	$(CC) -o $@ -MD -MP $(INCLUDE) -c $<

build/controlr2: $(OBJ_FILES)
	$(CC) -o $@ $^ \
		$(PROTOBUF)/src/.libs/libprotobuf.a \
		$(LIBUV)/.libs/libuv.a \
		$(RLIBS) \
		-lpthread 

ALL_DEPS := $(patsubst %.o,%.d,$(MAIN_OBS) $(LIB_OBS))
-include $(ALL_DEPS)

clean:
	rm -f build/*

