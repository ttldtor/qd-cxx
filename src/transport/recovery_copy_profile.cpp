// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "recovery_copy_profile.hpp"

#include <limits>
#include <string>

#ifndef ORG_TTLDTOR_QDCXX_RECOVERY_COPY_PROFILE
#    define ORG_TTLDTOR_QDCXX_RECOVERY_COPY_PROFILE 0
#endif

namespace org::ttldtor::qdcxx::transport::detail {
namespace {

constexpr std::size_t UTF16_COPY_PADDING = 8;        ///< Selected SSO capacity seven plus terminating unit.
constexpr std::size_t ASCII_COPY_PADDING = 16;       ///< Selected SSO capacity fifteen plus terminating byte.
constexpr std::size_t SCHEMA_RUNTIME_PEAK = 3;       ///< Endpoint, writer and live-session schema coexist.
constexpr std::size_t SETTINGS_PREPARATION_PEAK = 2; ///< Temporary settings and candidate State coexist.

/** Private scan escape; exception bookkeeping is outside logical backing and retains no input text. */
struct Refusal {
    RecoveryDiagnostic diagnostic; ///< Exact fixed category/checkpoint.
};

/** Stop a scan before an inadmissible request.
 * @param error Selected category.
 * @param stage Static checkpoint.
 */
[[noreturn]] void refuse(RecoveryError error, std::string_view stage) {
    throw Refusal{{error, stage}};
}

/** Debit one real scan coordinate without refund.
 * @param amount Required work.
 * @param available Actual remaining caller coordinate.
 */
void debit(std::size_t amount, std::size_t &available) {
    if (amount > available) {
        refuse(RecoveryError::WORK_LIMIT, "recovery-copy-scan");
    }

    available -= amount;
}

/** Add requested capacity with checked arithmetic.
 * @param target Current total.
 * @param amount Additional capacity in the same units.
 */
void add(std::size_t &target, std::size_t amount) {
    if (amount > std::numeric_limits<std::size_t>::max() - target) {
        refuse(RecoveryError::RESOURCE_LIMIT, "recovery-copy-arithmetic");
    }

    target += amount;
}

/** Multiply requested capacity with checked arithmetic.
 * @param value One owner capacity.
 * @param count Simultaneous owner count.
 * @return Representable product.
 */
std::size_t multiply(std::size_t value, std::size_t count) {
    if (count != 0 && value > std::numeric_limits<std::size_t>::max() / count) {
        refuse(RecoveryError::RESOURCE_LIMIT, "recovery-copy-arithmetic");
    }

    return value * count;
}

/** Bound one freshly copied string including selected SSO and termination.
 * @param size Logical source length.
 * @param padding Selected string-type allowance.
 * @return Conservative requested capacity.
 */
std::size_t copiedText(std::size_t size, std::size_t padding) {
    add(size, padding);

    return size;
}

/** Inspect original string backing, including its terminating unit.
 * @param text Actual caller-owned string.
 * @return Its complete requested-capacity coordinate.
 */
template <class Character> std::size_t actualText(const std::basic_string<Character> &text) {
    auto capacity = text.capacity();
    add(capacity, 1);

    return capacity;
}

/** Admit all four independent schema coordinates.
 * @param needed Required simultaneous backing.
 * @param limit Explicit caller ceiling.
 */
void admit(RecoverySchemaCapacity needed, RecoverySchemaCapacity limit) {
    if (needed.records > limit.records || needed.fields > limit.fields || needed.lookups > limit.lookups ||
        needed.nameUnits > limit.nameUnits) {
        refuse(RecoveryError::RESOURCE_LIMIT, "recovery-schema-capacity");
    }
}

/** Admit all three independent outgoing-layout coordinates.
 * @param needed Required copied or adopted backing.
 * @param limit Explicit caller ceiling.
 */
void admit(RecoveryBlueprintCapacity needed, RecoveryBlueprintCapacity limit) {
    if (needed.records > limit.records || needed.fields > limit.fields || needed.nameUnits > limit.nameUnits) {
        refuse(RecoveryError::RESOURCE_LIMIT, "recovery-blueprint-capacity");
    }
}

} // namespace

unsigned recoveryCopyProfile() noexcept {
#if ORG_TTLDTOR_QDCXX_RECOVERY_COPY_PROFILE == 1 && defined(_MSC_VER) && _MSC_VER == 1942 &&                           \
    defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION == 143 && defined(_MSVC_STL_UPDATE) &&                             \
    _MSVC_STL_UPDATE == 202408L && defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL == 0

    return sizeof(char16_t) == 2 ? 1U : 0U;
#elif ORG_TTLDTOR_QDCXX_RECOVERY_COPY_PROFILE == 2 && defined(__GNUC__) && __GNUC__ == 13 && __GNUC_MINOR__ == 3 &&    \
    defined(__GLIBCXX__) && __GLIBCXX__ == 20240904 && defined(_GLIBCXX_USE_CXX11_ABI) &&                              \
    _GLIBCXX_USE_CXX11_ABI == 1 && !defined(_GLIBCXX_DEBUG)

    return sizeof(char16_t) == 2 ? 2U : 0U;
#else

    return 0;
#endif
}

std::expected<RecoveryCapacity, RecoveryDiagnostic>
inspectRecoveryCopies(const client::EndpointOptions &endpoint, const std::vector<protocol::OutgoingRecord> &blueprint,
                      const RecoveryLimits &limits, RecoveryWork &work) noexcept {
    if (recoveryCopyProfile() == 0) {
        return std::unexpected(RecoveryDiagnostic{RecoveryError::UNSUPPORTED_PROFILE, "recovery-copy-profile"});
    }

    try {
        debit(1, work.operations);
        RecoveryCapacity result;
        auto &schema = result.schemaCopies;
        schema.records = endpoint.schema().records().size();
        debit(schema.records, work.visits);

        for (const auto &record : endpoint.schema().records()) {
            add(schema.fields, record.fields.size());
            add(schema.lookups, record.lookups.size());
            debit(record.fields.size(), work.visits);
            debit(record.lookups.size(), work.visits);
            debit(record.name.size(), work.textUnits);
            add(schema.nameUnits, copiedText(record.name.size(), UTF16_COPY_PADDING));

            for (const auto &lookup : record.lookups) {
                debit(lookup.name.size(), work.textUnits);
                add(schema.nameUnits, copiedText(lookup.name.size(), UTF16_COPY_PADDING));
            }
        }

        schema.records = multiply(schema.records, SCHEMA_RUNTIME_PEAK);
        schema.fields = multiply(schema.fields, SCHEMA_RUNTIME_PEAK);
        schema.lookups = multiply(schema.lookups, SCHEMA_RUNTIME_PEAK);
        schema.nameUnits = multiply(schema.nameUnits, SCHEMA_RUNTIME_PEAK);
        admit(schema, limits.schemaCopies);

        auto &copied = result.blueprintCopy;
        auto &adopted = result.adoptedBlueprint;
        copied.records = blueprint.size();
        adopted.records = blueprint.capacity();
        debit(blueprint.size(), work.visits);

        for (const auto &record : blueprint) {
            add(copied.fields, record.fields.size());
            add(adopted.fields, record.fields.capacity());
            debit(record.fields.size(), work.visits);
            debit(record.wireName.size(), work.textUnits);
            add(copied.nameUnits, copiedText(record.wireName.size(), UTF16_COPY_PADDING));
            add(adopted.nameUnits, actualText(record.wireName));

            for (const auto &field : record.fields) {
                debit(field.wireName.size(), work.textUnits);
                add(copied.nameUnits, copiedText(field.wireName.size(), UTF16_COPY_PADDING));
                add(adopted.nameUnits, actualText(field.wireName));
            }
        }

        admit(copied, limits.blueprintCopy);
        admit(adopted, limits.adoptedBlueprint);

        const auto &settings = endpoint.settings();
        const auto inspectString = [&](const std::string &text) {
            debit(1, work.visits);
            debit(text.size(), work.textUnits);
            add(result.endpointCopyBytes, copiedText(text.size(), ASCII_COPY_PADDING));
            add(result.sourceEndpointTextBytes, actualText(text));
        };
        inspectString(settings.host);

        if (settings.tls) {
            inspectString(settings.tls->referenceIdentity);
            inspectString(settings.tls->trustPem);
        }

        result.endpointCopyBytes = multiply(result.endpointCopyBytes, SETTINGS_PREPARATION_PEAK);

        if (result.endpointCopyBytes > limits.endpointCopyBytes) {
            refuse(RecoveryError::RESOURCE_LIMIT, "recovery-endpoint-capacity");
        }

        result.sourceSchemaBytes = endpoint.schema().storageBytes();

        return result;
    } catch (const Refusal &failure) {
        return std::unexpected(failure.diagnostic);
    }
}

bool recoverySchemaFits(const core::LocalSchema &schema, RecoverySchemaCapacity envelope) noexcept {
    if (schema.records().size() > envelope.records) {
        return false;
    }

    try {
        RecoverySchemaCapacity actual;
        actual.records = schema.records().size();

        for (const auto &record : schema.records()) {
            add(actual.fields, record.fields.capacity());
            add(actual.lookups, record.lookups.capacity());
            add(actual.nameUnits, actualText(record.name));

            for (const auto &lookup : record.lookups) {
                add(actual.nameUnits, actualText(lookup.name));
            }
        }

        admit(actual, envelope);

        return true;
    } catch (const Refusal &) {
        return false;
    }
}

} // namespace org::ttldtor::qdcxx::transport::detail
