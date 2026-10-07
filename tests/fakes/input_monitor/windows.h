#pragma once
#ifndef IDLEHARBOR_INPUT_MONITOR_TEST_DOUBLE
#error This header belongs only to the isolated input-monitor test target.
#endif
#include <cstdint>
using BOOL = int;
using DWORD = std::uint32_t;
using UINT = unsigned int;
using ULONG_PTR = std::uintptr_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using HWND = void*;
using HINSTANCE = void*;
using HHOOK = void*;
#define CALLBACK
inline constexpr BOOL FALSE = 0;
inline constexpr BOOL TRUE = 1;
inline constexpr int HC_ACTION = 0;
inline constexpr int WH_MOUSE_LL = 14;
inline constexpr int WH_KEYBOARD_LL = 13;
inline constexpr DWORD LLMHF_INJECTED = 1;
inline constexpr DWORD LLKHF_INJECTED = 16;
inline constexpr DWORD ERROR_INVALID_HOOK_HANDLE = 1404;
using HOOKPROC = LRESULT (*)(int, WPARAM, LPARAM);
struct MSLLHOOKSTRUCT { DWORD flags = 0; ULONG_PTR dwExtraInfo = 0; };
struct KBDLLHOOKSTRUCT { DWORD flags = 0; ULONG_PTR dwExtraInfo = 0; };
HINSTANCE GetModuleHandleW(const wchar_t*);
HHOOK SetWindowsHookExW(int, HOOKPROC, HINSTANCE, DWORD);
BOOL UnhookWindowsHookEx(HHOOK);
DWORD GetLastError();
LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM);
BOOL PostMessageW(HWND, UINT, WPARAM, LPARAM);
