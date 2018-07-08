all:
	g++ emulator.cpp ops.cpp op_table.cpp state.cpp display.cpp debug.cpp audio.cpp -lSDL2 -lstdc++fs -o build/emulator -Wall -Wextra -Wpedantic -Wno-unused -std=c++17
