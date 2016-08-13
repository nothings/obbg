srcs := $(wildcard ../src/*.c)
objs := $(patsubst ../src/%.c,%.o,$(srcs)) texture_loader.o
hdrs := $(wildcard ../src/*.h)

FLAGS    := -D_GNU_SOURCE -g -I../stb/ $(shell sdl2-config --cflags)
CFLAGS   = $(FLAGS) -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -std=c99
CXXFLAGS = $(FLAGS) -std=c++11 -fpermissive
LDFLAGS  := -lGL -lGLU -lSDL2 -lm -lSDL_net

all: ../obbg

32bit: FLAGS += -m32
32bit: all

../obbg: $(objs)
	g++ $(CFLAGS) $^ -o $@ $(LDFLAGS)

texture_loader.o: ../src/texture_loader.cpp $(hdrs)
	g++ $(CXXFLAGS) -c $< -o $@

%.o: ../src/%.c $(hdrs)
	gcc $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(objs) ../obbg
