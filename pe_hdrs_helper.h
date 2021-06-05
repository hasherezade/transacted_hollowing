#pragma once

#include <Windows.h>

WORD get_pe_architecture(const BYTE *pe_buffer);

DWORD get_entry_point_rva(const BYTE *pe_buffer);

bool pe_is64bit(IN const BYTE *pe_buffer);
