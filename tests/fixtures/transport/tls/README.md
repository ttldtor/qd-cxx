# Ordinary localhost TLS fixtures

[Русская версия](README.ru.md). English is authoritative.

These synthetic credentials serve local conformance tests. `server.key` and `server-rsa4096.key`
are intentionally published, unencrypted test-only RSA private keys. Use them only with the isolated
localhost test peer. They grant no production identity or external service authorization.
CA private keys are absent.

| File | Purpose |
| --- | --- |
| [ca.pem](ca.pem) | Explicit trusted synthetic CA. |
| [other-ca.pem](other-ca.pem) | Independent synthetic CA for an untrusted-issuer case. |
| [server.pem](server.pem) | Server certificate signed by `ca.pem`, serial 11. |
| [expired.pem](expired.pem) | Certificate signed by the same CA, with the same server key, serial 12 and a fixed expired interval. |
| [server.key](server.key) | Test-only key shared by both server certificates. |

The four baseline certificates use RSA-2048 public keys and SHA-256/RSA signatures. Both leaf certificates
have `CA:FALSE`, `digitalSignature,keyEncipherment` key usage, `serverAuth` extended usage and
SAN entries `DNS:localhost,IP:127.0.0.1`. Their CN is deliberately
`wrong-common-name.invalid`, so a matching CN cannot conceal a failed SAN check.
Both CAs have critical `CA:TRUE` and `keyCertSign,cRLSign` key usage.

The CA, other CA and ordinary server certificate are valid from **2026-11-01 08:28:52 UTC**
through **2046-10-27 08:28:52 UTC** (7300 days). The expired leaf is valid from
**2020-01-01 00:00:00 UTC** through **2020-01-02 00:00:00 UTC**. Tests using the real verification
clock depend on those intervals. Running outside the ordinary interval requires an explicit
fixture update; disabling certificate-date validation changes the tested contract.

[provenance.json](provenance.json) records the five exact file hashes, certificate metadata,
OpenSSL generator version and generation arguments. Tests on Windows and Linux consume this same
committed set. The recorded generation uses ordinary random key generation and current-time
validity starts; repeating the commands creates equivalent test roles with different bytes,
fingerprints, keys and dates. It does not reproduce these hashes.

To create an equivalent replacement set, use a fresh owned directory and the qualified
[static OpenSSL profile](../../../../cmake/tls-profile/openssl-3.5.9.json). Create the empty
configuration and `server.ext` contents recorded in `provenance.json`, set the child process's
`OPENSSL_CONF` to that empty file and run the recorded argument arrays in order. Relative names
resolve inside that new directory. The first three commands create fresh RSA keys; the last two
sign the ordinary and expired leaves. Keep only the five listed public fixtures, then verify
their signatures, key match, SAN identities and dates before replacing the committed set.

An ordinary CLI check with OpenSSL 3.5.9 accepted `server.pem` with `ca.pem` for `localhost` and
`127.0.0.1`; it rejected the wrong CN hostname, `other-ca.pem` trust and `expired.pem` with X509
errors 62, 20 and 10 respectively. These checks validate fixture roles. Runtime handshake and
delivery coverage belongs in the [transport conformance results](../../../conformance/transport/VALIDATION.md).

## Ordinary key-profile boundaries

Seven additional certificates are normally generated, self-signed CA certificates with SHA-256
signatures, critical `CA:TRUE` and `keyCertSign,cRLSign` usage. Their private keys are absent.
These valid-form inputs exercise the factory's trust-anchor key policy:

| Certificate | Public key | Expected key-policy result |
| --- | --- | --- |
| [ca-explicit-p256.pem](ca-explicit-p256.pem) | P-256, explicit EC parameters | Refuse explicit parameters. |
| [ca-p521.pem](ca-p521.pem) | Named P-521 | Refuse the unsupported named curve. |
| [ca-rsa5120.pem](ca-rsa5120.pem) | RSA-5120, exponent 65537 | Refuse the size above 4096 bits. |
| [ca-rsa1024.pem](ca-rsa1024.pem) | RSA-1024, exponent 65537 | Refuse the size below 2048 bits. |
| [ca-rsa4096.pem](ca-rsa4096.pem) | RSA-4096, exponent 65537 | Accept the admitted upper size boundary. |
| [ca-p256.pem](ca-p256.pem) | Named P-256 | Accept the admitted named curve. |
| [ca-p384.pem](ca-p384.pem) | Named P-384 | Accept the admitted named curve. |

