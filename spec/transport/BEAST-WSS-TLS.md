# TLS boundary for the Beast public-source gateway

[Русская версия](BEAST-WSS-TLS.ru.md). English is authoritative.

## Supported integration boundary

The first public-source gateway can use the pinned [Beast dependency profile](../../cmake/beast-wss-profile/README.md) in a dedicated process. One executor thread owns the TLS work for one live source attempt. An attempt retains its context, provider, socket and buffers through native completion. It finishes teardown before opening another attempt. Sharing the OpenSSL image with additional application TLS users requires a separately qualified shared-host profile. The [QTP TLS resource contract](TLS-RESOURCES.md) remains the authority for its own backend.

The intended route has one configured DNS identity. Resolve and connect to the route separately. Put the configured DNS identity in SNI and require a verified DNS SAN that matches the reference name, with partial-label wildcards and CN fallback disabled. Use explicit trust anchors. Configuration must preserve the existing trust admission and certificate envelope checks, including bounds on input bytes, certificate count and chain. Activate the HTTP upgrade only after verification succeeds. Resumption, early data, client credentials, renegotiation and post-handshake client authentication are outside this profile. [TLS verifier contract](TLS-RUNTIME.md)

The probe uses a private `OSSL_LIB_CTX` with the default provider and explicit RNG settings, then adopts `SSL_CTX_new_ex` output into Boost.Asio `ssl::context`. The selected fixture ciphers exercise ordinary RSA localhost certificates in TLS 1.2 and 1.3. The public source handshake and compatibility of its certificate chain are part of the separate WSS connection qualification. [SSL_CTX_new_ex](https://docs.openssl.org/3.5/man3/SSL_CTX_new/)

## Ownership and progress

Keep the context, provider and library context alive until the last stream is destroyed. Application code must leave the SSL and SSL_CTX application-data slots and the default-password callback's user data to Boost.Asio. Its owning destructors interpret these slots as its own callback storage. Use native SAN verification without a callback that overrides certificate validation.

Create the SSL stream at its final address. Keep at most one data read and one data write outstanding. Start the TLS handshake or orderly shutdown only with the data operations quiescent. For each operation hold the stream and its backing until the completion handler returns. The adapter sets a deadline for each stage. The cutoff requests cancellation and closes the lowest socket on the executor thread. The admitted completion can run later, including after the external caller drops its reference. Continue executor progress until those completions retire. Releasing the public owner or calling `cancel` alone does not establish retirement.

After construction, clear `SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER` and `SSL_MODE_ENABLE_PARTIAL_WRITE`. Keep one contiguous write window at the same address with the same length and contents through the ordered TLS operation. Losing an underlying socket or completing TLS shutdown cannot be used as an application-level delivery acknowledgement. [OpenSSL write requirements](https://docs.openssl.org/3.5/man3/SSL_write/)

## Buffer and allocator evidence

| Storage | Selected size and boundary |
| --- | --- |
| Asio encrypted read and write backing | Two explicit 17 KiB arrays requested by the pinned stream constructor. |
| OpenSSL memory BIO pair | Two independently passed 17 KiB windows. Check the same non-null SSL read/write BIO and its initial outbound write guarantee before using the stream. |
| Qualification plaintext | 4 KiB fixed read backing and 4 KiB fixed write backing per fixture stream. The asynchronous handlers retain those arrays. |

The pinned engine does not check the return from `BIO_new_bio_pair`. The adapter must refuse activation if its construction readback fails. The probe checks the outbound window directly. The input window and the two Asio arrays follow from the fixed constructor arguments and selected source. Their sizes do not bound certificate/provider/RNG allocations, executor operation storage, native socket memory, C++ allocator bookkeeping or RSS. A bounded WSS frame/message buffer and composed I/O costs need their own checks at the WebSocket layer.

The existing QTP requested-storage arithmetic and recoverable allocation admission do not carry through this implementation. The allocation receipt runs the same fixture matrix in a separate process with measurement hooks installed before using OpenSSL. It records only observed requested bytes across the client and local peer plus shared OpenSSL state. The ordinary functional run keeps the default allocator. The measured run supplies no universal allocator upper bound or allocation-failure qualification. [OpenSSL allocator interface](https://docs.openssl.org/3.5/man3/OPENSSL_malloc/)

The following observations come from one instrumented run per profile. They count both loopback endpoints and shared OpenSSL allocations. The final column is read after all fixture streams and private contexts retire, while the process is still alive.

| Profile | Peak requested bytes | Requested bytes at observation |
| --- | ---: | ---: |
| Windows MSVC 19.42 x64 Release /MD | 1,217,761 | 143,606 |
| Linux GCC 13.3 x86-64 Debug ASan/UBSan | 1,233,977 | 144,694 |

## Local qualification

The [probe](../../tests/conformance/beast-wss-dependency/tls_qualification.cpp) uses numeric IPv4 loopback, a fixed verification date and [synthetic fixtures](../../tests/fixtures/transport/tls/README.md). It performs no WebSocket upgrade or external calls. Both protocol versions complete a verified transfer and two-sided TLS close. Untrusted CA, hostname mismatch despite a matching CN, and an expired certificate are refused. The test also cancels an actually pending read and a write stalled by a silent peer. The stream and private context survive cutoff and retire after the canceled completion.

With the [dependency build](../../tools/deps/BEAST-WSS.md) enabled, run the native test suite from its existing build directory:

```sh
ctest --test-dir .work/wss-build --verbose --output-on-failure
```

The I/O probe gives its executor bounded poll intervals and has an external CTest timeout. The evidence applies to this loopback fixture and the pinned build profiles. It does not establish a hard wall-clock guarantee for an arbitrary host or peer.
