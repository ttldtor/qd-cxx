# QTP inspector

[Русский](tools-qtp-inspect.ru.md) · [Reference index](../CONFIGURATION-REFERENCE.md)

Field-specific consumer contracts and qualified declaration/source contracts are distinguished below. UNKNOWN identifies a missing fact. Known declared facts are retained; their semantic validation remains pending. Source links identify the declaration or consumer; they do not claim a fresh runtime test. Values describe the current API; accepted changes are listed in the [architecture contract](../CONFIGURATION.md).

- [Limits](tools-qtp-inspect.md#cfg-0821-owner) — `tools/qtp-inspect/inspect.hpp`

## Limits {#cfg-0821-owner}

[tools/qtp-inspect/inspect.hpp](../../tools/qtp-inspect/inspect.hpp)

### `Limits::chunkSize` — CFG-0821 {#cfg-0821}

- Type: `std::size_t`. Declared default: `= 4096`
- Classification: setting leaf; qualified declared facts follow. Declaration/source contract, not independently revalidated. Field-specific semantic validation and lifecycle verification remain pending; no runtime-test claim.
- Origin: the linked declaration. Explicit typed input; no per-field property alias or priority is established.
- Declared meaning: Positive input bytes per read.
- Units: UNKNOWN: see the declared meaning; no further unit is inferred from the type.
- Declared constraints: Positive input bytes per read.
- Read timing: explicit CLI parse/preparation before working I/O, except runtime state/observations identified separately
- Mutability and lifetime: no general CLI live-update route; explicit command-specific operations only
- Failure: CLI usage or bounded working failure; see owning command parser/help
- Additional owner vocabulary (not a field-to-error mapping): `CLI ArgumentStatus/static rejection or owning typed result; contract excerpts below`.

### `Limits::input` — CFG-0822 {#cfg-0822}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: whole-input bytes
- Accepted values: 0 requires EOF before the first byte; exhaustion uses peek, so an exactly bounded file succeeds at actual EOF. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Invalid decimal syntax or an unrepresentable value of this CLI option yields code 2, usage. An extra input byte after the remainder reaches zero yields code 6, input-limit; a read/peek failure separately yields code 3, read.

### `Limits::frame` — CFG-0823 {#cfg-0823}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: frame body bytes including message ID
- Accepted values: frame≤INT64_MAX and frame≤size_t max−9; 0 is locally admitted and allows only an empty body. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the value before opening the file; validLimits checks representability. FrameReader then receives {frame,frame+9}. The bound is also forwarded as payload/string-body capacity to the underlying description and data decoders.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: An unrepresentable bound is rejected with exit 2, stage=usage, before opening the file. When the decoded body length exceeds frame, FrameReader reports FrameError::BODY_LIMIT and the inspector exits 6 at framing.2. Other actual framing failures use their own numeric FrameError suffix.
- Consumer sources: [include/qd/protocol/frame_reader.hpp](../../include/qd/protocol/frame_reader.hpp).

### `Limits::frames` — CFG-0824 {#cfg-0824}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: all frames including empty heartbeats
- Accepted values: Every frame requires frameCount&lt;frames; 0 admits no first frame. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Invalid decimal syntax or an unrepresentable value of this CLI option yields code 2, usage. The next complete frame beyond the permitted count yields code 6, frame-limit.

### `Limits::descriptions` — CFG-0825 {#cfg-0825}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: descriptions across frames before first DATA
- Accepted values: The aggregate descriptions remainder is passed to the parser and debited for every description; 0 admits no description. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI parses the value before input. parseDescribeRecords receives the current remainder; bindRecord then receives its bounds for the parsed description, while StreamDataContext creation uses the original aggregate bounds.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: When this budget is exhausted in parseDescribeRecords, it returns DescriptionStatus::RESOURCE_LIMIT at DescriptionStage::DESCRIPTION; the tool exits 6 at description.1. parsed.consumed and parsed.errorOffset are preserved. Description count is checked before the next description.
- Consumer sources: [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp), [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp).

### `Limits::actions` — CFG-0826 {#cfg-0826}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: binding actions/declared fields across descriptions
- Accepted values: remaining.actions bounds parser fieldsPerDescription and totalFields, then bindRecord; charges persist across frames. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI parses the value before input. parseDescribeRecords receives the current remainder; bindRecord then receives its bounds for the parsed description, while StreamDataContext creation uses the original aggregate bounds.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: When this budget is exhausted in parseDescribeRecords, it returns DescriptionStatus::RESOURCE_LIMIT at DescriptionStage::RECORD_HEADER; the tool exits 6 at description.5. parsed.consumed and parsed.errorOffset are preserved. Aggregate/per-description field count is checked after reading the header.
- Consumer sources: [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp), [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp).

### `Limits::nameUnits` — CFG-0827 {#cfg-0827}

- Type: `std::size_t`. Declared default: `= 1024`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units per description name
- Accepted values: nameUnits bounds parser name.codeUnits and BindingLimits.nameUnits; 0 forbids a nonempty name. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI parses the value before input. parseDescribeRecords receives the current remainder; bindRecord then receives its bounds for the parsed description, while StreamDataContext creation uses the original aggregate bounds.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: When this budget is exhausted in parseDescribeRecords, it returns DescriptionStatus::RESOURCE_LIMIT at DescriptionStage::RECORD_NAME/FIELD_NAME; the tool exits 6 at description.3 or description.6. parsed.consumed and parsed.errorOffset are preserved. An individual name length is bounded while reading a record/field name.
- Consumer sources: [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp), [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp).

### `Limits::totalNameUnits` — CFG-0828 {#cfg-0828}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: aggregate UTF-16 description-name units
- Accepted values: The remaining aggregate UTF-16 allowance bounds each name read. After a completed description, the inspector debits remaining.totalNameUnits by exactly description.name.size() and every field.name.size(); prior frames and repeated names remain charged.
- Read timing: CLI parses the value before input. parseDescribeRecords receives the current remainder; bindRecord then receives its bounds for the parsed description, while StreamDataContext creation uses the original aggregate bounds.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: When this budget is exhausted in parseDescribeRecords, it returns DescriptionStatus::RESOURCE_LIMIT at DescriptionStage::RECORD_NAME/FIELD_NAME; the tool exits 6 at description.3 or description.6. parsed.consumed and parsed.errorOffset are preserved. Aggregate remaining name text bounds record/field name reads; actual lengths are debited after a complete description.
- Consumer sources: [src/protocol/describe_records.cpp](../../src/protocol/describe_records.cpp), [include/qd/protocol/describe_records.hpp](../../include/qd/protocol/describe_records.hpp).

### `Limits::records` — CFG-0829 {#cfg-0829}

- Type: `std::size_t`. Declared default: `= 1048576`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: wire DATA record attempts across the input
- Accepted values: Each DATA bound is min(messageRecords,remaining.records); processedRecords is debited even after failure following a completed prefix. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::RECORD; the inspector returns exit 6, stage=data.1. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `Limits::fields` — CFG-0830 {#cfg-0830}

- Type: `std::size_t`. Declared default: `= 4194304`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: wire DATA field attempts across the input
- Accepted values: Each DATA bound is min(messageFields,remaining.fields); wireFields is debited from the aggregate remainder. 0 allows no field visits. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::FIELD; the inspector returns exit 6, stage=data.5. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `Limits::values` — CFG-0831 {#cfg-0831}

- Type: `std::size_t`. Declared default: `= 4194304`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: completed local values across the input
- Accepted values: Each DATA receives min(messageValues,remaining.values); completed record.values.size() is charged. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::OUTPUT; the inspector returns exit 6, stage=data.6. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `Limits::symbolUnits` — CFG-0832 {#cfg-0832}

- Type: `std::size_t`. Declared default: `= 4194304`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of completed output symbols across the input
- Accepted values: DATA receives min(messageSymbolUnits,remaining.symbolUnits); completed-record symbol units are charged including repeats. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::OUTPUT; the inspector returns exit 6, stage=data.6. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `Limits::output` — CFG-0833 {#cfg-0833}

- Type: `std::size_t`. Declared default: `= 67108864`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: stdout bytes including header and completion
- Accepted values: Every complete row must fit remaining.output before writing; 0 cannot hold a nonempty header. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Invalid decimal syntax or an unrepresentable value of this CLI option yields code 2, usage. Insufficient formatter budget (OutputLimit) or completed-row budget in emit yields code 6, write; a physical write failure separately yields code 3, write.

### `Limits::messageRecords` — CFG-0834 {#cfg-0834}

- Type: `std::size_t`. Declared default: `= 65536`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: record attempts per DATA message
- Accepted values: Effective min(messageRecords,remaining.records) bounds processedRecords and emittedRecords. 0 admits no record. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::RECORD; the inspector returns exit 6, stage=data.1. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `Limits::messageFields` — CFG-0835 {#cfg-0835}

- Type: `std::size_t`. Declared default: `= 262144`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: field attempts per DATA message
- Accepted values: Effective min(messageFields,remaining.fields); 0 admits no field. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::FIELD; the inspector returns exit 6, stage=data.5. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `Limits::messageValues` — CFG-0836 {#cfg-0836}

- Type: `std::size_t`. Declared default: `= 262144`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: local output values per DATA
- Accepted values: Effective min(messageValues,remaining.values); 0 permits only a zero-field local layout. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::OUTPUT; the inspector returns exit 6, stage=data.6. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `Limits::messageSymbolUnits` — CFG-0837 {#cfg-0837}

- Type: `std::size_t`. Declared default: `= 262144`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 output-symbol units per DATA
- Accepted values: Effective min(messageSymbolUnits,remaining.symbolUnits); 0 gives no output-symbol text budget. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::OUTPUT; the inspector returns exit 6, stage=data.6. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).

### `Limits::symbol` — CFG-0838 {#cfg-0838}

- Type: `std::size_t`. Declared default: `= 4096`
- Origin: explicit native input to the linked owner; property-layer origins, where supported, follow the endpoint resolver. Field-specific consumer semantics.
- Units: UTF-16 units of current/previous symbol
- Accepted values: The single symbol value supplies both SymbolLimits.codeUnits and stateUnits; 0 provides no symbol-unit capacity. CLI accepts complete nonnegative decimals representable in both uint64_t and size_t, as checked by its parser. This counter has no other independent scalar validator.
- Read timing: CLI resolves the field before file input; inspect copies the profile into remaining and applies the stated counter/bound during traversal.
- Mutability and lifetime: The profile is fixed within a run while remainders are spent. A new run receives new values.
- Failure: Exhausting this coordinate produces StreamDataStatus::RESOURCE_LIMIT at StreamDataStage::SYMBOL; the inspector returns exit 6, stage=data.2. Diagnostics preserve decoded.consumed and decoded.errorOffset. Completed prior records are processed before reporting the failure.
- Consumer sources: [include/qd/protocol/stream_data.hpp](../../include/qd/protocol/stream_data.hpp), [src/protocol/stream_data.cpp](../../src/protocol/stream_data.cpp).
