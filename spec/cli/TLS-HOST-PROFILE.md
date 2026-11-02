# Closed TLS host profile

[Русская версия](TLS-HOST-PROFILE.ru.md)

The subscription executable's TLS preflight establishes the fixed digest-name closure required by
its controlled OpenSSL 3.5.9 host declaration. It runs before certificate decoding or recovery
construction. It does not detect arbitrary SDK users or qualify a different executable automatically.
The underlying [TLS resource contract](../transport/TLS-RESOURCES.md) and
[recovery contract](../transport/RECOVERY.md) still apply.

## Closed process and image

This profile requires a fresh process, exactly one qualified static SDK image, one serialized SDK
owner thread, one recovery owner/native attempt at a time, and a finite recovery attempt allowance.
An entered preflight or factory failure terminates the run. A retired private context must finish
before its successor is created. The process has no other SDK users, provider/configuration hooks,
configuration autoload, custom error registration, initialized secure heap or foreign allocator hooks.
Ambient identity, providers, properties and algorithm registration remain unchanged through final
SDK retirement. These are application obligations, not properties inferred from the returned values.

The exact configuration, source and static libraries are bound by the
[qualification metadata](../../cmake/tls-profile/openssl-3.5.9.json) and its source/platform manifests.
The selected host is Windows x64 MSVC 19.42 with the release dynamic CRT, or Linux x64 GCC 13.3.0,
with the qualified native binding. Headers and archives must come from that same explicit installation;
system OpenSSL discovery or replacement providers do not satisfy this profile.

The launch environment must have **no `OPENSSL_ia32cap` variable**, including an empty value.
The helper checks presence without copying or parsing the value and without changing the environment.
Windows preserves the caller's last-error slot; Linux inspects only the native getter's returned
pointer. No concurrent environment mutation is permitted. Presence or uncertainty causes terminal
`ENVIRONMENT` refusal before application-controlled SDK entry.

Admission precedes the first **application-controlled** SDK call, after qualified CRT/static-image
initialization. The selected x86-64 startup hook calls `OPENSSL_cpuid_setup` before `main`; its trigger
makes later base initialization return before an environment scan. Final link evidence must retain
the selected startup hook and guarded assembly probe. Source generators alone do not establish a
different executable's startup or dispatch. An absent override also prevents inherited-value parsing
if CPU setup is reached at application entry; it does not retroactively bound pre-main work.
This profile claims no loader/CRT deadline, instruction count, RSS limit or native-call latency.

## Entry and ownership

1. Parse arguments and calculate immutable TLS input ceilings without SDK calls. Help and plain
   transport paths do not enter this helper.
2. Call `preflightRequirements(limits)`, then provide its exact copied shape/revision, all shared
   requested-payload storage and every preflight work coordinate. Exactly one `PreflightOnce` instance
   and invocation are permitted in the process. A refusal consumes that scope.
3. Validate all axes before any debit. Invalid limits, shape, storage or work leave both allowances
   unchanged. Once admitted, debit the complete shared storage and vector before the presence check
   and before SDK entry. No admitted failure refunds storage/work or permits a larger retry.
4. The first application SDK operation is `ERR_clear_error`, followed by `ERR_new`,
   `ERR_set_debug(NULL,0,NULL)`, `ERR_set_error(USER,1,NULL)`, and `ERR_peek_error`. Require the fixed
   marker and clear it. No version/provider/context/allocator query precedes this prefix.
5. Fetch each of the 27 fixed declarations with `EVP_MD_fetch(NULL,name,"provider=default")`.
   Require a non-NULL method, no queued error, the exact provider name `default`, and actual
   `EVP_MD_is_a==1` hash membership for all 71 aliases. Release the owned reference and check ERR
   after every declaration. Alias-list enumeration does not replace these membership checks.
6. Return only detached numeric limits and reserved bytes. Every failure prevents recovery entry;
   there is no plain downgrade. Method references remain owned during provider/name borrows and
   retire exactly once on every return. No provider, method, context, callback or string borrow escapes.

The complete fixed names and aliases are in the
[compiled catalogue](../../tools/subscribe/tls_digest_catalog.hpp); there is no runtime-selected subset.

The persistent shared reservation stays charged through SDK retirement. Later recovery instance
storage and its public factory/start/entry/close work are separately funded; preflight neither
replenishes them nor changes the progress/drain work vector. Shared storage is reserved once at host
scope, without charging the same shared envelope again for each later private recovery instance.

