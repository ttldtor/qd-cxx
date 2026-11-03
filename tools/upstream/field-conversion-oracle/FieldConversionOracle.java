// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

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
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.market.MarketEvent;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.Profile;
import com.dxfeed.event.market.ProfileDelegate;
import com.dxfeed.event.market.Quote;
import com.dxfeed.event.market.QuoteDelegate;
import com.dxfeed.event.market.impl.ProfileMapping;
import com.dxfeed.event.market.impl.QuoteMapping;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInput;
import java.io.DataInputStream;
import java.io.DataOutput;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Observes generated production mappings and delegates on exact configurable physical types. */
public final class FieldConversionOracle {
  private static final int[] NUMERIC = {2, 4, 8, 24, 104, 120};
  private static final int[] RAW = {2, 4, 8, 24, 40, 56, 72, 88, 104, 120, 152};
  private static final String[] PROFILE_NUMERIC = {
    "Beta",
    "Eps",
    "DivFreq",
    "ExdDiv.Amount",
    "52High.Price",
    "52Low.Price",
    "Shares",
    "FreeFloat",
    "HighLimitPrice",
    "LowLimitPrice"
  };

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) out.print('\t');
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static String units(String text) {
    if (text == null) return "null";
    if (text.isEmpty()) return "-";
    StringBuilder result = new StringBuilder();
    for (char value : text.toCharArray()) result.append(String.format("%04x", (int) value));
    return result.toString();
  }

  private static boolean numeric(String name, String field) {
    return name.equals("Quote")
        ? field.matches("(Bid|Ask)\\.(Price|Size)")
        : Arrays.asList(PROFILE_NUMERIC).contains(field);
  }

  private static boolean raw(String name, String field) {
    return name.equals("Quote")
        ? field.equals("Sequence") || field.equals("TimeNanoPart") || field.endsWith(".Exchange")
        : field.equals("ExdDiv.Date") || field.equals("Flags") || field.startsWith("Halt.");
  }

  private static SerialFieldType type(int id) {
    for (SerialFieldType value :
        new SerialFieldType[] {
          SerialFieldType.BYTE,
          SerialFieldType.UTF_CHAR,
          SerialFieldType.SHORT,
          SerialFieldType.INT,
          SerialFieldType.COMPACT_INT,
          SerialFieldType.DECIMAL,
          SerialFieldType.SHORT_STRING,
          SerialFieldType.TIME_SECONDS,
          SerialFieldType.SEQUENCE,
          SerialFieldType.DATE,
          SerialFieldType.LONG,
          SerialFieldType.WIDE_DECIMAL,
          SerialFieldType.TIME_MILLIS,
          SerialFieldType.UTF_CHAR_ARRAY,
          SerialFieldType.STRING,
          SerialFieldType.BYTE_ARRAY,
          SerialFieldType.CUSTOM_OBJECT,
          SerialFieldType.SERIAL_OBJECT
        }) if (value.getId() == id) return value;
    throw new IllegalArgumentException("Unknown fixture type " + id);
  }

  private static DataRecord variant(DataRecord source, int id, String family, int type) {
    List<DataIntField> ints = new ArrayList<>();
    List<DataObjField> objects = new ArrayList<>();
    List<DataField> fields = new ArrayList<>();
    for (int i = 0; i < source.getIntFieldCount(); ++i) {
      DataIntField field = source.getIntField(i);
      if (field.getSerialType().getId() != 0) fields.add(field);
    }
    for (int i = 0; i < source.getObjFieldCount(); ++i) fields.add(source.getObjField(i));
    // Reverse order exercises logical names independently of physical slot positions.
    Collections.reverse(fields);
    for (DataField field : fields) {
      String local = field.getLocalName();
      boolean isNumeric = numeric(source.getName(), local);
      boolean isRaw = raw(source.getName(), local);
      if (family.equals("missing")
          && (!source.getName().equals("Quote") || !isNumeric)
          && !local.equals("Description")) continue;
      if (family.equals("required")
          && (source.getName().equals("Quote") ? isNumeric : local.equals("Description"))) continue;
      SerialFieldType selected = field.getSerialType();
      if (family.equals("numeric") && isNumeric
          || family.equals("raw") && isRaw
          || family.equals("text") && field instanceof DataObjField
          || family.equals("bad-numeric") && isNumeric
          || family.equals("bad-text") && field instanceof DataObjField) selected = type(type);
      if (selected.isObject())
        objects.add(selected.createDefaultObjInstance(objects.size(), field.getName()));
      else {
        ints.add(selected.createDefaultIntInstance(ints.size(), field.getName()));
        if (selected.isLong())
          ints.add(new VoidIntField(ints.size(), field.getName() + "$VoidTail"));
      }
    }
    return new DefaultRecord(
        id,
        source.getName(),
        false,
        ints.toArray(new DataIntField[0]),
        objects.toArray(new DataObjField[0]));
  }

  private static EventDelegate<?> delegate(DataRecord record) {
    for (EventDelegate<?> value : new MarketFactoryImpl().createStreamOnlyDelegates(record)) {
      if (value instanceof QuoteDelegate || value instanceof ProfileDelegate) return value;
    }
    throw new IllegalArgumentException("No production delegate");
  }

  private static long[] samples(int type) {
    if (type == 1) return new long[] {-128, -1, 0, 1, 127};
    if (type == 2) return new long[] {0, 65, 0xd800, 0xffff, 0x10ffff};
    if (type == 3) return new long[] {-32768, -1, 0, 1, 32767};
    if (type == 24) {
      long[] result = new long[128 * 5];
      int p = 0;
      for (int high : new int[] {Integer.MIN_VALUE, -128, 0, 128, 0x7fffff80})
        for (int low = 0; low < 128; ++low) result[p++] = high | low;
      return result;
    }
    if (type == 104) {
      List<Long> values =
          new ArrayList<>(Arrays.asList(Long.MIN_VALUE, Long.MAX_VALUE, 0L, -1L, 1L));
      long min = -(1L << 55), max = (1L << 55) - 1;
      for (long base :
          new long[] {
            min,
            max,
            min * 10,
            max * 10,
            min * 100,
            max * 100,
            min * 10 - 5,
            max * 10 + 5,
            min * 100 - 50,
            max * 100 + 50
          }) for (int delta = -6; delta <= 6; ++delta) values.add(base + delta);
      for (long base :
          new long[] {
            -9007199254740992L, 9007199254740992L, Long.MIN_VALUE + 1000, Long.MAX_VALUE - 1000
          })
        for (int delta : new int[] {-501, -500, -499, -50, -5, 0, 5, 50, 499, 500, 501})
          values.add(base + delta);
      long[] result = new long[values.size()];
      for (int i = 0; i < result.length; ++i) result[i] = values.get(i);
      return result;
    }
    if (type == 120 || type == 152)
      return new long[] {
        Long.MIN_VALUE, Long.MAX_VALUE, 0, 128, 256, -256, 0x123456789abcdef0L, -1
      };
    return new long[] {Integer.MIN_VALUE, Integer.MAX_VALUE, -1, 0, 1, 17, 0x12345678};
  }

  private static List<DataField> fields(DataRecord record) {
    List<DataField> fields = new ArrayList<>();
    for (int i = 0; i < record.getIntFieldCount(); ++i)
      if (record.getIntField(i).getSerialType().getId() != 0) fields.add(record.getIntField(i));
    for (int i = 0; i < record.getObjFieldCount(); ++i) fields.add(record.getObjField(i));
    return fields;
  }

  private static void observe(
      PrintWriter out, DataRecord record, String scenario, String family, int type)
      throws Exception {
    row(out, "record", scenario, record.getId(), record.getName());
    List<DataField> fields = fields(record);
    for (int i = 0; i < fields.size(); ++i) {
      DataField field = fields.get(i);
      row(
          out,
          "field",
          scenario,
          record.getId(),
          i,
          field.getSerialType().getId(),
          field.getLocalName(),
          field.getPropertyName());
    }
    EventDelegate<?> mapper;
    try {
      mapper = delegate(record);
    } catch (IllegalArgumentException failure) {
      row(out, "reject", scenario, record.getId(), failure.getClass().getSimpleName());
      return;
    }
    long[] samples = samples(type);
    for (int sample = 0; sample < samples.length; ++sample) {
      RecordCursor cursor = new RecordBuffer(RecordMode.DATA).add(record, 0, "AAPL");
      for (DataField field : fields) {
        long value =
            family.equals("raw")
                ? (raw(record.getName(), field.getLocalName()) ? samples[sample] : 0)
                : numeric(record.getName(), field.getLocalName()) ? samples[sample] : 0;
        if (field instanceof DataObjField) {
          Object text =
              sample % 4 == 0
                  ? null
                  : sample % 4 == 1 ? "" : sample % 4 == 2 ? "A\u0000\ud800\udc00\udfff" : "text";
          if (family.equals("bad-text")) text = new byte[] {1, 2};
          if (family.equals("text")) {
            DataObjField objectField = (DataObjField) field;
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            objectField.writeObj((DataOutput) new DataOutputStream(bytes), text);
            text =
                objectField.readObj(
                    (DataInput) new DataInputStream(new ByteArrayInputStream(bytes.toByteArray())));
            if (text != null && !(text instanceof String))
              throw new IllegalStateException("String reader returned another storage class");
            row(
                out,
                "text-read",
                scenario,
                record.getId(),
                sample,
                field.getLocalName(),
                text == null ? "null" : text.getClass().getName(),
                units((String) text));
          }
          cursor.setObj(field.getIndex(), text);
        } else if (field.getSerialType().isLong()) cursor.setLong(field.getIndex(), value);
        else cursor.setInt(field.getIndex(), (int) value);
      }
      try {
        MarketEvent event = (MarketEvent) mapper.createEvent(cursor);
        Object mapping =
            record.getName().equals("Quote")
                ? record.getMapping(QuoteMapping.class)
                : record.getMapping(ProfileMapping.class);
        List<Object> expected = new ArrayList<>();
        expected.add(record.getName().equals("Quote") ? "quote" : "profile");
        expected.add(scenario);
        expected.add(record.getId());
        expected.add(sample);
        if (event instanceof Quote) {
          Quote quote = (Quote) event;
          for (String property : new String[] {"BidPrice", "BidSize", "AskPrice", "AskSize"})
            expected.add(
                mapping
                    .getClass()
                    .getMethod("get" + property + "WideDecimal", RecordCursor.class)
                    .invoke(mapping, cursor));
          expected.add(quote.getBidTime());
          expected.add((int) quote.getBidExchangeCode());
          expected.add(quote.getAskTime());
          expected.add((int) quote.getAskExchangeCode());
          expected.add(quote.getTime());
          expected.add(quote.getSequence());
          expected.add(quote.getTimeNanoPart());
          expected.add(Double.doubleToRawLongBits(quote.getBidPrice()));
          expected.add(Double.doubleToRawLongBits(quote.getBidSizeAsDouble()));
          expected.add(Double.doubleToRawLongBits(quote.getAskPrice()));
          expected.add(Double.doubleToRawLongBits(quote.getAskSizeAsDouble()));
        } else {
          Profile profile = (Profile) event;
          for (String property :
              new String[] {
                "Beta",
                "Eps",
                "DivFreq",
                "ExdDivAmount",
                "HighPrice52",
                "LowPrice52",
                "Shares",
                "FreeFloat",
                "HighLimitPrice",
                "LowLimitPrice"
              })
            expected.add(
                mapping
                    .getClass()
                    .getMethod("get" + property + "WideDecimal", RecordCursor.class)
                    .invoke(mapping, cursor));
          expected.add(profile.getExDividendDayId());
          expected.add(profile.getHaltStartTime());
          expected.add(profile.getHaltEndTime());
          expected.add(((ProfileMapping) mapping).getFlags(cursor));
          expected.add(units(profile.getDescription()));
          expected.add(units(profile.getStatusReason()));
          expected.add(Double.doubleToRawLongBits(profile.getBeta()));
          expected.add(Double.doubleToRawLongBits(profile.getEarningsPerShare()));
          expected.add(Double.doubleToRawLongBits(profile.getDividendFrequency()));
          expected.add(Double.doubleToRawLongBits(profile.getExDividendAmount()));
          expected.add(Double.doubleToRawLongBits(profile.getHigh52WeekPrice()));
          expected.add(Double.doubleToRawLongBits(profile.getLow52WeekPrice()));
          expected.add(Double.doubleToRawLongBits(profile.getShares()));
          expected.add(Double.doubleToRawLongBits(profile.getFreeFloat()));
          expected.add(Double.doubleToRawLongBits(profile.getHighLimitPrice()));
          expected.add(Double.doubleToRawLongBits(profile.getLowLimitPrice()));
        }
        for (int i = 0; i < fields.size(); ++i) {
          DataField field = fields.get(i);
          boolean object = field instanceof DataObjField;
          row(
              out,
              "value",
              scenario,
              record.getId(),
              sample,
              i,
              object
                  ? 0
                  : field.getSerialType().isLong()
                      ? cursor.getLong(field.getIndex())
                      : cursor.getInt(field.getIndex()),
              object ? units((String) cursor.getObj(field.getIndex())) : "null");
        }
        row(out, expected.toArray());
      } catch (IllegalArgumentException | ClassCastException failure) {
        row(
            out,
            "conversion-reject",
            scenario,
            record.getId(),
            sample,
            failure.getClass().getSimpleName());
        break;
      }
    }
  }

  /** Takes output TSV and fixed scenario tag; starts no endpoint or network. */
  public static void main(String[] args) throws Exception {
    if (args.length != 2 || !args[1].equals("all"))
      throw new IllegalArgumentException("Expected output TSV and all");
    DataScheme source = DXFeedScheme.getInstance();
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8))) {
      row(
          out,
          "factory-reject",
          "factory1",
          0,
          1,
          SerialFieldType.BYTE.createDefaultIntInstance(0, "Quote.Bid.Price") == null);
      row(
          out,
          "factory-reject",
          "factory3",
          0,
          3,
          SerialFieldType.SHORT.createDefaultIntInstance(0, "Quote.Bid.Price") == null);
      for (String family :
          new String[] {
            "numeric", "raw", "text", "missing", "required", "bad-numeric", "bad-text"
          }) {
        int[] types =
            family.equals("numeric")
                ? NUMERIC
                : family.equals("raw")
                    ? RAW
                    : family.equals("text")
                        ? new int[] {10, 137}
                        : family.equals("bad-numeric")
                            ? new int[] {40, 56, 72, 88, 152, 137}
                            : family.equals("bad-text") ? new int[] {9, 233, 249} : new int[] {120};
        for (int type : types) {
          String scenario = family + type;
          DataRecord[] records = {
            variant(source.findRecordByName("Quote"), 0, family, type),
            variant(source.findRecordByName("Profile"), 1, family, type)
          };
          new DefaultScheme(PentaCodec.INSTANCE, records);
          for (DataRecord record : records) observe(out, record, scenario, family, type);
        }
      }
      if (out.checkError()) throw new IOException("Cannot write fixture");
    }
  }
}
