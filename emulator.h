#pragma once

#include <string>

extern bool quit;

int main(int argc, char* argv[]);
void load_rom(std::string filename, std::uint8_t* rom_buffer);
