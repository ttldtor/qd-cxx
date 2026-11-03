// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/ipf/instrument_profile_reader.hpp"
#include <array>
#include <fstream>
#include <istream>
#include <limits>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace org::ttldtor::qdcxx::ipf {
namespace {
constexpr std::u16string_view FORMAT_SUFFIX = u"::=TYPE";
constexpr char16_t REPLACEMENT = 0xfffd;
constexpr unsigned char UTF8_ASCII_LIMIT = 0x80;
constexpr unsigned char UTF8_CONTINUATION_MASK = 0xc0;
constexpr unsigned char UTF8_CONTINUATION_TAG = 0x80;
constexpr unsigned char UTF8_MIN_TWO_BYTE_LEAD = 0xc2;
constexpr unsigned char UTF8_MIN_THREE_BYTE_LEAD = 0xe0;
constexpr unsigned char UTF8_MIN_FOUR_BYTE_LEAD = 0xf0;
constexpr unsigned char UTF8_MAX_LEAD = 0xf4;
constexpr unsigned char UTF8_E0_MIN_SECOND = 0xa0;
constexpr unsigned char UTF8_F0_MIN_SECOND = 0x90;
constexpr unsigned char UTF8_F4_MAX_SECOND = 0x8f;
constexpr std::uint32_t UTF16_HIGH_SURROGATE = 0xd800;
constexpr std::uint32_t UTF16_LOW_SURROGATE = 0xdc00;
constexpr std::uint32_t UTF16_LAST_SURROGATE = 0xdfff;
constexpr std::uint32_t UTF16_BMP_MAX = 0xffff;
constexpr std::uint32_t UTF16_SUPPLEMENTARY_START = 0x10000;
constexpr std::uint32_t UTF16_SURROGATE_PAYLOAD_MASK = 0x3ff;
constexpr unsigned UTF16_SURROGATE_PAYLOAD_BITS = 10;

bool fits(std::size_t used, std::size_t extra, std::size_t limit) noexcept {
    return used <= limit && extra <= limit - used;
}

bool hasWork(const IpfReadWork &work) noexcept {
    return work.inputBytes != 0 && work.decodedUnits != 0 && work.fields != 0 && work.records != 0;
}
} // namespace

struct InstrumentProfileParser::Impl {
    struct Format {
        std::vector<std::u16string> fields;
        std::size_t units = 0;
    };
    enum class Csv { START, UNQUOTED, QUOTED, AFTER_QUOTE };
    enum class Row { UNKNOWN, DATA, HEADER, COMMENT, FLUSH, COMPLETE };
    IpfReadOptions options;
    IpfReadProgress progress;
    std::optional<IpfDiagnostic> error;
    std::map<std::u16string, Format, std::less<>> formats;
    Csv csv = Csv::START;
    Row row = Row::UNKNOWN;
    std::u16string cell;
    std::u16string type;
    std::vector<std::u16string> stagedFields;
    std::size_t stagedUnits = 0;
    Format *format = nullptr;
    std::optional<InstrumentProfile> current;
    std::size_t currentUnits = 0;
    std::size_t field = 0;
    std::size_t recordUnits = 0;
    std::uint64_t line = 1;
    std::uint64_t recordLine = 1;
    std::uint64_t fieldLine = 1;
    std::uint64_t rowByte = 0;
    std::uint64_t fieldByte = 0;
    bool rowStarted = false;
    bool pendingField = false;
    bool endedRow = false;
    bool pendingRow = false;
    bool waitLf = false;
    bool previousCr = false;
    bool eof = false;
    bool finished = false;
    std::array<unsigned char, 4> utf{};
    unsigned utfSize = 0;
    unsigned utfExpected = 0;
    std::uint64_t utfByte = 0;
    std::optional<char16_t> lowSurrogate;

    explicit Impl(IpfReadOptions value) : options(value) {
    }

    IpfPosition cursor(std::uint64_t byte) const noexcept {
        auto record =
            progress.records == std::numeric_limits<std::uint64_t>::max() ? progress.records : progress.records + 1;

        return {byte, line, record, field};
    }

