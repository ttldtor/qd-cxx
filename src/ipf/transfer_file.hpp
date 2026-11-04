// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "transfer_source.hpp"

#include <cstring>
#include <utility>

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#else
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

namespace org::ttldtor::qdcxx::ipf::detail {

#ifdef _WIN32
using TransferPathChar = wchar_t; ///< Native bounded UTF-16 Windows path element.
#else
using TransferPathChar = char; ///< Native strictly validated UTF-8 POSIX path element.
#endif
inline constexpr std::size_t TRANSFER_FILE_PATH_BYTES =
    (SOURCE_CAPACITY + 1) * sizeof(TransferPathChar); ///< Whole NUL-terminated native open workspace.

/** Sole regular-file handle; special streams are rejected by the opened identity before any read.
 * Acquisition on POSIX is nonblocking, including FIFO/device candidates. Ordinary filesystem
 * open/stat/read scheduling is native progress, without a hard disk/network-filesystem latency bound.
 */
class TransferFile {
#ifdef _WIN32
    HANDLE descriptor_ = INVALID_HANDLE_VALUE; ///< Owned opened disk-file handle.

    /** Adopt one already verified regular disk handle.
     * @param descriptor Opened regular-file handle.
     */
    explicit TransferFile(HANDLE descriptor) noexcept : descriptor_(descriptor) {
    }
#else
    int descriptor_ = -1; ///< Owned verified regular-file descriptor.

    /** Adopt one already verified regular descriptor.
     * @param descriptor Opened regular-file descriptor.
     */
    explicit TransferFile(int descriptor) noexcept : descriptor_(descriptor) {
    }
#endif

    public:
/** Transfer the sole descriptor; the source cannot close it afterward.
 * @param other Existing regular-file owner.
 */
#ifdef _WIN32
    TransferFile(TransferFile &&other) noexcept : descriptor_(std::exchange(other.descriptor_, INVALID_HANDLE_VALUE)) {
    }
#else
    TransferFile(TransferFile &&other) noexcept : descriptor_(std::exchange(other.descriptor_, -1)) {
    }
#endif
    TransferFile(const TransferFile &) = delete;
    TransferFile &operator=(const TransferFile &) = delete;

    /** Close only the matching owned descriptor. */
    ~TransferFile() {
#ifdef _WIN32

        if (descriptor_ != INVALID_HANDLE_VALUE) {
            CloseHandle(descriptor_);
        }

#else

        if (descriptor_ >= 0) {
            ::close(descriptor_);
        }

#endif
    }

    /** Open bounded validated UTF-8 and check the actual opened object, following regular symlinks.
     * @param path Borrowed decoded UTF-8 path, without NUL, used only during acquisition.
     * @return Sole regular owner, or static open/type refusal; no stream read occurs on refusal.
     */
    [[nodiscard]] static std::expected<TransferFile, IpfTransferDiagnostic> open(std::string_view path) noexcept {
        if (path.empty() || path.size() > SOURCE_CAPACITY) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::INVALID_SOURCE, "source-file-path"});
        }

        std::array<TransferPathChar, SOURCE_CAPACITY + 1> native{};
#ifdef _WIN32
        const auto count =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
                                native.data(), static_cast<int>(native.size() - 1));

        if (count == 0) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::INVALID_SOURCE, "source-file-utf8"});
        }

        const auto descriptor =
            CreateFileW(native.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (descriptor == INVALID_HANDLE_VALUE) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::IO_ERROR, "source-open"});
        }

        BY_HANDLE_FILE_INFORMATION information{};

        if (GetFileType(descriptor) != FILE_TYPE_DISK || !GetFileInformationByHandle(descriptor, &information) ||
            (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            CloseHandle(descriptor);

            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::INVALID_SOURCE, "source-not-regular"});
        }

#else
        std::memcpy(native.data(), path.data(), path.size());
        const auto descriptor = ::open(native.data(), O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOCTTY);

        if (descriptor < 0) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::IO_ERROR, "source-open"});
        }

        struct stat information{};

        if (::fstat(descriptor, &information) != 0 || !S_ISREG(information.st_mode)) {
            ::close(descriptor);

            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::INVALID_SOURCE, "source-not-regular"});
        }

#endif

        return TransferFile(descriptor);
    }

    /** Read one bounded chunk from the already verified regular identity.
     * @param output Stable owned writable window; size fits the selected platform call.
     * @return Exact read bytes, zero for regular EOF, or static I/O failure.
     */
    [[nodiscard]] std::expected<std::size_t, IpfTransferDiagnostic> read(std::span<std::byte> output) noexcept {
#ifdef _WIN32
        DWORD count = 0;

        if (!ReadFile(descriptor_, output.data(), static_cast<DWORD>(output.size()), &count, nullptr)) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::IO_ERROR, "source-read"});
        }

#else
        const auto count = ::read(descriptor_, output.data(), output.size());

        if (count < 0) {
            return std::unexpected(IpfTransferDiagnostic{IpfTransferError::IO_ERROR, "source-read"});
        }

#endif

        return static_cast<std::size_t>(count);
    }
};

} // namespace org::ttldtor::qdcxx::ipf::detail
