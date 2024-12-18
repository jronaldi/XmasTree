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

const char* LoopMarker = "LOOP ";
const char* WaitMarker = "WAIT ";

static bool NormalizeLine(char* buf)
{
    int len = strlen(buf);

    bool nonEOLCharFound = false;
    char* c = buf + len;
    while (--c >= buf) {
        // Remove spurious chars
        if (!nonEOLCharFound && 
            (*c == '\n' || *c == '\r' || *c == ' ' || *c == '\t')) *c = '\0';    
        else if (*c == '#') {
            *c = '\0';
            nonEOLCharFound = true;
        }
        else {
            nonEOLCharFound = true;
        }
    }

    return (strlen(buf) > 0);
}

// static const char* SequenceDecodingRegex =  
//     "(?'lights'\d{8})\s+(?'red'\d{1,3})\s+(?'green'\d{1,3}\s+(?'blue'\d{1,3}))\s+(?'delay'\d+)?";

enum SeqParsingState {
    BeginParse,
    LookupLabel,
    LookupLoop,
    LookupWait,
    LookupLights,
    LookupRed,
    LookupGreen,
    LookupBlue,
    LookupDelay,
    ParsedLightStep,
    ParsedLabel,
    ParsedLoop,
    ParsedWait,
    Success,
    Error
};

const char* SkipBlanks(const char* pCurChar)
{
    while (*pCurChar == ' ' || *pCurChar == '\t') ++pCurChar;
    return pCurChar;
}

const char* GetInteger(const char* pCurChar, unsigned *val)
{
    char buf[] = { 0,0,0,0,0,0,0,0,0,0 };
    char* pBuf = buf;

    while (*pCurChar >= '0' && *pCurChar <= '9' && pBuf - buf < sizeof(buf)) {
        *pBuf++ = *pCurChar++;
    }

    int length = pBuf - buf;
    *val = (unsigned)atoi(buf);

    return pCurChar;
}

bool IsValidBinaryState(const char* pCurChar)
{
    return (*pCurChar >= '0' && *pCurChar <= '1');
}

const char* GetBinaryStates(const char* pCurChar, unsigned *pLevelState)
{
    *pLevelState = 0;

    while (*pCurChar >= '0' && *pCurChar <= '1') 
    {
        *pLevelState = (*pLevelState << 1) | ((*pCurChar == '1') ? 1 : 0);
        ++pCurChar;
    }

    return pCurChar;
}

/// @brief Map brightness values from 0-99 (user) to 0-255 (hardware).
/// @param rawBrightness The desired brightness value (0=black, 99=100% bright)
/// @return 
unsigned MapLightBrightnessRange(unsigned rawBrightness)
{
    unsigned brightness;

    if (rawBrightness > 99) rawBrightness = 99;

    brightness = (rawBrightness * 255 / 99);
    return brightness;
}

bool IsMatchingMarker(const char* marker, const char* buffer)
{
    bool markerMatch = false;
    bool more = true;
    
    while (*marker != '\0' && *buffer != '\0' && tolower(*marker) == tolower(*buffer))
    {
        ++marker;
        ++buffer;
    }

    if (*marker == '\0') {      // The marker was found in full
        markerMatch = true;
    }

    return markerMatch;
}

const char*GetState(SeqParsingState state)
{
    static char* stateName;
    switch (state)
    {
        case BeginParse: stateName = "BeginParse"; break;
        case LookupLabel: stateName = "LookupLabel"; break;
        case LookupLoop: stateName = "LookupLoop"; break;
        case LookupWait: stateName = "LookupWait"; break;
        case LookupLights: stateName = "LookupLights"; break;
        case LookupRed: stateName = "LookupRed"; break;
        case LookupGreen: stateName = "LookupGreen"; break;
        case LookupBlue: stateName = "LookupBlue"; break;
        case LookupDelay: stateName = "LookupDelay"; break;
        case ParsedLightStep: stateName = "ParsedLightStep"; break;
        case ParsedLabel: stateName = "ParsedLabel"; break;
        case ParsedLoop: stateName = "ParsedLoop"; break;
        case ParsedWait: stateName = "ParsedWait"; break;
        case Success: stateName = "Success"; break;
        case Error: stateName = "Error"; break;
        break;
    }
    return stateName;
}