The first two are valid from **2026-11-01 08:34:24 UTC** through **2046-10-27 08:34:24 UTC**;
the remaining five use **08:34:25 UTC** on those dates. Each interval is 7300 days.
`keyProfileBoundaries` in `provenance.json` records individual hashes, public parameters,
generation arguments and expected typed failures. Each PEM is at most 2210 bytes.

OpenSSL 3.5.9 inspected all seven keys and verified their self-signatures with `-auth_level 0`.
That setting checks ordinary signature validity independently of the application's stronger
key-size policy; it is not the runtime's verification setting. The four expected factory refusals
are `TlsError::TRUST_FAILURE` at `tls-trust-anchor`. Actual factory acceptance/refusal evidence
belongs to the runtime conformance results; a valid self-signature alone does not establish it.
These fixtures contain ordinary generated certificates and no handcrafted DER or SPKI.

## Date, SAN and additional key-policy cases

Three additional leaves are signed by the committed `ca.pem` and use the existing `server.key`.
They preserve ordinary RSA-2048/SHA-256, `CA:FALSE`, server key usage and `serverAuth` usage:

| Certificate | Input distinction | Expected runtime result for `localhost` |
| --- | --- | --- |
| [server-not-yet-valid.pem](server-not-yet-valid.pem) | SAN `localhost`/`127.0.0.1`; valid only 2040-01-01 through 2041-01-01 UTC; serial 13. | `TRUST_FAILURE`, verification error 9 before 2040. |
| [server-cn-only.pem](server-cn-only.pem) | CN `localhost`, no SAN; serial 14. | `IDENTITY_FAILURE`; CN fallback is disabled. |
| [server-partial-wildcard.pem](server-partial-wildcard.pem) | CN `localhost`, SAN `DNS:local*host`; serial 15. | `IDENTITY_FAILURE`; partial-label wildcards are disabled. |

The future leaf's exact endpoints are **2040-01-01 00:00:00 UTC** and
**2041-01-01 00:00:00 UTC**. Its not-yet-valid assertion depends on running before the first
endpoint; after that date, update the fixture or its explicit time-sensitive assertion.
The CN-only and partial-wildcard leaves are valid from **2026-11-01 09:16:08 UTC** through
**2046-10-27 09:16:08 UTC**, subject to the issuing CA's own validity interval.
Ordinary OpenSSL CLI verification accepts the CN-only certificate via its default CN fallback;
the runtime's selected `NEVER_CHECK_SUBJECT` policy deliberately requires a SAN.

Three more self-signed CA certificates exercise ordinary algorithm identifiers and RSA parameters:

| Certificate | Public key and signature | Expected factory key-policy result |
| --- | --- | --- |
| [ca-rsa-exponent3.pem](ca-rsa-exponent3.pem) | RSA-2048/exponent 3, SHA-256/RSA signature. | `TRUST_FAILURE` at `tls-trust-anchor`; exponent 65537 is required. |
| [ca-rsa-pss.pem](ca-rsa-pss.pem) | RSA-PSS-2048/exponent 65537; restricted SPKI SHA-256/MGF1-SHA-256/minimum salt 32; SHA-256/PSS signature with salt 32. | Accept this admitted restricted PSS profile. |
| [ca-ed25519.pem](ca-ed25519.pem) | Ordinary Ed25519 key and Ed25519 self-signature. | `TRUST_FAILURE` at `tls-trust-anchor`; this key family is outside the profile. |

These CAs have critical `CA:TRUE` and `keyCertSign,cRLSign`; their private keys are absent.
They use the same 2026-11-01 09:16:08 UTC through 2046-10-27 09:16:08 UTC interval.
The generated PSS certificate's three encoded parameter sequences each occupy 54 bytes, with
default trailer field 1 omitted. Accepting its trust anchor does not establish negotiation of a
particular TLS signature algorithm.

`dateIdentityAndKeyPolicy` in `provenance.json` records exact hashes, metadata, ordinary generation
arguments and intended runtime/factory outcomes. Reproduction uses the same empty configuration
and fresh synthetic CA from the baseline recipe; the committed CA's private key is not distributed.
An equivalent regenerated CA requires replacing its dependent leaves and trust input together.
The three new self-signed CA keys stay outside the public set.

