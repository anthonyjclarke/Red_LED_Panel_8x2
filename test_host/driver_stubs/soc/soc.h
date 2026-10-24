#pragma once
#include <cstdint>
void fixtureWrite(int reg, uint32_t mask);
#define REG_WRITE(reg, mask) fixtureWrite(reg, mask)
