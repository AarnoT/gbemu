all:
	g++ emulator.cpp ops.cpp op_table.cpp state.cpp -o build/emulator -Wall -Wextra -Wpedantic -std=c++11
