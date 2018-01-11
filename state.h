#pragma once

#include <utility>
#include <cstdint>
#include <map>
#include <string>

class State {
public:
    std::uint8_t a = 0, b = 0, c = 0, d = 0,
            	 e = 0, h = 0, l = 0, f = 0;

    std::uint16_t sp = 0xfffe, pc = 0x100;
    std::uint32_t instructions_executed = 0;
    std::uint32_t stack_depth = 0;
    bool interrupts_enabled = false;
    bool halt_mode = false;
    bool stop_mode = false;

    std::map<std::string, std::uint8_t*> registers {
        {"A", &this->a},
        {"B", &this->b},
        {"C", &this->c},
        {"D", &this->d},
        {"E", &this->e},
        {"H", &this->h},
        {"L", &this->l},
        {"F", &this->f}
    };
    std::map<std::string, std::pair<std::uint8_t*, std::uint8_t*>> register_pairs {
        {"BC", {&this->b, &this->c}},
        {"DE", {&this->d, &this->e}},
        {"HL", {&this->h, &this->l}},
        {"AF", {&this->a, &this->f}}
    };

    State(); 
    ~State();
    State(const State& state) = delete;
    State& operator=(const State& state) = delete;

    void dump_memory_to_file(std::string filename);
    void load_file_to_memory(std::string filename);
    std::uint8_t read_memory(std::uint16_t addr);
    void write_memory(std::uint16_t addr, std::uint8_t value);
private:
    std::uint8_t* memory;
};

