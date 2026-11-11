// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
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
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolOption;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.MarketMaker;
import com.dxfeed.event.market.Order;
import com.dxfeed.event.market.SourceBasedDelegateImpl;
import com.dxfeed.event.market.impl.MarketMakerMapping;
import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Captures actual generated delegates, full declared scalars and production binary parser
 * observations.
 */
public final class MarketMakerOrderOracle {
  static PrintWriter out;
  static final long[] SAMPLES = {
    Long.MIN_VALUE,
    Long.MAX_VALUE,
    Integer.MIN_VALUE,
    Integer.MAX_VALUE,
    -1,
    0,
    1,
    9007199254740993L,
    -385,
    -384,
    -383,
    383,
    384,
    385,
    0x7fffffffffffff80L,
    0x8000000000000080L,
    0x7fffffffffffff00L,
    0x180L
  };

  static void row(Object... values) {
    for (int i = 0; i < values.length; i++) {
      if (i > 0) out.print('\t');
      out.print(values[i]);
    }
    out.println();
  }

  static String units(String text) {
    if (text == null) return "null";
    if (text.isEmpty()) return "-";
    StringBuilder s = new StringBuilder();
    for (char c : text.toCharArray()) s.append(String.format("%04x", (int) c));
    return s.toString();
  }

  static List<DataIntField> fields(DataRecord r) {
    List<DataIntField> fields = new ArrayList<>();
    for (int i = 0; i < r.getIntFieldCount(); i++)
      if (r.getIntField(i).getSerialType().getId() != 0) fields.add(r.getIntField(i));
    return fields;
  }

