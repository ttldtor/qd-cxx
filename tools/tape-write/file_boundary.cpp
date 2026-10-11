// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "file_boundary.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#else
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

namespace org::ttldtor::qdcxx::tape_write {
namespace {
constexpr std::size_t IO_CHUNK_BYTES = 65536;   ///< Fixed bounded read/write syscall chunk.
constexpr std::size_t TEMPORARY_ATTEMPTS = 64;  ///< Finite exclusive-name collision attempts.
std::atomic<std::uint64_t> temporarySequence{}; ///< Process-local name disambiguation, never an ownership test.

/** Construct a content-free first diagnostic.
 * @param stage Static operation name.
 * @param code Static error name.
 * @return Owned diagnostic without paths.
 */
FileDiagnostic failure(std::string_view stage, std::string_view code) {
    return {stage, code};
}

/** Validate a complete borrowed callback table before transferring handles.
 * @param operations Actual owning operation boundary.
 * @return Whether every necessary callback is present.
 */
bool validOperations(const FileOperations &operations) noexcept {
    return operations.open && operations.read && operations.write && operations.flush && operations.close &&
           operations.link && operations.remove;
}

/** Read the current native error, immediately after the failed syscall.
 * @return Native error category/value.
 */
std::error_code nativeError() noexcept {
#ifdef _WIN32

    return {static_cast<int>(GetLastError()), std::system_category()};
#else

    return {errno, std::generic_category()};
#endif
}

/** Open read-only or exclusively create write-only, never overwrite an existing entry.
 * @param context Unused stateless context.
 * @param path Bounded native filename.
 * @param exclusive True selects native create-new output.
 * @return Transferred handle or native error.
 */
std::expected<FileHandle, std::error_code> nativeOpen(void *context, const std::filesystem::path &path,
                                                      bool exclusive) noexcept {
    static_cast<void>(context);
#ifdef _WIN32
    const auto handle =
        CreateFileW(path.c_str(), exclusive ? GENERIC_WRITE : GENERIC_READ, exclusive ? 0 : FILE_SHARE_READ, nullptr,
                    exclusive ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        return std::unexpected(nativeError());
    }

    return reinterpret_cast<FileHandle>(handle);
#else
    const auto handle = ::open(path.c_str(), exclusive ? O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC : O_RDONLY | O_CLOEXEC,
                               S_IRUSR | S_IWUSR);

    if (handle < 0) {
        return std::unexpected(nativeError());
    }

    return handle;
#endif
}

/** Perform one bounded native read; zero uniquely represents EOF.
 * @param context Unused stateless context.
 * @param handle Borrowed live read handle.
 * @param bytes Borrowed writable syscall buffer.
 * @return Transferred count or error.
 */
std::expected<std::size_t, std::error_code> nativeRead(void *context, FileHandle handle,
                                                       std::span<std::byte> bytes) noexcept {
    static_cast<void>(context);
#ifdef _WIN32
    DWORD count{};

    if (!ReadFile(reinterpret_cast<HANDLE>(handle), bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr)) {
        return std::unexpected(nativeError());
    }

    return count;
#else
    const auto count = ::read(static_cast<int>(handle), bytes.data(), bytes.size());

    if (count < 0) {
        return std::unexpected(nativeError());
    }

    return static_cast<std::size_t>(count);
#endif
}

/** Perform one bounded native write; caller handles partial and zero progress.
 * @param context Unused stateless context.
 * @param handle Borrowed live write handle.
 * @param bytes Borrowed syscall bytes.
 * @return Transferred count or error.
 */
std::expected<std::size_t, std::error_code> nativeWrite(void *context, FileHandle handle,
                                                        std::span<const std::byte> bytes) noexcept {
    static_cast<void>(context);
#ifdef _WIN32
    DWORD count{};

    if (!WriteFile(reinterpret_cast<HANDLE>(handle), bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr)) {
        return std::unexpected(nativeError());
    }

    return count;
#else
    const auto count = ::write(static_cast<int>(handle), bytes.data(), bytes.size());

    if (count < 0) {
        return std::unexpected(nativeError());
    }

    return static_cast<std::size_t>(count);
#endif
}

/** Check native flush without promising directory/crash durability.
 * @param context Unused stateless context.
 * @param handle Borrowed live output handle.
 * @return Native error or success.
 */
std::error_code nativeFlush(void *context, FileHandle handle) noexcept {
    static_cast<void>(context);
#ifdef _WIN32

    if (!FlushFileBuffers(reinterpret_cast<HANDLE>(handle))) {
#else

    if (::fsync(static_cast<int>(handle)) != 0) {
#endif

        return nativeError();
    }

    return {};
}

/** Consume one native handle exactly once, including reported close errors.
 * @param context Unused stateless context.
 * @param handle Transferred handle; never reused or retried by the caller.
 * @return Native close error or success.
 */
std::error_code nativeClose(void *context, FileHandle handle) noexcept {
    static_cast<void>(context);
#ifdef _WIN32

    if (!CloseHandle(reinterpret_cast<HANDLE>(handle))) {
#else

    if (::close(static_cast<int>(handle)) != 0) {
#endif

        return nativeError();
    }

    return {};
}

/** Publish exclusively through the actual standard filesystem hard-link operation.
 * @param context Unused stateless context.
 * @param source Closed owned temporary.
 * @param destination Fresh destination; races must fail rather than overwrite.
 * @return Filesystem error or success.
 */
std::error_code nativeLink(void *context, const std::filesystem::path &source,
                           const std::filesystem::path &destination) noexcept {
    static_cast<void>(context);

    try {
        std::error_code error;
        std::filesystem::create_hard_link(source, destination, error);

        return error;
    } catch (...) {
        return std::make_error_code(std::errc::not_enough_memory);
    }
}

/** Remove only a name whose exclusive ownership was previously established.
 * @param context Unused stateless context.
 * @param path Owned temporary, never the destination.
 * @return Filesystem error or success, including an already absent name.
 */
std::error_code nativeRemove(void *context, const std::filesystem::path &path) noexcept {
    static_cast<void>(context);

    try {
        std::error_code error;
        std::filesystem::remove(path, error);

        return error;
    } catch (...) {
        return std::make_error_code(std::errc::not_enough_memory);
    }
}

/** Compare resolved native spelling, including Windows ordinary case aliases.
 * @param left First bounded resolved path.
 * @param right Second bounded resolved path.
 * @return Whether the native spelling denotes the same ordinary path.
 */
bool sameSpelling(const std::filesystem::path &left, const std::filesystem::path &right) noexcept {
#ifdef _WIN32

    return CompareStringOrdinal(left.c_str(), static_cast<int>(left.native().size()), right.c_str(),
                                static_cast<int>(right.native().size()), TRUE) == CSTR_EQUAL;
#else

    return left == right;
#endif
}
} // namespace

FileOperations productionFileOperations() noexcept {
    return {nullptr, nativeOpen, nativeRead, nativeWrite, nativeFlush, nativeClose, nativeLink, nativeRemove};
}

std::expected<FilePaths, FileDiagnostic> resolvePaths(const std::filesystem::path &input,
                                                      const std::filesystem::path &destination, FileLimits limits) {
    try {
        if (input.empty() || destination.empty() ||
            input.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos ||
            destination.native().find(std::filesystem::path::value_type{}) !=
                std::filesystem::path::string_type::npos) {
            return std::unexpected(failure("path", "INVALID_PATH"));
        }

        if (input.native().size() > limits.pathUnits || destination.native().size() > limits.pathUnits ||
            limits.pathUnits > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return std::unexpected(failure("path", "PATH_LIMIT"));
        }

        std::error_code error;
        auto source = std::filesystem::canonical(input, error);

        if (error || !std::filesystem::is_regular_file(source, error) || error) {
            return std::unexpected(failure("path", "INPUT_PATH_FAILED"));
        }

        const auto absoluteDestination = std::filesystem::absolute(destination, error).lexically_normal();

        if (error || absoluteDestination.filename().empty()) {
            return std::unexpected(failure("path", "OUTPUT_PATH_FAILED"));
        }

        auto parent = std::filesystem::canonical(absoluteDestination.parent_path(), error);

        if (error || !std::filesystem::is_directory(parent, error) || error) {
            return std::unexpected(failure("path", "OUTPUT_PARENT_FAILED"));
        }

        auto target = parent / absoluteDestination.filename();

        if (source.native().size() > limits.pathUnits || target.native().size() > limits.pathUnits) {
            return std::unexpected(failure("path", "PATH_LIMIT"));
        }

        if (sameSpelling(source, target)) {
            return std::unexpected(failure("path", "INPUT_OUTPUT_ALIAS"));
        }

        const auto status = std::filesystem::symlink_status(target, error);

        if (error && error != std::errc::no_such_file_or_directory) {
            return std::unexpected(failure("path", "OUTPUT_STATUS_FAILED"));
        }

        if (status.type() != std::filesystem::file_type::not_found) {
            error.clear();
            const auto equivalent = std::filesystem::equivalent(source, target, error);

            return std::unexpected(failure("path", equivalent && !error ? "INPUT_OUTPUT_ALIAS" : "OUTPUT_EXISTS"));
        }

        return FilePaths{std::move(source), std::move(target)};
    } catch (const std::bad_alloc &) {
        return std::unexpected(failure("path", "ALLOCATION_FAILED"));
    } catch (const std::filesystem::filesystem_error &) {
        return std::unexpected(failure("path", "INVALID_PATH"));
    }
}

std::expected<std::vector<std::byte>, FileDiagnostic> readInput(const FilePaths &paths, FileLimits limits,
                                                                FileOperations operations) {
    if (!validOperations(operations)) {
        return std::unexpected(failure("input-open", "INVALID_OPERATIONS"));
    }

    if (limits.workUnits == 0) {
        return std::unexpected(failure("input-open", "WORK_LIMIT"));
    }

    const auto opened = operations.open(operations.context, paths.input, false);

    if (!opened) {
        return std::unexpected(failure("input-open", "OPEN_FAILED"));
    }

    std::size_t work = 1;
    std::vector<std::byte> bytes;
    std::array<std::byte, IO_CHUNK_BYTES> buffer;
    FileDiagnostic diagnostic;

    try {
        while (true) {
            if (work == limits.workUnits) {
                diagnostic = failure("input-read", "WORK_LIMIT");

                break;
            }

            ++work;
            const auto read = operations.read(operations.context, *opened, buffer);

            if (!read || *read > buffer.size()) {
                diagnostic = failure("input-read", "READ_FAILED");

                break;
            }

            if (*read == 0) {
                break;
            }

            if (*read > limits.inputBytes - bytes.size()) {
                diagnostic = failure("input-read", "INPUT_BYTE_LIMIT");

                break;
            }

            if (*read > limits.workUnits - work) {
                diagnostic = failure("input-read", "WORK_LIMIT");

                break;
            }

            work += *read;
            const auto required = bytes.size() + *read;

            if (required > bytes.capacity()) {
                if (bytes.size() > limits.workUnits - work) {
                    diagnostic = failure("input-read", "WORK_LIMIT");

                    break;
                }

                work += bytes.size();
                const auto capacity = bytes.capacity();
                const auto doubled = capacity >= limits.inputBytes || capacity > limits.inputBytes - capacity
                                         ? limits.inputBytes
                                         : capacity * 2;
                bytes.reserve(std::max(required, doubled));
            }

            bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(*read));
        }
    } catch (const std::bad_alloc &) {
        diagnostic = failure("input-read", "ALLOCATION_FAILED");
    } catch (const std::length_error &) {
        diagnostic = failure("input-read", "INPUT_BYTE_LIMIT");
    }

    const auto close = operations.close(operations.context, *opened);

    if (!diagnostic.code.empty()) {
        return std::unexpected(std::move(diagnostic));
    }

    if (work == limits.workUnits) {
        return std::unexpected(failure("input-close", "WORK_LIMIT"));
    }

    if (close) {
        return std::unexpected(failure("input-close", "CLOSE_FAILED"));
    }

    return bytes;
}

OutputFile::OutputFile(FileHandle handle, std::filesystem::path temporary, std::filesystem::path destination,
                       FileLimits limits, FileOperations operations) noexcept
    : handle_(handle), temporary_(std::move(temporary)), destination_(std::move(destination)), operations_(operations),
      limits_(limits), owned_(true) {
}

OutputFile::OutputFile(OutputFile &&other) noexcept
    : handle_(std::exchange(other.handle_, -1)), temporary_(std::move(other.temporary_)),
      destination_(std::move(other.destination_)), operations_(other.operations_), limits_(other.limits_),
      work_(other.work_), published_(other.published_), owned_(std::exchange(other.owned_, false)),
      failed_(other.failed_) {
}

OutputFile::~OutputFile() {
    if (handle_ != -1) {
        operations_.close(operations_.context, std::exchange(handle_, -1));
    }

    if (owned_) {
        operations_.remove(operations_.context, temporary_);
    }
}

bool OutputFile::charge(std::size_t units) noexcept {
    if (units > limits_.workUnits - work_) {
        return false;
    }

    work_ += units;

    return true;
}

std::expected<OutputFile, FileDiagnostic> OutputFile::create(const FilePaths &paths, FileLimits limits,
                                                             FileOperations operations) {
    if (!validOperations(operations)) {
        return std::unexpected(failure("output-create", "INVALID_OPERATIONS"));
    }

    try {
        auto destination = paths.destination;
        const auto parent = destination.parent_path();
#ifdef _WIN32
        const auto processId = GetCurrentProcessId();
#else
        const auto processId = ::getpid();
#endif

        for (std::size_t attempt = 0; attempt < TEMPORARY_ATTEMPTS; ++attempt) {
            if (attempt >= limits.workUnits) {
                return std::unexpected(failure("output-create", "WORK_LIMIT"));
            }

            auto temporary =
                parent / (".qdcxx-tape-write-" + std::to_string(processId) + "-" +
                          std::to_string(temporarySequence.fetch_add(1, std::memory_order_relaxed)) + ".tmp");

            if (temporary.native().size() > limits.pathUnits || destination.native().size() > limits.pathUnits) {
                return std::unexpected(failure("output-create", "PATH_LIMIT"));
            }

            const auto opened = operations.open(operations.context, temporary, true);

            if (opened) {
                OutputFile owner(*opened, std::move(temporary), std::move(destination), limits, operations);
                owner.work_ = attempt + 1;

                return owner;
            }

            if (opened.error() != std::errc::file_exists) {
                return std::unexpected(failure("output-create", "CREATE_FAILED"));
            }
        }

        return std::unexpected(failure("output-create", "EXCLUSIVE_COLLISION"));
    } catch (const std::bad_alloc &) {
        return std::unexpected(failure("output-create", "ALLOCATION_FAILED"));
    }
}

FileDiagnostic OutputFile::discard(FileDiagnostic primary) {
    failed_ = true;
    primary.published = published_;

    if (handle_ != -1) {
        operations_.close(operations_.context, std::exchange(handle_, -1));
    }

    if (std::exchange(owned_, false) && operations_.remove(operations_.context, temporary_)) {
        primary.cleanupFailed = true;
        primary.ownedTemporary = std::move(temporary_);
    }

    return primary;
}

std::expected<void, FileDiagnostic> OutputFile::write(std::span<const std::byte> bytes) {
    if (failed_ || !owned_ || handle_ == -1) {
        return std::unexpected(failure("output-write", "INVALID_STATE"));
    }

    while (!bytes.empty()) {
        const auto chunk = bytes.first(std::min(bytes.size(), IO_CHUNK_BYTES));

        if (!charge(1)) {
            return std::unexpected(discard(failure("output-write", "WORK_LIMIT")));
        }

        const auto written = operations_.write(operations_.context, handle_, chunk);

        if (!written || *written > chunk.size()) {
            return std::unexpected(discard(failure("output-write", "WRITE_FAILED")));
        }

        if (*written == 0) {
            return std::unexpected(discard(failure("output-write", "ZERO_PROGRESS")));
        }

        if (!charge(*written)) {
            return std::unexpected(discard(failure("output-write", "WORK_LIMIT")));
        }

        bytes = bytes.subspan(*written);
    }

    return {};
}

std::expected<void, FileDiagnostic> OutputFile::finishPublish() {
    if (failed_ || !owned_ || handle_ == -1) {
        return std::unexpected(failure("output-finish", "INVALID_STATE"));
    }

    if (!charge(1)) {
        return std::unexpected(discard(failure("output-flush", "WORK_LIMIT")));
    }

    if (operations_.flush(operations_.context, handle_)) {
        return std::unexpected(discard(failure("output-flush", "FLUSH_FAILED")));
    }

    if (!charge(1)) {
        return std::unexpected(discard(failure("output-close", "WORK_LIMIT")));
    }

    if (operations_.close(operations_.context, std::exchange(handle_, -1))) {
        return std::unexpected(discard(failure("output-close", "CLOSE_FAILED")));
    }

    if (!charge(1)) {
        return std::unexpected(discard(failure("output-link", "WORK_LIMIT")));
    }

    if (operations_.link(operations_.context, temporary_, destination_)) {
        return std::unexpected(discard(failure("output-link", "LINK_FAILED")));
    }

    published_ = true;

    if (!charge(1)) {
        return std::unexpected(discard(failure("output-cleanup", "WORK_LIMIT")));
    }

    auto diagnostic = discard(failure("output-cleanup", "REMOVE_FAILED"));

    if (diagnostic.cleanupFailed) {
        return std::unexpected(std::move(diagnostic));
    }

    return {};
}
} // namespace org::ttldtor::qdcxx::tape_write
