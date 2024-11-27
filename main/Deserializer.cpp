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

static bool NormalizeLine(char* buf)
{
    bool validLine = false;
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
    LookupLights,
    LookupRed,
    LookupGreen,
    LookupBlue,
    LookupDelay,
    Parsed,
    Error
};

const char* SkipBlanks(const char* curChar)
{
    while (*curChar == ' ' || *curChar == '\t') ++curChar;
    return curChar;
}

long GetInteger(const char* curChar)
{
    const char* lastChar = curChar;
    while (*lastChar >= '0' && *lastChar <= '9') ++lastChar;
    long number = stol(string(curChar,lastChar-curChar));

    return number;
}

bool ParseSequenceLine(const char* line, struct sequenceCommand* seqLine)
{
    printf("%s\n---", line);

    SeqParsingState state = LookupLights;

    long lights = 0;

    const char* curChar = line;
    while (state != Parsed && state != Error) {
        switch (state) {
        case LookupLights:
            curChar = SkipBlanks(curChar);
            lights = GetInteger(curChar);
            printf("%ld---\n",lights);
            state = LookupRed;
            break;
        case LookupRed:
            curChar = SkipBlanks(curChar);
            state = LookupGreen;
            break;
        case LookupGreen:
            curChar = SkipBlanks(curChar);
            state = LookupBlue;
            break;
        case LookupBlue:
            curChar = SkipBlanks(curChar);
            state = LookupDelay;
            break;
        case LookupDelay:
            curChar = SkipBlanks(curChar);
            state = Parsed;
            break;
        case Parsed:
            break;
        case Error:
            break;
        }
    }

    return true;
}

// Fetch the customizable light-engine commands for the Xmas Tree driver
esp_err_t LoadCommands()
{
    struct sequenceCommand seqLine;
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

    while (fgets(buf, sizeof(buf), seqFile) != NULL)
    {
        if (NormalizeLine(buf) &&
            ParseSequenceLine(buf, &seqLine)) 
        {
        }
    }

    fclose(seqFile);
    return ESP_OK;
}
