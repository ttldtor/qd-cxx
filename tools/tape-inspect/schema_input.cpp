// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "schema_input.hpp"

#include <algorithm>
#include <array>
#include <istream>
#include <new>
#include <stdexcept>

namespace org::ttldtor::qdcxx::tape_inspect {

std::expected<std::string, SchemaInputError> readSchemaDocument(std::istream &input, std::size_t maximumBytes) {
    try {
        std::string document;
        std::array<char, 4096> buffer{}; // Fixed acquisition scratch, independent of input size.

        while (document.size() < maximumBytes) {
            const auto request = std::min(buffer.size(), maximumBytes - document.size());
            input.read(buffer.data(), static_cast<std::streamsize>(request));

            if (input.bad() || (input.fail() && !input.eof())) {
                return std::unexpected(SchemaInputError::IO_ERROR);
            }

            document.append(buffer.data(), static_cast<std::size_t>(input.gcount()));

            if (input.eof()) {
                return document;
            }
        }

        const auto next = input.peek();

        if (input.bad() || (input.fail() && !input.eof())) {
            return std::unexpected(SchemaInputError::IO_ERROR);
        }

        if (next != std::char_traits<char>::eof()) {
            return std::unexpected(SchemaInputError::RESOURCE_LIMIT);
        }

        return document;
    } catch (const std::bad_alloc &) {
        return std::unexpected(SchemaInputError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(SchemaInputError::ALLOCATION_FAILURE);
    } catch (const std::ios_base::failure &) {
        return std::unexpected(SchemaInputError::IO_ERROR);
    }
}

} // namespace org::ttldtor::qdcxx::tape_inspect
