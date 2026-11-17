// StorageStub.hpp
// Minimal host storage stub for the PayloadManager. Writes
// processed image products to files in a host directory so the
// capture path is end to end in simulation. This is NOT flight
// storage: the real system will use the F Prime file handling
// subtopology and downlink. The stub only exists so the manager
// has somewhere to hand results to while the flight storage
// interface is still open.
// Style rule for this project: plain punctuation only, no em dashes.

#ifndef Components_PayloadManager_StorageStub_HPP
#define Components_PayloadManager_StorageStub_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace Components {

// Write-only file storage in one directory.
class StorageStub {
public:
    explicit StorageStub(const std::string& directory);

    void setDirectory(const std::string& directory) { directory_ = directory; }
    const std::string& getDirectory() const { return directory_; }

    // Write size bytes to directory/filename. Creates the
    // directory if needed. Returns false on any failure.
    bool writeFile(const std::string& filename, const std::uint8_t* data,
                   std::size_t size);

    // Read a file back, for tests. Returns false on any failure.
    bool readFile(const std::string& filename, std::uint8_t* dest,
                  std::size_t capacity, std::size_t& size);

    // True when directory/filename exists.
    bool exists(const std::string& filename);

private:
    std::string directory_;
};

}  // namespace Components

#endif
