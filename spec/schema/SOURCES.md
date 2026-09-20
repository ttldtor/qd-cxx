# Locked upstream source inventory

All paths and links below refer to `devexperts/QD`, release 3.355, commit `0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`. Symbols identify the evidence more precisely than file names alone. See [analysis](README.md) for evidence labels.

## Reproduction and provenance

Inspected on 2026-09-20 using a separate temporary checkout outside qd-cxx:

```powershell
git clone --depth 1 --branch qd-qds-3.355 https://github.com/devexperts/QD.git "$env:TEMP/qdcxx-003-qd"
git -C "$env:TEMP/qdcxx-003-qd" rev-parse HEAD
git -C "$env:TEMP/qdcxx-003-qd" remote get-url origin
git -C "$env:TEMP/qdcxx-003-qd" rev-parse "refs/tags/qd-qds-3.355^{commit}"
```

HEAD and peeled tag must equal the commit above; origin must equal the locked repository. Read files with `Get-Content`, locate symbols with `rg -n`, and inventory resources with `rg --files`. No Java execution is claimed. Source/XML headers inspected identify MPL-2.0; links and original research prose are retained here, not copied upstream files. Redistribution of future fixture extracts requires the provenance/license review specified in the fixture policy.

## Entry points

- [qd-core/src/main/java/com/devexperts/qd/QDFactory.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/QDFactory.java) — createDefaultScheme(ClassLoader), getDefaultScheme.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/DXSchemeFactory.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/DXSchemeFactory.java) — createDataScheme; prefix and error wrapping.
- [dxfeed-scheme/src/main/resources/META-INF/services/com.devexperts.qd.DataSchemeFactory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/resources/META-INF/services/com.devexperts.qd.DataSchemeFactory) — XML factory service.

## Loading

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/SchemeModelLoader.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/SchemeModelLoader.java) — parseSpecification, loadSchemeModel, loadFileAndImports, getSchemeModelReader, resolveLoader.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/SchemeLoadingOptions.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/SchemeLoadingOptions.java) — applyOptions, setDXFeedDefaults and defaults.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/DXScheme.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/DXScheme.java) — Loader.fromURL/fromURLs/fromSpecification/load.

## XML

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/xml/XmlSchemeModelFormat.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/xml/XmlSchemeModelFormat.java) — constructor, SCHEMA_NAME, SCHEMA_NS.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/xml/XmlSchemeModelReader.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/xml/XmlSchemeModelReader.java) — readModel, loadImports, parseRecord, addRecordField, loadGenerator, loadMappings, loadVisibility, child/attribute helpers.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/xml/XmlSchemeModelWriter.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/xml/XmlSchemeModelWriter.java) — writeModel; model diagnostic output, not a runtime snapshot.
- [dxfeed-scheme/src/main/resources/dxfeed-schema.xsd](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/resources/dxfeed-schema.xsd) — active grammar, simple types, topType, recordType, basicFieldType, flagsFieldType, generatorType, mappingType, visibilityType.
- [dxfeed-scheme/src/main/resources/dxfeed-schema-1.1.xsd](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/resources/dxfeed-schema-1.1.xsd) — alternate resource not selected by loader.

## Model

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeModel.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeModel.java) — addType/addRecord/addGenerator, override/overrideEntityCollection, validateState, resolveType, visibility, generationOrder.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/NamedEntity.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/NamedEntity.java) — Mode, override, validateState.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeEntity.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeEntity.java) — file provenance.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeMapping.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeMapping.java) — placeholder.

## Types

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeType.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeType.java) — validateState, override, setUnresolved.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/DefaultEmbeddedTypes.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/DefaultEmbeddedTypes.java) — registry, getSerialType, canHaveBitfields.
- [qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/SerialFieldType.java) — constants, isObject/isLong, createDefaultIntInstance/createDefaultObjInstance.

## Enums

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeEnum.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeEnum.java) — addValue, override, validateState, Value.override.

## Records

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeRecord.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeRecord.java) — copyFrom, override, validateState, addField; Field.copyFrom/override/getMainAlias/addAlias/addTag/addBitfield/validateState; Bitfield.validateState; Alias.equals.

## Generator

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeRecordGenerator.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/SchemeRecordGenerator.java) — iterator/templates, getRecordsNames, override, validateState.

## Visibility

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/VisibilityRule.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/model/VisibilityRule.java) — getType, match overloads.

## Properties

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/DXScheme.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/DXScheme.java) — configureSchemeModel, mergeProperties, withProperties.
- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/properties/DXFeedPropertiesConverter.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/impl/properties/DXFeedPropertiesConverter.java) — convertProperties, loadTypeOverrides, loadRegionalsVisibility, loadOrderFieldsVisibility, loadGenerators, loadGenericFieldsVisibility.
- [dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeProperties.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeProperties.java) — property interpretation.