## Twenty-field host declaration

| Field | Bound | Meaning |
| --- | ---: | --- |
| namemapGroups | 6297 | Retained name/group attempts, including orphan/failure histories. |
| queryCount | 2 | Empty and `provider=default` historical query union. |
| queryBytes | 16 | Maximum stored query bytes excluding NUL. |
| propertyNames | 6 | Fixed builtin name atoms. |
| propertyValues | 22 | Complete selected static value superset, including yes/no. |
| propertyStringBytes | 28000 | `1000*(6+22)`, including NUL terminators. |
| propertyStringMaxBytes | 1000 | Maximum single property-string payload. |
| participatingThreads | 1 | One serialized SDK owner through teardown. |
| globalThreadHandlers | 9 | ERR prefix 5 + ambient RAND 2 + private RAND 2. |
| ownerThreadHandlers | 9 | The same nodes on the one owner thread. |
| ambientThreadPairs | 1 | Default context and one thread. |
| ambientGenerators | 2 | Ambient public/private generator slots. |
| concurrentEntries | 1 | No concurrent or reentrant SDK entry. |
| errorThreads | 1 | One dependent ERR-state thread. |
| errorCapacityBytes | 0 | No external/preexisting ERR data; SDK-owned buffers remain separately funded. |
| errorSlotCapacityBytes | 0 | External contribution; the public formula retains its own slot minimum. |
| errorDebugBytes | 0 | No preexisting debug strings; the marker uses NULL debug/format inputs. |
| failedPrivatePrefixes | 3 | Once-per-run recovery reserve, not three retries after factory failure. |
| failedAmbientPrefixes | 4 | Once-per-run potentially stranded ambient-prefix reserve. |
| errorTableEntries | 1904 | Full 39-array static builtin error-table upper, not validator floor 1773. |

The namemap bound is `J = 1404 + 27*71 + 10*6 + 12*243 = 6297`: one retained legacy import,
preflight MD alias attempts, then finite initial ambient RAND/CIPHER opportunities. A first retained
legacy entry prevents another import; empty failed passes retain nothing. Preflight failure is
terminal, so no failed prefix is silently retried. Successful complete alias checks ensure later
construction finds existing IDs before adding names. Further digest fetches are allowed: their
number is not assumed zero. Unknown requested OIDs do not publish arbitrary request strings.
The profile does not bound total lifetime feed CPU by this retained-storage argument.

Five ERR getter opportunities precede the first proven committed ERR state. The fixed marker then
proves reuse for later peek/clear operations. Raw MD metadata construction starts no RAND/ASYNC/FIPS
context or digest computation; it adds no handler class. Later ambient/private RAND contribute two
each, giving `H = 5+2+2 = 9`. Failed private graphs retire before successors. The public checked
`tlsRequirements(limits,closedHostLimits())` shared result is **125582389 bytes** on the qualified x64
profile. These are logical requested-payload bytes, not resident memory measurements.

## Separate preflight work

The copied TLS shape and preflight revision bind eight finite transitive classes. They are application
work coordinates, not generic SDK call credits, public `PREPARE`/`ENGINE_ENTRY` aliases or time units.

| Class | Whole-run units | Included body |
| --- | ---: | --- |
| ENVIRONMENT_GATE | 1 | Qualified presence getter, no override-value copy/parse. |
| ERROR_PRIME | 1 | Fixed cold ERR marker/init/string-load graph. |
| DIGEST_FETCH | 27 | Complete D1–D7 graph below, including entered failures. |
| PROVIDER_IDENTITY | 27 | Borrowed provider/name access, at most eight compared bytes. |
| ALIAS_MEMBERSHIP | 71 | Nonzero method-ID lookup in the actual namemap hash. |
| METHOD_RELEASE | 27 | Owned descriptor/ref retirement and bounded leaf cleanup. |
| ERROR_OBSERVE | 125 | 27 post-fetch + 71 post-alias + 27 post-release observations. |
| ERROR_CLEAR | 1 | Clear fixed 16 ERR rows without refunding retained buffers/state. |

One DIGEST_FETCH includes the following full cold graph; successful initialization reuse only
reduces actual work. Fixed source catalogues, qualified normal native once/lock/refcount semantics,
one provider/thread and terminal failure are required throughout.

