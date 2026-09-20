# Encoding specification

This area will describe primitive and stateful encodings used by schema and QTP, including canonical forms, ranges, state lifetime, malformed input, and partial-input behavior.

Encoding rules must be backed by upstream source/test evidence and representative fixtures. Do not assume a codec is stateless merely because its public entry point looks functional.

## QDCXX-004 dependency inventory

Descriptive **S** (source) / **D** (documentation) observations for Java QD 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`; **U** denotes unresolved coverage. This is not a full primitive-encoding specification. Immutable links are in [QTP source inventory](../protocol/SOURCES.md#encoding-and-core).

| Dependency | Observed use and state | Follow-up |
| --- | --- | --- |
| Compact integers/longs | **S/D:** BufferedInput/Output and IOUtil encode signed compact numbers; framing reads long and validates range. readCompactInt consumes the entire encoded number but returns low 32 bits when out of int range. Do not substitute unsigned LEB128 or apply a universal overflow-rejection rule. | **U:** canonical writer forms versus accepted overlong forms, every width/sign transition and truncated input; P01/P02/P08. |
| UTF strings and properties | **S/D:** UTFString uses compact byte length, -1 null, 0 empty. Reader accepts overlong UTF-8/CESU-8 surrogates per Javadoc and dedicated decoding routines. Properties use compact count and UTFString key/value pairs; readProperties uses a checked compact long count. | **U:** malformed Unicode, duplicate/null keys, length/resource limits and writer/reader asymmetry; P02/P08/P09. |
| Symbols/event flags | **S:** PentaCodec.ReaderImpl/WriterImpl retain last symbol and MRU flags within a message; QTP resets them per message. Repeat marker 0xFF requires an already defined symbol; 0xF8 repeats MRU flags, 0xF9 introduces compact flags. Reader also recognizes UTF-8/CESU-8 forms and rejects reserved prefixes. | **U:** full bit grammar and Unicode limits; P01/P11. Plain symbol encode/decode tests are not a session-state oracle. |
| Field serial types | **S:** BinaryRecordDesc selects byte, UTF char, short, int, compact long, char-array string, byte-array/UTF string/serialized object readers from wire type and local field class. VOID consumes no payload. | **U:** exact matrix for object/numeric mismatches and unknown representations; P05/P06/P08. |
| Numeric representation | **S/D:** SerialFieldType representation bits select decimal, wide decimal, time, sequence or long semantics; BinaryRecordDesc converts or retries local representation. FieldAdaptationTest asserts selected truncation, special-value and time behavior. | **U:** complete conversion/rounding rules, Profile.Shares and bidirectional requested-layout behavior; P06. |
| Event metadata/time | **S:** optional EventTime/EventSequence descriptor fields; HISTORY_ADD and REMOVE_EVENT use compact-long time in distinct payload paths. EventFlag masks depend on message type and options. | **U:** unknown-record interaction and all flag combinations; P07/P11. |

**S:** Primitive calls may consume bytes before throwing EOF. BinaryQTPParser's mark/reset and complete-frame check supply incremental stream behavior; a primitive decoder alone does not provide that contract. Description maps and PART assembly are protocol state, not primitive codec state. See [QTP boundaries](../protocol/README.md) and [schema exchange](../protocol/SCHEMA-EXCHANGE.md).

No primitive fixture was generated or Java test executed. Scenarios refer to the [protocol capture plan](../../tests/fixtures/protocol/README.md); remaining decisions are tracked as [QT questions](../../docs/upstream/OPEN-QUESTIONS.md#qtp-investigation-qdcxx-004).
