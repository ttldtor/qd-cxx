# DNS requested-storage and work envelope

[Русская версия](DNS-RESOURCES.ru.md). English is authoritative.

These are conservative source-derived admission bounds for the
[qualified c-ares 1.34.9+qdcxx2 sources](../../cmake/runtime-profile/README.md).
They are not measurements of physical heap, RSS, CPU instructions or latency.
The [TCP runtime](TCP-RUNTIME.md) must enforce every prerequisite below; a source
formula alone does not qualify an arbitrary SDK build or adapter.

## Profile prerequisites

- Verified source set and qualified x64 ABI; default allocators and no SDK event
  thread, reinit, ambient resolver configuration, search, aliases, cache entries,
  address sorting, EDNS, DNS0x20 or failed-server probe queries.
- One `ares_query_dnsrec` wire query, A then AAAA, with the next query submitted
  outside callbacks only after the previous entry, query completion and idle cleanup.
  At most four numeric IPv4 servers, one try and one UDP-to-TCP fallback.
- Absolute ASCII/A-label query name of at most 254 bytes including the root dot.
  No getaddrinfo, service lookup or CNAME follow-up query.
- At most nine candidate/active/retiring connections and 16 readiness events.
  A whole SDK entry admits at most two successful native reads and 131,070 input
  bytes. Ordinary native callback attempts share a cap of 128, including connect.
  Close uses a separate fixed reserve for at most 16 owned descriptors and does
  not require ordinary data credits.
  Exhaustion returns a non-EINTR result before another native attempt.
- An allocation-free fixed descriptor registry, no socket-state callback queue,
  and bounded readiness snapshots. UDP writes deferred by WOULD_BLOCK require
  an additional registry hint; the SDK's legacy snapshot alone can lose WRITE.
- Endpoint capacity at most 64 and CNAME depth at most 16. The copier performs
  at most 17 CNAME scans and one address scan, with bounded name and dedup passes.
  No application code runs in a C callback.
- Errors stop admission and retire the channel outside callbacks. The qdcxx2
  response-adoption correction is required; a lost response cannot be recovered
  through an adapter-side timeout or channel destruction.

Linux requires the compiled native `arc4random_buf` entropy path; the SDK's
getrandom/fread fallback is not qualified by this work bound. Windows uses its
selected native entropy path. Native OS/entropy availability and allocator
implementation overhead remain separate platform preconditions.

## Requested storage

Let `L = 65535` and `R = floor(L / 11) = 5957`. The parser validates the RR
count against remaining bytes before section allocation and accepts one question.
Names have at most 255 presentation bytes and 128 compression transitions;
escaped owned name buffers fit 1024 bytes. On the checked ABIs an RR occupies
80 bytes. Three power-of-two/minimum-four RR arrays together fit `2R + 12` slots.

One complete parsed-record envelope is:

```text
P = 80*(2R+12) + 1024*(3R+4) + 128R
    + 24*(2L+4R) + 34L + R + 1048576
  = 27020851 bytes < 32 MiB
```

Terms cover RR capacities; owner/SOA/NAPTR names and short strings; control
objects; TXT/OPT/SVCB/HTTPS item capacities; wire-consuming payloads; temporary
names and moving reallocations. All parsed types count even when the address
copier ignores their records. The SDK parses before checking the query ID.

A deferred response can overlap the next parsed unsolicited reply. Reserve
two rounded 32 MiB envelopes, nine connection input/reallocation peaks of 786,432
bytes, nine output allowances of 8192 bytes and 1 MiB for fixed/setup/query state:

```text
2*32MiB + 9*786432 + 9*8192 + 1MiB = 75309056 bytes < 80MiB
```

Thus the per-channel SDK reservation is **80 MiB**. A conservative simultaneous
allocation-count allowance is 220,000; allocator headers, fragmentation, native
socket buffers and process-wide OS state are not thereby measured or bounded.
Asio arenas, adapter buffers and protocol owners have separate reservations.

## Work credits

One logical unit represents a conservative byte visit/copy/zero/moving-copy,
object/loop/option visit or allocation/free request. It does not represent an
instruction or time interval. Precharge the complete allowance before an SDK entry;
callbacks share the remaining native quotas rather than resetting them.

At profile ceilings, carried and newly admitted bytes give:

```text
C=9; J=16; D=128; U=1048576
B=C*(L+2)+131070+4=720907
F=floor(B/2)=360453; T=floor(B/11)=65537; N=F+3T=557064
E=8*(1+3R)+32R+2L+32=464702
X=F*E+1024*(C+J+4)=167503259702
```

`F` counts even empty TCP frames. The deliberately loose `F*E` charges a maximum
parser allocation pattern for every possible frame. Each request is at most
1 MiB, with up to `2U` zero/moving-copy units. This avoids relying on an unproved
tighter aggregate-copy estimate.

The complete charge sums:

```text
F + 8T + 4B + N*(128+255+1) + floor(B/4)*floor(L/4)
+ 2X + 2U*X + 8*1024*N + 128B + 128T + 4096F
+ 2*(J+2)*524288 + 1024*(D+J+C+4+1)
+ (18R+64R+17*17) + (18R*16*1024+R*64*16+17*17*4*256)
= 351280142190671838 logical units
```

This fits `uint64_t`; checked arithmetic must retain 64-bit intermediates.
`dnsProgressWorkRequirement` supplies the conservative progress credits, rather
than making callers copy this number. It is sufficient, not a minimum or a
measured amount of work. Raising a profile ceiling requires a new derivation.

Channel preparation is separate. With server count `S` in 1–4:

```text
Einit=1024*(S+1)
byteUnits=2U*Einit+16384; visits=3*Einit
```

At four servers that is 10,737,434,624 byte-work credits and 15,360 visits.
`dnsPreparationWorkRequirement` reports this DNS-only allowance before creation;
wrapper validation/arena initialization and the pure-session factory have their
own charges. These credits do **not** request 10 GiB of memory: actual requested
storage remains under the separate envelope above. A deadline cannot interrupt
the synchronous parser or impose a hardware-independent duration.
