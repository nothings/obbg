srcs := $(wildcard ../src/*.c)
objs := $(patsubst ../src/%.c,%.o,$(srcs)) texture_loader.o
hdrs := $(wildcard ../src/*.h)

all: ../obbg

../obbg: $(objs)
	g++ -g -m32 $^ -o $@ -lGL -lSDL2 -lX11 -lm -lGLU -lSDL_net

texture_loader.o: ../src/texture_loader.cpp $(hdrs)
	g++ -g -std=c++11 -m32 -I../stb/ -I/usr/include/SDL2/ -c $< -o $@

%.o: ../src/%.c $(hdrs)
	gcc -g -D_GNU_SOURCE -std=c99 -m32 -I../stb/ -I/usr/include/SDL2/ -c $< -o $@

clean:
	$(RM) $(objs) ../obbg