bool ParseSequenceLine(const char* line, LightShowCommand* lsCmd)
{
    bool success = true;

    SeqParsingState state = BeginParse;

    static unsigned lastDelay = 1000;   // Default delay
    unsigned lights = 0;
    unsigned red = 99;
    unsigned green = 0;
    unsigned blue = 0;
    unsigned delay = lastDelay;
    unsigned repeat = 1;
    unsigned label = 0;

    const char* pCurChar = line;
    while (state != Success && state != Error) {
#ifdef LOCAL_TRACE
        printf("State=%s\n",GetState(state));
#endif
        switch (state) {
        case BeginParse:
            pCurChar = SkipBlanks(pCurChar);
            if (IsValidBinaryState(pCurChar)) {
                state = LookupLights; 
            }
            else if (*pCurChar == LabelMarker) {
                ++pCurChar;             // Skip marker
                state = LookupLabel;
            } 
            else if (IsMatchingMarker(LoopMarker,pCurChar)) {
                pCurChar += strlen(LoopMarker);
                pCurChar = SkipBlanks(pCurChar);
                state = LookupLoop;
            }
            else if (IsMatchingMarker(WaitMarker,pCurChar)) {
                pCurChar += strlen(WaitMarker);
                pCurChar = SkipBlanks(pCurChar);
                state = LookupWait;
            }
            else {
                state = Error;
            }
            break;
        case LookupLabel:
            pCurChar = GetInteger(pCurChar, &label);
            if (label >= 1 && label <= MAX_LABELID) {
                state = ParsedLabel;
            }
            else {
                ESP_LOGE(TAG, "Label ID out of range [1..%d] (%d)", MAX_LABELID, label);
                state = Error;
            }
            break;
        case LookupLoop:
            if (*pCurChar == LabelMarker) {
                ++pCurChar;             // Skip marker
                pCurChar = GetInteger(pCurChar, &label);
                pCurChar = SkipBlanks(pCurChar);
                if (*pCurChar) { 
                    pCurChar = GetInteger(pCurChar, &repeat);
                    state = ParsedLoop;
                } 
                else {
                    repeat = 1;
                    state = ParsedLoop;
                }
            }
            else {
                state = ParsedLoop;
            }
            break;
        case LookupWait:
            pCurChar = GetInteger(pCurChar, &delay);
            state = ParsedWait;
            break;
        case LookupLights:
            pCurChar = GetBinaryStates(pCurChar, &lights);
            state = LookupRed;
            break;
        case LookupRed:
            pCurChar = SkipBlanks(pCurChar);
            if (*pCurChar) { 
                pCurChar = GetInteger(pCurChar, &red);
                state = LookupGreen;
            } 
            else
            {
                state = ParsedLightStep;
            }
            break;
        case LookupGreen:
            pCurChar = SkipBlanks(pCurChar);
            if (*pCurChar) { 
                pCurChar = GetInteger(pCurChar, &green);
                state = LookupBlue;
            } 
            else
            {
                state = ParsedLightStep;
            }
            break;
        case LookupBlue:
            pCurChar = SkipBlanks(pCurChar);
            if (*pCurChar) { 
                pCurChar = GetInteger(pCurChar, &blue);
                state = LookupDelay;
            } 
            else
            {
                state = ParsedLightStep;
            }
            break;
        case LookupDelay:
            pCurChar = SkipBlanks(pCurChar);
            if (*pCurChar) { 
                pCurChar = GetInteger(pCurChar, &delay);
                lastDelay = delay;      // Remember the last specified delay
            } 
            else
            {
                delay = lastDelay;      // If unspecified, use last one specified or default
            }
            state = ParsedLightStep;
            break;
        case ParsedLightStep:    // Everything parsed successfully so assign the result
            lsCmd->stepType = Light;
            lsCmd->LightStep.lightRows = lights;
            lsCmd->LightStep.red = MapLightBrightnessRange(red);
            lsCmd->LightStep.green = MapLightBrightnessRange(green);
            lsCmd->LightStep.blue = MapLightBrightnessRange(blue);
            lsCmd->LightStep.delayMs = delay;
            state = Success;
            break;
        case ParsedLabel:
            lsCmd->stepType = Label;
            lsCmd->Label.idLabel = label-1; // Begins at 0 internally
            state = Success;
            break;
        case ParsedLoop:
            lsCmd->stepType = Loop;
            lsCmd->Loop.idLabel = label-1;  // Begins at 0 internally
            lsCmd->Loop.count = repeat;
            state = Success;
            break;
        case ParsedWait:
            lsCmd->stepType = Wait;
            lsCmd->Wait.delayMs = delay;
            state = Success;
            break;
        case Success:
            success = true;
            break;
        case Error:
            success = false; 
            break;
        }
    }

#ifdef LOCAL_TRACE
    printf("End State=%s\n",GetState(state));
#endif
    return success;
}

