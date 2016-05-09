srcs := $(wildcard ../src/*.c)
objs := $(patsubst ../src/%.c,%.o,$(srcs)) texture_loader.o
hdrs := $(wildcard ../src/*.h)

CFLAGS := -D_GNU_SOURCE -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -g -I../stb/ $(shell sdl2-config --cflags)
LDFLAGS := -lGL -lGLU -lSDL2 -lm -lSDL_net

all: ../obbg

../obbg: $(objs)
	g++ $(CFLAGS) $^ -o $@ $(LDFLAGS)

texture_loader.o: ../src/texture_loader.cpp $(hdrs)
	g++ $(CFLAGS) -std=c++11 -fpermissive -c $< -o $@

%.o: ../src/%.c $(hdrs)
	gcc $(CFLAGS) -std=c99 -c $< -o $@

clean:
	$(RM) $(objs) ../obbg
