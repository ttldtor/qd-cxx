// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.util.Decimal;
import com.devexperts.util.WideDecimal;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.Summary;
import com.dxfeed.event.market.SummaryDelegate;
import com.dxfeed.event.market.impl.SummaryMapping;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Observe actual production long accessors separately from the immutable delegate capture. */
public final class OpenInterestLongOracle {
  private static final int[] NUMERIC = {2, 4, 8, 24, 104, 120};

  private static final class Definition {
    String name;
    List<String> local = new ArrayList<>();
    List<SerialFieldType> types = new ArrayList<>();
    DataRecord record;
    SummaryDelegate delegate;
    SummaryMapping mapping;
    int[] slots;
    RecordCursor cursor;

    void prepare() {
      List<DataIntField> fields = new ArrayList<>();
      slots = new int[types.size()];
      for (int index = 0; index < types.size(); ++index) {
        SerialFieldType type = types.get(index);
        slots[index] = fields.size();
        fields.add(type.createDefaultIntInstance(fields.size(), name + "." + local.get(index)));
        if (type.isLong()) {
          fields.add(new VoidIntField(fields.size(), name + "." + local.get(index) + "$tail"));
        }
      }
      record =
          new DefaultRecord(
              0, name, false, fields.toArray(new DataIntField[0]), new DataObjField[0]);
      new DefaultScheme(PentaCodec.INSTANCE, new DataRecord[] {record});
      for (EventDelegate<?> value : new MarketFactoryImpl().createStreamOnlyDelegates(record)) {
        if (value instanceof SummaryDelegate) {
          delegate = (SummaryDelegate) value;
          break;
        }
      }
      if (delegate == null) {
        throw new IllegalArgumentException("No production Summary delegate");
      }
      mapping = record.getMapping(SummaryMapping.class);
    }
  }

  private static SerialFieldType type(int id) {
    for (SerialFieldType type :
        new SerialFieldType[] {
          SerialFieldType.UTF_CHAR,
          SerialFieldType.INT,
          SerialFieldType.COMPACT_INT,
          SerialFieldType.DECIMAL,
          SerialFieldType.SHORT_STRING,
          SerialFieldType.TIME_SECONDS,
          SerialFieldType.SEQUENCE,
          SerialFieldType.DATE,
          SerialFieldType.LONG,
          SerialFieldType.WIDE_DECIMAL,
          SerialFieldType.TIME_MILLIS
        }) {
      if (type.getId() == id) {
        return type;
      }
    }
    throw new IllegalArgumentException("Unexpected captured field type " + id);
  }

  private static void row(PrintWriter output, Object... cells) {
    for (int index = 0; index < cells.length; ++index) {
      if (index != 0) {
        output.print('\t');
      }
      output.print(cells[index]);
    }
    output.print('\n');
  }

  private static String units(String value) {
    StringBuilder output = new StringBuilder();
    for (char unit : value.toCharArray()) {
      output.append(String.format("%04x", (int) unit));
    }
    return output.toString();
  }

  private static String text(String hex) {
    StringBuilder value = new StringBuilder();
    for (int offset = 0; offset < hex.length(); offset += 4) {
      value.append((char) Integer.parseInt(hex.substring(offset, offset + 4), 16));
    }
    return value.toString();
  }

  private static void reference(
      PrintWriter output, String scenario, int record, int sample, long delegated, long mapped) {
    row(
        output,
        "reference",
        scenario,
        record,
        sample,
        delegated,
        mapped,
        mapped,
        delegated == mapped ? "IDENTICAL" : "CORRECTED_INT_ACCESSOR_LOSS");
  }

  private static void replay(Path input, PrintWriter output) throws IOException {
    Map<String, Definition> definitions = new HashMap<>();
    try (BufferedReader reader = Files.newBufferedReader(input, StandardCharsets.UTF_8)) {
      String line;
      int lines = 0;
      while ((line = reader.readLine()) != null) {
        if (++lines > 100000 || line.length() > 8192) {
          throw new IOException("Raw capture bound exceeded");
        }
        String[] cells = line.split("\t", -1);
        if (cells[0].equals("factory-reject") || cells[0].equals("interest")) {
          continue;
        }
        String key = cells[1] + "/" + cells[2];
        if (cells[0].equals("record")) {
          Definition definition = new Definition();
          definition.name = cells[3];
          definitions.put(key, definition);
          continue;
        }
        Definition definition = definitions.get(key);
        if (cells[0].equals("field")) {
          definition.types.add(type(Integer.parseInt(cells[4])));
          definition.local.add(cells[5]);
        } else if (cells[0].equals("symbol")) {
          if (definition.record == null) {
            definition.prepare();
          }
          definition.cursor =
              new RecordBuffer(RecordMode.DATA.withEventTimeSequence())
                  .add(definition.record, 0, text(cells[4]));
          definition.cursor.setEventTimeSequence(Long.parseLong(cells[5]));
        } else if (cells[0].equals("value")) {
          int index = Integer.parseInt(cells[4]);
          long raw = Long.parseLong(cells[5]);
          if (definition.types.get(index).isLong()) {
            definition.cursor.setLong(definition.slots[index], raw);
          } else {
            definition.cursor.setInt(definition.slots[index], (int) raw);
          }
        } else if (cells[0].equals("summary")) {
          long delegated = definition.delegate.createEvent(definition.cursor).getOpenInterest();
          if (delegated != Long.parseLong(cells[8])) {
            throw new IllegalStateException("Original delegate capture changed: " + key);
          }
          reference(
              output,
              cells[1],
              Integer.parseInt(cells[2]),
              Integer.parseInt(cells[3]),
              delegated,
              definition.mapping.getOpenInterestLong(definition.cursor));
        }
      }
    }
  }

