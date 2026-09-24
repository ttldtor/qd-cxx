package qdcxx.schemaexport;

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.VoidIntField;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/** Read-only projection of a resolved scheme; see spec/schema/JAVA-EXPORT.md. */
public final class ResolvedSchemaExporter {
  private ResolvedSchemaExporter() {}

  /** Finite native and sidecar limits, checked before proportional projection allocations. */
  public static final class Limits {
    public int bytes = 1048576;
    public int records = 1024;
    public int fields = 16384;
    public int lookups = 32768;
    public int nameUnits = 1024;
    public int totalNameUnits = 1048576;
    public int sidecarBytes = 4194304;

    void validate() {
      require(
          bytes >= 0
              && records >= 0
              && fields >= 0
              && lookups >= 0
              && nameUnits >= 0
              && totalNameUnits >= 0
              && sidecarBytes >= 0,
          "INVALID_SCHEMA");
    }
  }

  /** Owned deterministic strings; neither references the input scheme. */
  public static final class Snapshot {
    public final String schemaJson;
    public final String projectionJson;

    Snapshot(String schemaJson, String projectionJson) {
      this.schemaJson = schemaJson;
      this.projectionJson = projectionJson;
    }
  }

  /**
   * Exports all records when selection is empty; exact selection otherwise retains source order.
   * The caller must not mutate the supplied scheme during export. No global properties are read.
   */
  public static Snapshot export(DataScheme scheme, List<String> selection, Limits limits)
      throws ReflectiveOperationException {
    limits.validate();
    require(scheme != null && selection != null, "INVALID_SCHEMA");
    Set<String> requested = new HashSet<>();
    require(selection.size() <= limits.records, "RESOURCE_LIMIT");
    for (String name : selection) {
      scalar(name, limits.nameUnits);
      require(requested.add(name) && scheme.findRecordByName(name) != null, "INVALID_SCHEMA");
    }
    Writer json = new Writer(limits.bytes);
    Writer projection = new Writer(limits.sidecarBytes);
    json.add(
        "{\"formatVersion\":1,\"profile\":{\"id\":\"qdcxx-native\",\"version\":1},\"records\":[");
    projection.add("[");
    int recordCount = 0;
    long fieldCount = 0;
    long lookupCount = 0;
    long nameCount = 0;
    Set<String> recordNames = new HashSet<>();
    for (int sourceId = 0; sourceId < scheme.getRecordCount(); sourceId++) {
      DataRecord record = scheme.getRecord(sourceId);
      if (!requested.isEmpty() && !requested.contains(record.getName())) {
        continue;
      }
      require(recordCount < limits.records, "RESOURCE_LIMIT");
      require(record.getClass() == DefaultRecord.class, "UNSUPPORTED_RECORD");
      require(
          record.getId() == sourceId && scheme.findRecordByName(record.getName()) == record,
          "INVALID_SCHEMA");
      require(!record.hasTime(), "UNSUPPORTED_INDEXED");
      scalar(record.getName(), limits.nameUnits);
      nameCount += record.getName().length();
      require(nameCount <= limits.totalNameUnits, "RESOURCE_LIMIT");
      require(recordNames.add(record.getName()), "INVALID_SCHEMA");
      // Each supported logical value occupies at most two integer slots.
      require(
          (long) record.getIntFieldCount() + record.getObjFieldCount()
              <= 2L * (limits.fields - fieldCount),
          "RESOURCE_LIMIT");
      List<DataField> fields = new ArrayList<>();
      IdentityHashMap<DataField, Integer> retained = new IdentityHashMap<>();
      IdentityHashMap<DataField, Integer> tails = new IdentityHashMap<>();
      for (int slot = 0; slot < record.getIntFieldCount(); slot++) {
        DataField field = record.getIntField(slot);
        require(field.getIndex() == slot && field.getRecord() == record, "INVALID_SCHEMA");
        checkType(field, false);
        require(++fieldCount <= limits.fields, "RESOURCE_LIMIT");
        retained.put(field, fields.size());
        fields.add(field);
        if (field.getSerialType().isLong()) {
          require(slot + 1 < record.getIntFieldCount(), "INVALID_SCHEMA");
          DataField tail = record.getIntField(++slot);
          require(
              tail.getClass() == VoidIntField.class
                  && tail.getSerialType().getId() == 0
                  && tail.getIndex() == slot
                  && tail.getRecord() == record
                  && tail.getName().equals(field.getName() + "$VoidTail"),
              "INVALID_SCHEMA");
          tails.put(tail, fields.size() - 1);
        }
      }
      for (int slot = 0; slot < record.getObjFieldCount(); slot++) {
        DataField field = record.getObjField(slot);
        require(field.getIndex() == slot && field.getRecord() == record, "INVALID_SCHEMA");
        checkType(field, true);
        require(++fieldCount <= limits.fields, "RESOURCE_LIMIT");
        retained.put(field, fields.size());
        fields.add(field);
      }
      Map<?, ?> actual = lookupMap(record);
      // Account omitted-tail lookup work too, so introspection cannot evade finite budgets.
      require(actual.size() <= limits.lookups - lookupCount, "RESOURCE_LIMIT");
      TreeMap<String, DataField> sorted = new TreeMap<>();
      for (Map.Entry<?, ?> entry : actual.entrySet()) {
        require(
            entry.getKey() instanceof String && entry.getValue() instanceof DataField,
            "UNSUPPORTED_LOOKUPS");
        String key = (String) entry.getKey();
        DataField value = (DataField) entry.getValue();
        scalar(key, limits.nameUnits);
        nameCount += key.length();
        require(nameCount <= limits.totalNameUnits, "RESOURCE_LIMIT");
        require(
            record.findFieldByName(key) == value
                && (retained.containsKey(value) || tails.containsKey(value)),
            "UNSUPPORTED_LOOKUPS");
        sorted.put(key, value);
      }
      lookupCount += sorted.size();
      if (recordCount != 0) {
        json.add(",");
        projection.add(",");
      }
      json.add(
          "{\"id\":"
              + recordCount
              + ",\"name\":"
              + quote(record.getName())
              + ",\"hasTime\":false,\"fields\":[");
      projection.add(
          "{\"sourceId\":" + sourceId + ",\"exportedId\":" + recordCount + ",\"slots\":[");
      for (int id = 0; id < fields.size(); id++) {
        if (id != 0) {
          json.add(",");
          projection.add(",");
        }
        DataField field = fields.get(id);
        json.add("{\"serialTypeId\":" + field.getSerialType().getId() + "}");
        projection.add(
            "{\"fieldId\":"
                + id
                + ",\"storage\":"
                + quote(field.getSerialType().isObject() ? "object" : "integer")
                + ",\"slot\":"
                + field.getIndex()
                + ",\"tail\":"
                + (field.getSerialType().isLong() ? Integer.toString(field.getIndex() + 1) : "null")
                + "}");
      }
      json.add("],\"lookups\":[");
      projection.add("],\"omittedLookups\":[");
      boolean first = true;
      boolean firstTail = true;
      boolean[] referenced = new boolean[fields.size()];
      for (Map.Entry<String, DataField> entry : sorted.entrySet()) {
        Integer id = retained.get(entry.getValue());
        if (id != null) {
          if (!first) {
            json.add(",");
          }
          first = false;
          referenced[id] = true;
          json.add("{\"name\":" + quote(entry.getKey()) + ",\"fieldId\":" + id + "}");
        } else {
          if (!firstTail) {
            projection.add(",");
          }
          firstTail = false;
          projection.add(
              "{\"name\":"
                  + quote(entry.getKey())
                  + ",\"ownerFieldId\":"
                  + tails.get(entry.getValue())
                  + "}");
        }
      }
      for (boolean found : referenced) {
        require(found, "UNSUPPORTED_LOOKUPS");
      }
      json.add("]}");
      projection.add("]}");
      recordCount++;
    }
    require(requested.isEmpty() || recordCount == requested.size(), "INVALID_SCHEMA");
    json.add("]}\n");
    projection.add("]");
    return new Snapshot(json.toString(), projection.toString());
  }

