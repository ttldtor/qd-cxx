package qdcxx.schemaexport;

import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Collections;

/** Focused boundary tests over production records, independent of captured expectations. */
public final class ExporterTests {
  private ExporterTests() {}

  interface Checked {
    void run() throws Exception;
  }

  static void check(boolean condition) {
    if (!condition) {
      throw new AssertionError();
    }
  }

  static void rejects(String code, Checked operation) throws Exception {
    try {
      operation.run();
      throw new AssertionError("Accepted " + code);
    } catch (IllegalArgumentException expected) {
      check(expected.getMessage().equals(code));
    }
  }

  static DefaultRecord mixed(int id, String name) {
    return new DefaultRecord(
        id,
        name,
        false,
        new DataIntField[] {
          SerialFieldType.WIDE_DECIMAL.createDefaultIntInstance(0, name + ".Price"),
          new VoidIntField(1, name + ".Price$VoidTail"),
          SerialFieldType.COMPACT_INT.createDefaultIntInstance(2, name + ".Count")
        },
        new DataObjField[] {
          SerialFieldType.UTF_CHAR_ARRAY.createDefaultObjInstance(0, name + ".Text")
        });
  }

  static ResolvedSchemaExporter.Snapshot export(
      DataScheme scheme, ResolvedSchemaExporter.Limits limits) throws Exception {
    return ResolvedSchemaExporter.export(scheme, Collections.emptyList(), limits);
  }

