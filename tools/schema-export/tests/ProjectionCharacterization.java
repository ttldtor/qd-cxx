package qdcxx.schemaexport.tests;

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.scheme.DXScheme;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** Independent production-object observations supporting the Java export projection contract. */
public final class ProjectionCharacterization {
  private ProjectionCharacterization() {}

  /** Runs assertions independently of the exporter; emits only deterministic observations. */
  public static void main(String[] args) throws Exception {
    if (args.length != 0) {
      throw new IllegalArgumentException("This characterization takes no arguments");
    }
    DefaultRecord mixed = mixedRecord(0, "Mixed");
    DataField price = mixed.getIntField(0);
    mixed.addFieldAlias("Price.Value", "FirstAlias");
    mixed.addFieldAlias("FirstAlias", "SecondAlias");
    check(price.getPropertyName().equals("SecondAlias"), "Latest property name");
    check(mixed.findFieldByName("FirstAlias") == price, "Accumulated first alias");
    check(mixed.findFieldByName("SecondAlias") == price, "Accumulated second alias");
    check(mixed.findFieldByName("PriceValue") == price, "Initial property name retained");
    try {
      mixed.addFieldAlias("Count", "FirstAlias");
      throw new AssertionError("Conflicting alias accepted");
    } catch (IllegalArgumentException expected) {
      check(mixed.findFieldByName("FirstAlias") == price, "Rejected alias changed target");
    }
    List<DataField> logical = observe(mixed);
    check(logical.size() == 3, "Wide plus int plus object logical count");
    check(logical.get(0) == price, "Wide field identity");
    check(logical.get(1) == mixed.getIntField(2), "Real integer after tail retained");
    check(logical.get(2) == mixed.getObjField(0), "Object field retained");
    DefaultRecord other = mixedRecord(1, "Other");
    other.addFieldAlias("Count", "FirstAlias");
    check(other.findFieldByName("FirstAlias") != price, "Lookup scope is per record");
    DataScheme scheme = new DefaultScheme(PentaCodec.INSTANCE, mixed, other);
    List<String> requested = Arrays.asList("Other", "Mixed");
    int exportedId = 0;
    for (int sourceId = 0; sourceId < scheme.getRecordCount(); sourceId++) {
      DataRecord record = scheme.getRecord(sourceId);
      if (requested.contains(record.getName())) {
        check(record.getId() == exportedId, "Selection retains source order");
        System.out.println("selection\t" + record.getId() + "\t" + exportedId++);
      }
    }
    check(exportedId == 2, "Complete selection");
    List<DataRecord> subset = new ArrayList<>();
    for (int sourceId = 0; sourceId < scheme.getRecordCount(); sourceId++) {
      DataRecord record = scheme.getRecord(sourceId);
      if (record.getName().equals("Other")) {
        subset.add(record);
      }
    }
    check(subset.size() == 1 && subset.get(0) == other, "Selected object identity");
    check(subset.get(0).getId() == 1, "Source ID differs from exported array position");
    System.out.println("selection-subset\t" + subset.get(0).getId() + "\t0");
    Field inaccessible = DefaultRecord.class.getDeclaredField("fieldsByName");
    try {
      inaccessible.get(mixed);
      throw new AssertionError("Private lookup map unexpectedly accessible");
    } catch (IllegalAccessException expected) {
      System.out.println("rejection\tinaccessible-map");
    }
    reject(
        new DefaultRecord(
            0,
            "Standalone",
            false,
            new DataIntField[] {
              SerialFieldType.VOID.createDefaultIntInstance(0, "Standalone.Void")
            },
            null));
    reject(
        new DefaultRecord(
            0,
            "Unsupported",
            false,
            new DataIntField[] {
              SerialFieldType.DECIMAL.createDefaultIntInstance(0, "Unsupported.Value")
            },
            null));
    reject(new DefaultRecord(0, "Subclass", false, null, null) {});
    observe(DXFeedScheme.getInstance().findRecordByName("Quote"));
    observe(DXFeedScheme.getInstance().findRecordByName("Profile"));
    DataScheme xml = DXScheme.newLoader().fromSpecification("resource:dxlink.xml").load();
    observe(xml.findRecordByName("Quote"));
    observe(xml.findRecordByName("Profile"));
    System.out.println("result\tPASS");
  }

