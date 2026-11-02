// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <array>
#include <span>

namespace org::ttldtor::qdcxx::connect::closed_tls {
/** Exact compiled declaration and every alias expected on both qualified images. */
struct DigestDeclaration {
    const char *canonical;                ///< Static canonical fetch name, with process-image lifetime.
    std::span<const char *const> aliases; ///< Complete static alias set; no runtime/provider borrow.
};

inline constexpr std::array<const char *, 4> DIGEST_ALIASES_1{"SHA1", "SHA-1", "SSL3-SHA1", "1.3.14.3.2.26"};
inline constexpr std::array<const char *, 4> DIGEST_ALIASES_2{"SHA2-224", "SHA-224", "SHA224",
                                                              "2.16.840.1.101.3.4.2.4"};
inline constexpr std::array<const char *, 4> DIGEST_ALIASES_3{"SHA2-256", "SHA-256", "SHA256",
                                                              "2.16.840.1.101.3.4.2.1"};
inline constexpr std::array<const char *, 3> DIGEST_ALIASES_4{"SHA2-256/192", "SHA-256/192", "SHA256-192"};
inline constexpr std::array<const char *, 4> DIGEST_ALIASES_5{"SHA2-384", "SHA-384", "SHA384",
                                                              "2.16.840.1.101.3.4.2.2"};
inline constexpr std::array<const char *, 4> DIGEST_ALIASES_6{"SHA2-512", "SHA-512", "SHA512",
                                                              "2.16.840.1.101.3.4.2.3"};
inline constexpr std::array<const char *, 4> DIGEST_ALIASES_7{"SHA2-512/224", "SHA-512/224", "SHA512-224",
                                                              "2.16.840.1.101.3.4.2.5"};
inline constexpr std::array<const char *, 4> DIGEST_ALIASES_8{"SHA2-512/256", "SHA-512/256", "SHA512-256",
                                                              "2.16.840.1.101.3.4.2.6"};
inline constexpr std::array<const char *, 2> DIGEST_ALIASES_9{"SHA3-224", "2.16.840.1.101.3.4.2.7"};
inline constexpr std::array<const char *, 2> DIGEST_ALIASES_10{"SHA3-256", "2.16.840.1.101.3.4.2.8"};
inline constexpr std::array<const char *, 2> DIGEST_ALIASES_11{"SHA3-384", "2.16.840.1.101.3.4.2.9"};
inline constexpr std::array<const char *, 2> DIGEST_ALIASES_12{"SHA3-512", "2.16.840.1.101.3.4.2.10"};
inline constexpr std::array<const char *, 1> DIGEST_ALIASES_13{"KECCAK-224"};
inline constexpr std::array<const char *, 1> DIGEST_ALIASES_14{"KECCAK-256"};
inline constexpr std::array<const char *, 1> DIGEST_ALIASES_15{"KECCAK-384"};
inline constexpr std::array<const char *, 1> DIGEST_ALIASES_16{"KECCAK-512"};
inline constexpr std::array<const char *, 2> DIGEST_ALIASES_17{"KECCAK-KMAC-128", "KECCAK-KMAC128"};
inline constexpr std::array<const char *, 2> DIGEST_ALIASES_18{"KECCAK-KMAC-256", "KECCAK-KMAC256"};
inline constexpr std::array<const char *, 3> DIGEST_ALIASES_19{"SHAKE-128", "SHAKE128", "2.16.840.1.101.3.4.2.11"};
inline constexpr std::array<const char *, 3> DIGEST_ALIASES_20{"SHAKE-256", "SHAKE256", "2.16.840.1.101.3.4.2.12"};
inline constexpr std::array<const char *, 3> DIGEST_ALIASES_21{"BLAKE2S-256", "BLAKE2s256",
                                                               "1.3.6.1.4.1.1722.12.2.2.8"};
inline constexpr std::array<const char *, 3> DIGEST_ALIASES_22{"BLAKE2B-512", "BLAKE2b512",
                                                               "1.3.6.1.4.1.1722.12.2.1.16"};
inline constexpr std::array<const char *, 2> DIGEST_ALIASES_23{"SM3", "1.2.156.10197.1.401"};
inline constexpr std::array<const char *, 3> DIGEST_ALIASES_24{"MD5", "SSL3-MD5", "1.2.840.113549.2.5"};
inline constexpr std::array<const char *, 1> DIGEST_ALIASES_25{"MD5-SHA1"};
inline constexpr std::array<const char *, 5> DIGEST_ALIASES_26{"RIPEMD-160", "RIPEMD160", "RIPEMD", "RMD160",
                                                               "1.3.36.3.2.1"};
inline constexpr std::array<const char *, 1> DIGEST_ALIASES_27{"NULL"};
inline constexpr std::array<DigestDeclaration, 27> DIGEST_CATALOG{{
    {"SHA1", DIGEST_ALIASES_1},
    {"SHA2-224", DIGEST_ALIASES_2},
    {"SHA2-256", DIGEST_ALIASES_3},
    {"SHA2-256/192", DIGEST_ALIASES_4},
    {"SHA2-384", DIGEST_ALIASES_5},
    {"SHA2-512", DIGEST_ALIASES_6},
    {"SHA2-512/224", DIGEST_ALIASES_7},
    {"SHA2-512/256", DIGEST_ALIASES_8},
    {"SHA3-224", DIGEST_ALIASES_9},
    {"SHA3-256", DIGEST_ALIASES_10},
    {"SHA3-384", DIGEST_ALIASES_11},
    {"SHA3-512", DIGEST_ALIASES_12},
    {"KECCAK-224", DIGEST_ALIASES_13},
    {"KECCAK-256", DIGEST_ALIASES_14},
    {"KECCAK-384", DIGEST_ALIASES_15},
    {"KECCAK-512", DIGEST_ALIASES_16},
    {"KECCAK-KMAC-128", DIGEST_ALIASES_17},
    {"KECCAK-KMAC-256", DIGEST_ALIASES_18},
    {"SHAKE-128", DIGEST_ALIASES_19},
    {"SHAKE-256", DIGEST_ALIASES_20},
    {"BLAKE2S-256", DIGEST_ALIASES_21},
    {"BLAKE2B-512", DIGEST_ALIASES_22},
    {"SM3", DIGEST_ALIASES_23},
    {"MD5", DIGEST_ALIASES_24},
    {"MD5-SHA1", DIGEST_ALIASES_25},
    {"RIPEMD-160", DIGEST_ALIASES_26},
    {"NULL", DIGEST_ALIASES_27},
}};
static_assert([] {
    std::size_t count = 0;

    for (const auto &row : DIGEST_CATALOG)
        count += row.aliases.size();

    return count == 71;
}());
} // namespace org::ttldtor::qdcxx::connect::closed_tls
