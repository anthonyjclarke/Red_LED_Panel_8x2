#pragma once
#include <cstdint>
// Host fixture for the Arduino-ESP32 2.x SPI HAL calls used by the driver.
struct spi_t;
#define VSPI 3
#define SPI_MODE0 0
#define SPI_MSBFIRST 1
uint32_t spiFrequencyToClockDiv(uint32_t freq);
spi_t* spiStartBus(uint8_t spi_num, uint32_t clockDiv, uint8_t dataMode, uint8_t bitOrder);
void spiStopBus(spi_t* spi);
void spiAttachSCK(spi_t* spi, int8_t sck);
void spiAttachMOSI(spi_t* spi, int8_t mosi);
void spiWriteNL(spi_t* spi, const void* data, uint32_t len);
