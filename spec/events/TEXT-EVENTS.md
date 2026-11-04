# TextMessage and TextConfiguration

[Русская версия](TEXT-EVENTS.ru.md). English is authoritative.

`TextMessage` is a STREAM event; `TextConfiguration` is a lasting event. Both own their symbol and nullable UTF-16 text. A null text differs from an empty string. The event and text remain valid independently of the decoded record and callback span.

The standard `Text` field has full serial identity `UTF_CHAR_ARRAY` (10). Its wire length counts UTF-16 units and its body uses the QD character-array encoding, including unpaired surrogates. See [FIELD-TYPES](../encoding/FIELD-TYPES.md). The prepared mapping also accepts the exact `STRING` identity for caller-declared schemas. It validates the complete record shape, including unselected extra fields.

`TextMessage` requires `Time`, `Sequence`, and `Text`; `TextConfiguration` requires `Text` and permits `Time`, `Sequence`, and `Version` to be absent. Missing optional scalar fields project as zero. The high 32 bits of `timeSequence` contain signed seconds, and the low word contains milliseconds in bits 22–31 and sequence in bits 0–21. `eventTime` comes independently from the configured FIELD metadata policy. Both symbol and payload copies are subject to finite per-value and aggregate UTF-16 limits.

For a TICKER configuration record with an enabled `Version` field, the shared collector accepts an incoming signed physical version only when it is at least the cached version; an equal version can still update changed data. With no `Version` field, all updates are eligible. STREAM does not apply this version comparison. TextMessage subscription requires STREAM.

The reference is the pinned Java [TextMessage](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/misc/TextMessage.java), [TextConfiguration](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/misc/TextConfiguration.java) and [misc factory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/misc/MiscFactoryImpl.java).
