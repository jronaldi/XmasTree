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
//#include <regex>
#include "App.h"
#include "SdCard.h"

using namespace std;


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

esp_err_t RunLightshow(void* lightShowPtr, int pgmLength)
{
    LightShowCommand* lightShowPgm = (LightShowCommand*)lightShowPtr;
    LightShowCommand* pgmStep = NULL;

    printf("RunLightshow received %d-steps program.\n", pgmLength);

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
        else if (loopCounter[pgmIndex] == 0) {
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