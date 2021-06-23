#pragma once

#include <Windows.h>

HANDLE make_transacted_section(wchar_t* targetPath, BYTE* payladBuf, DWORD payloadSize);

