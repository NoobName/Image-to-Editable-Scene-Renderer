#pragma once
#include <filesystem>
#include <span>
#include <cstdint>
namespace isr {
// A same-directory temporary file is flushed before a single Windows rename publishes it.
// New-file mode never replaces a file created by another writer in the meantime.
void AtomicWrite(const std::filesystem::path&,std::span<const uint8_t>,bool overwrite=false);
}
