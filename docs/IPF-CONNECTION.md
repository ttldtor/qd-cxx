# Live instrument profile connections

[Русская версия](IPF-CONNECTION.ru.md). English is authoritative.

Enable ORG_TTLDTOR_QDCXX_BUILD_IPF_LIVE=ON with the explicit IPF transfer and verified
TLS prerequisites; link qdcxx_ipf_live. The connection uses one background worker.
The independent qdcxx_ipf collector/value/parser library remains usable with transport,
transfer and this option disabled.

Create InstrumentProfileConnectionRuntime on main before application threads in owned
bootstrap mode, or supply an explicit pre-thread RuntimeBootstrap. Its stable moved-to
owner must survive runtime join/destruction. No hidden ApiRuntime bootstrap is borrowed.
The supported profile is exactly one lifetime connection slot per runtime, without slot
reuse. Independent runtimes may share an explicit bootstrap only when borrow capacity
and truthful combined same-image TLS funding/history cover them.

~~~cpp
#include <iostream>
#include <qd/ipf/instrument_profile_connection.hpp>

int main() {
    using namespace org::ttldtor::qdcxx::ipf;
    auto prepared = InstrumentProfileConnectionRuntime::create();

    if (!prepared) {
        return 1;
    }

    auto runtime = std::move(*prepared);
    std::shared_ptr<InstrumentProfileCollector> collector = InstrumentProfileCollector::create();
    auto listener = collector->addUpdateListener([](auto batch) {
        for (const auto &revision : batch->profiles()) {
            std::cout << revision->revision << '\n';
        }
    });

    if (!listener) {
        return 2;
    }

    auto created = runtime->createConnection("http://127.0.0.1:8080/profiles.ipf", collector);

    if (!created) {
        return 3;
    }

    auto connection = std::move(*created);

    if (!connection->start()) {
        return 4;
    }

    // Run the application's ordinary work here; no transport polling is required.
    connection->close();

    return runtime->closeAndAwaitTermination() ? 0 : 5;
}
~~~

This example starts and immediately closes; replace the marked application work with
the application's lifecycle. Creator-thread join is shutdown, not a snapshot-completion
wait. No completion-wait API is provided here.

start is one-shot. NOT_CONNECTED → CONNECTING → CONNECTED → COMPLETED → CLOSED records
the initial phase; retries do not reset it. initialSnapshotComplete remains sticky
independently of later failed/active attempts. Observe activeAttempt, lastAttempt,
diagnostic and retired separately. HTTP framing, selected gzip/ZIP integrity, IPF EOF,
a completion marker and native retirement are separate evidence. A marker followed by
a bad trailer/close preserves owning collector revisions and reports the later error.

Requests send X-Live: yes and retain Accept-Encoding: identity. Only exact response
value provided enables live semantics. Without live capability, clean EOF completes
a replacement snapshot; live EOF flushes pending data without inventing completion.
Flush, COMPLETE, batch size and clean EOF publish in source order. Failed attempts
retain unflushed pending profiles for the next attempt, following pinned QD behavior.
Completion removes older attempt generations; one attempt keeps one generation even
after a completion marker.

A successful changed attempt commits Last-Modified only after native retirement.
Equal nonzero dates skip body without changing capability/generations. Fallback HTTP
sends If-Modified-Since and accepts conditional304; unsolicited304 is an error.
Update delay defaults to60000ms; retry begins at1000ms and doubles after errors up to
the current update delay, without reset on success. Changing the period does not shorten
a latched wait. The optional [update=period] suffix uses QD day/hour/minute/second syntax,
for example1m30s,0.5s,PT1S. The full nonnegative signed64 millisecond delay, including
QD inf, is accepted; inf remains a numeric delay under independent finite lifetime,
attempt and work bounds. Scheduling clamps the wait to the lifetime deadline without
adding an overflowing delay to the clock.

Connection/listener methods are producer-safe; ordinary concurrent destruction of the
same facade needs caller synchronization. State/auth callbacks run on the worker outside owner/native locks. Collector notifications
follow its independent dispatcher and may run on another current updating thread and may close/delete connection, remove listeners or mutate
collector. For never-started connections, final CLOSED dispatch occurs on creator during
shutdown. A selected callback may finish after removal; exceptions are contained/counted.
Authorization callbacks and final capture destruction also occur outside owner locks
on worker. Keep arbitrary callback work finite. Never destroy/join runtime from worker
or its callbacks: creator owns join and process SDK cleanup. Handles may outlive joined
runtime destruction as detached closed/retired observations.

Configure pending profile/unit, generation, attempt, lifetime, listener, backing and
copy/control work limits. Source/native/TLS/HTTP/decoder grants are cumulative across
retries/redirects; attempts do not reset shared TLS history or mint work. Collector
model/revision/delivery bounds remain independent. STL backing admission is conservative
requested storage, not exact allocator metadata/RSS; application callback captures and
arbitrary callback allocations remain caller-owned.

See [contract](../spec/ipf/CONNECTION.md), [collector](IPF-COLLECTOR.md),
[source reader](../spec/ipf/URL-READER.md) and [ownership decision](decisions/ADR-0009-live-ipf-connection-ownership.md).

Pending copy admission reserves aggregate requested backing before allocation: a possible empty-map sentinel, custom node/key/value objects, every standard/custom string capacity plus terminator and a conservative library alignment-request allowance. Supported tree headers use a four-pointer allowance; actual allocation-counting regressions bind the MSVC/GCC profile. Inline string capacity/padding are conservatively counted again. Fixed vector objects and the existing logical pending-unit reservation are included once in resident admission; unused resident backing, including that reservation, funds simultaneous copies. Dense custom data may exhaust resident backing before logical unit/profile limits. Flush releases copy bytes only after destroying pending values; failed copy restores storage reservation without refunding work. This is new connection-owned backing, without changing the normal source-reader or collector logical-budget contracts.
