#pragma once

#include <Windows.h>
#include "pe_hdrs_helper.h"

bool redirect_to_payload(BYTE* loaded_pe, PVOID load_base, PROCESS_INFORMATION &pi, bool is32bit);
