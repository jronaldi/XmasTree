#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool InitializeSdCard();
bool DeinitializeSdCard();

bool GetFilePath(char* filePath, int len, char* fileName);

#ifdef __cplusplus
}
#endif
