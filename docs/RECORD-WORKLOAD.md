# Record workload qualification

[Русская версия](RECORD-WORKLOAD.ru.md).

The record workload checks a physical `TimeAndSale` record under STREAM with the default binary-v2 schema. Each run uses one source connection and at most 10 records per DATA frame. Direct NetTest uses an offered rate of 100 records per second. The filtered relay runs use 100 and 250 records per second. All runs are finite functional workloads.

## NetTest interoperability

QD 3.355 NetTest and qd-cxx NetTest were connected directly in both directions. The Java tool selects `TimeAndSale` through `com.devexperts.qd.tools.NetTest.record` and uses `com.devexperts.qd.tools.NetTest.batchSize=10`. Both sides subscribe to `IBM`. The relay's full-catalog source profile requires all three QTP contracts from its upstream, while each NetTest instance advertises its selected contract. Relay and filter coverage uses the separate test below.

The NetTest guide describes schema export, build prerequisites, the CLI and the direct Java loopback command. The Java record producer is pinned to QD 3.355. [Record NetTest guide](../tools/nettest/README.md), [QD 3.355 record producer](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-tools/src/main/java/com/devexperts/qd/tools/NetTestProducerDistributorThread.java)

| Build and direction | Consumer observation | Producer observation |
|---|---|---|
| Windows MSVC Release, Java → C++ | 590 records in 6.00974 s | Java rate-limited to 100 records/s |
| Windows MSVC Release, C++ → Java | Java reported a positive interval rate | 798 DATA records written in 8 s |
| Windows MSVC Release, C++ → C++ | 598 records in 6.00162 s | 600 DATA records written in 8 s |
| Linux GCC ASan/UBSan, C++ → C++ | 595 records in 6.00001 s | 595 DATA records written in 8 s |
| Linux GCC ASan/UBSan, Java → C++ | 350 records in 6.0008 s | Java rate-limited to 100 records/s |
| Linux GCC ASan/UBSan, Java → C++, repeated | 600 records in 6.00025 s | Java rate-limited to 100 records/s |
| Linux GCC ASan/UBSan, C++ → Java | Java reported a positive interval rate | 799 DATA records written in 8 s |

Producer and consumer windows start and finish independently. Their counts therefore cannot establish a loss count. C++ NetTest currently reports delivery counts and rates without a latency distribution. Java's reported consumer lag follows its own field interpretation and millisecond-resolution timestamp; it is a separate measurement.

The Linux Java → C++ run was repeated with the same classpath contents staged on the native Linux filesystem. In the first C++ → Java attempt, matching demand did not arrive before the startup deadline. A retry using the original mounted classpath succeeded with a 120-second startup allowance. A separate run with the staged classpath also succeeded.

## Filtered relay and sequence check

An independent finite TCP source sends records through a full-catalog relay to two clients. Both clients request `IBM` and `MSFT`. The first policy accepts `TimeAndSale`/`IBM` and the second accepts `TimeAndSale`/`MSFT`. The source sends 250 numbered records per symbol and an explicit end marker. Each consumer verifies the entire sequence, expected symbol, duplicates and ordering. Both policies use the same source connection and requested batch bound.

| Build | Offered aggregate rate | Delivery | Batch-send to complete-frame p95 |
|---|---:|---|---|
| Windows MSVC Release | 100/s | 250/250 per consumer, no gaps or duplicates | 5.45 s and 5.46 s |
| Windows MSVC Release | 250/s | 250/250 per consumer, no gaps or duplicates | 8.23 s and 8.26 s |
| Linux GCC ASan/UBSan | 100/s | 250/250 per consumer, no gaps or duplicates | 40.6 ms and 44.1 ms |
| Linux GCC ASan/UBSan | 250/s | 250/250 per consumer, no gaps or duplicates | 89.9 ms and 93.8 ms |

The latency clock is `monotonic_ns` in the same test process. It starts before each source batch is encoded and ends after a complete frame is read by a consumer. All records in a batch share the start timestamp. Results include socket, relay queue, and scheduling time. The total source-send and final-delivery windows are different. Windows uses a Release build and Linux uses ASan/UBSan, so the table does not isolate an operating-system effect. The cause of the longer measured interval on Windows has not been determined.

The relay and test-process CPU and peak resident memory were sampled separately. Those intervals include negotiation and startup. The finite sequence run checks loss for this workload, while the NetTest rate runs have independent measurement windows.

| Build and offered aggregate rate | Relay sampled wall | Relay CPU | Relay peak resident memory |
|---|---:|---:|---:|
| Windows Release, 100/s | 11.27 s | 203 ms | 24,969,216 bytes |
| Windows Release, 250/s | 11.27 s | 219 ms | 25,014,272 bytes |
| Linux ASan/UBSan, 100/s | 6.53 s | 1,690 ms | 221,532,160 bytes |
| Linux ASan/UBSan, 250/s | 3.41 s | 1,220 ms | 221,626,368 bytes |

For the sequence scenario, use one plain TCP producer, one relay and two downstream clients. Configure the source with `TimeAndSale`, alternate `IBM` and `MSFT`, send 250 records per symbol at the selected aggregate rate in batches of at most 10, and send a separate end marker after each sequence. Configure the per-peer STREAM policies as `:TimeAndSale&IBM` and `:TimeAndSale&MSFT`. Both clients request both symbols. Start delivery after both accepted source subscriptions are observed, then verify each client's complete sequence and absence of the other symbol. The publisher conformance tests contain the per-address policy setup. [Publisher filter tests](../tests/conformance/publisher_tests.cpp)

Other local build activity overlapped the Linux measurements. The figures describe these individual runs and do not establish sustained capacity.
