#include "platform/runtime_paths.h"

#include <unistd.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace lotui {

std::filesystem::path executableDirectory() {
    std::vector<char> buffer(512);
    while (true) {
        const ssize_t length = readlink(
            "/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            throw std::runtime_error("failed to locate the executable");
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            return std::filesystem::path(
                std::string(buffer.data(), static_cast<std::size_t>(length)))
                .parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

} // namespace lotui
