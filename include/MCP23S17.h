#ifndef MCP23S17_H
#define MCP23S17_H

#include <Arduino.h>
#include "config.h"

enum MCP23S17Register
{
    IODIRA = 0x00,
    IODIRB = 0x01,
    GPINTENA = 0x04,
    GPINTENB = 0x05,
    INTCONA = 0x08,
    INTCONB = 0x09,
    IOCON = 0x0A,
    GPPUA = 0x0C,
    GPPUB = 0x0D,
    INTCAPA = 0x10,
    INTCAPB = 0x11,
    INTFA = 0x0E,
    INTFB = 0x0F,
    GPIOA = 0x12,
    GPIOB = 0x13
};

enum MCP23S17Command
{
    WRITE = 0x40,
    READ = 0x41
};

enum MCP23S17Pin
{
    GPA0 = 0,
    GPA1 = 1,
    GPA2 = 2,
    GPA3 = 3,
    GPA4 = 4,
    GPA5 = 5,
    GPA6 = 6,
    GPA7 = 7,

    GPB0 = 8,
    GPB1 = 9,
    GPB2 = 10,
    GPB3 = 11,
    GPB4 = 12,
    GPB5 = 13,
    GPB6 = 14,
    GPB7 = 15
};

enum MCP23S17Mode
{
    INPUT_MODE = 1,
    OUTPUT_MODE = 0
};

void setup_Spi();
void testSpi();
void readMCP23S17GPIOB();
void callbackMCP23S17Int();


#endif