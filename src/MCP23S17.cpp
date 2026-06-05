
#include "MCP23S17.h"
#include <SPI.h>
#include "SerialComm.h"

namespace {

TaskHandle_t mcp23s17IntTaskHandle = nullptr;


void mcp23s17IntTask(void *parameter)
{
    (void)parameter;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        digitalWrite(MCP23S17_CS, LOW);
        SPI.transfer(READ);
        SPI.transfer(INTCAPB);
        uint8_t capturedState = SPI.transfer(0x00);
        digitalWrite(MCP23S17_CS, HIGH);

        char message[64];
        snprintf(message, sizeof(message), "GPIOB interrupt, captured state: 0x%02X", capturedState);
        SerialComm::info(SerialComm::LogSource::SPI, message);
    }
}

} // namespace

void setup_Spi()
{
    // setup SPI
    pinMode(CLK, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(MISO, INPUT);
    pinMode(W25Q128JVS_CS, OUTPUT);
    pinMode(MCP23S17_CS, OUTPUT);
    //setup pull-up for MCP23S17 interrupt pin
    pinMode(MCP23S17_INT, INPUT_PULLUP);
    //setup interrupt for MCP23S17
    digitalWrite(W25Q128JVS_CS, HIGH);
    digitalWrite(MCP23S17_CS, HIGH);
    SPI.begin(CLK, MISO, MOSI, -1);

    // setup MCP23s17 as GPIOA as output
    digitalWrite(MCP23S17_CS, LOW);
    SPI.transfer(WRITE);       // Write command for MCP23S17
    SPI.transfer(IODIRA);      // IODIRA register address
    SPI.transfer(OUTPUT_MODE); // Set all pins of GPIOA as output
    digitalWrite(MCP23S17_CS, HIGH);
    delay(100);
    // setup MCP23s17 as GPIOB as input
    digitalWrite(MCP23S17_CS, LOW);
    SPI.transfer(WRITE);      // Write command for MCP23S17
    SPI.transfer(IODIRB);     // IODIRB register address
    SPI.transfer(0xFF);       // Set all pins of GPIOB as input

    digitalWrite(MCP23S17_CS, HIGH);
    delay(100);
    // optional pull-ups on GPIOB inputs
    digitalWrite(MCP23S17_CS, LOW);
    SPI.transfer(WRITE);
    SPI.transfer(GPPUB);
    SPI.transfer(0xFF);
    digitalWrite(MCP23S17_CS, HIGH);
    delay(100);
    // setup interrupt on GPIOB
    digitalWrite(MCP23S17_CS, LOW);
    SPI.transfer(WRITE);  // Write command for MCP23S17
    SPI.transfer(GPINTENB);
    SPI.transfer(0xFF);   // Enable interrupt on all pins of GPIOB
    digitalWrite(MCP23S17_CS, HIGH);
    delay(100);
    // setup interrupt control on GPIOB
    digitalWrite(MCP23S17_CS, LOW);
    SPI.transfer(WRITE);  // Write command for MCP23S17
    SPI.transfer(INTCONB);
    SPI.transfer(0x00);   // Interrupt on change from previous value
    digitalWrite(MCP23S17_CS, HIGH);

    xTaskCreate(
        mcp23s17IntTask,
        "MCP23S17IntTask",
        3072,
        nullptr,
        1,
        &mcp23s17IntTaskHandle);

    attachInterrupt(digitalPinToInterrupt(MCP23S17_INT), callbackMCP23S17Int, FALLING);
    
}

void testSpi()
{

    // Test SPI communication with MCP23S17
    // toggle relays on GPA0-7
    for (uint8_t i = 0; i < 8; i++)
    {
        digitalWrite(MCP23S17_CS, LOW);
        SPI.transfer(WRITE);  // Write command for MCP23S17
        SPI.transfer(GPIOA);  // GPIOA register address
        SPI.transfer(1 << i); // Set the corresponding bit to toggle the relay

        digitalWrite(MCP23S17_CS, HIGH);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    SerialComm::info(SerialComm::LogSource::SPI, "Relays toggled successfully on MCP23S17");
}

void IRAM_ATTR callbackMCP23S17Int()
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (mcp23s17IntTaskHandle != nullptr)
    {
        vTaskNotifyGiveFromISR(mcp23s17IntTaskHandle, &higherPriorityTaskWoken);
        if (higherPriorityTaskWoken == pdTRUE)
        {
            portYIELD_FROM_ISR();
        }
    }
}