const char *GetBinaryLights(unsigned lights, char buf[8])
{
    for (int i = 7; i >= 0; --i) {
        buf[i] = (lights & 0x1) ? '1' : '0';
        lights >>= 1;
    }
    return buf;
}

int GetMaxPgmLength(FILE* seqFile)
{
    char buf[1024];
    const char* pCurChar;
    int lineNo = 0;

    fseek(seqFile, 0L, SEEK_SET);   // Reset to beginning of the file

    // Count all non-empty, non-comment lines
    while (fgets(buf, sizeof(buf), seqFile) != NULL)
    {
        pCurChar = buf;
        pCurChar = SkipBlanks(pCurChar);
        if (*pCurChar && *pCurChar != '#') ++lineNo;
    }

    fseek(seqFile, 0L, SEEK_SET);   // Reset to beginning of the file

    return lineNo;
}

// Fetch the customizable light-engine commands for the Xmas Tree driver
esp_err_t LoadCommands(void** rawLightShowPgm, int* pgmLength)
{
    static char binaryLights[9] = "00000000";    
    LightShowCommand lsCmd;
    char buf[1024];
    
    if (!GetFilePath(buf,sizeof(buf),"sequence.txt"))
    {
        ESP_LOGE(TAG, "Failed to get path to sequence file");
        return ESP_FAIL;
    }

    FILE* seqFile = fopen(buf,"r");
    if (seqFile == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    int maxProgramLength = GetMaxPgmLength(seqFile);
    LightShowCommand* lightshowPgm = new LightShowCommand[maxProgramLength];
    int lastPgmIndex = -1;          // Empty


    int lineNo = 0;
    while (fgets(buf, sizeof(buf), seqFile) != NULL)
    {
        ++lineNo;
#ifdef LOCAL_TRACE
        printf("%04d: Read: %s\n", lineNo, buf);
#endif

        if (NormalizeLine(buf) &&
            ParseSequenceLine(buf, &lsCmd)) 
        {
            ++lastPgmIndex;
            memcpy(&lightshowPgm[lastPgmIndex], &lsCmd, sizeof(lightshowPgm[0]));

            switch (lsCmd.stepType)
            {
            case Light:
                printf("%04d: LIGHTS(%s) RGB(%u,%u,%u) D=%ums\n", lineNo,
                    GetBinaryLights(lsCmd.LightStep.lightRows, binaryLights),
                    lsCmd.LightStep.red, lsCmd.LightStep.green, lsCmd.LightStep.blue,
                    lsCmd.LightStep.delayMs);
                break;
            case Label:
                printf("%04d: LABEL :%d\n", lineNo,
                    lsCmd.Label.idLabel+1);
                break;
            case Loop:
                printf("%04d: LOOP :%d * %d times\n", lineNo,
                    lsCmd.Loop.idLabel+1,
                    lsCmd.Loop.count);
                break;
            case Wait:
                printf("%04d: WAIT %dms\n", lineNo,
                    lsCmd.Wait.delayMs);
                break;
            case Stop:
                break;
            case Call:
                break;
            case Return:
                break;
            }
        }
    }

    *rawLightShowPgm = (void*)lightshowPgm;
    *pgmLength = lastPgmIndex + 1;

    fclose(seqFile);
    return ESP_OK;
}
