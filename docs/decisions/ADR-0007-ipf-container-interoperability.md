# ADR-0007: IPF container interoperability

[Русская версия](ADR-0007-ipf-container-interoperability.ru.md). English is authoritative.

Status: selected technical decision for the bounded IPF source implementation. Implementation
qualification, project acceptance and release remain separate.

## Context

Pinned QD StreamCompression detects gzip/ZIP by content and its ZIP wrapper skips directories
before reading the first file. Actual JDK8 stream observations include runtime defects that reject
otherwise valid ZIP descriptors. Compatibility policy requires explicit decisions for deviations.

[PKWARE APPNOTE6.3.10](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT), sections4.3.9.1–4.3.9.3
and4.5.3, permits unsigned descriptors and small ZIP64 entries, selects eight-byte descriptor sizes
when ZIP64 extended information is present, and requires both local ZIP64 size fields.

## Decision

Preserve QD first-file selection and ordinary gzip stream behavior, but support valid unencrypted
stored/deflated ZIP first-file forms within finite limits. Do not reproduce the JDK8 descriptor
lookahead/off-by-one defect or use actual small payload size to misclassify a ZIP64 descriptor.

| Captured fixture | Actual JDK8/QD | C++ contract |
| --- | --- | --- |
| zip-descriptor-nosignature | EOF failure at exact descriptor end | Accept valid unsigned ZIP32 descriptor without stealing suffix bytes |
| zip-directory-nosignature-first | No file entry after defective pushback | Skip directory correctly and read the following first file |
| zip64-descriptor64-small | Invalid-size failure despite valid small ZIP64 data | Accept64-bit sizes selected by local ZIP64 extra |
| zip64-descriptor64-nosignature-small | Invalid-size failure | Accept valid unsigned64-bit descriptor |
| zip64-directory-nosignature-first | Invalid-size failure | Skip directory and read the following first file |
| zip64-descriptor-small | Accepts32-bit sizes despite ZIP64 extra | Reject internally inconsistent descriptor width |

The captured Java outputs remain unchanged. Divergence rows are not Java parity evidence.
ZIP metadata/CRC/actual size errors are explicit container failures. The selected file end does not
validate later entries or central directory: HTTP/native framing and source limits still apply to
the remaining received bytes. No archive extraction or filesystem path interpretation is added.

Ordinary HTTP Content-Encoding:gzip chooses the same single gzip layer. Absent/identity coding
keeps magic detection. Expanded bytes go directly to the IPF parser, including when they start with
another compression magic; recursive decoding and other coding negotiation remain outside scope.

## Alternatives

Exact reproduction of JDK8 defects would reject valid producer output and couple interoperability
to one runtime bug. The tape tool's strict single-file ZIP32 policy excludes required descriptors
and ZIP64 forms. Full archive validation changes QD first-file behavior and is unnecessary for
reading the selected IPF snapshot. None of these alternatives is selected.

## Consequences and migration

Valid unsigned and small ZIP64 producers can interoperate without changing their archive format.
A producer emitting ZIP64 metadata with32-bit descriptors must emit consistent64-bit descriptors
or ordinary ZIP32 metadata. Such previously Java-accepted malformed data fails explicitly here.

The URL reader reserves fixed incremental decoder backing plus a finite inflater allowance; plain
reading through this compression-capable facade also funds that backing. Existing exact/one-below
tests measure actual layout, and no old resource golden is relaxed. Defaults remain finite.
Rebuild static consumers after the appended public envelope limits/progress/error extension.
The independent raw UTF-8/IPF parser remains unchanged.

## Evidence and checks

[Compression contract](../../spec/ipf/COMPRESSION.md) defines limits, completion and source policy.
[Synthetic oracle fixtures](../../tests/fixtures/ipf/compression/README.md) identify the pinned QD
revision, JDK8 observation and intentional divergence rows. Decoder tests fragment source input,
exercise signed/unsigned ZIP32/ZIP64, skipped directories, ordinary full archives, gzip optional
headers/members, corruption/truncation, expansion and exact/one-below budgets. Transfer tests use
local files and controlled HTTP/HTTPS, including marker-before-bad-trailer, first cause, cache-only
prefix drain and request-only cross-thread cancellation. Native qualification remains required.