  private static void dump(
      PrintWriter output,
      PrintWriter references,
      Definition definition,
      String scenario,
      int record,
      int sample) {
    Summary event = definition.delegate.createEvent(definition.cursor);
    SummaryMapping mapping = definition.mapping;
    RecordCursor cursor = definition.cursor;
    row(output, "symbol", scenario, record, sample, units("AAPL"), 0);
    for (int index = 0; index < definition.types.size(); ++index) {
      row(
          output,
          "value",
          scenario,
          record,
          sample,
          index,
          definition.types.get(index).isLong()
              ? cursor.getLong(definition.slots[index])
              : cursor.getInt(definition.slots[index]),
          "null");
    }
    int flags = mapping.getFlags(cursor);
    row(
        output,
        "summary",
        scenario,
        record,
        sample,
        units(event.getEventSymbol()),
        event.getEventTime(),
        event.getDayId(),
        event.getPrevDayId(),
        event.getOpenInterest(),
        flags,
        event.getDayClosePriceType().getCode(),
        event.getPrevDayClosePriceType().getCode(),
        mapping.getDayOpenPriceWideDecimal(cursor),
        mapping.getDayHighPriceWideDecimal(cursor),
        mapping.getDayLowPriceWideDecimal(cursor),
        mapping.getDayClosePriceWideDecimal(cursor),
        mapping.getPrevDayClosePriceWideDecimal(cursor),
        mapping.getPrevDayVolumeWideDecimal(cursor),
        Double.doubleToRawLongBits(event.getDayOpenPrice()),
        Double.doubleToRawLongBits(event.getDayHighPrice()),
        Double.doubleToRawLongBits(event.getDayLowPrice()),
        Double.doubleToRawLongBits(event.getDayClosePrice()),
        Double.doubleToRawLongBits(event.getPrevDayClosePrice()),
        Double.doubleToRawLongBits(event.getPrevDayVolume()));
    reference(
        references,
        scenario,
        record,
        sample,
        event.getOpenInterest(),
        mapping.getOpenInterestLong(cursor));
  }

  private static void boundaries(PrintWriter output, PrintWriter references) {
    long[] values = {
      Long.MIN_VALUE,
      Long.MIN_VALUE + 1,
      -4294967297L,
      -4294967296L,
      -4294967295L,
      -2147483649L,
      -2147483648L,
      -2147483647L,
      -1,
      0,
      1,
      2147483647L,
      2147483648L,
      2147483649L,
      3000000000L,
      4294967295L,
      4294967296L,
      4294967297L,
      Long.MAX_VALUE - 1,
      Long.MAX_VALUE
    };
    for (int numeric : NUMERIC) {
      String scenario = "oi-long" + numeric;
      for (int record = 0; record < 2; ++record) {
        DataRecord source =
            DXFeedScheme.getInstance().findRecordByName(record == 0 ? "Summary" : "Summary&Q");
        Definition definition = new Definition();
        definition.name = source.getName();
        for (int index = source.getIntFieldCount() - 1; index >= 0; --index) {
          DataIntField field = source.getIntField(index);
          if (field.getSerialType().getId() == 0) {
            continue;
          }
          definition.local.add(field.getLocalName());
          definition.types.add(
              field.getLocalName().equals("OpenInterest") ? type(numeric) : field.getSerialType());
        }
        definition.prepare();
        row(output, "record", scenario, record, definition.name);
        for (int index = 0; index < definition.types.size(); ++index) {
          row(
              output,
              "field",
              scenario,
              record,
              index,
              definition.types.get(index).getId(),
              definition.local.get(index),
              definition.record.getIntField(definition.slots[index]).getPropertyName());
        }
        int sample = 0;
        for (long value : values) {
          if (numeric == 2 && (value < 0 || value > 0x10ffff)) {
            continue;
          }
          definition.cursor =
              new RecordBuffer(RecordMode.DATA.withEventTimeSequence())
                  .add(definition.record, 0, "AAPL");
          definition.mapping.setOpenInterestLong(definition.cursor, value);
          dump(output, references, definition, scenario, record, sample++);
        }
        if (numeric == 24 || numeric == 120) {
          for (long raw :
              new long[] {
                0,
                numeric == 24 ? Decimal.POSITIVE_INFINITY : WideDecimal.POSITIVE_INFINITY,
                numeric == 24 ? Decimal.NEGATIVE_INFINITY : WideDecimal.NEGATIVE_INFINITY
              }) {
            definition.cursor =
                new RecordBuffer(RecordMode.DATA.withEventTimeSequence())
                    .add(definition.record, 0, "AAPL");
            for (int index = 0; index < definition.local.size(); ++index) {
              if (definition.local.get(index).equals("OpenInterest")) {
                if (numeric == 24) {
                  definition.cursor.setInt(definition.slots[index], (int) raw);
                } else {
                  definition.cursor.setLong(definition.slots[index], raw);
                }
              }
            }
            dump(output, references, definition, scenario, record, sample++);
          }
        }
      }
    }
  }

  public static void main(String[] args) throws IOException {
    if (args.length != 3) {
      throw new IllegalArgumentException(
          "Expected immutable rawcapture,referenceoutput,boundaryoutput");
    }
    try (PrintWriter references =
            new PrintWriter(Files.newBufferedWriter(Paths.get(args[1]), StandardCharsets.UTF_8));
        PrintWriter boundary =
            new PrintWriter(Files.newBufferedWriter(Paths.get(args[2]), StandardCharsets.UTF_8))) {
      replay(Paths.get(args[0]), references);
      boundaries(boundary, references);
      if (references.checkError() || boundary.checkError()) {
        throw new IOException("Cannot write long reference");
      }
    }
  }
}