    void fail(IpfReadError code, std::optional<ProfileError> nested = {},
              std::optional<std::uint64_t> byte = {}) noexcept {
        if (!error) {
            auto position = cursor(byte.value_or(progress.inputBytes));

            if (code == IpfReadError::PROFILE_CONVERSION) {
                position.line = fieldLine;
            } else if (code == IpfReadError::UNDEFINED_FORMAT || code == IpfReadError::FORBIDDEN_FORMAT) {
                position.line = recordLine;
                position.field = 0;
            }

            error = IpfDiagnostic{code, position, recordLine, fieldLine, nested};
        }

        current.reset();
        cell.clear();
        stagedFields.clear();
    }

    bool newLine() noexcept {
        if (line == std::numeric_limits<std::uint64_t>::max()) {
            fail(IpfReadError::POSITION_OVERFLOW);

            return false;
        }

        ++line;

        return true;
    }

    bool acceptByte(IpfReadWork &work) noexcept {
        if (progress.inputBytes >= options.limits.inputBytes) {
            fail(IpfReadError::INPUT_LIMIT);

            return false;
        }

        if (progress.inputBytes == std::numeric_limits<std::uint64_t>::max()) {
            fail(IpfReadError::POSITION_OVERFLOW);

            return false;
        }

        --work.inputBytes;
        ++progress.inputBytes;

        return true;
    }

    void append(char16_t unit) {
        if (cell.size() >= options.limits.fieldUnits) {
            fail(IpfReadError::FIELD_LIMIT);

            return;
        }

        cell.push_back(unit);
    }

    void character(char16_t unit, std::uint64_t byte) {
        rowStarted = true;
        bool terminator = (unit == u'\r' || unit == u'\n') && csv != Csv::QUOTED;

        if (!terminator) {
            if (recordUnits >= options.limits.recordUnits) {
                fail(IpfReadError::RECORD_LIMIT, {}, byte);

                return;
            }

            ++recordUnits;
        }

        if (unit == u'"') {
            if (csv == Csv::START) {
                csv = Csv::QUOTED;
            } else if (csv == Csv::UNQUOTED) {
                fail(IpfReadError::UNQUOTED_QUOTE, {}, byte);

                return;
            } else if (csv == Csv::QUOTED) {
                csv = Csv::AFTER_QUOTE;
            } else {
                append(unit);
                csv = Csv::QUOTED;
            }
        } else if (csv != Csv::QUOTED && (unit == u',' || terminator)) {
            pendingField = true;
            endedRow = terminator;
            waitLf = unit == u'\r';
        } else if (csv == Csv::AFTER_QUOTE) {
            fail(IpfReadError::UNPAIRED_QUOTE, {}, byte);

            return;
        } else {
            append(unit);

            if (csv == Csv::START) {
                csv = Csv::UNQUOTED;
            }
        }

        if (error) {
            return;
        }

        if (unit == u'\r') {
            if (!newLine()) {
                return;
            }

            previousCr = true;
        } else if (unit == u'\n') {
            if (!previousCr && !newLine()) {
                return;
            }

            previousCr = false;
        } else {
            previousCr = false;
        }
    }

    void completeField() {
        if (field >= options.limits.fieldsPerRecord) {
            fail(IpfReadError::RECORD_LIMIT);

            return;
        }

        if (field == 0) {
            auto found = formats.find(cell);

            if (found != formats.end()) {
                if (cell.empty() && endedRow) {
                    row = Row::COMMENT;

                    return;
                }

                row = Row::DATA;
                format = &found->second;
                current.emplace(options.limits.profile);
                auto stored = current->setType(found->first);

                if (!stored) {
                    fail(IpfReadError::PROFILE_CONVERSION, stored.error(), fieldByte);

                    return;
                }

                currentUnits = found->first.size();
            } else if (cell.starts_with(u"#")) {
                if (cell.ends_with(FORMAT_SUFFIX)) {
                    row = Row::HEADER;
                    type = std::move(cell);
                    type.erase(0, 1);
                    type.resize(type.size() - FORMAT_SUFFIX.size());
                    stagedUnits = type.size();
                    stagedFields.clear();
                } else if (cell == u"##") {
                    row = Row::FLUSH;
                } else if (cell == u"##COMPLETE") {
                    row = Row::COMPLETE;
                } else {
                    row = Row::COMMENT;
                }
            } else {
                row = Row::UNKNOWN;
                type = std::move(cell);
            }

            return;
        }

        if (row == Row::HEADER) {
            // The active row budget independently bounds a complete replacement while old
            // format state remains intact. Retained-header admission is checked before commit.
            if (!fits(stagedUnits, cell.size(), options.limits.recordUnits)) {
                fail(IpfReadError::RECORD_LIMIT);

                return;
            }

            stagedUnits += cell.size();
            stagedFields.push_back(std::move(cell));
        } else if (row == Row::DATA && field <= format->fields.size() && !cell.empty()) {
            auto &name = format->fields[field - 1];
            auto previous = current->getField(name);
            auto oldUnits = previous.size() + (findField(name) == nullptr && !previous.empty() ? name.size() : 0);
            auto stored = current->setField(name, cell);

            if (!stored) {
                fail(IpfReadError::PROFILE_CONVERSION, stored.error(), fieldByte);

                return;
            }

            auto next = current->getField(name);
            auto newUnits = next.size() + (findField(name) == nullptr && !next.empty() ? name.size() : 0);
            currentUnits = currentUnits - oldUnits + newUnits;
        }
    }

