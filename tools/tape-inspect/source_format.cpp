// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "source_format.hpp"
#include "../qtp-inspect/format_row.hpp"
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace org::ttldtor::qdcxx::tape_inspect {
namespace {
using inspect::detail::build;
using inspect::detail::Row;
constexpr std::size_t TOKEN_BYTES = 64;

/** Validate bounded static grammar tokens before any row allocation. */
bool token(std::string_view value) {
    if (value.empty() || value.size() > TOKEN_BYTES) {
        return false;
    }

    for (const auto c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }

    return true;
}

/** Return only closed grammar names; no caller text is emitted. */
std::string_view policyName(SourceTimePolicy policy) {
    switch (policy) {
    case SourceTimePolicy::NONE:
        return "none";
    case SourceTimePolicy::MESSAGE:
        return "message";
    case SourceTimePolicy::FIELD:
        return "field";
    case SourceTimePolicy::LONG:
        return "long";
    case SourceTimePolicy::TEXT:
        return "text";
    }

    throw std::invalid_argument("invalid source time policy");
}

/** Return an explicitly selected envelope name. */
std::string_view containerName(ContainerKind kind) {
    switch (kind) {
    case ContainerKind::NONE:
        return "none";
    case ContainerKind::GZIP:
        return "gzip";
    case ContainerKind::ZIP:
        return "zip";
    }

    throw std::invalid_argument("invalid source container");
}

/** Append a cell with unknown represented separately from zero. */
template <typename T> void optionalCell(Row &row, const std::optional<T> &value) {
    row.add("\t");

    if (value) {
        row.number(*value);
    } else {
        row.add("-");
    }
}

/** Encode native units in little-endian byte order, independent of host byte order. */
void nativePath(Row &row, const std::filesystem::path &path) {
    constexpr std::string_view DIGITS = "0123456789abcdef";
    using Unit = std::filesystem::path::value_type;
    using UnsignedUnit = std::make_unsigned_t<Unit>;

    for (const auto unit : path.native()) {
        const auto value = static_cast<UnsignedUnit>(unit);

        for (std::size_t i = 0; i < sizeof(Unit); ++i) {
            const auto byte = static_cast<unsigned>((value >> (i * 8)) & 0xff);
            const std::array<char, 2> pair{DIGITS[byte >> 4], DIGITS[byte & 15]};
            row.add({pair.data(), pair.size()});
        }
    }
}

/** Compose fixed counters with no incidental allocation. */
void summaryCells(Row &row, const SequenceSummary &value) {
    for (const auto number :
         {value.sources, value.accepted, value.frames, value.emitted, value.processed, value.wireFields}) {
        row.add("\t");
        row.number(number);
    }
}
} // namespace

std::string sourceHeader(std::size_t budget) {
    return build(budget, [](Row &row) {
        row.add("format\tqdcxx-tape-inspect\t9\tbinary-catalog-v1\tself-contained-segments-v1\n");
    });
}

std::string sourcePolicy(SourceTimePolicy policy, ContainerKind data, std::optional<ContainerKind> time,
                         std::size_t budget) {
    const auto policyText = policyName(policy);
    const auto dataText = containerName(data);
    const auto timeText = time ? containerName(*time) : "-";

    return build(budget, [&](Row &row) {
        row.add("source-policy\t");
        row.add(policyText);
        row.add("\t");
        row.add(dataText);
        row.add("\t");
        row.add(timeText);
        row.add("\n");
    });
}

std::string sourceIdentity(std::size_t ordinal, const std::filesystem::path &path,
                           const std::filesystem::path *companion, std::size_t budget) {
    return build(budget, [&](Row &row) {
        row.add("source\t");
        row.number(ordinal);
        row.add(sizeof(std::filesystem::path::value_type) == 2 ? "\tutf16le\t" : "\tbytes\t");
        nativePath(row, path);
        row.add("\t");

        if (companion) {
            nativePath(row, *companion);
        } else {
            row.add("-");
        }

        row.add("\n");
    });
}

std::optional<std::size_t> sourceIdentityBytes(std::size_t ordinal, const std::filesystem::path &path,
                                               const std::filesystem::path *companion) noexcept {
    constexpr auto MAX = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t NATIVE_HEX_BYTES = sizeof(std::filesystem::path::value_type) * 2;
    std::size_t digits = 1;

    for (auto remaining = ordinal; remaining >= 10; remaining /= 10) {
        ++digits;
    }

    const std::string_view encoding = sizeof(std::filesystem::path::value_type) == 2 ? "utf16le" : "bytes";
    std::size_t size = std::string_view("source").size() + 4 + digits + encoding.size() + 1;

    for (const auto *selected : {&path, companion}) {
        if (!selected) {
            if (size == MAX) {
                return std::nullopt;
            }

            ++size;
            continue;
        }

        if (selected->native().size() > (MAX - size) / NATIVE_HEX_BYTES) {
            return std::nullopt;
        }

        size += selected->native().size() * NATIVE_HEX_BYTES;
    }

    return size;
}

std::string sourceBegin(std::size_t ordinal, std::size_t budget) {
    return build(budget, [&](Row &row) {
        row.add("source-begin\t");
        row.number(ordinal);
        row.add("\n");
    });
}

std::string sourceEnd(const SourceEnd &counters, std::size_t budget) {
    return build(budget, [&](Row &row) {
        row.add("source-end\t");
        row.number(counters.sourceOrdinal);

        for (const auto value : {counters.accepted, counters.frames, counters.emitted, counters.processed,
                                 counters.wireFields, counters.physicalFetched, counters.physicalConsumed}) {
            row.add("\t");
            row.number(value);
        }

        row.add("\n");
    });
}

std::string sourceComplete(const SequenceSummary &counters, std::size_t budget) {
    return build(budget, [&](Row &row) {
        row.add("complete-v9");
        summaryCells(row, counters);
        row.add("\n");
    });
}

std::string sourceResource(std::string_view scope, std::optional<std::size_t> ordinal, std::string_view name,
                           std::uint64_t reserved, std::optional<std::uint64_t> observed, std::size_t budget) {
    if ((scope != "sequence" && scope != "data" && scope != "time" && scope != "scanner" && scope != "parser") ||
        !token(name)) {
        throw std::invalid_argument("invalid resource token");
    }

    return build(budget, [&](Row &row) {
        row.add("resource\t");
        row.add(scope);
        optionalCell(row, ordinal);
        row.add("\t");
        row.add(name);
        row.add("\t");
        row.number(reserved);
        optionalCell(row, observed);
        row.add("\n");
    });
}

std::string sourceDiagnostic(const SourceDiagnostic &diagnostic, bool cleanup, std::size_t budget) {
    if (diagnostic.exitCode <= 0 || !token(diagnostic.stage) || !token(diagnostic.staticReason) ||
        (diagnostic.layer != "data" && diagnostic.layer != "time" && diagnostic.layer != "parser" &&
         diagnostic.layer != "sequence" && diagnostic.layer != "sink")) {
        throw std::invalid_argument("invalid source diagnostic");
    }

    return build(budget, [&](Row &row) {
        row.add(cleanup ? "cleanup-v3\t" : "incomplete-v3\t");
        row.number(diagnostic.exitCode);
        row.add("\t");
        row.add(diagnostic.stage);
        row.add("\t");
        row.add(diagnostic.staticReason);
        optionalCell(row, diagnostic.sourceOrdinal);
        row.add("\t");
        row.add(diagnostic.layer);
        optionalCell(row, diagnostic.physicalFetched);
        optionalCell(row, diagnostic.physicalConsumed);
        optionalCell(row, diagnostic.logicalProduced);
        optionalCell(row, diagnostic.parserAccepted);
        optionalCell(row, diagnostic.frameStart);
        optionalCell(row, diagnostic.payloadStart);
        optionalCell(row, diagnostic.sidecarLine);
        row.add("\n");
    });
}

std::optional<std::size_t> terminalReserveWorstCase(std::size_t resourceRows, std::size_t diagnosticRows) noexcept {
    constexpr auto SIZE_MAXIMUM = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t U64_DIGITS = std::numeric_limits<std::uint64_t>::digits10 + 1;
    constexpr std::size_t SIZE_DIGITS = std::numeric_limits<std::size_t>::digits10 + 1;
    constexpr std::size_t INT_DIGITS = std::numeric_limits<int>::digits10 + 1;
    // Labels, tabs and LF are counted separately from maximum-width values. No allocation occurs.
    constexpr std::size_t summaryBytes = std::string_view("complete-v9").size() + 6 + 6 * U64_DIGITS + 1;
    constexpr std::size_t resourceBytes = std::string_view("resource").size() + 5 +
                                          std::string_view("sequence").size() + SIZE_DIGITS + TOKEN_BYTES +
                                          2 * U64_DIGITS + 1;
    constexpr std::size_t diagnosticBytes = std::string_view("incomplete-v3").size() + 12 + INT_DIGITS +
                                            2 * TOKEN_BYTES + SIZE_DIGITS + std::string_view("sequence").size() +
                                            7 * U64_DIGITS + 1;

    if (resourceRows > (SIZE_MAXIMUM - summaryBytes) / resourceBytes) {
        return std::nullopt;
    }

    const auto partial = summaryBytes + resourceRows * resourceBytes;

    if (diagnosticRows > (SIZE_MAXIMUM - partial) / diagnosticBytes) {
        return std::nullopt;
    }

    return partial + diagnosticRows * diagnosticBytes;
}
} // namespace org::ttldtor::qdcxx::tape_inspect
