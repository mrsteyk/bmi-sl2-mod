#include "game_hooks.h"

#include <windows.h>
#include <cstdio>
#include <print>

#include <safetyhook.hpp>

namespace {
safetyhook::InlineHook FStreamlineRHIPreInitModule__StartupModule{};
uintptr_t FStreamlineRHIPreInitModule__StartupModule_hk()
{
    auto ret = FStreamlineRHIPreInitModule__StartupModule.call<uintptr_t>();

    if (bmi::hooks::install())
    {
        #if !defined(NDEBUG)
        std::println("[STK] Hooks installed AFTER FStreamlineRHIPreInitModule::StartupModule");
        #endif
    } else {
        #if !defined(NDEBUG)
        std::println("[STK] Hooks failed to install!!!");
        #endif
    }

    return ret;
}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);

        #if !defined(NDEBUG)
        if constexpr (true) {
            AllocConsole();
            FILE *_f;
            freopen_s(&_f, "CONIN$", "r", stdin);
            freopen_s(&_f, "CONOUT$", "w", stderr);
            freopen_s(&_f, "CONOUT$", "w", stdout);
            HANDLE hConOut = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            HANDLE hConIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
            SetStdHandle(STD_ERROR_HANDLE, hConOut);
            SetStdHandle(STD_INPUT_HANDLE, hConIn);
        }
        #endif

        FStreamlineRHIPreInitModule__StartupModule = safetyhook::create_inline(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + 0xBCFA90), reinterpret_cast<void*>(&FStreamlineRHIPreInitModule__StartupModule_hk));

        std::println("[STK] Loaded.");
    } else if (reason == DLL_PROCESS_DETACH)
    {
        if (!reserved) bmi::hooks::uninstall();
    }
    return TRUE;
}
