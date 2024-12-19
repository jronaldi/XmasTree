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

#define XTREE_LEVELS        8     // How many levels on the tree?
#define XTREE_COLOR_GPIOS   3     // The number of GPIOs for colors
#define MAX_LABELID         99    // Maximum ID of labels (1..MAX_LABELID)

static const char *TAG = TAG_NAME;

enum LightShowCmdType {
    Light,
    Label,
    Loop,
    Wait,
    Stop,
    Call,
    Return
};

const char LabelMarker = ':';
extern const char* LoopMarker;
extern const char* WaitMarker;

struct LightShowCommand {
    enum LightShowCmdType stepType;
    union {
        struct {
            unsigned lightRows;     // 8-bits on/off state for each of the 8 levels lsb=lowest level)  
            unsigned red;           // Luminosity (0-100%) of the red light.
            unsigned green;         // Luminosity (0-100%) of the green light.
            unsigned blue;          // Luminosity (0-100%) of the blue light.
            unsigned delayMs;       // Delay before moving to the next step.
        } LightStep;
        struct {
            unsigned idLabel;
        } Label;
        struct {
            unsigned idLabel;       // ID of label where to loop to. Start at 0.
            unsigned count;         // Number of times to loop.
        } Loop;
        struct {
            unsigned delayMs;
        } Wait;
    };
};

// Fetch the customizable light-engine commands for the Xmas Tree driver
#ifdef __cplusplus
extern "C" {
#endif

    void SetErrorLed(bool on);

    const char *GetBinaryLights(unsigned lights, char buf[8]);

    esp_err_t LoadCommands(void** lightshow, int* pgmLength);
    esp_err_t RunLightshow(void* lightshow, int pgmLength);
    
#ifdef __cplusplus
};
#endif
