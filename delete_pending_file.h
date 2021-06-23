#pragma once

#pragma once

#include <Windows.h>

HANDLE make_section_from_delete_pending_file(wchar_t* filePath, BYTE* payladBuf, DWORD payloadSize);