  private static DefaultRecord mixedRecord(int id, String name) {
    return new DefaultRecord(
        id,
        name,
        false,
        new DataIntField[] {
          SerialFieldType.WIDE_DECIMAL.createDefaultIntInstance(0, name + ".Price.Value"),
          SerialFieldType.VOID.createDefaultIntInstance(1, name + ".Price.Value$VoidTail"),
          SerialFieldType.COMPACT_INT.createDefaultIntInstance(2, name + ".Count")
        },
        new DataObjField[] {
          SerialFieldType.UTF_CHAR_ARRAY.createDefaultObjInstance(0, name + ".Text")
        });
  }

  private static void reject(DataRecord record) throws Exception {
    try {
      observe(record);
      throw new AssertionError("Unsupported record accepted");
    } catch (IllegalArgumentException expected) {
      System.out.println("rejection\t" + record.getName());
    }
  }

  private static List<DataField> observe(DataRecord record) throws Exception {
    if (record.getClass() != DefaultRecord.class || record.hasTime()) {
      throw new IllegalArgumentException("Unsupported record implementation or indexing");
    }
    List<DataField> logical = new ArrayList<>();
    IdentityHashMap<DataField, Integer> logicalIds = new IdentityHashMap<>();
    IdentityHashMap<DataField, Integer> tails = new IdentityHashMap<>();
    for (int index = 0; index < record.getIntFieldCount(); index++) {
      DataField field = record.getIntField(index);
      requireSupported(field);
      logicalIds.put(field, logical.size());
      logical.add(field);
      if (field.getSerialType().isLong()) {
        check(++index < record.getIntFieldCount(), "Missing physical tail");
        DataField tail = record.getIntField(index);
        check(tail.getClass() == VoidIntField.class, "Tail implementation");
        check(tail.getSerialType() == SerialFieldType.VOID, "Tail identity");
        check(tail.getName().equals(field.getName() + "$VoidTail"), "Tail naming");
        tails.put(tail, logical.size() - 1);
      }
    }
    for (int index = 0; index < record.getObjFieldCount(); index++) {
      DataField field = record.getObjField(index);
      requireSupported(field);
      logicalIds.put(field, logical.size());
      logical.add(field);
    }
    Field lookupField = DefaultRecord.class.getDeclaredField("fieldsByName");
    check(lookupField.getType() == Map.class, "Lookup member type");
    check(Modifier.isPrivate(lookupField.getModifiers()), "Lookup member private");
    check(Modifier.isFinal(lookupField.getModifiers()), "Lookup member final");
    lookupField.setAccessible(true);
    Object lookupObject = lookupField.get(record);
    check(lookupObject instanceof Map, "Lookup member value");
    Map<?, ?> lookups = (Map<?, ?>) lookupObject;
    TreeMap<String, DataField> sorted = new TreeMap<>();
    for (Map.Entry<?, ?> entry : lookups.entrySet()) {
      check(
          entry.getKey() instanceof String && entry.getValue() instanceof DataField, "Entry shape");
      String key = (String) entry.getKey();
      DataField field = (DataField) entry.getValue();
      check(record.findFieldByName(key) == field, "Production lookup identity");
      check(logicalIds.containsKey(field) || tails.containsKey(field), "Lookup has known target");
      sorted.put(key, field);
    }
    for (DataField field : logical) {
      check(sorted.containsValue(field), "Logical field has lookup");
    }
    System.out.println(
        "record\t"
            + record.getName()
            + "\t"
            + record.getIntFieldCount()
            + "\t"
            + record.getObjFieldCount()
            + "\t"
            + logical.size()
            + "\t"
            + tails.size());
    for (Map.Entry<String, DataField> entry : sorted.entrySet()) {
      DataField field = entry.getValue();
      System.out.println(
          "lookup\t"
              + record.getName()
              + "\t"
              + entry.getKey()
              + "\t"
              + (tails.containsKey(field)
                  ? "tail:" + tails.get(field)
                  : "field:" + logicalIds.get(field)));
    }
    return logical;
  }

  private static void requireSupported(DataField field) {
    int type = field.getSerialType().getId();
    if (type != 2
        && type != 8
        && type != 10
        && type != 40
        && type != 56
        && type != 72
        && type != 88
        && type != 120) {
      throw new IllegalArgumentException("Unsupported complete field identity");
    }
  }

  private static void check(boolean condition, String message) {
    if (!condition) {
      throw new AssertionError(message);
    }
  }
}
