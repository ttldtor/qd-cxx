// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

namespace org::ttldtor::qdcxx::tape_write {
/** Finite physical-file storage and operation limits, independent of parser counters. */
struct FileLimits {
    std::size_t inputBytes{16777216}; ///< Maximum owned completed input bytes.
    std::size_t workUnits{67108864};  ///< One unit per transferred/copied byte and attempted operation, per owner.
    std::size_t pathUnits{4096};      ///< Maximum native code units in each retained resolved path.
};

/** Content-free primary failure plus bounded information about failed owned-file cleanup. */
struct FileDiagnostic {
    std::string_view stage;               ///< Static stage identifying the first failed operation.
    std::string_view code;                ///< Static failure token, never input contents.
    bool published{};                     ///< Whether a fully closed destination was successfully linked.
    bool cleanupFailed{};                 ///< Whether removing the exclusively owned temporary failed.
    std::filesystem::path ownedTemporary; ///< Exact bounded owned path, present only on failed cleanup.
};

/** Resolved paths after ordinary alias and pre-existing destination refusal. */
struct FilePaths {
    std::filesystem::path input;       ///< Existing canonical regular input file.
    std::filesystem::path destination; ///< Canonical existing parent plus fresh final entry.
};

/** Native owned file handle; -1 denotes no ownership on both supported platforms. */
using FileHandle = std::intptr_t;

/** Narrow physical operations used by production and fault-injection tests.
 * Callbacks never throw. A successful open transfers one handle. Close consumes that handle even
 * when reporting an error; callers never retry close. Context is borrowed until every owner dies.
 */
struct FileOperations {
    void *context{}; ///< Borrowed context, not retained beyond the owning boundary's lifetime.
    std::expected<FileHandle, std::error_code> (*open)(void *, const std::filesystem::path &, bool) noexcept {};
    ///< Open read-only, or create a new write-only file exclusively when the last argument is true.
    std::expected<std::size_t, std::error_code> (*read)(void *, FileHandle, std::span<std::byte>) noexcept {};
    ///< Return transferred bytes; zero authenticates physical read EOF.
    std::expected<std::size_t, std::error_code> (*write)(void *, FileHandle, std::span<const std::byte>) noexcept {};
    ///< Return transferred bytes; zero progress is an error to the owning loop.
    std::error_code (*flush)(void *, FileHandle) noexcept {}; ///< Check native file flush.
    std::error_code (*close)(void *, FileHandle) noexcept {}; ///< Consume handle exactly once, including failure.
    std::error_code (*link)(void *, const std::filesystem::path &, const std::filesystem::path &) noexcept {};
    ///< Create a hard link exclusively, without overwrite or rename fallback.
    std::error_code (*remove)(void *, const std::filesystem::path &) noexcept {};
    ///< Remove only an exclusively owned temporary entry.
};

/** Select actual Windows/POSIX operations, with no global fault state.
 * @return Complete stateless native operation table.
 */
FileOperations productionFileOperations() noexcept;

/** Resolve bounded paths and reject input aliases and every existing destination entry.
 * @param input Caller-supplied input filename.
 * @param destination Caller-supplied fresh output filename.
 * @param limits Finite path bounds.
 * @return Owning resolved paths or a content-free refusal; no files are opened or created.
 */
std::expected<FilePaths, FileDiagnostic> resolvePaths(const std::filesystem::path &input,
                                                      const std::filesystem::path &destination, FileLimits limits = {});

/** Read a bounded complete source, authenticating physical EOF and checked close.
 * @param paths Resolved input; no destination is created here.
 * @param limits Owned-byte and independently charged read-work limits.
 * @param operations Borrowed native/fault operation table; context must outlive the call.
 * @return Complete owned bytes only after successful EOF and close, or the first failure.
 */
std::expected<std::vector<std::byte>, FileDiagnostic> readInput(const FilePaths &paths, FileLimits limits = {},
                                                                FileOperations operations = productionFileOperations());

/** Move-only owner of one exclusively created same-directory temporary.
 * A destructor makes best-effort cleanup only; finishPublish is required for checked completion.
 */
class OutputFile {
    FileHandle handle_{-1};
    std::filesystem::path temporary_;
    std::filesystem::path destination_;
    FileOperations operations_;
    FileLimits limits_;
    std::size_t work_{};
    bool published_{};
    bool owned_{};
    bool failed_{};

    /** Construct after exclusive creation without further path allocation.
     * @param handle Transferred native handle.
     * @param temporary Transferred exclusively owned name.
     * @param destination Transferred fresh destination name.
     * @param limits Finite operation limits.
     * @param operations Borrowed operation context/table.
     */
    OutputFile(FileHandle handle, std::filesystem::path temporary, std::filesystem::path destination, FileLimits limits,
               FileOperations operations) noexcept;
    /** Charge without overflowing or refunding work.
     * @param units Additional logical operation/byte units.
     * @return Whether the charge fits.
     */
    bool charge(std::size_t units) noexcept;

    public:
    /** Transfer sole handle/name ownership.
     * @param other Owner left inactive.
     */
    OutputFile(OutputFile &&other) noexcept;
    OutputFile(const OutputFile &) = delete;
    OutputFile &operator=(const OutputFile &) = delete;
    OutputFile &operator=(OutputFile &&) = delete;
    /** Best-effort close/remove of owned temporary; never delete the destination. */
    ~OutputFile();

    /** Exclusively create one same-directory temporary after completed admission.
     * @param paths Previously resolved input/output paths.
     * @param limits Finite path and output-operation limits.
     * @param operations Borrowed actual operation boundary, including injected tests.
     * @return Sole owner or refusal; collisions never transfer ownership or authorize removal.
     */
    static std::expected<OutputFile, FileDiagnostic> create(const FilePaths &paths, FileLimits limits = {},
                                                            FileOperations operations = productionFileOperations());
    /** Write the complete borrowed chunk through checked partial-progress operations.
     * @param bytes Borrowed bytes, retained only until this call returns.
     * @return Success or first failure after owned-temp cleanup; no later writes are accepted.
     */
    std::expected<void, FileDiagnostic> write(std::span<const std::byte> bytes);
    /** Flush, consume checked close, publish create-only hard link, then remove owned temporary.
     * @return Checked completion or first failure; cleanup failure after link preserves published=true.
     */
    std::expected<void, FileDiagnostic> finishPublish();
    /** Stop an unfinished output after a caller's parser/writer failure.
     * @param primary First authoritative error, whose stage/code remain unchanged.
     * @return Primary error augmented only with owned cleanup outcome and publication state.
     */
    FileDiagnostic discard(FileDiagnostic primary);
};
} // namespace org::ttldtor::qdcxx::tape_write
