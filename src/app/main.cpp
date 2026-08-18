#include <windows.h>

#include <string>

#include "idleharbor/version.hpp"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const std::wstring message =
        std::wstring(idleharbor::kProductName) + L" " + std::wstring(idleharbor::kVersion) +
        L"\n\nThe native project foundation is ready. Keep-awake behavior is not implemented yet.";

    MessageBoxW(nullptr, message.c_str(), idleharbor::kProductName.data(), MB_OK | MB_ICONINFORMATION);
    return 0;
}