    void replaceFormat() {
        if ((type.starts_with(u"#") && type.ends_with(FORMAT_SUFFIX)) || type == u"##" || type == u"##COMPLETE") {
            fail(IpfReadError::FORBIDDEN_FORMAT, {}, rowByte);

            return;
        }

        if (type.empty() && stagedFields.empty()) {
            return;
        }

        auto old = formats.find(type);

        if (old == formats.end() && formats.size() >= options.limits.formats) {
            fail(IpfReadError::FORMAT_LIMIT);

            return;
        }

        auto retained = progress.headerUnits - (old == formats.end() ? 0 : old->second.units);

        if (!fits(retained, stagedUnits, options.limits.headerUnits)) {
            fail(IpfReadError::HEADER_LIMIT);

            return;
        }

        Format replacement{std::move(stagedFields), stagedUnits};

        if (old == formats.end()) {
            formats.emplace(std::move(type), std::move(replacement));
        } else {
            old->second = std::move(replacement);
        }

        progress.headerUnits = retained + stagedUnits;
        progress.formats = formats.size();
    }

    void resetRow() noexcept {
        csv = Csv::START;
        row = Row::UNKNOWN;
        cell.clear();
        type.clear();
        stagedFields.clear();
        stagedUnits = 0;
        current.reset();
        currentUnits = 0;
        format = nullptr;
        field = 0;
        recordUnits = 0;
        rowStarted = false;
        pendingField = false;
        endedRow = false;
        pendingRow = false;
        waitLf = false;
        previousCr = false;
        rowByte = progress.inputBytes;
        fieldByte = rowByte;
        recordLine = line;
        fieldLine = line;
    }

    std::optional<IpfItem> completeRow() {
        if (progress.records >= options.limits.records) {
            fail(IpfReadError::RECORD_LIMIT);

            return {};
        }

        auto start = IpfPosition{rowByte, recordLine, progress.records + 1, 0};
        std::optional<IpfItem> item;

        if (row == Row::UNKNOWN) {
            if (!type.empty() || field != 0) {
                fail(IpfReadError::UNDEFINED_FORMAT, {}, rowByte);

                return {};
            }
        } else if (row == Row::HEADER) {
            replaceFormat();
        } else if (row == Row::DATA) {
            if (field != format->fields.size()) {
                fail(IpfReadError::FIELD_COUNT);

                return {};
            }

            if (progress.profiles >= options.limits.profiles ||
                !fits(progress.profileUnits, currentUnits, options.limits.profileUnits)) {
                fail(IpfReadError::OUTPUT_LIMIT);

                return {};
            }

            ++progress.profiles;
            progress.profileUnits += currentUnits;
            item.emplace(IpfItem{IpfItemKind::PROFILE, std::move(current), start});
        } else if (row == Row::FLUSH) {
            if (progress.flushes == std::numeric_limits<std::uint64_t>::max()) {
                fail(IpfReadError::POSITION_OVERFLOW);

                return {};
            }

            ++progress.flushes;
            item.emplace(IpfItem{IpfItemKind::FLUSH, {}, start});
        } else if (row == Row::COMPLETE) {
            if (progress.completions == std::numeric_limits<std::uint64_t>::max()) {
                fail(IpfReadError::POSITION_OVERFLOW);

                return {};
            }

            ++progress.completions;
            progress.completionSeen = true;
            item.emplace(IpfItem{IpfItemKind::COMPLETE, {}, start});
        }

        if (error) {
            return {};
        }

        ++progress.records;
        resetRow();

        return item;
    }

