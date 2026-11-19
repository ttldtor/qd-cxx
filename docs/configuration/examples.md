# Controlled stream example

[Русский](examples.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [StreamOptions](examples.md#cfg-0929-owner) — `examples/subscriptions/stream_client.hpp`

## StreamOptions {#cfg-0929-owner}

[examples/subscriptions/stream_client.hpp](../../examples/subscriptions/stream_client.hpp)

### `StreamOptions::host` — CFG-0929 {#cfg-0929}

- Type: `std::string_view`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: borrowed STREAM source-address bytes
- Accepted values: Low-level runPrepared requires a host length of 1..45 but does not itself validate numeric syntax; TcpRuntime validates it later and DNS without servers is refused. The EndpointOptions overload permits empty options.host to select settings.host, otherwise it must match.
- Read timing: Both runStream overloads and runPrepared take StreamOptions by value; validOptions checks the local copy before preparation. host remains a view of external string storage. The EndpointOptions overload checks selection consistency and replaces host and port with endpoint values.
- Mutability and lifetime: Changing the original StreamOptions after argument passing does not change the local copy. Storage referenced by host and symbols must remain valid until return; mutating it is not a supported reconfiguration mechanism. The external signal flag may change under the sig_atomic_t contract.
- Failure: Invalid length/endpoint mismatch is StreamFailure::CONFIGURATION. Deferred TcpRuntime failure is retained in preparation diagnostics.

### `StreamOptions::port` — CFG-0930 {#cfg-0930}

- Type: `std::uint16_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Numeric port in 1..65535.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Numeric port in 1..65535.
- Read timing: Explicit argument of public operations: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: StreamFailure / StreamResult with separate local DispatchTerminal, TcpTerminal and TcpDiagnostic; no peer ACK implication

### `StreamOptions::recordId` — CFG-0931 {#cfg-0931}

- Type: `std::int32_t`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: schema local record ID
- Accepted values: recordId≥0 and &lt;schema.records().size(); the selected record must have hasTime=false. The endpoint overload additionally requires dispatchTurn.fields≥2×field count without overflow.
- Read timing: Both runStream overloads and runPrepared take StreamOptions by value; validOptions checks the local copy before preparation. recordId is read during schema validation and declaration/subscription preparation.
- Mutability and lifetime: Changing the original StreamOptions after argument passing does not change the local copy. Storage referenced by host and symbols must remain valid until return; mutating it is not a supported reconfiguration mechanism. The external signal flag may change under the sig_atomic_t contract.
- Failure: Missing/negative ID or hasTime is StreamFailure::CONFIGURATION. Insufficient turn fields yields CONFIGURATION with TcpError::WORK_LIMIT, endpoint-record-work.

### `StreamOptions::symbols` — CFG-0932 {#cfg-0932}

- Type: `std::span<const std::u16string>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Borrowed 1..16 exact selectors, at most 128 units each.
- Units: see declaration: UTF-16/code units
- Declared constraints: Borrowed 1..16 exact selectors, at most 128 units each.
- Read timing: Explicit argument of public operations: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: StreamFailure / StreamResult with separate local DispatchTerminal, TcpTerminal and TcpDiagnostic; no peer ACK implication

### `StreamOptions::duration` — CFG-0933 {#cfg-0933}

- Type: `std::chrono::milliseconds`. Declared default: `{5000}`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Admission interval in 1..600000 milliseconds.
- Units: milliseconds
- Declared constraints: Admission interval in 1..600000 milliseconds.
- Read timing: Explicit argument of public operations: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: StreamFailure / StreamResult with separate local DispatchTerminal, TcpTerminal and TcpDiagnostic; no peer ACK implication

### `StreamOptions::removeAfter` — CFG-0934 {#cfg-0934}

- Type: `std::optional<std::chrono::milliseconds>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: milliseconds to all-initial-selector removal or absence
- Accepted values: nullopt disables removal; a present value must be strictly &gt;0 and &lt;duration. The caller progress loop observes the timer; removal is requested once after addComposed.
- Read timing: Both runStream overloads and runPrepared take StreamOptions by value; validOptions checks the local copy before preparation. removeAfter is checked against duration; the progress loop compares elapsed time with the copied deadline and prepares removal once.
- Mutability and lifetime: Changing the original StreamOptions after argument passing does not change the local copy. Storage referenced by host and symbols must remain valid until return; mutating it is not a supported reconfiguration mechanism. The external signal flag may change under the sig_atomic_t contract.
- Failure: Invalid timing relationship is StreamFailure::CONFIGURATION before work; a later remove command may separately fail under its library contract.

### `StreamOptions::maximumRecords` — CFG-0935 {#cfg-0935}

- Type: `std::optional<std::size_t>`. Declared default: No in-class initializer. This does not establish requiredness; value-initialization differs.
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive completed-record target; admitted prefix can exceed it.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive completed-record target; admitted prefix can exceed it.
- Read timing: Explicit argument of public operations: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: StreamFailure / StreamResult with separate local DispatchTerminal, TcpTerminal and TcpDiagnostic; no peer ACK implication

### `StreamOptions::interrupted` — CFG-0936 {#cfg-0936}

- Type: `const volatile std::sig_atomic_t *`. Declared default: `= nullptr`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: optional signal-flag address
- Accepted values: nullptr disables external interruption; any observed nonzero *interrupted requests cooperative stop. Pointer/object must remain valid until return; lifetime is not locally validated.
- Read timing: Both runStream overloads and runPrepared take StreamOptions by value; validOptions checks the local copy before preparation. The interrupted pointer is copied; observeCutoff reads the external flag in the progress loop.
- Mutability and lifetime: Changing the original StreamOptions after argument passing does not change the local copy. Storage referenced by host and symbols must remain valid until return; mutating it is not a supported reconfiguration mechanism. The external signal flag may change under the sig_atomic_t contract.
- Failure: A nonzero value is not an error: result.interrupted=true and beginStop(false). An invalid pointer violates the caller precondition.

### `StreamOptions::ownerOperationsPerTurn` — CFG-0937 {#cfg-0937}

- Type: `std::size_t`. Declared default: `= 10000`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Local progress attempts shared across one owner turn, in 1..10000.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Local progress attempts shared across one owner turn, in 1..10000.
- Read timing: Explicit argument of public operations: [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const core::LocalSchema &amp;schema, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle) / [[nodiscard]] StreamResult runStream(transport::RuntimeBootstrap &amp;bootstrap, const client::EndpointOptions &amp;endpoint, StreamOptions options, const std::shared_ptr&lt;client::Consumer&gt; &amp;consumer, std::optional&lt;client::ClientHandle&gt; &amp;publishedHandle)
- Mutability and lifetime: Read timing and ownership follow the public owner contract; no general live-replacement guarantee is established. Field-specific lifetime remains independently unvalidated.
- Failure: StreamFailure / StreamResult with separate local DispatchTerminal, TcpTerminal and TcpDiagnostic; no peer ACK implication
