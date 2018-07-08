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
    bool save_pending = false;

    std::uint32_t ram_size = 0;
    std::uint16_t rom_bank = 0;
    std::uint16_t rom_banks = 0;
    std::uint8_t ram_bank = 0;
    std::uint8_t ram_banks = 0;
    bool ram_enabled = false;
    bool ram_bank_mode = false;

    uint8_t rtc_seconds = 0;
    uint8_t rtc_minutes = 0;
    uint8_t rtc_hours = 0;
    uint8_t rtc_days = 0;
    uint8_t rtc_flags = 0;
    uint8_t prev_rtc_latch = 0xff;

    std::uint8_t* tile_data = nullptr;
    std::uint8_t prev_oam_tile_ids[40]{0};
    std::uint8_t sorted_sprites[40]{0};

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

    void dump_memory_to_file(std::string filename,
		             std::string memory);
    bool load_file_to_memory(std::string filename,
		             std::string memory);
    bool load_file_to_rom(std::string filename);
    std::uint8_t read_memory(std::uint16_t addr);
    std::uint8_t read_mbc1(std::uint16_t addr);
    std::uint8_t read_mbc2(std::uint16_t addr);
    std::uint8_t read_mbc3(std::uint16_t addr);
    std::uint8_t read_mbc5(std::uint16_t addr);
    void write_memory(std::uint16_t addr, std::uint8_t value);
    void write_mbc1(std::uint16_t addr, std::uint8_t value);
    void write_mbc2(std::uint16_t addr, std::uint8_t value);
    void write_mbc3(std::uint16_t addr, std::uint8_t value);
    void write_mbc5(std::uint16_t addr, std::uint8_t value);
    void update_tile_data();
private:
    std::uint8_t* memory = nullptr;
    std::uint8_t* ram = nullptr;
    std::uint8_t* rom = nullptr;
};

