all:
	g++ -lncurses -o chip-gr8 src/main.cpp

clean:
	rm chip-gr8
