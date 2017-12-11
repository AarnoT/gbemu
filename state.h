#include <cstdint>

class State {
public:
    std::uint8_t a = 0, b = 0, c = 0, d = 0,
            	 e = 0, h = 0, l = 0, f = 0;
    std::uint16_t sp = 0, pc = 0;
    std::uint32_t instructions_executed = 0;
    bool interrupts_enabled = false;
    State(); 
    ~State();
    std::uint8_t read_memory(std::uint16_t addr, std::uint8_t value);
    void write_memory(std::uint16_t addr, std::uint8_t value);
    State(const State& state) = delete;
    State& operator=(const State& state) = delete;
private:
    std::uint8_t* memory;
};

