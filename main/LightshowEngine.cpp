#define LOCAL_TRACE  // NO_LOCAL_TRACE / LOCAL_TRACE

#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <esp_chip_info.h>
#include <freertos/freertos.h>
#include <freertos/task.h>
#include <freertos/projdefs.h>
#include "led_indicator.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
//#include <regex>
#include "App.h"
#include "SdCard.h"

using namespace std;

// Other GPIOs used by the external xSD card reader: 19, 23, 18, 5.
#define XTREE_GPIO_LOADREG      (gpio_num_t)CONFIG_XTREE_GPIO_LOADREG //27
#define XTREE_GPIO_RED          (gpio_num_t)CONFIG_XTREE_GPIO_RED //17
#define XTREE_GPIO_GREEN        (gpio_num_t)CONFIG_XTREE_GPIO_GREEN //21
#define XTREE_GPIO_BLUE         (gpio_num_t)CONFIG_XTREE_GPIO_BLUE //14
#define XTREE_GPIO_LEVEL1       (gpio_num_t)CONFIG_XTREE_GPIO_LEVEL1 //26
#define XTREE_GPIO_LEVEL2       (gpio_num_t)CONFIG_XTREE_GPIO_LEVEL2 //25
#define XTREE_GPIO_LEVEL3       (gpio_num_t)CONFIG_XTREE_GPIO_LEVEL3 //33
#define XTREE_GPIO_LEVEL4       (gpio_num_t)CONFIG_XTREE_GPIO_LEVEL4 //32
#define XTREE_GPIO_LEVEL5       (gpio_num_t)CONFIG_XTREE_GPIO_LEVEL5 //0
#define XTREE_GPIO_LEVEL6       (gpio_num_t)CONFIG_XTREE_GPIO_LEVEL6 //16
#define XTREE_GPIO_LEVEL7       (gpio_num_t)CONFIG_XTREE_GPIO_LEVEL7 //4
#define XTREE_GPIO_LEVEL8       (gpio_num_t)CONFIG_XTREE_GPIO_LEVEL8 //22

//Can be  : 0,2,4,5,12,13,14,15,16,21,22,23,25,26,27,32,33
//Are used: *   * *             ** ** ** ** ** ** ** ** **

void TraceCurrentStep(int lineNo, LightShowCommand* lsCmd)
{
#ifdef LOCAL_TRACE
    static char binaryLights[9] = "00000000";    

    switch (lsCmd->stepType)
    {
    case Light:
        printf("%04d: LIGHTS(%s) RGB(%u,%u,%u) D=%ums\n", lineNo,
            GetBinaryLights(lsCmd->LightStep.lightRows, binaryLights),
            lsCmd->LightStep.red, lsCmd->LightStep.green, lsCmd->LightStep.blue,
            lsCmd->LightStep.delayMs);
        break;
    case Label:
        printf("%04d: LABEL :%d\n", lineNo,
            lsCmd->Label.idLabel+1);
        break;
    case Loop:
        printf("%04d: LOOP :%d * %d times\n", lineNo,
            lsCmd->Loop.idLabel+1,
            lsCmd->Loop.count);
        break;
    case Wait:
        printf("%04d: WAIT %dms\n", lineNo,
            lsCmd->Wait.delayMs);
        break;
    case Stop:
        printf("%04d: STOP\n", lineNo);
        break;
    case Call:
        printf("%04d: CALL\n", lineNo);
        break;
    case Return:
        printf("%04d: RETURN\n", lineNo);
        break;
    }
#endif
}

#define XTREE_COLOR_INDEX_R     0
#define XTREE_COLOR_INDEX_G     1
#define XTREE_COLOR_INDEX_B     2

#define XTREE_REGISTER_FOLLOW   1   // Registers follow inputs
#define XTREE_REGISTER_LATCH    0   // Registers latched and isolated from inputs

#define GPIO_HIGH               1   // VCC
#define GPIO_LOW                0   // GND

#define XTREE_LEVELS        8     // How many levels on the tree?
#define XTREE_COLOR_GPIOS   3     // The number of GPIOs for colors

//TEMPORARY///////////////////////////////////////////////////
static gpio_num_t xtreeLevels[XTREE_LEVELS] = {
    XTREE_GPIO_LEVEL1,
    XTREE_GPIO_LEVEL2,
    XTREE_GPIO_LEVEL3,
    XTREE_GPIO_LEVEL4,
    XTREE_GPIO_LEVEL5,
    XTREE_GPIO_LEVEL6,
    XTREE_GPIO_LEVEL7,
    XTREE_GPIO_LEVEL8
};

static gpio_num_t xtreeColors[XTREE_COLOR_GPIOS] = {
    XTREE_GPIO_RED,
    XTREE_GPIO_GREEN,
    XTREE_GPIO_BLUE,
};

void TriggerRegisterLoading()
{
    gpio_set_level((gpio_num_t)XTREE_GPIO_LOADREG, XTREE_REGISTER_FOLLOW);
    //vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)XTREE_GPIO_LOADREG, XTREE_REGISTER_LATCH);
}

