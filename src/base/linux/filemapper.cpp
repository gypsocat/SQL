#include "../mtb-system.hxx"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <format>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

static uint32_t logical_block_size = 65536;
constexpr int   line_size = 512 + 512;

namespace MTB {

uint32_t FileMapper::GetLogicalBlockSize() {
    return logical_block_size;
}
bool FileMapper::SetLogicalBlockSize(uint32_t block_size) {
    if ((block_size & (block_size - 1)) != 0)
        return false;
    logical_block_size = block_size;
    return true;
}

class LinuxFileMapper final: public FileMapper {
public:
    using fd_t = int;
    using stat_t = struct stat;
public:
    LinuxFileMapper(std::string_view filename);
    ~LinuxFileMapper() override;

    /* virtual getters and setters*/
    pointer get() override { return _memory; }
    std::string_view get_filename() override {
        return std::string_view(_filename);
    }
    int get_file_size() override {
        return _size;
    }
    int get_logical_block_size() override {
        return _logical_block;
    }
private:
    std::string _filename;
    pointer     _memory;
    size_t      _size, _logical_block;
    fd_t        _fd;
    stat_t      _file_stat;

    void _createFile();
    void _doResizeAppend() override;
}; // class

static inline void check_file_state(LinuxFileMapper::stat_t &self,
                                    LinuxFileMapper &owner)
{
    uint32_t    file_type  = self.st_mode & S_IFMT;
    ErrorLevel  errorlevel = ErrorLevel::NORMAL;
    std::string msg;
    if (file_type != S_IFREG) {
        errorlevel = ErrorLevel::FATAL;
        msg = std::format("required file {} is not regular",
                          owner.get_filename());
    }

    if (errorlevel != ErrorLevel::NORMAL) {
        throw FileMapper::Exception {
            errorlevel, msg
        };
    }
}

LinuxFileMapper::LinuxFileMapper(std::string_view filename)
    : _filename(filename),
      _memory(nullptr),
      _size(logical_block_size),
      _logical_block(logical_block_size) {
    const char *filename_cstr = _filename.c_str();
    // check if the file exists
    int code = stat(filename_cstr, &_file_stat);
    if (code == -1 && errno == ENOENT) {
        _createFile();
    } else {
        check_file_state(_file_stat, *this);
        _fd = open(filename_cstr, O_RDWR);
        _size = _file_stat.st_size;
    }

    // then map the file to the `_memory` pointer
    _memory = mmap(nullptr, _size,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   _fd,       0);
    if (_memory == nullptr) {
        perror("mmap");
        throw Exception{
            ErrorLevel::FATAL,
            "mmap for LinuxFileMapper failed!"
        };
    }
}

LinuxFileMapper::~LinuxFileMapper()
{
    msync(_memory, _size, MS_SYNC);
    fsync(_fd);
    munmap(_memory, _size);
    close(_fd);
}

void LinuxFileMapper::_createFile()
{
    _fd = open(_filename.c_str(), 
               O_CREAT|O_RDWR,
               S_IRUSR|S_IWUSR| S_IRGRP| S_IROTH);
    int result = fallocate(_fd, 0, 0, _logical_block);
    if (result == -1) {
        perror("fallocate");
        throw FileMapper::Exception {
            ErrorLevel::FATAL,
            "fallocate failed"
        };
    }
}

void LinuxFileMapper::_doResizeAppend()
{
    msync(_memory, _size, MS_SYNC);
    munmap(_memory, _size);
    int result = fallocate(_fd, 0, _size, _logical_block);
    if (result == -1) {
        perror("fallocate");
        throw FileMapper::Exception {
            ErrorLevel::FATAL,
            "fallocate failed"
        };
    }
    check_file_state(_file_stat, *this);
    _size += _logical_block;

    // then map the file to the `_memory` pointer
    _memory = mmap(nullptr, _size,
                   PROT_READ | PROT_WRITE,
                   MAP_DENYWRITE | MAP_SHARED,
                   _fd, 0);
    if (_memory == nullptr) {
        perror("mmap");
        throw Exception{
            ErrorLevel::FATAL,
            "mmap for LinuxFileMapper failed!"
        };
    }
}

FileMapper* CreateFileMapper(std::string_view filename)
{
    FileMapper* ret = new LinuxFileMapper(filename);
    return ret;
}

} // namespace MTB
