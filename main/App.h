// XmasTree App
#pragma once

#define RED_LED_PIN     GPIO_NUM_16 

#define TAG_NAME "XmasTree"

// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO  CONFIG_EXAMPLE_PIN_MISO
#define PIN_NUM_MOSI  CONFIG_EXAMPLE_PIN_MOSI
#define PIN_NUM_CLK   CONFIG_EXAMPLE_PIN_CLK
#define PIN_NUM_CS    CONFIG_EXAMPLE_PIN_CS

static const char *TAG = TAG_NAME;

struct sequenceCommand {
    unsigned lightRows;     // 8-bits on/off state for each of the 8 levels lsb=lowest level)  
    unsigned red;           // Luminosity (0-100%) of the red light.
    unsigned green;         // Luminosity (0-100%) of the green light.
    unsigned blue;          // Luminosity (0-100%) of the blue light.
    unsigned delai;         // The delay wait time before executing the next command.
};

// Fetch the customizable light-engine commands for the Xmas Tree driver
#ifdef __cplusplus
extern "C" {
#endif

    void SetErrorLed(bool on);

    esp_err_t LoadCommands();
    
#ifdef __cplusplus
};
#endif
