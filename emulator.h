#pragma once

#include "state.h"

#include <string>

extern bool quit;

const int SCREEN_WIDTH = 160;
const int SCREEN_HEIGHT = 144;

int main(int argc, char* argv[]);
void handle_events();
void handle_interrupts(State& state);
