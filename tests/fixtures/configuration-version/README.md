# Configuration version observations

[Русская версия](README.ru.md). English is authoritative.

`capture-v1/oracle.tsv` contains481 raw input/output/count observations from the
[production Java helper](../../../tools/upstream/configuration-version-oracle/README.md).
`provenance.json` pins the reference revision, Java executables, harness, launcher, selected upstream
sources, exact class inventory and fixture bytes. The measured classpath has14 roots,1290 class files
and27 resources, unchanged before/after capture. No Java artifact is rebuilt by the helper.

Standard and explicit production absent/wide VersionedRecord schemas are exercised through actual
TICKER/STREAM collectors and ConfigurationDelegate. Fresh negative versions compare against the
default-zero cache and yield version0/null attachment/eventTime0 with symbol CONFIG. Equal versions
remain eligible for data changes; lower versions do not replace TICKER values. Signed extrema, staged
batch selection and STREAM ordering are retained. Absent Version disables the filter. Native Feed late-subscription cache replay is covered separately in
`misc_feed_tests.cpp`.
These observations concern event versions independently of attachment codecs. The unchanged
[attachment fixtures](../attachments/README.md) cover the serialization/delegate/wire pipeline.

Permanent native regressions live in `ticker_collector_tests.cpp`, `misc_event_tests.cpp` and
`misc_feed_tests.cpp`: signed/default boundaries, owning field projections and actual public Feed
delivery/replay. The public [contract](../../../docs/MISC-EVENTS.md) distinguishes normalized projected
fields from preserved raw provenance. Capture and Verify compare UTC/Pacific-Honolulu outputs exactly.