    bool continuation(unsigned char byte) const noexcept {
        if ((byte & UTF8_CONTINUATION_MASK) != UTF8_CONTINUATION_TAG) {
            return false;
        }

        if (utfSize != 1) {
            return true;
        }

        if (utf[0] == UTF8_MIN_THREE_BYTE_LEAD && byte < UTF8_E0_MIN_SECOND) {
            return false;
        }

        if (utf[0] == UTF8_MIN_FOUR_BYTE_LEAD && byte < UTF8_F0_MIN_SECOND) {
            return false;
        }

        if (utf[0] == UTF8_MAX_LEAD && byte > UTF8_F4_MAX_SECOND) {
            return false;
        }

        return true;
    }

    void decoded(IpfReadWork &work) {
        // Remove the lead prefix and append each six-bit continuation payload.
        std::uint32_t scalar = utf[0] & (utfExpected == 2 ? 0x1f : utfExpected == 3 ? 0x0f : 0x07);

        for (unsigned index = 1; index < utfExpected; ++index) {
            scalar = (scalar << 6) | (utf[index] & 0x3f);
        }

        utfSize = 0;
        --work.decodedUnits;

        if (scalar >= UTF16_HIGH_SURROGATE && scalar <= UTF16_LAST_SURROGATE) {
            character(REPLACEMENT, utfByte);
        } else if (scalar <= UTF16_BMP_MAX) {
            character(static_cast<char16_t>(scalar), utfByte);
        } else {
            scalar -= UTF16_SUPPLEMENTARY_START;
            lowSurrogate = static_cast<char16_t>(UTF16_LOW_SURROGATE + (scalar & UTF16_SURROGATE_PAYLOAD_MASK));
            character(static_cast<char16_t>(UTF16_HIGH_SURROGATE + (scalar >> UTF16_SURROGATE_PAYLOAD_BITS)), utfByte);
        }
    }