  /**
   * Requires a fresh scratch directory; verifies no partial or replaced destination on failures.
   */
  public static void main(String[] args) throws Exception {
    Path scratch = Files.createDirectory(Paths.get(args[0]));
    DefaultRecord mixed = mixed(0, "Mixed");
    mixed.addFieldAlias("Price", "AliasOne");
    mixed.addFieldAlias("AliasOne", "AliasTwo");
    DataScheme scheme = new DefaultScheme(PentaCodec.INSTANCE, mixed, mixed(1, "Second"));
    ResolvedSchemaExporter.Limits limits = new ResolvedSchemaExporter.Limits();
    String json = export(scheme, limits).schemaJson;
    check(json.contains("\"name\":\"AliasOne\",\"fieldId\":0"));
    check(json.contains("\"name\":\"AliasTwo\",\"fieldId\":0"));
    check(!json.contains("VoidTail"));
    check(json.indexOf("Mixed") < json.indexOf("Second"));
    check(
        json.equals(
            ResolvedSchemaExporter.export(scheme, Arrays.asList("Second", "Mixed"), limits)
                .schemaJson));
    check(
        ResolvedSchemaExporter.export(scheme, Arrays.asList("Second"), limits)
            .projectionJson
            .contains("\"sourceId\":1,\"exportedId\":0"));
    rejects(
        "INVALID_SCHEMA",
        () -> ResolvedSchemaExporter.export(scheme, Arrays.asList("Missing"), limits));
    rejects(
        "INVALID_SCHEMA",
        () -> ResolvedSchemaExporter.export(scheme, Arrays.asList("Mixed", "Mixed"), limits));
    limits.bytes = json.getBytes(StandardCharsets.UTF_8).length;
    check(export(scheme, limits).schemaJson.equals(json));
    limits.bytes--;
    rejects("RESOURCE_LIMIT", () -> export(scheme, limits));
    limits.bytes++;
    limits.records = 2;
    export(scheme, limits);
    limits.records = 1;
    rejects("RESOURCE_LIMIT", () -> export(scheme, limits));
    limits.records = 2;
    limits.fields = 6;
    export(scheme, limits);
    limits.fields = 5;
    rejects("RESOURCE_LIMIT", () -> export(scheme, limits));
    limits.fields = 6;
    limits.lookups = 17; // Eight keys per unaliased record, including tails, plus two aliases.
    rejects("RESOURCE_LIMIT", () -> export(scheme, limits));
    limits.lookups = 18;
    export(scheme, limits);
    limits.lookups = 32768;
    limits.nameUnits = "Second.Price$VoidTail".length();
    export(scheme, limits);
    limits.nameUnits--;
    rejects("RESOURCE_LIMIT", () -> export(scheme, limits));
    limits.nameUnits = 1024;
    int totalUnits = 0;
    for (DataRecord record : new DataRecord[] {scheme.getRecord(0), scheme.getRecord(1)}) {
      java.lang.reflect.Field map = DefaultRecord.class.getDeclaredField("fieldsByName");
      map.setAccessible(true);
      totalUnits += record.getName().length();
      for (Object key : ((java.util.Map<?, ?>) map.get(record)).keySet()) {
        totalUnits += ((String) key).length();
      }
    }
    limits.totalNameUnits = totalUnits;
    export(scheme, limits);
    limits.totalNameUnits--;
    rejects("RESOURCE_LIMIT", () -> export(scheme, limits));
    limits.totalNameUnits = 1048576;
    String projection = export(scheme, limits).projectionJson;
    limits.sidecarBytes = projection.getBytes(StandardCharsets.UTF_8).length;
    export(scheme, limits);
    limits.sidecarBytes--;
    rejects("RESOURCE_LIMIT", () -> export(scheme, limits));
    ResolvedSchemaExporter.Limits normal = new ResolvedSchemaExporter.Limits();
    DefaultRecord scalar = mixed(0, "A\u0000\"\\\ud83d\ude00");
    String scalarJson = export(new DefaultScheme(PentaCodec.INSTANCE, scalar), normal).schemaJson;
    check(scalarJson.contains("\\u0000\\\"\\\\\ud83d\ude00"));
    rejects(
        "INVALID_SCHEMA",
        () -> export(new DefaultScheme(PentaCodec.INSTANCE, mixed(0, "A\ud800")), normal));
    DefaultRecord standalone =
        new DefaultRecord(
            0, "Void", false, new DataIntField[] {new VoidIntField(0, "Void.Value")}, null);
    rejects(
        "UNSUPPORTED_TYPE",
        () -> export(new DefaultScheme(PentaCodec.INSTANCE, standalone), normal));
    DefaultRecord unsupported =
        new DefaultRecord(
            0,
            "Tiny",
            false,
            new DataIntField[] {SerialFieldType.DECIMAL.createDefaultIntInstance(0, "Tiny.Value")},
            null);
    rejects(
        "UNSUPPORTED_TYPE",
        () -> export(new DefaultScheme(PentaCodec.INSTANCE, unsupported), normal));
    DefaultRecord subclass = new DefaultRecord(0, "Subclass", false, null, null) {};
    rejects(
        "UNSUPPORTED_RECORD",
        () -> export(new DefaultScheme(PentaCodec.INSTANCE, subclass), normal));
    DefaultRecord indexed =
        new DefaultRecord(
            0,
            "Indexed",
            true,
            new DataIntField[] {
              SerialFieldType.COMPACT_INT.createDefaultIntInstance(0, "Indexed.A"),
              SerialFieldType.COMPACT_INT.createDefaultIntInstance(1, "Indexed.B")
            },
            null);
    rejects(
        "UNSUPPORTED_INDEXED",
        () -> export(new DefaultScheme(PentaCodec.INSTANCE, indexed), normal));
    String empty = export(new DefaultScheme(PentaCodec.INSTANCE), normal).schemaJson;
    check(empty.endsWith("\"records\":[]}\n"));
    Path destination = scratch.resolve("bundle");
    SchemaExport.publish(destination, json, "{}\n");
    check(
        new String(Files.readAllBytes(destination.resolve("schema.json")), StandardCharsets.UTF_8)
            .equals(json));
    try {
      SchemaExport.publish(destination, "wrong", "wrong");
      throw new AssertionError("Replaced destination");
    } catch (java.nio.file.FileAlreadyExistsException expected) {
      check(
          new String(Files.readAllBytes(destination.resolve("schema.json")), StandardCharsets.UTF_8)
              .equals(json));
    }
    Path file = scratch.resolve("existing-file");
    Files.write(file, new byte[] {42});
    try {
      SchemaExport.publish(file, "wrong", "wrong");
      throw new AssertionError("Replaced file");
    } catch (java.nio.file.FileAlreadyExistsException expected) {
      check(Files.readAllBytes(file)[0] == 42);
    }
    Path emptyDirectory = Files.createDirectory(scratch.resolve("empty-directory"));
    try {
      SchemaExport.publish(emptyDirectory, "wrong", "wrong");
      throw new AssertionError("Replaced empty directory");
    } catch (java.nio.file.FileAlreadyExistsException expected) {
      try (java.util.stream.Stream<Path> paths = Files.list(emptyDirectory)) {
        check(paths.count() == 0);
      }
    }
    java.lang.reflect.Field lookupMap = DefaultRecord.class.getDeclaredField("fieldsByName");
    lookupMap.setAccessible(true);
    @SuppressWarnings("unchecked")
    java.util.Map<Object, Object> entries = (java.util.Map<Object, Object>) lookupMap.get(mixed);
    entries.put("bad-target", new Object());
    rejects("UNSUPPORTED_LOOKUPS", () -> export(scheme, normal));
    entries.remove("bad-target");
    try (java.util.stream.Stream<Path> paths = Files.list(scratch)) {
      check(paths.noneMatch(p -> p.getFileName().toString().startsWith(".qdcxx-schema-")));
    }
    System.out.println("ExporterTests PASS");
  }
}
