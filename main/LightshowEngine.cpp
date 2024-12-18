#define NO_LOCAL_TRACE  // NO_LOCAL_TRACE / LOCAL_TRACE

#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <esp_log.h>
#include <esp_err.h>
#include <string>
//#include <regex>
#include "App.h"
#include "SdCard.h"

using namespace std;

esp_err_t RunLightshow(void** lightShowPtr, int pgmLength)
{
    LightShowCommand* lightShowPgm = (LightShowCommand*)lightShowPtr;

    printf("***RunLightshow received %d-steps program.", pgmLength);

    return ESP_OK;
}