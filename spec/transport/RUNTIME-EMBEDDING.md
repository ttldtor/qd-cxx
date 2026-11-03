# Runtime embedding and bootstrap ownership {#runtime-embedding}

[Русская версия](RUNTIME-EMBEDDING.ru.md) · [TCP runtime](TCP-RUNTIME.md) · [Feed API](../../docs/FEED-API.md)

`ApiRuntime(RuntimeOptions)` owns its bootstrap and requires main-thread construction
before other application threads. The explicit
`ApiRuntime(transport::RuntimeBootstrap &, RuntimeOptions)` overload borrows a live
bootstrap prepared by the host before those threads started. Both runtime forms
must be constructed and destroyed on that bootstrap's main/creator thread.
The host retains ownership in borrowed mode; its SDK state stays live until the
borrowed runtime is destroyed, including after `closeAndAwaitTermination()`.

## Existing requirements

The qualified profile is Windows x64/MSVC or Linux x86-64/GCC with static
[c-ares 1.34.9+qdcxx2 and `CARES_THREADS=OFF`](../../cmake/runtime-profile/README.md).
Use the default c-ares allocators. Create `RuntimeBootstrap` on the application
main thread before other application threads, coordinate every other user of the
same SDK state, and retain it until all runtimes and application threads retire.
Destroy it on that creating thread, outside Windows `DllMain`.

Each successfully owned bootstrap acquires its own balanced SDK reference;
Windows also acquires a Winsock reference before c-ares initialization. Runtime
borrowers retain stable bootstrap state and never perform global SDK cleanup.
Destruction with outstanding borrowers or on the wrong thread terminates.
Closing an endpoint does not by itself release the process bootstrap.

