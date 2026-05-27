#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include "device/Device.hpp"

class Memory;

class DisplayDevice : public Device {
public:
    explicit DisplayDevice(Memory& mem);

    void write(uint32_t addr, uint8_t* data, size_t size) override;
    void read(uint32_t addr, uint8_t* data, size_t size) override;

private:
    Memory& mem_;
};

#endif
