#include "device/Display.hpp"
#include "device/Memory.hpp"

DisplayDevice::DisplayDevice(Memory& mem) : mem_(mem) {}

void DisplayDevice::write(uint32_t addr, uint8_t* data, size_t size) {
    (void)mem_;
    (void)addr;
    (void)data;
    (void)size;
}

void DisplayDevice::read(uint32_t addr, uint8_t* data, size_t size) {
    (void)addr;
    if (size == 4) {
        word_to_bytes(0, data);
    }
}