    IpfReadStep run(std::span<const std::byte> input, IpfReadWork &work) {
        IpfReadStep result;
        auto outcome = [&](IpfReadStatus status) {
            result.status = status;

            return std::move(result);
        };

        if (error) {
            return outcome(IpfReadStatus::FAILED);
        }

        if (finished) {
            return outcome(IpfReadStatus::FINISHED);
        }

        if (!input.empty() && eof) {
            fail(IpfReadError::INVALID_STATE);

            return outcome(IpfReadStatus::FAILED);
        }

        while (true) {
            if (!hasWork(work)) {
                return outcome(IpfReadStatus::WORK_EXHAUSTED);
            }

            if (pendingField) {
                --work.fields;
                completeField();

                if (error) {
                    return outcome(IpfReadStatus::FAILED);
                }

                pendingField = false;
                cell.clear();
                csv = Csv::START;

                if (endedRow) {
                    pendingRow = true;
                } else {
                    ++field;
                    fieldLine = line;
                    fieldByte = progress.inputBytes;
                }

                continue;
            }

            if (pendingRow) {
                if (waitLf) {
                    if (result.accepted == input.size() && !eof) {
                        return outcome(IpfReadStatus::NEED_INPUT);
                    }

                    if (result.accepted < input.size() && input[result.accepted] == std::byte{0x0a}) {
                        if (!acceptByte(work)) {
                            return outcome(IpfReadStatus::FAILED);
                        }

                        ++result.accepted;
                        --work.decodedUnits;
                    }

                    waitLf = false;
                }

                --work.records;
                result.item = completeRow();

                if (error) {
                    return outcome(IpfReadStatus::FAILED);
                }

                if (result.item) {
                    return outcome(IpfReadStatus::ITEM_READY);
                }

                continue;
            }

            if (lowSurrogate) {
                auto unit = *lowSurrogate;
                lowSurrogate.reset();
                --work.decodedUnits;
                character(unit, utfByte);

                if (error) {
                    return outcome(IpfReadStatus::FAILED);
                }

                continue;
            }

            if (utfSize != 0) {
                if (result.accepted == input.size()) {
                    if (!eof) {
                        return outcome(IpfReadStatus::NEED_INPUT);
                    }

                    utfSize = 0;
                    --work.decodedUnits;
                    character(REPLACEMENT, utfByte);
                } else {
                    auto byte = std::to_integer<unsigned char>(input[result.accepted]);

                    if (!continuation(byte)) {
                        utfSize = 0;
                        --work.decodedUnits;
                        character(REPLACEMENT, utfByte);
                    } else {
                        if (!acceptByte(work)) {
                            return outcome(IpfReadStatus::FAILED);
                        }

                        ++result.accepted;
                        utf[utfSize++] = byte;

                        if (utfSize == utfExpected) {
                            decoded(work);
                        }
                    }
                }

                if (error) {
                    return outcome(IpfReadStatus::FAILED);
                }

                continue;
            }

            if (result.accepted < input.size()) {
                auto byte = std::to_integer<unsigned char>(input[result.accepted]);
                auto offset = progress.inputBytes;

                if (!acceptByte(work)) {
                    return outcome(IpfReadStatus::FAILED);
                }

                ++result.accepted;

                if (byte < UTF8_ASCII_LIMIT) {
                    --work.decodedUnits;
                    character(byte, offset);
                } else if (byte >= UTF8_MIN_TWO_BYTE_LEAD && byte <= UTF8_MAX_LEAD) {
                    utfExpected = byte < UTF8_MIN_THREE_BYTE_LEAD ? 2 : byte < UTF8_MIN_FOUR_BYTE_LEAD ? 3 : 4;
                    utf[0] = byte;
                    utfSize = 1;
                    utfByte = offset;
                } else {
                    --work.decodedUnits;
                    character(REPLACEMENT, offset);
                }

                if (error) {
                    return outcome(IpfReadStatus::FAILED);
                }

                continue;
            }

            if (!eof) {
                return outcome(IpfReadStatus::NEED_INPUT);
            }

            if (csv == Csv::QUOTED) {
                fail(IpfReadError::UNTERMINATED_QUOTE);

                return outcome(IpfReadStatus::FAILED);
            }

            if (rowStarted) {
                pendingField = true;
                endedRow = true;
                continue;
            }

            if (options.completion == IpfCompletionPolicy::REQUIRE && !progress.completionSeen) {
                fail(IpfReadError::MISSING_COMPLETE);

                return outcome(IpfReadStatus::FAILED);
            }

            finished = true;

            return outcome(IpfReadStatus::FINISHED);
        }
    }
};

InstrumentProfileParser::InstrumentProfileParser(IpfReadOptions options) : impl_(std::make_unique<Impl>(options)) {
}

InstrumentProfileParser::~InstrumentProfileParser() = default;

