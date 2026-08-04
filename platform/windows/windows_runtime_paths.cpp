#include "platform/runtime_paths.h"

#include <windows.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace lotui {

std::filesystem::path executableDirectory() {
    std::vector<wchar_t> buffer(512);
    while (true) {
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw std::runtime_error("failed to locate the executable");
        }
        if (length < buffer.size() - 1) {
            return std::filesystem::path(
                std::wstring(buffer.data(), length)).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace lotui
