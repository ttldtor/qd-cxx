// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <qd/ipf/instrument_profile_writer.hpp>
#include <set>
#include <stdexcept>

namespace org::ttldtor::qdcxx::ipf {
namespace {
constexpr std::string_view COMPLETE_MARKER = "##COMPLETE\r\n";
constexpr std::size_t WRITE_CHUNK_BYTES = 4096;

struct Format {
    std::array<bool, 31> standard{};
    std::set<std::u16string, std::less<>> custom;
    std::size_t count = 2;
};

std::expected<std::size_t, IpfWriteDiagnostic> preflight(std::span<const InstrumentProfile> profiles,
                                                         const IpfWriteLimits &limits) {
    if (profiles.size() > limits.profiles) {
        return std::unexpected(IpfWriteDiagnostic{IpfWriteError::PROFILE_LIMIT, 0, 0});
    }

    if (profiles.empty()) {
        return 0;
    }

    const auto &profile = profiles.front();
    const auto units = profile.retainedUnits();

    if (units > limits.inputUnits) {
        return std::unexpected(IpfWriteDiagnostic{IpfWriteError::INPUT_LIMIT, 0, 0});
    }

    std::size_t work = 0;

    for (const auto cost : {units, standardFields().size(), profile.customFieldCount()}) {
        if (cost > limits.work - work) {
            return std::unexpected(IpfWriteDiagnostic{IpfWriteError::WORK_LIMIT, 0, 0});
        }

        work += cost;
    }

    if (limits.fieldsPerType < 2 || profile.customFieldCount() > limits.fieldsPerType - 2) {
        return std::unexpected(IpfWriteDiagnostic{IpfWriteError::FIELD_LIMIT, 0, 0});
    }

    return work;
}

class Composer {
    std::span<const InstrumentProfile> profiles_;
    IpfWriteLimits limits_;
    std::size_t work_ = 0, input_ = 0, headers_ = 0;
    std::size_t profile_ = 0, field_ = 0;
    std::map<std::u16string, Format, std::less<>> formats_;
    std::string bytes_;
    std::optional<IpfWriteDiagnostic> error_;

    bool reject(IpfWriteError error) {
        if (!error_) {
            error_ = IpfWriteDiagnostic{error, profile_, field_};
        }

        return false;
    }

    bool add(std::size_t &used, std::size_t amount, std::size_t limit, IpfWriteError error) {
        if (amount > limit - used) {
            return reject(error);
        }

        used += amount;

        return true;
    }

    bool work(std::size_t amount) {
        return add(work_, amount, limits_.work, IpfWriteError::WORK_LIMIT);
    }

    bool validate(std::u16string_view text) {
        for (std::size_t i = 0; i < text.size(); ++i) {
            const auto c = text[i];

            if (c >= 0xd800 && c <= 0xdbff) {
                if (i + 1 == text.size() || text[i + 1] < 0xdc00 || text[i + 1] > 0xdfff) {
                    return reject(IpfWriteError::UNREPRESENTABLE_UTF16);
                }

                ++i;
            } else if (c >= 0xdc00 && c <= 0xdfff) {
                return reject(IpfWriteError::UNREPRESENTABLE_UTF16);
            }
        }

        return true;
    }

    bool header(std::size_t units) {
        return add(headers_, units, limits_.headerUnits, IpfWriteError::HEADER_LIMIT);
    }

    bool field(Format &format, std::u16string_view name, std::optional<std::size_t> standard) {
        const bool present = standard ? format.standard[*standard] : format.custom.contains(name);

        if (present) {
            return true;
        }

        if (format.count >= limits_.fieldsPerType) {
            return reject(IpfWriteError::FIELD_LIMIT);
        }

        if (!header(name.size())) {
            return false;
        }

        if (standard) {
            format.standard[*standard] = true;
        } else {
            format.custom.emplace(name);
        }

        ++format.count;

        return true;
    }

