// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace event_time_fixture {
namespace core = org::ttldtor::qdcxx::core;

/** One complete actual Java cursor and delegate observation. */
struct Sample {
    std::u16string symbol;                ///< Physical cursor symbol, owned independently of the input stream.
    std::int64_t rawEventTime = 0;        ///< Original timestamp sequence, independent of business fields.
    std::vector<core::FieldValue> values; ///< Complete logical values, excluding physical VOID tails.
    std::vector<std::string> expected;    ///< Original production delegate result cells.
};

/** One bounded standard schema variant and its original production observations. */
struct Scenario {
    std::vector<core::LocalRecord> records; ///< Exact generated names, local types and lookup keys.
    std::map<std::pair<std::size_t, std::size_t>, Sample> samples; ///< Record/sample identities and owned values.
    std::vector<std::size_t> rejected;                ///< Records for which the actual factory refused a delegate.
    std::vector<std::vector<std::string>> routes;     ///< Actual delegate physical/logical conversion cells.
    std::vector<std::vector<std::string>> selections; ///< Actual delegate-set selected record and physical symbol.
};

/** Decode exact hexadecimal UTF-16 without normalization, preserving unpaired units.
 * @param text Oracle units, or '-' for an engaged empty string.
 * @return Independently owned UTF-16 text; malformed input throws.
 */
inline std::u16string units(const std::string &text) {
    if (text == "-") {
        return {};
    }

    if (text.size() % 4 != 0) {
        throw std::runtime_error("Invalid oracle UTF-16");
    }

    std::u16string result;

    for (std::size_t offset = 0; offset < text.size(); offset += 4) {
        result.push_back(static_cast<char16_t>(std::stoul(text.substr(offset, 4), nullptr, 16)));
    }

    return result;
}

/** Split one raw TSV observation without trimming its cells.
 * @param line Complete LF-delimited oracle line.
 * @return Owned cells, including any empty final cell.
 */
inline std::vector<std::string> cells(const std::string &line) {
    std::vector<std::string> result;
    std::size_t start = 0;

    for (;;) {
        const auto end = line.find('\t', start);
        result.push_back(line.substr(start, end == std::string::npos ? end : end - start));

        if (end == std::string::npos) {
            return result;
        }

        start = end + 1;
    }
}

/** Read the finite checked-in corpus; this test helper performs no schema inference or network work.
 * @param path Explicit raw production oracle TSV.
 * @return Independently owned scenarios; malformed, missing or excessive inputs throw.
 */
inline std::map<std::string, Scenario> read(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        throw std::runtime_error("Cannot open event-time oracle");
    }

    std::map<std::string, Scenario> result;
    std::string line;
    std::size_t lines = 0;

    while (std::getline(input, line)) {
        if (++lines > 10000 || line.size() > 8192) {
            throw std::runtime_error("Event-time oracle exceeds finite test input bounds");
        }

        const auto row = cells(line);
        auto &scenario = result[row.at(1)];
        const auto recordId = std::stoull(row.at(2));

        if (recordId >= 16) {
            throw std::runtime_error("Event-time oracle record exceeds receiving profile");
        }

        if (row[0] == "record") {
            if (recordId != scenario.records.size()) {
                throw std::runtime_error("Event-time oracle record order differs");
            }

            scenario.records.push_back({static_cast<std::int32_t>(recordId),
                                        std::u16string(row.at(3).begin(), row.at(3).end()),
                                        false,
                                        {},
                                        {}});
        } else if (row[0] == "field") {
            auto &record = scenario.records.at(recordId);
            const auto fieldId = std::stoull(row.at(3));

            if (fieldId != record.fields.size() || fieldId >= 128) {
                throw std::runtime_error("Event-time oracle field order differs");
            }

            record.fields.push_back({std::stoi(row.at(4))});
            record.lookups.push_back({std::u16string(row.at(5).begin(), row.at(5).end()), fieldId});

            if (row.at(5) != row.at(6)) {
                record.lookups.push_back({std::u16string(row.at(6).begin(), row.at(6).end()), fieldId});
            }
        } else if (row[0] == "route") {
            scenario.routes.emplace_back(row.begin() + 2, row.end());
        } else if (row[0] == "selection") {
            scenario.selections.emplace_back(row.begin() + 2, row.end());
        } else if (row[0] == "reject") {
            scenario.rejected.push_back(recordId);
        } else {
            const auto sampleId = std::stoull(row.at(3));

            if (sampleId >= 16) {
                throw std::runtime_error("Event-time oracle sample exceeds finite bound");
            }

            auto &sample = scenario.samples[{recordId, sampleId}];

            if (row[0] == "input") {
                sample.symbol = units(row.at(4));
                sample.rawEventTime = std::stoll(row.at(5));
            } else if (row[0] == "value") {
                const auto fieldId = std::stoull(row.at(4));

                if (fieldId != sample.values.size()) {
                    throw std::runtime_error("Event-time oracle value order differs");
                }

                sample.values.push_back(
                    {static_cast<core::SerialFieldType>(scenario.records.at(recordId).fields.at(fieldId).serialTypeId),
                     std::stoll(row.at(5))});

                if (row.at(6) != "null") {
                    sample.values.back().units = units(row[6]);
                }
            } else if (row[0] == "quote" || row[0] == "profile") {
                sample.expected.assign(row.begin() + 4, row.end());
            } else {
                throw std::runtime_error("Unknown event-time oracle row");
            }
        }
    }

    if (!input.eof() || result.size() != 9 || lines != 2152) {
        throw std::runtime_error("Event-time oracle inventory differs");
    }

    return result;
}

/** Read the separate production delegate roundtrip observations without changing the original time corpus.
 * @param path Explicit four-row companion TSV.
 * @return Original owned cells; malformed or unavailable input throws before any consumer starts.
 */
inline std::vector<std::vector<std::string>> representations(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    std::vector<std::vector<std::string>> result;
    std::string line;

    while (std::getline(input, line)) {
        auto row = cells(line);

        if (row.size() != 5 || row[0] != "roundtrip" || result.size() == 4 || line.size() > 4096) {
            throw std::runtime_error("Invalid regional representability fixture");
        }

        result.push_back(std::move(row));
    }

    if (!input.eof() || result.size() != 4) {
        throw std::runtime_error("Regional representability fixture inventory differs");
    }

    return result;
}
} // namespace event_time_fixture