IpfReadStep InstrumentProfileParser::feed(std::span<const std::byte> input, IpfReadWork &work) {
    auto before = impl_->progress.inputBytes;
    try {
        return impl_->run(input, work);
    } catch (const std::bad_alloc &) {
        impl_->fail(IpfReadError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        impl_->fail(IpfReadError::ALLOCATION_FAILURE);
    }

    return {static_cast<std::size_t>(impl_->progress.inputBytes - before), IpfReadStatus::FAILED, {}};
}

IpfReadStep InstrumentProfileParser::finish(IpfReadWork &work) {
    impl_->eof = true;

    return feed({}, work);
}

IpfReadProgress InstrumentProfileParser::progress() const noexcept {
    return impl_->progress;
}

std::optional<IpfDiagnostic> InstrumentProfileParser::diagnostic() const noexcept {
    return impl_->error;
}

IpfReadResult readInstrumentProfiles(std::istream &input, IpfReadOptions options) {
    IpfReadResult result;

    if (!hasWork(options.turnWork)) {
        result.error = IpfDiagnostic{IpfReadError::INVALID_OPTIONS, {}, 1, 1, {}};

        return result;
    }

    try {
        InstrumentProfileParser parser(options);
        auto finishResult = [&]() {
            auto progress = parser.progress();
            result.acceptedBytes = progress.inputBytes;
            result.flushes = progress.flushes;
            result.completions = progress.completions;
            result.completionSeen = progress.completionSeen;
            result.complete = result.finished && result.completionSeen;
        };
        auto driverFailure = [&](IpfReadError error) {
            parser.impl_->fail(error);
            result.error = parser.diagnostic();
            finishResult();
        };
        try {
            auto capture = [&](IpfReadStep step) {
                if (step.item && step.item->profile) {
                    try {
                        result.profiles.push_back(std::move(*step.item->profile));
                    } catch (const std::bad_alloc &) {
                        auto start = step.item->position;
                        result.error =
                            IpfDiagnostic{IpfReadError::ALLOCATION_FAILURE, start, start.line, start.line, {}};
                        finishResult();

                        return false;
                    } catch (const std::length_error &) {
                        auto start = step.item->position;
                        result.error =
                            IpfDiagnostic{IpfReadError::ALLOCATION_FAILURE, start, start.line, start.line, {}};
                        finishResult();

                        return false;
                    }
                }

                if (step.status == IpfReadStatus::FAILED) {
                    result.error = parser.diagnostic();
                    finishResult();

                    return false;
                }

                return true;
            };

            while (true) {
                std::istream::int_type value;
                try {
                    value = input.get();
                } catch (...) {
                    if (input.eof() && !input.bad()) {
                        break;
                    }

                    throw;
                }

                if (input.bad()) {
                    driverFailure(IpfReadError::IO_ERROR);

                    return result;
                }

                if (value == std::char_traits<char>::eof()) {
                    if (!input.eof()) {
                        driverFailure(IpfReadError::IO_ERROR);

                        return result;
                    }

                    break;
                }

                auto byte = static_cast<std::byte>(static_cast<unsigned char>(value));
                std::size_t accepted = 0;

                while (true) {
                    auto work = options.turnWork;
                    auto remaining = std::span<const std::byte>(&byte, 1).subspan(accepted);
                    auto step = parser.feed(remaining, work);
                    accepted += step.accepted;
                    auto status = step.status;

                    if (!capture(std::move(step))) {
                        return result;
                    }

                    // Drain completed fields/rows before attempting more I/O, so a later stream
                    // failure cannot mask an already determined conversion/syntax outcome.
                    if (status == IpfReadStatus::NEED_INPUT) {
                        break;
                    }
                }
            }

            while (true) {
                auto work = options.turnWork;
                auto step = parser.finish(work);
                auto status = step.status;

                if (!capture(std::move(step))) {
                    return result;
                }

                if (status == IpfReadStatus::FINISHED) {
                    result.finished = true;
                    finishResult();

                    return result;
                }
            }
        } catch (const std::ios_base::failure &) {
            driverFailure(IpfReadError::IO_ERROR);
        } catch (const std::bad_alloc &) {
            driverFailure(IpfReadError::ALLOCATION_FAILURE);
        } catch (const std::length_error &) {
            driverFailure(IpfReadError::ALLOCATION_FAILURE);
        } catch (...) {
            driverFailure(IpfReadError::IO_ERROR);
        }
    } catch (const std::bad_alloc &) {
        result.error = IpfDiagnostic{IpfReadError::ALLOCATION_FAILURE, {}, 1, 1, {}};
    }

    return result;
}

IpfReadResult readInstrumentProfilesFile(const std::filesystem::path &path, IpfReadOptions options) {
    if (!hasWork(options.turnWork)) {
        IpfReadResult result;
        result.error = IpfDiagnostic{IpfReadError::INVALID_OPTIONS, {}, 1, 1, {}};

        return result;
    }

    try {
        std::ifstream input(path, std::ios::binary);

        if (!input.is_open()) {
            IpfReadResult result;
            result.error = IpfDiagnostic{IpfReadError::IO_ERROR, {}, 1, 1, {}};

            return result;
        }

        return readInstrumentProfiles(input, options);
    } catch (const std::ios_base::failure &) {
        IpfReadResult result;
        result.error = IpfDiagnostic{IpfReadError::IO_ERROR, {}, 1, 1, {}};

        return result;
    } catch (const std::bad_alloc &) {
        IpfReadResult result;
        result.error = IpfDiagnostic{IpfReadError::ALLOCATION_FAILURE, {}, 1, 1, {}};

        return result;
    } catch (...) {
        IpfReadResult result;
        result.error = IpfDiagnostic{IpfReadError::IO_ERROR, {}, 1, 1, {}};

        return result;
    }
}
} // namespace org::ttldtor::qdcxx::ipf
