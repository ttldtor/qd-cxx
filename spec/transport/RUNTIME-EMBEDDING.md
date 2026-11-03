# Runtime embedding and bootstrap ownership {#runtime-embedding}

[Русская версия](RUNTIME-EMBEDDING.ru.md) · [TCP runtime](TCP-RUNTIME.md) · [Feed API](../../docs/FEED-API.md)

This is the bootstrap ownership decision for a future embedding API. The current
`ApiRuntime` constructor still acquires its own bootstrap and requires main-thread
construction before other application threads. Borrowed initialization described
below is not implemented by this decision. No new constructor, overload, runtime
check or supported binary/plugin integration is introduced here.

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

The current feed runtime owns this bootstrap, starts its worker lazily, and joins
that worker before bootstrap destruction. Endpoint/subscription handles can remain
as closed detached ports after native retirement. This does not permit those
handles to continue DNS, sockets or callbacks against retired runtime state.
See the [TCP ownership/retirement contract](TCP-RUNTIME.md#ownership-and-progress)
and [feed lifetime rules](../../docs/FEED-API.md).

## Selected future ownership modes

Both modes retain the existing native/DNS resource profile and caller obligations.
Their spelling and public error mapping belong to the implementation.

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

The implementation must validate the token's supported provenance and reject an
incompatible instance before native activation. A matching C++ type, version
string or shared-library name does not qualify cross-DSO, cross-SDK-image or plugin
interoperability. Binary/plugin arrangements without an explicitly supported
provenance contract remain unsupported. No new ABI or SDK-image discovery scheme
is selected here.

## Borrowed lifetime and failure boundaries

1. The host creates the bootstrap during its main-thread, pre-thread startup and
   retains ownership of its SDK references. All same-state foreign c-ares users
   must be coordinated; none may change allocators or perform premature cleanup.
2. The future borrowed constructor validates configuration, live token/provenance
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
preconditions. The implementation must keep these runtime checks; this decision
is not evidence that the current API already performs all of them. It cannot
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
its own IPC, resources and shutdown costs; no isolation facility is implemented
or qualified by this decision.

The implementation must verify at least: owned startup compatibility; a main-thread
host bootstrap followed by host threads and a later borrowed runtime; multiple
borrowers and idle intervals; failed construction without host cleanup; invalid or
moved token and wrong-thread rejection before activation; retirement with pending
DNS/socket work and callback close/throw; release only after quiescence; and
fail-fast owner destruction with a live borrower. Supported token provenance must
have positive and rejection evidence. Native verification remains required on the
qualified Windows and Linux profiles for that implementation. These are future
acceptance scenarios, not executed test results.

This ownership decision changes no protocol, TLS, credentials, global settings,
SDK dependency, runtime resource envelope or existing current-source validation
claim. The [controlled TLS profile](TLS-RUNTIME.md) retains its separate contract.
