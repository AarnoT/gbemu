#pragma once

#include <string>

extern bool quit;

const int SCREEN_WIDTH = 256;
const int SCREEN_HEIGHT = 256;

int main(int argc, char* argv[]);
void load_rom(std::string filename, std::uint8_t* rom_buffer);
void handle_events();