| Node | Transitive body and ownership |
| --- | --- |
| D1 | Generic fixed-name/query EVP fetch, lookup/cache/construction and temporary-store cleanup on every exit; internal candidates and outer owned reference are distinct. |
| D2 | Cold default `context_init`: native key/lock/ex-data, four method stores, decoder cache, provider/RAND/property/namemap/definition/BIO/self-test/indicator holders, thread/child-provider bookkeeping and optional empty compression holder. Partial `context_deinit_objs` frees completed non-NULL holders, method stores before provider retirement, then initialized ex-data/key/lock and zeroes failure state. It creates no worker, child provider or second private TLS context. |
| D3 | Fixed default fallback/provider activation, dispatch walks, provider refs/list copies and operation-bit allocation/failure; no configuration/module/user-provider catalogue. No successful operation bit is assumed. |
| D4 | Default provider context/BIO_METHOD setup and fixed BIO/seeding dispatch-pointer installation. Failed setters free partial owners. Installed entropy/nonce/BIO callbacks are not invoked; no file/socket/BIO I/O is opened. |
| D5 | Copy at most 126 ALG + 4 ALGC cipher rows and a terminator into static image storage. Four capability calls: two fixed SHA1 word checks and up to two SHA256 NULL assembly probes. No EVP_CIPHER construction, alias registration, random generation or dynamic exported array. |
| D6 | At most 27 raw MD declarations/71 alias attempts, finite dispatch/legacy/orphan lookup, provider refs and constant metadata. A five-entry/four-key parameter array requires at most 20 key comparisons. No digest newctx/init/update/final/squeeze or data processing. Failed candidates retire within the shared/current envelope. |
| D7 | Two historical queries, canonical definitions/atoms, bounded sparse/cache/map paths, old/current replacement and failure cleanup. Namemap/orphan backing is bounded by J; no external expression or callback catalogue enters. |

The underlying fixed fetch domain has at most eight sparse-path levels, 628 retained static
declaration groups, two queries per group and 57 distinct definition strings (at most 70 bytes/four
clauses each). Query flush visits at most `628*2` records. Normalized name/hash keys are bounded by
63/64 bytes; a lookup visits at most 2048 hash slots with at most `2048*64` compared bytes. Orphan-list
scans/copies use J6297. Legacy OBJ_NAME lookup uses its 436-item catalogue and fixed alias-depth
guard. Failed candidates, old/new backing and current raw methods remain separately funded.

These bodies bind the pinned `crypto/evp/{digest,evp_fetch,evp_lib}.c`, `crypto/context.c`,
`crypto/provider_{core,child}.c`, `crypto/core_{fetch,algorithm}.c`, property/cache sources and
`providers/defltprov.c`. The cold leaves additionally bind `crypto/thread/internal.c`,
`crypto/{comp_methods.c,comp/c_zlib.c}`, `providers/common/{provider_util,provider_seeding,bio_prov}.c`,
`providers/implementations/ciphers/cipher_aes_cbc_hmac_sha{1,256}_hw.c`, `include/crypto/aes_platform.h`
and `crypto/aes/asm/aesni-sha256-x86_64.pl` from the exact source manifest. The startup binding uses
`crypto/cpuid.c`, `crypto/x86_64cpuid.pl` and `crypto/perlasm/x86_64-xlate.pl`.

The selected SHA256 assembly generator checks a NULL first input and branches directly to
`.Lprobe: ret` in both generated AVX/non-AVX variants, before payload/key/hash-state dereference,
processing loops or variable scratch. These capability probes do not authorize non-NULL data calls.
The qualified configuration disables ZLIB/BROTLI/ZSTD backends; only an empty compression holder may
be constructed. Thread bookkeeping creates native synchronization objects, not a running worker;
child-provider bookkeeping creates an empty holder, not another provider/context.

Across 27 complete units, conservative internal bounds include 27 cold prefixes/provider exports,
3510 cipher-row visits, 27 terminator copies, 108 capability calls, 54 guarded NULL probes, 729 raw
MD constructions and 1917 alias attempts. These bounds explain the unit bodies; they are not extra
top-level axes. Existing shared/current/failure terms cover their requested owners. Neither these
counts nor normal successful tests qualify arbitrary allocator/native corruption or every fault prefix.

The source binding applies only to the exact pinned implementation/configuration and selected
linked startup/probe image. Static SDK metadata, absolute headers/libraries and final link evidence
must be checked before use. A different source, mutation domain, provider, ABI or native binding
requires a new qualification; larger numeric allowances alone do not establish it.
