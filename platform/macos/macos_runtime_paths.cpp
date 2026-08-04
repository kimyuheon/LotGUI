#include "platform/runtime_paths.h"

#include <mach-o/dyld.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace lotui {

std::filesystem::path executableDirectory() {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        throw std::runtime_error("failed to locate the executable");
    }
    return std::filesystem::weakly_canonical(buffer.data()).parent_path();
}

} // namespace lotui