    bool collect() {
        for (profile_ = 0; profile_ < profiles_.size(); ++profile_) {
            const auto &p = profiles_[profile_];

            field_ = 0;

            if (profile_ != 0) {
                if (!add(input_, p.retainedUnits(), limits_.inputUnits, IpfWriteError::INPUT_LIMIT) ||
                    !work(p.retainedUnits()) || !work(standardFields().size()) || !work(p.customFieldCount())) {
                    return false;
                }

                if (limits_.fieldsPerType < 2 || p.customFieldCount() > limits_.fieldsPerType - 2) {
                    return reject(IpfWriteError::FIELD_LIMIT);
                }
            }

            const auto type = p.getType();

            if (type == u"##" || type == u"##COMPLETE" || (type.starts_with(u"#") && type.ends_with(u"::=TYPE"))) {
                return reject(IpfWriteError::INVALID_TYPE);
            }

            for (const auto &info : standardFields()) {
                field_ = static_cast<std::size_t>(info.field);

                const auto value = p.getField(info.name);

                if (!validate(value)) {
                    return false;
                }

                if (type == u"REMOVED" && field_ > 1 && !value.empty()) {
                    return reject(IpfWriteError::REMOVED_FIELDS);
                }
            }

            auto names = p.customFieldNames();

            for (std::size_t i = 0; i < names.size(); ++i) {
                field_ = standardFields().size() + i;

                if (!validate(names[i]) || !validate(p.getField(names[i]))) {
                    return false;
                }

                if (type == u"REMOVED") {
                    return reject(IpfWriteError::REMOVED_FIELDS);
                }
            }

            field_ = 0;

            auto found = formats_.find(type);

            if (found == formats_.end()) {
                if (formats_.size() >= limits_.types) {
                    return reject(IpfWriteError::TYPE_LIMIT);
                }

                if (!header(type.size()) || !header(std::u16string_view(u"SYMBOL").size())) {
                    return false;
                }

                found = formats_.emplace(std::u16string(type), Format{}).first;
                found->second.standard[1] = true;
            }

            auto &format = found->second;

            for (const auto &info : standardFields()) {
                field_ = static_cast<std::size_t>(info.field);

                if (field_ > 1 && !p.getField(info.name).empty() && !field(format, info.name, field_)) {
                    return false;
                }
            }

            for (std::size_t i = 0; i < names.size(); ++i) {
                field_ = standardFields().size() + i;

                if (!field(format, names[i], {})) {
                    return false;
                }
            }
        }

        return true;
    }

    bool byte(char value) {
        if (bytes_.size() >= limits_.outputBytes) {
            return reject(IpfWriteError::OUTPUT_LIMIT);
        }

        if (!work(1)) {
            return false;
        }

        bytes_.push_back(value);

        return true;
    }

    bool utf8(std::u16string_view text, bool quote) {
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (!work(1)) {
                return false;
            }

            std::uint32_t c = text[i];

            if (c >= 0xd800 && c <= 0xdbff) {
                if (!work(1)) {
                    return false;
                }

                c = 0x10000 + ((c - 0xd800) << 10) + (text[++i] - 0xdc00);
            }

            if (c < 0x80) {
                if (!byte(static_cast<char>(c)) || (quote && c == '"' && !byte('"'))) {
                    return false;
                }
            } else if (c < 0x800) {
                if (!byte(static_cast<char>(0xc0 | (c >> 6))) || !byte(static_cast<char>(0x80 | (c & 0x3f)))) {
                    return false;
                }
            } else if (c < 0x10000) {
                if (!byte(static_cast<char>(0xe0 | (c >> 12))) || !byte(static_cast<char>(0x80 | ((c >> 6) & 0x3f))) ||
                    !byte(static_cast<char>(0x80 | (c & 0x3f)))) {
                    return false;
                }
            } else if (!byte(static_cast<char>(0xf0 | (c >> 18))) ||
                       !byte(static_cast<char>(0x80 | ((c >> 12) & 0x3f))) ||
                       !byte(static_cast<char>(0x80 | ((c >> 6) & 0x3f))) ||
                       !byte(static_cast<char>(0x80 | (c & 0x3f)))) {
                return false;
            }
        }

        return true;
    }

    std::optional<bool> quoted(std::u16string_view text) {
        bool quote = false;

        for (char16_t c : text) {
            if (!work(1)) {
                return {};
            }

            if (c == u',' || c == u'"' || c == u'\r' || c == u'\n') {
                quote = true;
            }
        }

        return quote;
    }

    bool cell(std::u16string_view text) {
        const auto quote = quoted(text);

        if (!quote || (*quote && !byte('"')) || !utf8(text, *quote) || (*quote && !byte('"'))) {
            return false;
        }

        return true;
    }

    bool headerCell(std::u16string_view type) {
        const auto quote = quoted(type);

        if (!quote || (*quote && !byte('"')) || !utf8(u"#", *quote) || !utf8(type, *quote) ||
            !utf8(u"::=TYPE", *quote) || (*quote && !byte('"'))) {
            return false;
        }

        return true;
    }

    bool end() {
        return byte('\r') && byte('\n');
    }

