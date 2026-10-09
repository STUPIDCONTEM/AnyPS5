#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"

#include <stdexcept>
#include <string>
#if defined(__linux__)
#include <cerrno>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
using NativeStat = struct __stat64;
static int DoStat(const std::filesystem::path& p, NativeStat* st) {
    return _wstat64(p.wstring().c_str(), st);
}
static int DoFstat(int fd, NativeStat* st) {
    if (const auto directory = File::DirectoryDescriptorPath(fd)) return DoStat(*directory, st);
    return _fstat64(fd, st);
}
#else
#if defined(__linux__)
#include <fcntl.h>
#endif
#include <sys/stat.h>
using NativeStat = struct stat;
static int DoStat(const std::filesystem::path& p, NativeStat* st) {
    return ::stat(p.c_str(), st);
}
static int DoFstat(int fd, NativeStat* st) {
    return ::fstat(fd, st);
}
#endif

#if defined(__linux__)
#if defined(SYS_statx) && defined(STATX_BTIME) && defined(STATX_BASIC_STATS) && defined(STATX_INO) && defined(AT_STATX_SYNC_AS_STAT) && defined(AT_EMPTY_PATH)
#define APS5_HAS_LINUX_STATX_BTIME 1
static bool BirthTime(int directory, const char* path, int flags, const NativeStat& native, KernelTimespec* birthTime) {
    const int savedErrno = errno;
    struct statx info{};
    const auto mask = static_cast<unsigned int>(STATX_BASIC_STATS | STATX_BTIME);
    const bool available = ::syscall(SYS_statx, directory, path, flags | AT_STATX_SYNC_AS_STAT, mask, &info) == 0 &&
                           (info.stx_mask & (STATX_INO | STATX_BTIME)) == (STATX_INO | STATX_BTIME) &&
                           info.stx_ino == static_cast<decltype(info.stx_ino)>(native.st_ino) &&
                           info.stx_dev_major == static_cast<unsigned int>(major(native.st_dev)) &&
                           info.stx_dev_minor == static_cast<unsigned int>(minor(native.st_dev));
    if (available) {
        birthTime->tv_sec = static_cast<std::int64_t>(info.stx_btime.tv_sec);
        birthTime->tv_nsec = static_cast<std::int64_t>(info.stx_btime.tv_nsec);
    }
    errno = savedErrno;
    return available;
}
#endif
#endif

static void CopyNativeStat(const NativeStat& st, FileStat* sb, const KernelTimespec* birthTime = nullptr) {
    *sb = FileStat{};
    sb->st_mode = static_cast<std::uint16_t>(st.st_mode);
    sb->st_size = static_cast<std::int64_t>(st.st_size);
#ifdef _WIN32
    sb->st_dev = static_cast<std::uint32_t>(st.st_dev);
    sb->st_ino = static_cast<std::uint32_t>(st.st_ino);
    sb->st_nlink = static_cast<std::uint16_t>(st.st_nlink);
    sb->st_uid = 0;
    sb->st_gid = 0;
    sb->st_rdev = static_cast<std::uint32_t>(st.st_rdev);
    sb->st_blksize = 512;
    sb->st_blocks = (sb->st_size + 511LL) / 512LL;
    sb->st_atim.tv_sec = static_cast<std::int64_t>(st.st_atime);
    sb->st_atim.tv_nsec = 0;
    sb->st_mtim.tv_sec = static_cast<std::int64_t>(st.st_mtime);
    sb->st_mtim.tv_nsec = 0;
    sb->st_ctim.tv_sec = static_cast<std::int64_t>(st.st_ctime);
    sb->st_ctim.tv_nsec = 0;
    sb->st_birthtim.tv_sec = static_cast<std::int64_t>(st.st_ctime);
    sb->st_birthtim.tv_nsec = 0;
#else
    sb->st_dev = static_cast<std::uint32_t>(st.st_dev);
    sb->st_ino = static_cast<std::uint32_t>(st.st_ino);
    sb->st_nlink = static_cast<std::uint16_t>(st.st_nlink);
    sb->st_uid = st.st_uid;
    sb->st_gid = st.st_gid;
    sb->st_rdev = static_cast<std::uint32_t>(st.st_rdev);
    sb->st_blksize = static_cast<std::uint32_t>(st.st_blksize);
    sb->st_blocks = static_cast<std::int64_t>(st.st_blocks);
    sb->st_atim.tv_sec = static_cast<std::int64_t>(st.st_atim.tv_sec);
    sb->st_atim.tv_nsec = static_cast<std::int64_t>(st.st_atim.tv_nsec);
    sb->st_mtim.tv_sec = static_cast<std::int64_t>(st.st_mtim.tv_sec);
    sb->st_mtim.tv_nsec = static_cast<std::int64_t>(st.st_mtim.tv_nsec);
    sb->st_ctim.tv_sec = static_cast<std::int64_t>(st.st_ctim.tv_sec);
    sb->st_ctim.tv_nsec = static_cast<std::int64_t>(st.st_ctim.tv_nsec);
#if defined(__APPLE__)
    sb->st_birthtim.tv_sec = static_cast<std::int64_t>(st.st_birthtimespec.tv_sec);
    sb->st_birthtim.tv_nsec = static_cast<std::int64_t>(st.st_birthtimespec.tv_nsec);
#elif defined(__linux__)
    if (birthTime) {
        sb->st_birthtim = *birthTime;
    } else {
        sb->st_birthtim.tv_sec = -1;
        sb->st_birthtim.tv_nsec = 0;
    }
#else
    sb->st_birthtim.tv_sec = static_cast<std::int64_t>(st.st_birthtim.tv_sec);
    sb->st_birthtim.tv_nsec = static_cast<std::int64_t>(st.st_birthtim.tv_nsec);
#endif
#endif
}

namespace File {

void FillFileStat(const std::filesystem::path& nativePath, FileStat* sb) {
    NativeStat st{};
    if (DoStat(nativePath, &st) != 0) {
        throw std::runtime_error(std::string("FillFileStat: stat failed for ") + nativePath.string());
    }
#if defined(APS5_HAS_LINUX_STATX_BTIME)
    KernelTimespec birthTime{};
    CopyNativeStat(st, sb, BirthTime(AT_FDCWD, nativePath.c_str(), 0, st, &birthTime) ? &birthTime : nullptr);
#else
    CopyNativeStat(st, sb);
#endif
}

void FillFileStat(int nativeDescriptor, FileStat* sb) {
    NativeStat st{};
    if (DoFstat(nativeDescriptor, &st) != 0) {
        throw std::runtime_error(std::string("FillFileStat: fstat failed for fd ") + std::to_string(nativeDescriptor));
    }
#if defined(APS5_HAS_LINUX_STATX_BTIME)
    KernelTimespec birthTime{};
    CopyNativeStat(st, sb, BirthTime(nativeDescriptor, "", AT_EMPTY_PATH, st, &birthTime) ? &birthTime : nullptr);
#else
    CopyNativeStat(st, sb);
#endif
}

bool FillFileStatFromDescriptor(int fd, FileStat* sb) {
    NativeStat st{};
    if (DoFstat(fd, &st) != 0) return false;
#if defined(APS5_HAS_LINUX_STATX_BTIME)
    KernelTimespec birthTime{};
    CopyNativeStat(st, sb, BirthTime(fd, "", AT_EMPTY_PATH, st, &birthTime) ? &birthTime : nullptr);
#else
    CopyNativeStat(st, sb);
#endif
    return true;
}

}
