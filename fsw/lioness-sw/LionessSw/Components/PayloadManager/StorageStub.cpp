// StorageStub.cpp
// See StorageStub.hpp. Host file storage stub only.
// Uses POSIX calls, not std::filesystem, because F Prime
// builds with C++14 and std::filesystem needs C++17.
// Style rule for this project: plain punctuation only, no em dashes.

#include "LionessSw/Components/PayloadManager/StorageStub.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <fstream>

namespace Components {

namespace {

// mkdir -p for one path, creating parents as needed.
bool makeDirectories(const std::string& path) {
    if (path.empty()) return false;
    std::string current;
    std::size_t start = 0;
    if (path[0] == '/') {
        current = "/";
        start = 1;
    }
    while (start <= path.size()) {
        std::size_t slash = path.find('/', start);
        std::string part = path.substr(
            start, slash == std::string::npos ? std::string::npos
                                              : slash - start);
        if (!part.empty() && part != ".") {
            if (!current.empty() && current[current.size() - 1] != '/') {
                current += "/";
            }
            current += part;
            if (::mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
                return false;
            }
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

}  // namespace

StorageStub::StorageStub(const std::string& directory) : directory_(directory) {}

bool StorageStub::writeFile(const std::string& filename, const std::uint8_t* data,
                            std::size_t size) {
    if (data == nullptr && size > 0) return false;
    if (!makeDirectories(directory_)) return false;
    std::string path = directory_ + "/" + filename;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (size > 0) {
        out.write(reinterpret_cast<const char*>(data),
                  static_cast<std::streamsize>(size));
    }
    out.close();
    return static_cast<bool>(out);
}

bool StorageStub::readFile(const std::string& filename, std::uint8_t* dest,
                           std::size_t capacity, std::size_t& size) {
    size = 0;
    if (dest == nullptr) return false;
    std::string path = directory_ + "/" + filename;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    std::streampos end = in.tellg();
    if (end < 0) return false;
    std::size_t fileSize = static_cast<std::size_t>(end);
    if (fileSize > capacity) return false;
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(dest), static_cast<std::streamsize>(fileSize));
    if (!in) return false;
    size = fileSize;
    return true;
}

bool StorageStub::exists(const std::string& filename) {
    std::string path = directory_ + "/" + filename;
    return ::access(path.c_str(), F_OK) == 0;
}

}  // namespace Components
