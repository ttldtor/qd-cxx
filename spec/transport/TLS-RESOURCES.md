# Controlled TLS resource contract

[Русская версия](TLS-RESOURCES.ru.md). English is authoritative.

The pure resource implementation computes finite requested-storage and work requirements without
entering OpenSSL. Its source-bound native checks are recorded in [TLS validation](../../tests/conformance/transport/TLS-VALIDATION.md).
TLS backend, whole-run accounting, security/credential execution and installed consumers remain
separate qualification work. No production TLS availability or release follows from the resource tests.

## Choosing a supported host profile

The first profile requires a caller-selected controlled host using one qualified OpenSSL image.
OpenSSL 3.5.9 static source/configuration, `no-autoload-config`, `no-ec2m`, `no-ml-kem`, built-in
default provider, query domain, selected RNG and native/compiler profile belong to its identity.
An API-compatible alternative SDK is not automatically qualified. The
[pinned upstream source](https://github.com/openssl/openssl/tree/45e844fa2a14ec92d146bd8f5778ac130b6625fb)
is the source authority; release archive SHA-256 is
`603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`.

The embedding application must bound all relevant users of that same image, including other
contexts/threads, current entries, failed initialization prefixes and retained history. The profile
requires fixed built-in method/provider/query/RNG behavior, the default allocator, an uninitialized
secure heap, controlled global registries and the same effective ambient identity at dependent
operations. The adapter must not rewrite SDK source, install provider/RNG hooks or mutate process-wide
allocator/provider/RNG settings. Private-context setup must stay within the selected profile.

On Linux this qualification also requires kernel 4.8 or newer and the qualified, unmodified native
entropy dispatch with its normal entropy source available. The finite `GETRANDOM_ATTEMPT_BOUND`
depends on those premises; it does not apply unchanged to an older kernel or another entropy route.
A bounded number of requests does not bound syscall duration or time waiting for entropy.
Other explicit contexts are not forbidden; all relevant shared effects and dependent create/use/free
operations must preserve the declared qualification and effective ambient identity.

A private `OSSL_LIB_CTX` is not isolation from every process-global registry, thread resource,
ambient default or other context. Host/profile, participating-thread/context, historical/shared and
ERR declarations must include our reserved increments and other relevant users. They cannot be
assumed detectable or inferred from one runtime. Their truth through final retirement is an explicit
embedding obligation. Context/thread cleanup has its own OpenSSL lifetime rules:
[official library-context documentation](https://docs.openssl.org/3.5/man3/OSSL_LIB_CTX/).

## Obtaining and comparing requirements

[`tlsRequirements`](../../include/qd/transport/tls_resources.hpp) computes requirements for immutable
input ceilings and the complete declared host qualification. Before SDK construction,
compare the returned requested-storage scopes and every work coordinate with caller limits. A
matching outer count does not authorize a larger or differently qualified graph. Requirement overflow,
unsupported profile or insufficient capacity must fail before the corresponding action.

| Input | First-profile bound and meaning |
| --- | --- |
| `T` | Immutable trust-byte ceiling `trustBytes` in 1..524,288; actual admitted bytes fit that ceiling |
| `A` | Anchor ceiling `anchors` in 1..256; every actual ordinary CERTIFICATE block counted before decode |
| `C` | `certificateBytes`: peer Certificate body cap exactly 65,536 before parsing |
| `Cr` | `certificateRequestBytes`: CertificateRequest body cap exactly 65,536 |
| `I` | Identity ceiling `identityBytes` in 1..255 for a nonempty ASCII/A-label or numeric reference identity |

These are proof/profile limits, not recommended application defaults. The twenty-field host
qualification must also bound:

| Fields | Required scope |
| --- | --- |
| `J` / `namemapGroups` | Ambient registration-attempt/orphan highwater, including failed passes |
| `Qh` / `queryCount`, `Lqh` / `queryBytes` | Historical query union count and maximum query extent |
| `An` / `propertyNames`, `Av` / `propertyValues`, `Batoms` / `propertyStringBytes`, `Latom` / `propertyStringMaxBytes` | Ambient property atom counts, total stored bytes and maximum extent |
| `p` / `participatingThreads`, `hg` / `globalThreadHandlers`, `ho` / `ownerThreadHandlers` | Participating-thread highwater and global/current-owner handler nodes |
| `t` / `ambientThreadPairs`, `k` / `ambientGenerators` | Dependent ambient context/thread pairs and their generator owners |
| `Qe` / `concurrentEntries`, `Ethreads` / `errorThreads`, `ErrCapacitySum` / `errorCapacityBytes`, `ErrCapacityMax` / `errorSlotCapacityBytes`, `Dh` / `errorDebugBytes` | Concurrent entries and dependent ERR threads, capacities and debug backing |
| `Fpriv` / `failedPrivatePrefixes`, `Famb` / `failedAmbientPrefixes`, `Herr` / `errorTableEntries` | Still-reserved stranded prefixes and shared error-table history |

Zero is zero; it never means unlimited. The synthetic twenty-field calculator fixture is arithmetic
test data, not observed host state or a suggested configuration. Readback of some SDK facts does not
prove the external history/concurrency declarations or a different native profile.

## Requested dynamic backing

The SDK requested-backing metric includes every reached heap object, backing capacity, embedded heap-object array,
old/candidate overlap, temporary, retired owner and explicitly selected variable `alloca`/ASM area.
It excludes ordinary fixed native frames, physical allocator bookkeeping/retention, RSS, instructions
and hard wall-clock time. The unknown total of fixed compiler/native frames is not zero. Adapter,
session/composer, credential-provider application storage and common native-runtime scopes require
their own explicit accounting; none disappears inside an SDK coefficient.

The source-qualified local allowance is deliberately conservative across several phases. At the
maximal input fixture it is 601,607,223 bytes before explicit shared/ERR/host terms. This is not an
allocated arena, measured RSS or a default memory recommendation. Actual adapter `sizeof`, capacities,
native dispatch and whole-run comparisons still require runtime qualification. Sanitizer overhead is outside
the production requested-SDK metric; an ASan run does not prove direct glibc `qsort` dispatch.

The complete storage requirement includes local backing, touched ambient state, concurrent candidates,
thread/registry metadata, global initialization/error tables, ERR backing and persistent failure history.
Do not duplicate a shared container per runtime or omit it because a private context exists. Possible
stranded private initialization retains 40,865,186 bytes per declared prefix plus the separate shared
history until actual reclamation is established. An error return or paid unwind does not justify blind
refund, global cleanup or a hidden attempt quota.

## Named work vector

Work is a shape-qualified vector of the following 25 coordinates. They are bounded algorithm,
direct-byte/metadata, RNG and qualified-native requests, not CPU instructions or milliseconds.
The total serial `malloc`/free count is not the chosen metric; it is neither proved nor assigned zero.

| Unit | Bounded meaning |
| --- | --- |
| `PREPARE` | Selected shared/private initialization, profile checks, catalog/ERR setup and partial unwind |
| `TRUST_READ` | Bounded ordinary-CERTIFICATE trust framing/reader opportunity; final extra slot is EOF/framing only |
| `CERTIFICATE_TREE` | One bounded initial/partial certificate tree, Names and ASN.1 processing |
| `DECODER_CACHE` | One source-closed initial public-key decoder-cache preparation |
| `FAMILY_ATTEMPT` | One selected bounded public-SPKI family decoder attempt, including failed-prefix cleanup |
| `KEY_GUARD` | Cached key/type/completeness/raw-SPKI and signature-parameter guards |
| `ANCHOR_STORE` | One guarded anchor insertion and bounded comparison/fingerprint work |
| `CHAIN_VERIFY` | One same-context verification after every required certificate guard |
| `INITIAL_FLIGHT` | The selected finite initial TLS 1.2/1.3 state-machine graph |
| `ENGINE_ENTRY` | One bounded memory-BIO entry/classification wrapper |
| `RECORD_CONTROL` | One bounded record or completed control-message opportunity |
| `CLOSE_CONTROL` | One orderly close-notify transition |
| `ABORT_CONTROL` | One idempotent cutoff/abort transition |
| `RETIRE_GRAPH` | One qualified normal/failed-prefix graph retirement, including transitive cleanup |
| `DIRECT_INPUT_BYTES` | Direct trust-prefilter/feed byte visits |
| `DIRECT_TEXT_BYTES` | Direct reference-identity validation bytes |
| `DIRECT_METADATA_VISITS` | Direct numeric-field validation and live-owner/attempt checks |
| `RNG_EVENT` | One bounded source RNG opportunity or first generator preparation prefix |
| `RNG_GENERATE` | One selected bounded CTR-DRBG generation |
| `RNG_INSTANTIATE` | One selected bounded CTR-DRBG instantiation |
| `RNG_RESEED` | One selected bounded CTR-DRBG reseed |
| `RNG_POOL` | One bounded SDK entropy-pool acquisition |
| `RNG_AES_BLOCKS` | Qualified AES block requests at fixed shape |
| `NATIVE_ENTROPY_POOL` | One selected bounded native-backend pool acquisition |
| `GETRANDOM_ATTEMPT_BOUND` | Qualified Linux progress/failure request bound, not all native syscalls |

ASN.1, Name, sort, swallowed errors and metadata work are transitively included at their source call
sites under finite proof classes. No arbitrary opaque SSL-call credit replaces them. Ordinary native
allocator/synchronization work inside a class does not turn this vector into a total syscall counter.
Native/compiler/API-set/slot/default-mapping and normal sort/comparator premises remain conditional.
`GETRANDOM_ATTEMPT_BOUND` may conservatively remain in a Windows vector; it is not a Windows syscall claim.
Selected recurring AEAD/NST/KeyUpdate has no new key-decoder/RNG-engine graph under this profile;
that source-qualified nonreachability is not inferred from elapsed connection time.

## Phase admission and retirement

Use `F` for factory work, `H` for the one initial-handshake graph, `E` for each SDK entry, and `R` for
owned retirement. These phase labels are mathematical requirements, not frozen public API names.

| Boundary | Required admission |
| --- | --- |
| Factory | Atomically fund `F+R` before any SDK allocation |
| First indivisible handshake entry | Fund `H+E`; preserve the single-attempt `H` permit across WANT |
| Later SDK entry, including WANT retry | Fund fresh `E` from this turn; `H` cannot authorize a new attempt |
| Orderly TLS close entry | Fund `E+closeControl` for each attempt |
| Public cutoff/deferred disposal | Retain copied owned `R`; no caller-budget alias or nested new turn |

Precharge checks profile identity, arithmetic and every coordinate before subtracting any coordinate
or changing phase/ownership. Zero/one-short refuses with typed failure before the indivisible action.
No wraparound, saturation, hidden refill or WANT/failure refund is allowed within a turn. New explicit
turns may supply new work after the prior call returns; this steady-entry bound is not a lifetime feed quota.

`R` must be copied into stable internal ownership before SDK allocation. It survives failed factory
prefixes, public-owner cutoff, in-flight operations and deferred free without retaining a caller work
pointer. Actual graph cleanup consumes it once after the last required in-flight/native owner drains.
Persistent shared failure storage remains reserved independently of paid retirement work.

## Technical appendix: numerical binding

The lower-camel notation below denotes the same coordinates as the uppercase `TlsWorkUnit` table.
For the selected source shape, `d=912+912²=832656`, `Np=floor(C/37)+1`. Let `RV(G)` supply
`rngEvent=G`, `rngGenerate/rngInstantiate/rngReseed=3G`, `rngPool/nativeEntropyPool=6G`,
`rngAesBlocks=33213G`, `getrandomAttemptBound=221202G`. Coordinates are not summed into a scalar.

`F` contains `prepare=1`, `trustRead=A+1`, `certificateTree/decoderCache/keyGuard/anchorStore=A`,
`familyAttempt=A*d`, `directInputBytes=T`, `directTextBytes=I`, `directMetadataVisits=25`,
and `RV(120*A*d+9)`. `H` contains `certificateTree/decoderCache=Np`, `familyAttempt=Np*d`,
`keyGuard=Np+1`, `chainVerify=1`, `initialFlight=1`, and `RV(120*Np*d+2405)`.
`E` contains `engineEntry=1`, `directInputBytes=17408`, `directMetadataVisits=2`, and
`recordControl=2+floor(245960/4)+floor(245960/5)=110684`. `R` contains `abortControl=1`, `retireGraph=1`.
Unmentioned coordinates are absent for that phase; this is not a claim that excluded metrics are zero.

The complete storage formula retains the qualified local/ambient/concurrent/thread/global/ERR/
failed-prefix decomposition and its source/platform identity. No unconditional instance-only heap
claim follows.
The [runtime foundation](TLS-RUNTIME.md) specifies trust, callback and pending-I/O requirements.

<a id="storage-equations"></a>

## Complete requested-storage equations

This appendix fixes the arithmetic implemented by `instanceBytes`, `sharedBytes`, `nameBytes`,
`decoderBytes`, `stackBytes`, `buckets` and `hashBytes` in
[`tls_resources.cpp`](../../src/transport/tls_resources.cpp). The input letters are mapped to all
five resource ceilings and twenty host fields in the tables above. All values are nonnegative
integers; every addition, multiplication, encoded-size intermediate and required representability
check must succeed before admission. Integer division rounds down. A result larger than `size_t`
also fails. An invalid or overflowing expression never yields a smaller usable allowance.

```text
L = 56
q(x) = floor(x/2)
g(x) = 4*floor((x+3)/3)
S(n) = 64+16*n
b(n) = 4*n+16
LH(n) = 104+24*n+8*b(n)
Np = floor(C/37)+1
N = A+Np
Ns = N+1
Nr = floor(Cr/4)+1
B = T+C
X = max(T,C)

NameRetained(x) = 72+S(floor(x/6)+1)+96*(floor(x/6)+1)+12*x+g(x)
NamePeak(x) = NameRetained(x)+S(floor(x/2)+1)+64*(floor(x/2)+1)
              +16*(floor(x/6)+1)+S(floor(x/6)+1)+64*(floor(x/6)+1)
              +16*(floor(x/6)+1)+89*(floor(x/6)+1)+5*x
              +24*(floor(x/6)+1)+9*x+2*g(x+1)+24
Decoder(x,k) = 768+3*g(2*x+16384)+x+max(145+2*x+176*q(x),k)+5616

M = 39308258
Z = 1554392
Trees = 802*Ns+3*B+176*q(B)+280*Ns+32*B+140*Nr+32*Cr+192*B
Keys = 19596*N+8*B
Montgomery = 3224*N
VerifyPointers = S(Np)+S(A)+2*S(6)
Containers = 9330+464+34268+8640+S(A)+33360+VerifyPointers+124207+4*(I+1)+S(1)
WireRecords = 69632+354072+240+2121528+368+8360+63281+67500+216
Current = 5244680+max(Decoder(T,34481016),Decoder(C,20412616))+10560
          +NamePeak(X)+192*X+2*g(X+1)+2*S(q(X))+24+8*floor((X+7)/8)
          +4*X+82+2*X+3+21710+272+1160+393716+X+376
SignatureShell = 3324+3*X
SignatureDer = 336+16*floor((X+7)/8)+2*g(2*(X+16))
CertificateCrypto = SignatureShell+SignatureDer+600+45293259+21710+41576
Crypto = max(63407559,CertificateCrypto)
SortCount = q(X)+1
SortDirect = 24*SortCount
SortIndirect = 16*SortCount+24
SortScratch = SortDirect <= 1024 ? 0 : 2*SortDirect
Instance = M+Z+Trees+Keys+Montgomery+Containers+WireRecords+Current+Crypto+SortScratch

NameMap = 33048+152*J+104*min(J,2048)+392
Maps = 4*(80+2*L)+1080*628+628*(192+Qh*(72+Lqh)+8*b(Qh))
Atoms = 48+L+LH(An)+LH(Av)+S(An)+S(Av)+24*(An+Av)+Batoms
AmbientRng = 2592+69+(3+k)*L+2424*k
Ambient = 1710+NameMap+Maps+120696+87292+14710+Atoms+AmbientRng
AtomCandidate = 24+Latom+24+S(An)+S(Av)+8*b(An)+8*b(Av)
AmbientCurrent = 40+24+S(628)+LH(Qh)+1024+296+75424+48+Lqh+8*b(Qh)
                 +104+L+S(J)+82+328+8*b(57)+24+AtomCandidate+36976+2536
Threads = 16+L+S(p)+8*p+32*hg+S(p)+8
Global = 35168+24+8*b(436)+LH(Herr)+8*b(Herr)+24+8*L+256+16
Errors = 904*Ethreads+ErrCapacitySum+16384*Ethreads+Dh
         +32*278*Ethreads+Qe*max(ErrCapacityMax,1024)
FailedPrivatePrefix = M+Z+2536 = 40865186
FailedAmbientPrefix = 2536
Shared = Ambient+Qe*AmbientCurrent+Threads+Global+Errors
         +Fpriv*FailedPrivatePrefix+Famb*FailedAmbientPrefix
Total = Instance+Shared
```

`SortIndirect` is checked even though the selected private arrays have width at most 24 and use
`SortDirect`. Two simultaneously live native-sort requests are retained. The two fixed 17 KiB SDK
BIO payloads and two adapter wire windows are included once in `WireRecords`; composition with common
transport resources must not add a second copy of those same owners. Adapter object fields, retained
endpoint/snapshot/session/plaintext/DNS/provider-application storage remain separate scopes.

The host-domain checks also require `J`, `An`, `Av`, `p` and `ErrCapacityMax` to fit the qualified SDK
signed-int domain; `Qe>=1`, `1<=Ethreads<=p`, `Herr>=1773`, `Qh>=2`, `Lqh>=16`, `An>=6`, `Av>=22`,
and `k<=2*t` using checked arithmetic. `ho` is an independent bound for owner-thread handler visits;
`t` bounds ambient context/thread pairs. They are part of the copied shape and work qualification
although this storage equation uses `hg` for total handler nodes and `k` for generator owners.
The checked metadata sum `J+An+Av+p+hg+ho` must also be representable. Loose independent maxima do
not acquire extra undocumented cross-field constraints. All declared history includes this admission's
required increments. Arithmetic checks cannot establish the truth of external host declarations.

The constants conservatively combine both qualified platforms: lock extent 56, ERR_STATE 904 and
error debug string capacity 278. They do not silently qualify another ABI, compiler, CRT or SDK.
For the maximal five-ceiling fixture, `Instance=601607223`; the synthetic host fixture in the resource
tests adds `Shared=44048771`, hence `Total=645655994`. These are equation checks, not allocated arenas
or measured peaks. The fixed-automatic-frame exclusion and conditional native/host premises above
still apply.

### Source-to-term map

The [SDK profile and source manifest](../../cmake/tls-profile/openssl-3.5.9.json) identify the exact
OpenSSL source/configuration, installed assets and conditional native-sort bindings. The following
links refer to that pinned source revision; path, function and entry line identify a derivation
boundary, not a claim that one source line alone proves a coefficient. The coefficients cover the
full qualified transitive owner graph, including old/candidate backing, cleanup and failures.

| Terms | Qualified owner/algorithm boundary |
| --- | --- |
| S, b, LH | [crypto/stack/stack.c:227 OPENSSL_sk_new_reserve](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/stack/stack.c#L227). Stack/header/backing growth; hash header/nodes/bucket candidates also use crypto/lhash/lhash.c:63 OPENSSL_LH_new and :242 expand. |
| M, Z, Maps, NameMap | [crypto/evp/evp_fetch.c:391 evp_generic_fetch](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/evp/evp_fetch.c#L391). Builtin method/query/name/property supersets: 628 methods, 912 decoder alternatives, 22101 cache keys. The retained/current split keeps incomplete fetches and old/candidate owners. |
| Trees, NamePeak, Current | [crypto/asn1/tasn_dec.c:188 asn1_item_embed_d2i](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/asn1/tasn_dec.c#L188). Joint 176-byte schema-node envelope and input-sized BER/encoding owners; two nested Name roots and six-link verification bound. |
| NameRetained, NamePeak | [crypto/x509/x_name.c:141 x509_name_ex_d2i](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/x509/x_name.c#L141). Name entries, retained/current encodings, nested RDN/DER_ENC sort owners; lazy encoding enters x509_name_ex_i2d at line 214. |
| Decoder, Keys | [crypto/encode_decode/decoder_pkey.c:71 decoder_construct_pkey](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/encode_decode/decoder_pkey.c#L71). Initial public-key family attempts are funded before type rejection. The reader graph has depth two; complete schema/key owners and failures remain charged. |
| Decoder, M, Z | [crypto/encode_decode/decoder_lib.c:556 OSSL_DECODER_CTX_add_extra](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/encode_decode/decoder_lib.c#L556). Decoder-cache retained/current union and candidate instances; removal of ML-KEM does not assume every GF2m-named assembler symbol absent. |
| Montgomery, Crypto | [crypto/bn/bn_mont.c:260 BN_MONT_CTX_set](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/bn/bn_mont.c#L260). 3224 bytes retained per distinct RSA key. Initial key decoding uses the separate 5824-get/66-frame/128-word source envelope; later arithmetic uses 5504/62 with EC128/RSA1024-word maxima. |
| SignatureShell, SignatureDer, CertificateCrypto | [crypto/ec/ecdsa_ossl.c:419 ossl_ecdsa_verify](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/ec/ecdsa_ossl.c#L419). Signature bytes and TBS encoding use the actual trust/peer certificate input ceiling, including pre-arithmetic DER owners; 21710 covers the bounded raw-PSS guard/reparse. |
| Containers, Crypto | [crypto/x509/x509_vfy.c:1953 internal_verify](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/x509/x509_vfy.c#L1953). All anchors/leaf/untrusted entries are guarded before one same-context X509_verify_cert at line 284. Depth 4, chain 6, search 8 and TRUSTED_FIRST exclude alternate-path/callback widening. |
| Containers, WireRecords | [ssl/ssl_lib.c:690 SSL_new](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/ssl/ssl_lib.c#L690). SSL/store/session/identity owners, four fixed windows, old/current/candidate record backing, bounded handshake/transcript owners and three empty record digest contexts. |
| WireRecords | [ssl/record/methods/tls_common.c:1397 tls_new_record_layer](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/ssl/record/methods/tls_common.c#L1397). Single pipeline, fixed fragment/record limits and retained input/output/control backing; no socket access through the memory BIO. |
| Atoms, AtomCandidate | [crypto/property/property_string.c:253 ossl_property_name](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/property/property_string.c#L253). Shared interned name/value nodes and strings, total/max string bytes and overlapping stack/hash candidates. |
| AmbientRng, FailedPrivatePrefix, FailedAmbientPrefix | [crypto/rand/rand_lib.c:829 RAND_get0_primary](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/rand/rand_lib.c#L829). Qualified READY CTR/AES-256/SEED-SRC ownership; an unpublished primary may strand method/provider references. Failed private prefixes retain M+Z+2536 until demonstrated reclamation. |
| Threads | [crypto/initthread.c:385 ossl_init_thread_start](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/initthread.c#L385). Global/current-owner handler history and participating-thread backing; local cutoff does not authorize foreign thread cleanup. |
| Global, Errors | [crypto/err/err.c:718 ERR_get_state](https://github.com/openssl/openssl/blob/45e844fa2a14ec92d146bd8f5778ac130b6625fb/crypto/err/err.c#L718). Historical error table, per-thread state and retained/reset/replacement slot capacities; debug call-site extents use the qualified installed archives. |

## Resource API and diagnostics

The resource header declares `TlsResourceLimits` with the five explicit ceilings above,
`TlsHostLimits` with all twenty mapped host fields, and the 25 `TlsWorkUnit` enumerators shown in
the work table. `COUNT` is a sentinel, not a coordinate; `TLS_WORK_UNIT_COUNT` describes the array.
Zero-initialized input ceilings are unconfigured and invalid rather than usable defaults. Host
fields must satisfy their declared domains and include required increments; zero does not relax minima.

`tlsRequirements` is declared to return owned `TlsRequirements`: `shape`, `sdk`, `factory`,
`initialHandshake`, `initialStart`, `entry`, `close`, `retirement`, `factoryAdmission`.
`TlsSdkRequirements` separates `instanceBytes`, `sharedBytes`, `totalBytes`,
`failedPrivatePrefixBytes`, `failedAmbientPrefixBytes`. Shared backing belongs to its declared host
scope, not an isolated duplicate per runtime. Input ceilings may conservatively exceed actual
immutable trust/identity input; actual framing/anchor-count rejection still precedes decoding.

`TlsWorkShape` copies `profileRevision`, `limits`, `host`; `TLS_RESOURCE_PROFILE_REVISION` identifies
the model edition. `TlsWork` carries `shape` and `remaining`; `TlsWorkVector` carries `units`.
`admitTlsWork` is declared for atomic identity/coordinate admission; `addTlsWork` is declared for
checked coordinate addition and does not itself establish matching qualification of arbitrary vectors.
The runtime must supply its genuine expected shape/need, not accept a caller's verification assertion.

`TlsResourceError` declares `NONE`, `INVALID_CONFIGURATION`, `ARITHMETIC_OVERFLOW`, `SHAPE_MISMATCH`,
`WORK_LIMIT`; `TlsResourceDiagnostic` declares static `error`/`stage` values without caller/peer text.
The pure implementation and overflow/one-short/shape checks have compiled native evidence. These
checks do not execute TLS, providers or callbacks, and do not prove TLS-disabled package isolation
or completed runtime integration. See the separate qualification status.

## Exact resource declarations

These excerpts are declarations in namespace `org::ttldtor::qdcxx::transport` from the actual
standard/project-only header. They are not complete programs or synthetic host configurations.
The resource implementation/tests compile these functions without executing OpenSSL. Supply your
own valid five ceilings and twenty truthful host maxima; the arithmetic fixture is not a defaults recipe.

```cpp
[[nodiscard]] std::expected<TlsRequirements, TlsResourceDiagnostic> tlsRequirements(TlsResourceLimits limits,
                                                                                    TlsHostLimits host) noexcept;
```

```cpp
[[nodiscard]] std::expected<void, TlsResourceDiagnostic> admitTlsWork(TlsWork &work, const TlsWorkShape &shape,
                                                                      const TlsWorkVector &need) noexcept;
```

```cpp
[[nodiscard]] std::expected<TlsWorkVector, TlsResourceDiagnostic> addTlsWork(const TlsWorkVector &left,
                                                                             const TlsWorkVector &right) noexcept;
```

## Implementation evidence boundary

Arithmetic and admission are checked separately: 8 tests and 835 assertions on each OS, plus a 32-case independent oracle. The focused runtime suite exercises actual TLS 1.2/1.3 and ownership boundaries; the [evidence matrix](../../tests/conformance/transport/TLS-VALIDATION.md) separates these observations from SDK source reasoning. Native tests do not measure the entire SDK heap, RSS or hard wall time; host preconditions and fixed-frame exclusions remain in force.
