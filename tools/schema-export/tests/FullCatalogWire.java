// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

package qdcxx.schemaexport.tests;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.Marshalled;
import com.devexperts.qd.DataField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.qtp.BinaryRecordDesc;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.scheme.DXScheme;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Generate field bodies with pinned QD, or read the corresponding native result. */
public final class FullCatalogWire extends BinaryRecordDesc {
  private static final int REMOVE = 2;
  private static final long REMOVAL_TIME = 0x81234567fedcba98L;
  private static final String SYMBOL = "IBM";

  private FullCatalogWire(DataRecord record, int direction) throws Exception {
    super(record, false, direction, true);
  }

  public static void main(String[] args) throws Exception {
    DataScheme scheme =
        "default".equals(args[0])
            ? DXFeedScheme.getInstance()
            : DXScheme.newLoader().fromSpecification("resource:dxlink.xml").load();
    List<String> nativeBodies =
        args.length == 3 ? Files.readAllLines(Paths.get(args[2]), StandardCharsets.UTF_8) : null;
    if (nativeBodies != null && nativeBodies.size() != scheme.getRecordCount()) {
      throw new AssertionError("Native record count differs");
    }
    for (int index = 0; index < scheme.getRecordCount(); index++) {
      DataRecord record = scheme.getRecord(index);
      FullCatalogWire writer = new FullCatalogWire(record, DIR_WRITE);
      RecordBuffer source = RecordBuffer.getInstance(RecordMode.FLAGGED_DATA);
      RecordCursor cursor = source.add(record, 0, SYMBOL);
      fill(cursor, record, args[1]);
      boolean indexedRemoval = "default".equals(args[1]) && record.hasTime();
      List<String> row = new ArrayList<>();
      row.add(record.getName());
      row.add(encode(writer, cursor, 0));
      row.add("_");
      if (indexedRemoval) {
        cursor.setTime(REMOVAL_TIME);
        row.set(2, encode(writer, cursor, REMOVE));
        cursor.setTime(0);
      }
      int advertised = 0;
      for (int slot = 0; slot < record.getIntFieldCount() + record.getObjFieldCount(); slot++) {
        DataField field =
            slot < record.getIntFieldCount()
                ? record.getIntField(slot)
                : record.getObjField(slot - record.getIntFieldCount());
        String name = field.getPropertyName();
        int type = field.getSerialType().getId();
        if (!(type == 0
            && slot > 0
            && slot < record.getIntFieldCount()
            && record.getIntField(slot - 1).getSerialType().isLong())) {
          row.add(name);
        }
        if (type != 0) {
          if (advertised >= writer.nDesc
              || !name.equals(writer.names[advertised])
              || type != writer.types[advertised]) {
            throw new AssertionError("Description differs: " + record.getName() + "." + name);
          }
          advertised++;
        }
      }
      if (advertised != writer.nDesc) {
        throw new AssertionError("Description count differs: " + record.getName());
      }
      if (nativeBodies == null) {
        System.out.println(String.join("\t", row));
      } else {
        String[] nativeRow = nativeBodies.get(index).split("\t", -1);
        if (nativeRow.length != 3 || !record.getName().equals(nativeRow[0])) {
          throw new AssertionError("Malformed native row for " + record.getName());
        }
        check(record, cursor, nativeRow[1], 0);
        if (indexedRemoval) {
          check(record, cursor, nativeRow[2], REMOVE);
        } else if (!"_".equals(nativeRow[2])) {
          throw new AssertionError("Unexpected removal for " + record.getName());
        }
      }
      source.release();
    }
  }

  private static void fill(RecordCursor cursor, DataRecord record, String scenario) {
    if ("default".equals(scenario)) {
      return;
    }
    for (int slot = 0; slot < record.getIntFieldCount(); slot++) {
      if (record.getIntField(slot).getSerialType().isLong()) {
        cursor.setLong(slot, Long.MIN_VALUE + 37);
        slot++;
      } else {
        int type = record.getIntField(slot).getSerialType().getId();
        if (type != 0) {
          cursor.setInt(slot, type == 2 ? 0x0416 : Integer.MIN_VALUE + 37);
        }
      }
    }
    if ("boundary".equals(scenario)) {
      return;
    }
    boolean empty = "empty".equals(scenario);
    for (int slot = 0; slot < record.getObjFieldCount(); slot++) {
      int type = record.getObjField(slot).getSerialType().getId();
      if (type == 10) {
        cursor.setObj(slot, empty ? "" : "\u0416\uD83D\uDE80");
      } else if (type == 249) {
        cursor.setObj(
            slot, Marshalled.forBytes(empty ? new byte[0] : new byte[] {(byte) 0xff, 0x42}));
      }
    }
  }

  private static String encode(FullCatalogWire writer, RecordCursor cursor, int flags)
      throws Exception {
    ByteArrayOutput output = new ByteArrayOutput();
    writer.writeRecord(output, cursor, flags, 0);
    byte[] data = output.toByteArray();
    if (data.length == 0) {
      return "_";
    }
    StringBuilder hex = new StringBuilder();
    for (byte value : data) {
      hex.append(Character.forDigit((value & 0xff) >>> 4, 16));
      hex.append(Character.forDigit(value & 0x0f, 16));
    }
    return hex.toString();
  }

  private static Object objectValue(Object value) {
    // The Java decoder represents a null serialized field with a Marshalled wrapper.
    return value instanceof Marshalled && ((Marshalled<?>) value).getBytes() == null ? null : value;
  }

  private static void check(DataRecord record, RecordCursor expected, String hex, int flags)
      throws Exception {
    byte[] body = new byte["_".equals(hex) ? 0 : hex.length() / 2];
    for (int index = 0; index < body.length; index++) {
      body[index] = (byte) Integer.parseInt(hex.substring(2 * index, 2 * index + 2), 16);
    }
    ByteArrayInput input = new ByteArrayInput(body);
    RecordBuffer decoded = RecordBuffer.getInstance(RecordMode.FLAGGED_DATA);
    RecordCursor actual =
        new FullCatalogWire(record, DIR_READ).readRecord(input, decoded, 0, SYMBOL, flags);
    if (input.available() != 0) {
      throw new AssertionError("Trailing field bytes: " + record.getName());
    }
    if (flags == REMOVE) {
      if (actual.getTime() != REMOVAL_TIME) {
        throw new AssertionError("Incorrect removal time: " + record.getName());
      }
    } else {
      for (int slot = 0; slot < record.getIntFieldCount(); slot++) {
        if (expected.getInt(slot) != actual.getInt(slot)) {
          throw new AssertionError("Integer mismatch: " + record.getName() + ", " + slot);
        }
      }
      for (int slot = 0; slot < record.getObjFieldCount(); slot++) {
        if (!Objects.equals(objectValue(expected.getObj(slot)), objectValue(actual.getObj(slot)))) {
          throw new AssertionError("Object mismatch: " + record.getName() + ", " + slot);
        }
      }
    }
    decoded.release();
  }
}