  static SerialFieldType type(int id) {
    for (SerialFieldType t :
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
        }) if (t.getId() == id) return t;
    throw new IllegalArgumentException("Unknown fixture type");
  }

  static boolean numeric(String name) {
    return name.endsWith("Price") || name.endsWith("Size") || name.endsWith("Count");
  }

  static boolean raw(String name) {
    return name.equals("MMExchange")
        || name.equals("MMID")
        || name.endsWith("Exchange")
        || name.equals("Sequence")
        || name.equals("TimeNanoPart");
  }

  static boolean required(String name) {
    return name.equals("MMExchange")
        || name.equals("MMID")
        || name.endsWith("Price")
        || name.endsWith("Size");
  }

  static DataRecord variant(DataRecord original, int id, String family, int selected) {
    List<DataIntField> result = new ArrayList<>();
    for (DataIntField f : fields(original)) {
      String name = f.getLocalName();
      if (family.equals("missing") && !required(name)) continue;
      if (family.startsWith("required-") && name.equals(family.substring(9))) continue;
      SerialFieldType kind = f.getSerialType();
      if (family.equals("numeric") && numeric(name)
          || family.equals("raw") && raw(name)
          || family.equals("time") && name.endsWith("Time")) kind = type(selected);
      result.add(kind.createDefaultIntInstance(result.size(), original.getName() + "." + name));
      if (kind.isLong())
        result.add(new VoidIntField(result.size(), original.getName() + "." + name + "$VoidTail"));
    }
    return new DefaultRecord(
        id,
        original.getName(),
        original.hasTime(),
        result.toArray(new DataIntField[0]),
        new DataObjField[0]);
  }

  static void observe(
      String scenario, String phase, int sample, RecordCursor c, List<EventDelegate<?>> delegates)
      throws Exception {
    DataRecord r = c.getRecord();
    row(
        "INPUT",
        scenario,
        phase,
        r.getId(),
        sample,
        units(c.getDecodedSymbol()),
        c.getEventFlags(),
        r.hasTime() ? c.getTime() : "none",
        c.getInt(0),
        c.getInt(1));
    List<DataIntField> logical = fields(r);
    for (int i = 0; i < logical.size(); i++) {
      DataIntField f = logical.get(i);
      row(
          "VALUE",
          scenario,
          phase,
          r.getId(),
          sample,
          i,
          f.getSerialType().isLong() ? c.getLong(f.getIndex()) : c.getInt(f.getIndex()));
    }
    for (EventDelegate<?> d : delegates) {
      if (!d.isSub()) continue;
      Object e;
      try {
        e = d.createEvent(c);
      } catch (IllegalArgumentException failure) {
        row(
            "REFUSAL",
            scenario,
            phase,
            r.getId(),
            sample,
            d.getClass().getSimpleName(),
            units(failure.getMessage()));
        continue;
      }
      if (e instanceof Order) {
        Order o = (Order) e;
        row(
            "ORDER",
            scenario,
            phase,
            r.getId(),
            sample,
            d.getClass().getSimpleName(),
            units(o.getEventSymbol()),
            o.getEventTime(),
            o.getIndex(),
            o.getSource().id(),
            o.getEventFlags(),
            o.getTimeSequence(),
            o.getTime(),
            o.getSequence(),
            o.getTimeNanoPart(),
            (int) o.getExchangeCode(),
            units(o.getMarketMaker()),
            o.getOrderSide().getCode(),
            o.getScope().getCode(),
            Double.doubleToRawLongBits(o.getPrice()),
            Double.doubleToRawLongBits(o.getSizeAsDouble()),
            o.getCount());
      } else if (e instanceof MarketMaker) {
        MarketMaker m = (MarketMaker) e;
        MarketMakerMapping mapping = (MarketMakerMapping) d.getMapping();
        row(
            "MM",
            scenario,
            phase,
            r.getId(),
            sample,
            units(m.getEventSymbol()),
            m.getEventTime(),
            m.getSource().id(),
            m.getIndex(),
            m.getEventFlags(),
            (int) m.getExchangeCode(),
            units(m.getMarketMaker()),
            m.getBidTime(),
            m.getAskTime(),
            m.getTime(),
            Double.doubleToRawLongBits(m.getBidPrice()),
            Double.doubleToRawLongBits(m.getAskPrice()),
            Double.doubleToRawLongBits(m.getBidSize()),
            Double.doubleToRawLongBits(m.getAskSize()),
            m.getBidCount(),
            m.getAskCount(),
            mapping.getBidPriceWideDecimal(c),
            mapping.getBidSizeWideDecimal(c),
            mapping.getAskPriceWideDecimal(c),
            mapping.getAskSizeWideDecimal(c));
      }
    }
  }

  static void capture(
      Path wireRoot, DataScheme scheme, String scenario, String family, int selected)
      throws Exception {
    MarketFactoryImpl factory = new MarketFactoryImpl();
    for (int id = 0; id < scheme.getRecordCount(); id++) {
      DataRecord record = scheme.getRecord(id);
      row(
          "RECORD",
          scenario,
          id,
          units(record.getName()),
          record.hasTime(),
          record.getIntFieldCount());
      List<DataIntField> logical = fields(record);
      for (int i = 0; i < logical.size(); i++) {
        DataIntField f = logical.get(i);
        row(
            "FIELD",
            scenario,
            id,
            i,
            f.getIndex(),
            f.getSerialType().getId(),
            units(f.getLocalName()),
            units(f.getPropertyName()));
      }
      List<EventDelegate<?>> delegates;
      try {
        delegates = new ArrayList<>(factory.createDelegates(record));
      } catch (IllegalArgumentException failure) {
        row("PREPARE_REFUSAL", scenario, id, units(failure.getMessage()));
        continue;
      }
      if (delegates.isEmpty()) {
        row("PREPARE_REFUSAL", scenario, id, "NO_DELEGATES");
        continue;
      }
      for (EventDelegate<?> d : delegates)
        row(
            "DELEGATE",
            scenario,
            id,
            d.getClass().getSimpleName(),
            d.getEventType().getSimpleName(),
            d.getContract(),
            d.isSub(),
            d.isPub(),
            d.isWildcard(),
            d.isTimeSeries(),
            d instanceof SourceBasedDelegateImpl
                ? ((SourceBasedDelegateImpl<?>) d).getSource().id()
                : "none");
      int count = family.equals("flags") ? 32 : family.equals("remove") ? 5 : SAMPLES.length;
      RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
      for (int sample = 0; sample < count; sample++) {
        RecordCursor c = input.add(record, 0, "AAPL&Q");
        long chosen =
            selected == 2
                ? new long[] {0, 65, 0xd800, 0xffff, 0x10041, 0x10ffff}[sample % 6]
                : SAMPLES[sample % SAMPLES.length];
        for (DataIntField f : logical) {
          String name = f.getLocalName();
          long value =
              numeric(name)
                  ? 0x180
                  : name.equals("MMExchange") || name.endsWith("Exchange")
                      ? 65
                      : name.equals("MMID")
                          ? (sample % 3 == 0 ? -1 : sample % 3 == 1 ? 0 : 0x41420043)
                          : name.endsWith("Time") ? -3 : name.equals("Sequence") ? -1 : 0;
          if (family.equals("numeric") && numeric(name)
              || family.equals("raw") && raw(name)
              || family.equals("time") && name.endsWith("Time")) value = chosen;
          if (f.getSerialType().isLong()) c.setLong(f.getIndex(), value);
          else c.setInt(f.getIndex(), (int) value);
        }
        c.setEventFlags(family.equals("flags") ? sample : family.equals("remove") ? 2 : 0);
        if (family.equals("remove") && record.hasTime())
          c.setTime(new long[] {0, -1, Long.MIN_VALUE, Long.MAX_VALUE, (1L << 32) | 5}[sample]);
        observe(scenario, "ORIG", sample, c, delegates);
      }
      BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
      composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
      ByteArrayOutput bytes = new ByteArrayOutput();
      composer.setOutput(bytes);
      composer.visitData(
          input,
          record.getName().equals("MarketMaker")
              ? MessageType.HISTORY_DATA
              : MessageType.TICKER_DATA);
      byte[] wire = Arrays.copyOf(bytes.getBuffer(), bytes.getPosition());
      Files.write(wireRoot.resolve(scenario + "-" + id + ".qtp"), wire);
      final int[] seen = {0};
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parser.setInput(new ByteArrayInput(wire));
      parser.parse(
          new MessageConsumerAdapter() {
            protected void processData(DataIterator it, MessageType kind) {
              RecordCursor c;
              while ((c = ((RecordSource) it).next()) != null)
                try {
                  observe(scenario, "WIRE", seen[0]++, c, delegates);
                } catch (Exception failure) {
                  throw new IllegalStateException(failure);
                }
            }
          });
      if (seen[0] != count) throw new IllegalStateException("Parser count mismatch");
    }
  }

  public static void main(String[] args) throws Exception {
    out = new PrintWriter(new File(args[0]), "UTF-8");
    Path wire = Paths.get(args[0] + ".wire");
    Files.createDirectory(wire);
    DataScheme standard = DXFeedScheme.getInstance();
    for (String family :
        args[1].equals("nano")
            ? Arrays.asList("nano")
            : Arrays.asList(
                "default",
                "numeric",
                "raw",
                "time",
                "missing",
                "flags",
                "remove",
                "required-Bid.Price",
                "required-Ask.Size",
                "required-MMBid.Price",
                "required-MMAsk.Size",
                "required-MMExchange",
                "required-MMID")) {
      int[] types =
          family.equals("numeric")
              ? new int[] {2, 4, 8, 24, 104, 120}
              : family.equals("raw")
                  ? new int[] {2, 4, 8, 24, 40, 56, 72, 88, 104, 120, 152}
                  : family.equals("time") ? new int[] {56, 152} : new int[] {120};
      for (int selected : types) {
        DataRecord[] records = new DataRecord[3];
        int i = 0;
        for (String name : Arrays.asList("Quote", "Quote&Q", "MarketMaker"))
          records[i] = variant(standard.findRecordByName(name), i++, family, selected);
        capture(
            wire,
            new DefaultScheme(PentaCodec.INSTANCE, records),
            args[1] + "-" + family + selected,
            family,
            selected);
      }
    }
    out.close();
    if (out.checkError()) throw new IOException("Output failure");
  }
}
