#include "ALU.hpp"
uint32_t alu_execute(ALUOp op, uint32_t a, uint32_t b) {
    switch (op) {
        case ALUOp::ADD: return a + b;
        case ALUOp::SUB: return a - b;
        case ALUOp::AND: return a & b;
        case ALUOp::OR:  return a | b;
        case ALUOp::XOR: return a ^ b;
        case ALUOp::SLT:  return static_cast<uint32_t>(static_cast<int32_t>(a) < static_cast<int32_t>(b));
        case ALUOp::SLTU: return a < b;
        case ALUOp::SLL: return a << (b & 0x1F);
        case ALUOp::SRL: return a >> (b & 0x1F);
        //case ALUOp::SRA: return ((int32_t)a) >> (b & 0x1F);
        default: return 0;
    }
}