OpenSSL 3.5.9 checked all six certificates' metadata and ordinary signatures, confirmed the three
leaf keys match the existing test credential and observed the future leaf's current-time error 9.
Offline chain checks used `-no_check_time` solely to separate signature validity from the intentional
future interval. These settings are not runtime settings, and expected runtime/factory outcomes
remain separate from these offline fixture checks.

## Certificate-body and chain-depth boundaries

Two ordinary, valid SAN certificates use the existing `server.key` and are signed by `ca.pem`:

| Certificate | DER bytes | TLS 1.2 Certificate body bytes with this leaf alone | Intended boundary |
| --- | --- | --- | --- |
| [server-certificate-max.pem](server-certificate-max.pem) | 65530 | 65536 | Accept the configured 65536-byte body ceiling. |
| [server-certificate-over.pem](server-certificate-over.pem) | 65531 | 65537 | Refuse the body one byte above that ceiling. |

The TLS 1.2 body contains a three-byte certificate-list length, a three-byte leaf length and DER:
`3 + 3 + DER`. These counts exclude TLS record and handshake headers and assume no extra sent
certificates. They describe the TLS 1.2 fixture case specifically.
Each certificate has 254 ordinary ASCII DNS SAN entries: matching `localhost` first, 252 distinct
253-byte names under `.invalid`, then a 162-byte or 163-byte name. Every DNS label has at most
63 bytes and every hostname at most 253. Ordinary OpenSSL config and signing produce these sizes;
no DER or custom OID payload is manufactured. Metadata and generation formulas are in
`certificateBodyAndDepth` in `provenance.json`.

Two PEM bundles test the configured depth of four intermediate CAs. Each starts with a leaf using
the existing `server.key`, followed by its issuers in order toward the root. The root is omitted;
the client explicitly trusts the existing `ca.pem`.

| Bundle | Sent certificates | Verified certificates including root | Ordinary `verify_depth=4` result |
| --- | --- | --- | --- |
| [server-depth4-chain.pem](server-depth4-chain.pem) | Leaf + 4 intermediates = 5 | 6 | Accepted. |
| [server-depth5-chain.pem](server-depth5-chain.pem) | Leaf + 5 intermediates = 6 | 7 | Rejected with X509 error 22. |

The five intermediates have different RSA-2048 public keys and distinct subjects
`QDCXX depth1 intermediate` through `QDCXX depth5 intermediate`. Each has a SHA-256/RSA signature,
critical `CA:TRUE,pathlen:6` and `keyCertSign,cRLSign`. The leaves have ordinary SAN
`localhost`/`127.0.0.1` and `serverAuth` usage. All new CA private keys remain outside this public set.
OpenSSL accepts the longer chain at `verify_depth=5`, separating the intended depth limit from an
invalid signature or broken issuer chain.

The padded leaves and first two intermediates are valid from **2026-11-01 09:39:50 UTC** through
**2046-10-27 09:39:50 UTC**; the chain leaves and remaining intermediates use **09:39:51 UTC**
on those dates. The existing trust anchor's earlier expiry still limits whole-chain validity.
The offline checks establish sizes, ordinary signatures, key matches and verifier-depth behavior.
Actual runtime Certificate-body admission and handshake results require their separate conformance checks.

## RSA-4096 server signature boundary

[server-rsa4096.pem](server-rsa4096.pem) and [server-rsa4096.key](server-rsa4096.key) provide an
additional **intentionally public, unencrypted test-only** server credential. The key is RSA-4096
with exponent 65537. The leaf is signed by the existing `ca.pem`, has CN
`wrong-common-name.invalid`, SAN `localhost`/`127.0.0.1`, critical `CA:FALSE`,
`digitalSignature,keyEncipherment` usage and `serverAuth` extended usage.
Its validity is **2026-11-01 09:47:14 UTC** through **2046-10-27 09:47:14 UTC**; the issuing CA's
own interval also applies.

This key supplies the largest admitted RSA width for testing 512-byte TLS handshake signatures.
The leaf's certificate signature itself uses the issuing CA's RSA-2048 key and SHA-256/RSA.
OpenSSL 3.5.9 checked the new key, certificate/key match and ordinary DNS/IP chains.
`maximumRsaServerKey` in `provenance.json` records generation and exact hashes; actual TLS 1.2
ServerKeyExchange and TLS 1.3 CertificateVerify signature observations are separate runtime tests.
No CA private key is included.
