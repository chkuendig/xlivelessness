#pragma once
#ifndef DEBUGTEXT
#define DEBUGTEXT

INT InitDebugLog(DWORD dwInstanceId);
INT UninitDebugLog();

bool addDebugTextBlacklist(char *black_text);
int getDebugTextArrayMaxLen();
void addDebugText(char* text);
void addDebugText(const char* text);
void addDebugText(wchar_t* wtext);
char* getDebugText(int ordered_index);

void setDebugTextDisplay(bool setOn);
bool getDebugTextDisplay();

void trace_func(const char *fxname);

VOID XllnDebugBreak(char* message);
VOID XllnDebugBreak(const char* message);

#define TRACE_FX() \
    trace_func(__func__)

#define FUNC_STUB() \
    extern void FUNC_STUB2(const char* func);\
    FUNC_STUB2(__func__)

#endif
