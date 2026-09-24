// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace org::ttldtor::qdcxx::dev {

/** Receives raw bytes, or an empty view once a stream reaches EOF; stream is 0 or 1. */
using StreamSink = std::function<void(int, std::string_view)>;

/** Owns an unbuffered native log destination, sharing the active process cleanup deadline. */
class LogFile {
    std::intptr_t handle_ = -1;

    public:
    /** Opens or truncates the selected log without introducing a userspace output buffer.
     * @param path Native filesystem path; POSIX FIFO opens require an existing reader.
     */
    explicit LogFile(const std::filesystem::path &path);
    /** Closes without retrying a failed buffered write. */
    ~LogFile();
    /** Forbids copying ownership.
     * @param other Existing owner.
     */
    LogFile(const LogFile &other) = delete;
    /** Forbids assigning ownership.
     * @param other Existing owner.
     * @return Not callable.
     */
    LogFile &operator=(const LogFile &other) = delete;
    /** Writes all bytes to the native destination, or throws on failure/deadline expiry.
     * @param bytes Exact UTF-8 bytes, including their already-selected LF terminator.
     */
    void write(std::string_view bytes);
    /** Closes the destination and reports any native close failure. */
    void close();
};

/** Runs a command in pre-established ownership and drains its two pipes independently.
 * Root exit or sink failure triggers bounded owned cleanup, independent of pipe EOF.
 * @param command UTF-8 executable and exact arguments, with at least one element.
 * @param sink Synchronous stream receiver; exceptions request cleanup and propagate.
 * @param complete Optional final output callback; runs with the same cleanup deadline still active.
 * @return Normal exit status, or 128 plus a POSIX terminating signal.
 * @throws std::runtime_error On launch, cancellation, pipe or five-second cleanup failure.
 */
int runOwned(const std::vector<std::string> &command, const StreamSink &sink,
             const std::function<void(int)> &complete = {});

/** Writes all bytes directly to standard output without newline or encoding conversion.
 * @param bytes UTF-8 bytes to write.
 * @throws std::runtime_error On a failed or short destination write.
 */
void writeConsole(std::string_view bytes);

/** Attempts one stderr diagnostic for at most 100 milliseconds before immediate process exit.
 * A stalled writer owns all its data and touches no stdio locks; process exit discards it.
 * Call only immediately before returning the final launcher status.
 * @param prefix Static diagnostic prefix.
 * @param bytes Diagnostic bytes; loss or truncation does not change the authoritative exit code.
 */
void writeDiagnostic(std::string_view prefix, std::string_view bytes) noexcept;

#ifdef _WIN32
/** Converts native Unicode arguments independently of the active Windows code page.
 * @param value UTF-16 text.
 * @return Exact UTF-8 encoding; malformed UTF-16 is rejected.
 */
std::string toUtf8(std::wstring_view value);
#endif

} // namespace org::ttldtor::qdcxx::dev