    bool render() {
        profile_ = 0;

        for (const auto &[type, format] : formats_) {
            field_ = 0;

            if (!headerCell(type)) {
                return false;
            }

            for (const auto &info : standardFields()) {
                field_ = static_cast<std::size_t>(info.field);

                if (format.standard[field_] && (!byte(',') || !cell(info.name))) {
                    return false;
                }
            }

            std::size_t customIndex = standardFields().size();

            for (const auto &name : format.custom) {
                field_ = customIndex++;

                if (!byte(',') || !cell(name)) {
                    return false;
                }
            }

            if (!end()) {
                return false;
            }
        }

        for (profile_ = 0; profile_ < profiles_.size(); ++profile_) {
            const auto &p = profiles_[profile_];
            const auto &format = formats_.find(p.getType())->second;

            field_ = 0;

            if (!cell(p.getType())) {
                return false;
            }

            for (const auto &info : standardFields()) {
                field_ = static_cast<std::size_t>(info.field);

                if (!work(1) || (format.standard[field_] && (!byte(',') || !cell(p.getField(info.name))))) {
                    return false;
                }
            }

            std::size_t customIndex = standardFields().size();

            for (const auto &name : format.custom) {
                field_ = customIndex++;

                if (!work(1) || !byte(',') || !cell(p.getField(name))) {
                    return false;
                }
            }

            if (!end()) {
                return false;
            }
        }

        profile_ = 0;
        field_ = 0;

        for (char c : COMPLETE_MARKER) {
            if (!byte(c)) {
                return false;
            }
        }

        return true;
    }

    public:
    Composer(std::span<const InstrumentProfile> profiles, IpfWriteLimits limits, std::size_t chargedWork)
        : profiles_(profiles), limits_(limits), work_(chargedWork),
          input_(profiles.empty() ? 0 : profiles.front().retainedUnits()) {
    }

    std::expected<std::string, IpfWriteDiagnostic> compose() {
        if (!collect() || !render()) {
            return std::unexpected(*error_);
        }

        return std::move(bytes_);
    }
};

IpfWriteResult publish(std::ostream &output, std::string_view document) {
    IpfWriteResult result;
    auto failure = [&](IpfWriteError error) {
        result.error = IpfWriteDiagnostic{error, 0, 0};
    };
    try {
        auto part = [&](std::string_view text) {
            while (!text.empty()) {
                const auto count = std::min(text.size(), WRITE_CHUNK_BYTES);

                output.write(text.data(), static_cast<std::streamsize>(count));

                if (!output) {
                    failure(IpfWriteError::IO_ERROR);

                    return false;
                }

                result.confirmedBytes += count;
                text.remove_prefix(count);
            }

            output.flush();

            if (!output) {
                failure(IpfWriteError::IO_ERROR);

                return false;
            }

            return true;
        };

        if (part(document.substr(0, document.size() - COMPLETE_MARKER.size())) && part(COMPLETE_MARKER))
            result.complete = true;
    } catch (const std::bad_alloc &) {
        failure(IpfWriteError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        failure(IpfWriteError::ALLOCATION_FAILURE);
    } catch (...) {
        failure(IpfWriteError::IO_ERROR);
    }

    return result;
}
} // namespace

std::expected<std::string, IpfWriteDiagnostic> composeInstrumentProfiles(std::span<const InstrumentProfile> profiles,
                                                                         IpfWriteOptions options) {
    const auto checked = preflight(profiles, options.limits);

    if (!checked) {
        return std::unexpected(checked.error());
    }

    try {
        return Composer(profiles, options.limits, *checked).compose();
    } catch (const std::bad_alloc &) {
        return std::unexpected(IpfWriteDiagnostic{IpfWriteError::ALLOCATION_FAILURE, 0, 0});
    } catch (const std::length_error &) {
        return std::unexpected(IpfWriteDiagnostic{IpfWriteError::ALLOCATION_FAILURE, 0, 0});
    }
}

IpfWriteResult writeInstrumentProfiles(std::ostream &output, std::span<const InstrumentProfile> profiles,
                                       IpfWriteOptions options) {
    auto document = composeInstrumentProfiles(profiles, options);

    if (!document)
        return {0, false, document.error()};

    return publish(output, *document);
}

IpfWriteResult writeInstrumentProfilesFile(const std::filesystem::path &path,
                                           std::span<const InstrumentProfile> profiles, IpfWriteOptions options) {
    auto document = composeInstrumentProfiles(profiles, options);

    if (!document)
        return {0, false, document.error()};

    IpfWriteResult result;
    try {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);

        if (!output.is_open())
            return {0, false, IpfWriteDiagnostic{IpfWriteError::IO_ERROR, 0, 0}};

        result = publish(output, *document);
        output.close();

        if (output.fail() && !result.error) {
            result.complete = false;
            result.error = IpfWriteDiagnostic{IpfWriteError::IO_ERROR, 0, 0};
        }
    } catch (const std::bad_alloc &) {
        result.complete = false;

        if (!result.error)
            result.error = IpfWriteDiagnostic{IpfWriteError::ALLOCATION_FAILURE, 0, 0};
    } catch (...) {
        result.complete = false;

        if (!result.error)
            result.error = IpfWriteDiagnostic{IpfWriteError::IO_ERROR, 0, 0};
    }

    return result;
}
} // namespace org::ttldtor::qdcxx::ipf