void SetLights(LightShowCommand* pgmStep)
{
    unsigned lights = (pgmStep->LightStep.lightRows);
    unsigned mask = 0x80;
    for (int i=0; i<XTREE_LEVELS; ++i) {
        gpio_set_level(xtreeLevels[i], (lights&mask)?1:0);
        mask >>= 1;
    }

    printf("Config: R%d G%d B%d\n", (pgmStep->LightStep.red)?GPIO_LOW:GPIO_HIGH, 
        (pgmStep->LightStep.green)?GPIO_LOW:GPIO_HIGH, 
        (pgmStep->LightStep.blue)?GPIO_LOW:GPIO_HIGH);
    gpio_set_level(xtreeColors[XTREE_COLOR_INDEX_R], (pgmStep->LightStep.red)?GPIO_LOW:GPIO_HIGH);
    gpio_set_level(xtreeColors[XTREE_COLOR_INDEX_G], (pgmStep->LightStep.green)?GPIO_LOW:GPIO_HIGH);
    gpio_set_level(xtreeColors[XTREE_COLOR_INDEX_B], (pgmStep->LightStep.blue)?GPIO_LOW:GPIO_HIGH);
}

esp_err_t RunLightshow(void* lightShowPtr, int pgmLength)
{
    LightShowCommand* lightShowPgm = (LightShowCommand*)lightShowPtr;
    LightShowCommand* pgmStep = NULL;

    printf("RunLightshow received %d-steps program.\n", pgmLength);


    //TEMPORARY///////////////////////////////////////////////////
    for (int i=0; i<XTREE_LEVELS; ++i) {
        printf("Config: LEVEL%d GPIO%d OUTPUT %d\n", i, xtreeLevels[i], GPIO_HIGH);
        gpio_set_direction(xtreeLevels[i], GPIO_MODE_OUTPUT);
        gpio_set_level(xtreeLevels[i], GPIO_HIGH);
    }
    
    for (int i=0; i<XTREE_COLOR_GPIOS; ++i) {
        printf("Config: COLOR%d GPIO%d OUTPUT %d\n", i, xtreeColors[i], GPIO_LOW);
        if (xtreeColors[i] >= 12 && xtreeColors[i] <= 15) {
            gpio_reset_pin(xtreeColors[i]);
        }
        gpio_set_direction(xtreeColors[i], GPIO_MODE_OUTPUT);
        gpio_set_level(xtreeColors[i], GPIO_LOW);
    }
    // Initialize REGISTER-LOAD triggering GPIO and set 'NOLOAD'.
    // This must be called AFTER all REGISTER GPIOs have been initialized.
    printf("Config: LATCH GPIO%d OUTPUT %d\n", XTREE_GPIO_LOADREG, GPIO_HIGH);
    gpio_set_direction((gpio_num_t)XTREE_GPIO_LOADREG, GPIO_MODE_OUTPUT);
    vTaskDelay(pdMS_TO_TICKS(1000));
    TriggerRegisterLoading();
    //TEMPORARY///////////////////////////////////////////////////





    int labelIndex[MAX_LABELID];
    for (int i=0; i<MAX_LABELID; ++i) labelIndex[i] = 0;

    // Create index to label steps in the program for quick branching.
    for (int i=0; i<pgmLength; ++i) {
        if (lightShowPgm[i].stepType == Label)
        {
            labelIndex[lightShowPgm[i].Label.idLabel] = i;
        }
    }

    // Create loop counters
    int loopCounter[pgmLength];

Restart:
#ifdef LOCAL_TRACE
    printf("Program Restart\n");
#endif
    for (int i=0; i<pgmLength; ++i) loopCounter[i] = -1; // Reset all loop counters

    int pgmIndex = 0;

NextStep:

    pgmStep = &lightShowPgm[pgmIndex];

#ifdef LOCAL_TRACE
    TraceCurrentStep(pgmIndex, pgmStep);
#endif

    switch (pgmStep->stepType) {
    case Light:
        // Execute lighting step
        SetLights(pgmStep);
        TriggerRegisterLoading();

        // Wait specified time
        if (pgmStep->LightStep.delayMs != 0) {
            vTaskDelay(pdMS_TO_TICKS(pgmStep->LightStep.delayMs));
        }
        ++pgmIndex;     // Advance to next step
        break;
    case Label:     // NOOP
        ++pgmIndex;     // Advance to next step
        break;
    case Loop:
        if (loopCounter[pgmIndex] == -1) {
            loopCounter[pgmIndex] = pgmStep->Loop.count-1;
        }
        if (loopCounter[pgmIndex] > 0) {
            --loopCounter[pgmIndex];
            pgmIndex = labelIndex[pgmStep->Loop.idLabel];
        }
        else if (loopCounter[pgmIndex] <= 0) {
            loopCounter[pgmIndex] = -1;
            ++pgmIndex;     // Loop ended: advance to next step
        }
        break;
    case Wait:
        vTaskDelay(pdMS_TO_TICKS(pgmStep->Wait.delayMs));
        ++pgmIndex;     // Advance to next step
        break;
    case Stop:
        goto Exit;
        break;
    case Call:      // NOOP
        ++pgmIndex;     // Advance to next step
        break;
    case Return:    // NOOP
        ++pgmIndex;     // Advance to next step
        break;
    }

    if (pgmIndex >= pgmLength) goto Exit;    // Restart program by default.
    if (pgmIndex >= pgmLength) goto Restart;    // Restart program by default.
    goto NextStep;

Exit:
    return ESP_OK;
}