  private static void checkType(DataField field, boolean object) {
    int id = field.getSerialType().getId();
    require(
        id == 2 || id == 8 || id == 10 || id == 40 || id == 56 || id == 72 || id == 88 || id == 120,
        "UNSUPPORTED_TYPE");
    require(field.getSerialType().isObject() == object, "INVALID_SCHEMA");
  }

  /** Isolated read-only adapter for the locked DefaultRecord map shape. */
  private static Map<?, ?> lookupMap(DataRecord record) throws ReflectiveOperationException {
    Field member = DefaultRecord.class.getDeclaredField("fieldsByName");
    require(
        member.getType() == Map.class
            && Modifier.isPrivate(member.getModifiers())
            && Modifier.isFinal(member.getModifiers())
            && !Modifier.isStatic(member.getModifiers()),
        "UNSUPPORTED_LOOKUPS");
    try {
      member.setAccessible(true);
      Object value = member.get(record);
      require(value instanceof Map, "UNSUPPORTED_LOOKUPS");
      return (Map<?, ?>) value;
    } catch (SecurityException failure) {
      throw new IllegalArgumentException("UNSUPPORTED_LOOKUPS");
    }
  }

  static void require(boolean condition, String code) {
    if (!condition) {
      throw new IllegalArgumentException(code);
    }
  }

  static void scalar(String value, int limit) {
    require(value != null && !value.isEmpty(), "INVALID_SCHEMA");
    require(value.length() <= limit, "RESOURCE_LIMIT");
    for (int i = 0; i < value.length(); i++) {
      char c = value.charAt(i);
      if (Character.isHighSurrogate(c)) {
        require(
            ++i < value.length() && Character.isLowSurrogate(value.charAt(i)), "INVALID_SCHEMA");
      } else {
        require(!Character.isLowSurrogate(c), "INVALID_SCHEMA");
      }
    }
  }

  /** Fixed JSON string spelling; scalar names are validated before serialization. */
  public static String quote(String value) {
    StringBuilder out = new StringBuilder("\"");
    for (int i = 0; i < value.length(); i++) {
      char c = value.charAt(i);
      if (c == '"' || c == '\\') {
        out.append('\\').append(c);
      } else if (c < 0x20) {
        out.append("\\u00")
            .append("0123456789abcdef".charAt(c >>> 4))
            .append("0123456789abcdef".charAt(c & 15));
      } else {
        out.append(c);
      }
    }
    return out.append('"').toString();
  }

  /** Byte-limited append-only writer; proportional strings are bounded before reaching it. */
  static final class Writer {
    private final int limit;
    private long bytes;
    private final StringBuilder value = new StringBuilder();

    Writer(int limit) {
      this.limit = limit;
    }

    void add(String part) {
      bytes += part.getBytes(StandardCharsets.UTF_8).length;
      require(bytes <= limit, "RESOURCE_LIMIT");
      value.append(part);
    }

    @Override
    public String toString() {
      return value.toString();
    }
  }
}
