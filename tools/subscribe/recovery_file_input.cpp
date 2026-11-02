// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_file_input.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace org::ttldtor::qdcxx::connect {
namespace {
constexpr std::size_t MAXIMUM_INPUT_BYTES = 1024 * 1024;  ///< Largest selected application document, not SDK storage.
constexpr std::size_t MAXIMUM_PATH_BYTES = 4096;          ///< Explicit UTF-8 argument cap, before native conversion.
constexpr std::size_t MAXIMUM_RESOLVED_PATH_UNITS = 4096; ///< Existing canonical native-path ceiling.

/** Validate UTF-8 scalars consistently before platform-specific filesystem conversion.
 * @param text Borrowed bounded path bytes.
 * @return True for complete shortest-form UTF-8, excluding NUL, surrogates and values above U+10FFFF.
 */
bool validUtf8Path(std::string_view text) noexcept {
    for (std::size_t offset = 0; offset < text.size();) {
        const auto lead = static_cast<unsigned char>(text[offset++]);

        if (lead == 0)
            return false;

        if (lead < 0x80)
            continue;

        std::size_t trailing = 0;
        std::uint32_t scalar = 0;
        std::uint32_t minimum = 0;

        if (lead >= 0xc2 && lead <= 0xdf) {
            trailing = 1;
            scalar = lead & 0x1f;
            minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            trailing = 2;
            scalar = lead & 0x0f;
            minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            trailing = 3;
            scalar = lead & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }

        if (trailing > text.size() - offset)
            return false;

        for (std::size_t index = 0; index < trailing; ++index) {
            const auto byte = static_cast<unsigned char>(text[offset++]);

            if ((byte & 0xc0) != 0x80)
                return false;

            scalar = (scalar << 6) | (byte & 0x3f);
        }

        if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
            return false;
    }

    return true;
}
} // namespace

std::expected<std::string, std::string_view> readRecoveryFile(std::string_view pathBytes, std::size_t maximumBytes) {
    if (maximumBytes == 0 || maximumBytes > MAXIMUM_INPUT_BYTES)
        return std::unexpected("recovery input byte ceiling is invalid");

    if (pathBytes.empty() || pathBytes.size() > MAXIMUM_PATH_BYTES || !validUtf8Path(pathBytes))
        return std::unexpected("recovery input path is invalid");

    try {
        std::u8string utf8Path;
        utf8Path.reserve(pathBytes.size());

        for (const unsigned char byte : pathBytes)
            utf8Path.push_back(static_cast<char8_t>(byte));

        const std::filesystem::path inputPath(utf8Path);
        std::error_code error;
        const auto path = std::filesystem::canonical(inputPath, error);

        if (error || path.native().size() > MAXIMUM_RESOLVED_PATH_UNITS ||
            !std::filesystem::is_regular_file(path, error) || error)
            return std::unexpected("recovery input must resolve to an ordinary regular file");

        const auto bytes = std::filesystem::file_size(path, error);

        if (error || bytes > maximumBytes)
            return std::unexpected("recovery input exceeds its byte ceiling or cannot be sized");

        const auto modified = std::filesystem::last_write_time(path, error);

        if (error)
            return std::unexpected("recovery input metadata cannot be read");

        std::ifstream input(path, std::ios::binary);

        if (!input)
            return std::unexpected("recovery input cannot be opened");

        // One extra byte detects growth past the preflight size, even when still below the cap.
        // Both addition and streamsize conversion are safe because bytes <= 1 MiB.
        std::string document(static_cast<std::size_t>(bytes) + 1, '\0');
        input.read(document.data(), static_cast<std::streamsize>(document.size()));
        const auto count = input.gcount();

        if (input.bad() || (input.fail() && !input.eof()) || count < 0)
            return std::unexpected("recovery input read failed");

        if (static_cast<std::uintmax_t>(count) != bytes)
            return std::unexpected("recovery input changed during preparation");

        input.clear();
        input.close();

        if (input.fail())
            return std::unexpected("recovery input close failed");

        const auto finalBytes = std::filesystem::file_size(path, error);

        if (error || finalBytes != bytes || !std::filesystem::is_regular_file(path, error) || error)
            return std::unexpected("recovery input changed during preparation");

        const auto finalModified = std::filesystem::last_write_time(path, error);

        if (error || finalModified != modified)
            return std::unexpected("recovery input changed during preparation");

        document.resize(static_cast<std::size_t>(count));

        if (document.find('\0') != std::string::npos)
            return std::unexpected("recovery input contains a forbidden NUL byte");

        return document;
    } catch (...) {
        return std::unexpected("recovery input preparation failed");
    }
}

} // namespace org::ttldtor::qdcxx::connect
