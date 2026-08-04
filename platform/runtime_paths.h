#pragma once

#include <filesystem>

namespace lotui {

// Returns the directory containing the current executable. Runtime resources
// should be resolved from this directory, not from the process working folder.
std::filesystem::path executableDirectory();

} // namespace lotui
