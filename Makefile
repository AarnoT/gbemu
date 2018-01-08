all:
	g++ emulator.cpp ops.cpp op_table.cpp state.cpp display.cpp -lSDL2 -o build/emulator -Wall -Wextra -Wpedantic -Wno-unused -std=c++11