The default feed runtime owns this bootstrap, starts its worker lazily, and joins
that worker before bootstrap destruction. Endpoint/subscription handles can remain
as closed detached ports after native retirement. This does not permit those
handles to continue DNS, sockets or callbacks against retired runtime state.
See the [TCP ownership/retirement contract](TCP-RUNTIME.md#ownership-and-progress)
and [feed lifetime rules](../../docs/FEED-API.md).

## Supported ownership modes

Both modes retain the existing native/DNS resource profile and caller obligations.
The default constructor selects owned mode; the explicit bootstrap overload selects borrowed mode.

| Mode | SDK ownership | Supported creation order |
| --- | --- | --- |
| Owned | The feed runtime acquires and eventually releases its own bootstrap reference, preserving the existing path. | Construct on main before other application threads; destroy there after they retire. |
| Borrowed | The feed runtime holds an explicit lease on an existing live `RuntimeBootstrap`; it performs no global initialization, cleanup or allocator changes. | Create the bootstrap on main before other application threads; construct and destroy the feed runtime later on that same main/creator thread. |

The borrowed mode supports a host that is already threaded only when that host
arranged the controlled bootstrap before starting its threads. It accepts a live
QDCXX bootstrap capability rather than a Boolean assertion that another library
initialized c-ares. The capability identifies a controlled lifetime relationship;
it does not prove that all external users obeyed the allocator, ordering and
coordination requirements.

The constructor checks an opaque per-linked-runtime identity marker stored in the token's
stable state and rejects an incompatible instance before native activation. A matching C++ type, version
string or shared-library name does not qualify cross-DSO, cross-SDK-image or plugin
interoperability. Binary/plugin arrangements without an explicitly supported
provenance contract remain unsupported. The marker is a same-instance guard, not an ABI compatibility or security attestation.

## Borrowed lifetime and failure boundaries

1. The host creates the bootstrap during its main-thread, pre-thread startup and
   retains ownership of its SDK references. All same-state foreign c-ares users
   must be coordinated; none may change allocators or perform premature cleanup.
2. The borrowed constructor validates configuration, live token/provenance
   and the shared main/creator-thread identity before publishing a usable runtime
   or activating native work. A moved-from or otherwise incompatible token fails
   without acquiring or releasing a global SDK reference. Failure releases any
   provisional lease and leaves the host owner intact.
3. Successful construction retains a lease on stable owner state, including idle
   gaps, pending endpoints, worker startup, failure and asynchronous retirement.
   Moving the owner's facade must not invalidate that state or relax its thread
   and lifetime preconditions. Borrower construction does not make SDK progress.
4. Runtime shutdown fences new admission, retires native DNS/socket state and
   waits for its worker and owned callbacks/captures to finish. Only then may it
   release the lease. A close request, cancellation request or endpoint terminal
   notification alone is insufficient evidence of complete retirement.
5. The host destroys its bootstrap on the original main thread after every
   borrower and application thread has retired. The existing outstanding-borrow
   and wrong-thread destruction protections remain in force. No endpoint,
   callback or detached handle may perform global cleanup on behalf of the host.

Wrong-thread calls and invalid token/state conditions must be rejected through
the public error mechanism where an operation can report failure, before native
activation. Non-reporting destruction retains the existing fail-fast lifetime
preconditions. These checks run for each borrowed construction, before the resolver snapshot. It cannot
reliably reconstruct whether the host initialized before unrelated threads or
whether every foreign SDK user honored its obligations.

## Why initialized-state detection is insufficient

The pinned `src/lib/ares_library_init.c` uses a non-atomic global unsigned reference
counter. `ares_library_init` increments it; `ares_library_cleanup` decrements it
and resets global allocator hooks at zero. `ares_library_init_mem` writes those
hooks before calling `ares_library_init`, even if initialization already exists.
A local mutex protecting only QDCXX callers cannot coordinate foreign SDK users.

On Windows, `ares_library_initialized()` checks only whether the counter is
nonzero. On Linux, its `USE_WINSOCK` check is absent and it returns success even
with a zero counter. Neither result establishes allocator identity, balanced
ownership, pre-thread initialization or the absence of concurrent foreign calls.
`ares_threadsafety()` and version checks do not supply those missing facts.
Thread enumeration cannot establish this history or prevent a subsequent race.

The pinned init/cleanup manual pages retain their before-threads/after-threads
requirements and prohibit Windows `DllMain` calls. Although this qualified init
implementation is small, that observation does not relax the supported contract.
The [profile source manifest](../../cmake/runtime-profile/c-ares-1.34.9-qdcxx2.sources.json)
binds `src/lib/ares_library_init.c` to SHA-256
`d4baba72385dc656ff9afc4015d7fbc67805194f563e4995ac9d3a04ffbee90c`
and includes `docs/ares_library_init.3` and `docs/ares_library_cleanup.3`.
Upstream reference pages describe [initialization](https://c-ares.org/docs/ares_library_init.html)
and [cleanup](https://c-ares.org/docs/ares_library_cleanup.html); the pinned sources
and manual pages govern this qualified profile. The SDK's MIT notice and the
profile's existing source/license receipts remain required.

## Unsupported host conditions and implementation checks

A cold late bootstrap in an already-threaded host, raw external-initialized flags,
detection-only adoption, uncoordinated same-state SDK use and `DllMain` activation
are unsupported. Static linking or a second DLL does not by itself prove isolated
SDK state. A separate process is a possible application-level alternative with
its own IPC, resources and shutdown costs; no isolation facility is provided or qualified by this API.

The [focused conformance cases](../../tests/conformance/feed_bootstrap_tests.cpp)
exercise host threads, multiple idle borrowers, moved-owner activation, moved and
incompatible tokens, wrong-thread/capacity refusals, configuration rollback,
actual pending DNS retirement, callback close/throw and final capture release.
The [constructor allocation sweep](../../tests/conformance/feed_bootstrap_allocations.cpp)
covers one-shot creator storage refusals in both modes without starting native work.
The [subprocess checks](../../tests/conformance/feed_bootstrap_death.cmake) require
fail-fast owner destruction with a live borrower and wrong-thread owner destruction.
The incompatible-token case changes only a private test token marker; it does not
qualify a cross-DSO or foreign ABI arrangement. Qualified Windows/Linux runtime,
consumer and regression checks remain distinct from these source-level test descriptions.

Invalid/moved token or provenance is `ErrorCode::INVALID_ARGUMENT` at
`bootstrap-lease-configuration`; wrong creator thread is `WRONG_THREAD` at
`bootstrap-borrow-thread`; borrow capacity refusal is `RESOURCE_LIMIT` at
`bootstrap-lease-borrowers`. Ordinary options, resolver and allocation failures
retain their existing public errors. An owned bootstrap storage refusal remains
`BOOTSTRAP_FAILURE` at `bootstrap-storage` with nested allocation evidence. Forming
`Error` can itself allocate; safe unwind does not guarantee allocation-free error representation. Failed construction releases its provisional
lease and leaves the host owner intact. Non-reporting destruction retains fail-fast
preconditions rather than silently releasing live SDK state.

This embedding API changes no protocol, TLS, credentials, global settings,
SDK dependency, runtime resource envelope or existing current-source validation
claim. The [controlled TLS profile](TLS-RUNTIME.md) retains its separate contract.