## Resources

- [dxfeed-scheme/src/main/resources/dxfeed.xml](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/resources/dxfeed.xml) — types.xml/visibility.xml imports, records, generators.
- [dxfeed-scheme/src/main/resources/types.xml](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/resources/types.xml) — custom type aliases.
- [dxfeed-scheme/src/main/resources/visibility.xml](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/resources/visibility.xml) — regional and field defaults.
- [dxfeed-scheme/src/main/resources/dxlink.xml](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/resources/dxlink.xml) — dxfeed import, suppression rules.
- [dxfeed-api/src/main/java/com/dxfeed/event/market/MarketEventSymbols.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/event/market/MarketEventSymbols.java) — EXCHANGES_PATTERN, SUPPORTED_EXCHANGES, DEFAULT_EXCHANGES.

## Runtime

- [dxfeed-scheme/src/main/java/com/dxfeed/scheme/DXScheme.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/main/java/com/dxfeed/scheme/DXScheme.java) — loadRecords, buildRecord, buildOneRecord, addIndexField.
- [qd-core/src/main/java/com/devexperts/qd/DataScheme.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/DataScheme.java) — lifetime contract and API.
- [qd-core/src/main/java/com/devexperts/qd/kit/DefaultScheme.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/DefaultScheme.java) — constructor, lookups, setParentReferences, getService, calculateDigest/serializeField.
- [qd-core/src/main/java/com/devexperts/qd/kit/DefaultRecord.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/DefaultRecord.java) — constructor, putDefaultFieldNames, putNonDefaultFieldNames, addFieldAlias, update.
- [qd-core/src/main/java/com/devexperts/qd/kit/AbstractDataField.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/AbstractDataField.java) — setRecord, getDefaultPropertyName.
- [dxfeed-impl/src/main/java/com/dxfeed/api/impl/VersionedRecord.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/VersionedRecord.java) — constructor, update.

## Mappings

- [qd-core/src/main/java/com/devexperts/qd/kit/RecordMappingFactoryHolder.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/qd-core/src/main/java/com/devexperts/qd/kit/RecordMappingFactoryHolder.java) — service discovery, createMapping overloads.
- [dxfeed-impl/src/main/resources/META-INF/services/com.devexperts.qd.ng.RecordMappingFactory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/resources/META-INF/services/com.devexperts.qd.ng.RecordMappingFactory) — provider list.
- [dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketFactoryImpl.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/MarketFactoryImpl.java) — buildScheme, createMapping, getBaseRecordName.
- [dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/QuoteMapping.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/event/market/impl/QuoteMapping.java) — constructor, property names, optional-field getters.

## Tests and programmatic path

- [dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedScheme.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/DXFeedScheme.java) — createRecords invokes EventDelegateFactory.buildScheme.
- [dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeBuilder.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/java/com/dxfeed/api/impl/SchemeBuilder.java) — addRequiredField/addOptionalField, buildRecords, RecordInfo.createRecord.
- [dxfeed-impl/src/main/resources/META-INF/services/com.devexperts.qd.DataScheme](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/resources/META-INF/services/com.devexperts.qd.DataScheme) — scheme registration.
- [dxfeed-impl/src/main/resources/META-INF/services/com.dxfeed.api.impl.EventDelegateFactory](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-impl/src/main/resources/META-INF/services/com.dxfeed.api.impl.EventDelegateFactory) — delegate providers.
- [dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderTests.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderTests.java) — testDefaultScheme, testEmptyScheme, testEmptySchemeIsEmpty.
- [dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderTypesTests.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderTypesTests.java) — basic, overrides, cycles, malformed XML.
- [dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderEnumsTests.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderEnumsTests.java) — ordinal/name/update assertions.
- [dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderRecordsParseTests.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderRecordsParseTests.java) — metadata, copy and override assertions.
- [dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderRecordsGenTests.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeLoaderRecordsGenTests.java) — runtime indexes, aliases, copies; testRecordUpdateWithoutType.
- [dxfeed-scheme/src/test/java/com/dxfeed/scheme/VisibilityRuleTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/test/java/com/dxfeed/scheme/VisibilityRuleTest.java) — testMatchWithoutUseEventName, testMatchWithUseEventName.
- [dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeDefaultSchemeTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/test/java/com/dxfeed/scheme/DXSchemeDefaultSchemeTest.java) — testDefaultSchemeEquality, compareRecord, parameterized setup.
- [dxfeed-scheme/src/test/java/com/dxfeed/scheme/DxSchemeExtSchemeTest.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-scheme/src/test/java/com/dxfeed/scheme/DxSchemeExtSchemeTest.java) — property precedence, suffixes, nanoseconds, decimal selection.
