#pragma once

#include "state.h"

#include <string>

extern bool quit;

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 576;

int main(int argc, char* argv[]);
void handle_events(State& state);
void handle_interrupts(State& state);
