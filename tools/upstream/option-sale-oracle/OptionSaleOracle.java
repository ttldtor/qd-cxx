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
import com.devexperts.qd.QDContract;
import com.devexperts.qd.kit.DefaultRecord;
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
import com.dxfeed.event.market.OptionSale;
import com.dxfeed.event.market.OptionSaleDelegate;
import com.dxfeed.event.market.impl.OptionSaleMapping;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Capture production OptionSale HISTORY schema, wire data and public event values. */
public final class OptionSaleOracle {
  private static final int[] INDEX = {Integer.MIN_VALUE, -1, 0, 1, Integer.MAX_VALUE, 7, -7, 17};
  private static final int[] EVENT_FLAGS = {4, 1, 0, 8, 16, 64, 2, 3};
  private static final int[] SALE_FLAGS = {0, 1, 2, 3, 0x515d, 0x806e, -1, 0x12345678};
  private static final int[] SECONDS = {-1, Integer.MIN_VALUE, 0, 1, Integer.MAX_VALUE, -7, 23, 55};
  private static final int[] SEQUENCES = {-1, 0, 1, 0x7fffffff, 0xffc00001, 17, 1, 0};
  private static final int[] NANOS = {0, -1, 1, 999999, 1000000, Integer.MIN_VALUE, 17, 42};
  private static final double[] NUMBERS = {
    0, 1.25, -8.5, Double.NaN, Double.POSITIVE_INFINITY, 3.125, Double.NEGATIVE_INFINITY, -2
  };
  private static final String[] OPTIONS = {
    null, "", ".IBM270115C100", ".SPY270115P99", "Ω", "A&B", "X", "Y"
  };
  private static final String[] CONDITIONS = {null, "", "AB", "Z", "ÿ", "X", "Y", "Z"};
  private static final String[] REQUIRED = {
    "Index",
    "Time",
    "Sequence",
    "ExchangeCode",
    "Price",
    "Size",
    "BidPrice",
    "AskPrice",
    "ExchangeSaleConditions",
    "Flags",
    "UnderlyingPrice",
    "Volatility",
    "Delta",
    "OptionSymbol"
  };
  private static final long[] REMOVALS = {0x00000000fffffff9L, 0x00000001ffffffffL};
  private static final char[] HEX = "0123456789abcdef".toCharArray();

  private static void row(PrintWriter out, Object... values) {
    for (int index = 0; index < values.length; ++index) {
      if (index != 0) {
        out.print((char) 9);
      }
      out.print(values[index]);
    }
    out.print((char) 10);
  }

  /** Encode null, empty and arbitrary exact UTF-16 without TSV delimiters. */
  private static String text(String value) {
    if (value == null) {
      return "null";
    }
    StringBuilder out = new StringBuilder("u");
    for (int index = 0; index < value.length(); ++index) {
      int code = value.charAt(index);
      out.append(HEX[(code >>> 12) & 15]);
      out.append(HEX[(code >>> 8) & 15]);
      out.append(HEX[(code >>> 4) & 15]);
      out.append(HEX[code & 15]);
    }
    return out.toString();
  }

  private static List<DataIntField> logicalFields(DataRecord record) {
    List<DataIntField> fields = new ArrayList<>();
    for (int index = 0; index < record.getIntFieldCount(); ++index) {
      DataIntField field = record.getIntField(index);
      if (field.getSerialType().getId() != 0) {
        fields.add(field);
      }
    }
    return fields;
  }

  private static OptionSaleDelegate historyDelegate(DataRecord record) {
    for (EventDelegate<?> delegate : new MarketFactoryImpl().createDelegates(record)) {
      if (delegate instanceof OptionSaleDelegate && delegate.getContract() == QDContract.HISTORY) {
        return (OptionSaleDelegate) delegate;
      }
    }
    throw new IllegalStateException("No production HISTORY OptionSale delegate");
  }

  private static void metadata(PrintWriter out, DataRecord record) {
    row(out, "record", record.getId(), record.getName(), record.hasTime());
    for (int index = 0; index < record.getIntFieldCount(); ++index) {
      DataIntField field = record.getIntField(index);
      row(out, "physical", index, field.getSerialType().getId(), field.getLocalName());
    }
    for (DataIntField field : logicalFields(record)) {
      row(out, "int", field.getSerialType().getId(), field.getLocalName(), field.getPropertyName());
    }
    for (int index = 0; index < record.getObjFieldCount(); ++index) {
      DataObjField field = record.getObjField(index);
      row(
          out,
          "object",
          field.getSerialType().getId(),
          field.getLocalName(),
          field.getPropertyName());
    }
  }

  private static void observed(PrintWriter out, int id, RecordCursor cursor) {
    OptionSaleDelegate delegate = historyDelegate(cursor.getRecord());
    OptionSaleMapping mapping = delegate.getMapping();
    OptionSale event = delegate.createEvent(cursor);
    for (DataIntField field : logicalFields(cursor.getRecord())) {
      long raw =
          field.getSerialType().isLong()
              ? cursor.getLong(field.getIndex())
              : cursor.getInt(field.getIndex());
      row(out, "value", id, field.getLocalName(), raw);
    }
    for (int index = 0; index < cursor.getRecord().getObjFieldCount(); ++index) {
      DataObjField field = cursor.getRecord().getObjField(index);
      row(
          out,
          "object-value",
          id,
          field.getLocalName(),
          text((String) cursor.getObj(field.getIndex())));
    }
    row(
        out,
        "event",
        id,
        text(event.getEventSymbol()),
        event.getEventTime(),
        event.getEventFlags(),
        event.getSource().id(),
        event.getIndex(),
        event.getTimeSequence(),
        event.getTime(),
        event.getTimeNanoPart(),
        event.getTimeNanos(),
        event.getSequence(),
        (int) event.getExchangeCode(),
        Double.doubleToLongBits(event.getPrice()),
        Double.doubleToLongBits(event.getSize()),
        Double.doubleToLongBits(event.getBidPrice()),
        Double.doubleToLongBits(event.getAskPrice()),
        text(event.getExchangeSaleConditions()),
        mapping.getFlags(cursor),
        (int) event.getTradeThroughExempt(),
        event.getAggressorSide().getCode(),
        event.isSpreadLeg(),
        event.isExtendedTradingHours(),
        event.isValidTick(),
        event.getType().getCode(),
        event.isNew(),
        event.isCorrection(),
        event.isCancel(),
        Double.doubleToLongBits(event.getUnderlyingPrice()),
        Double.doubleToLongBits(event.getVolatility()),
        Double.doubleToLongBits(event.getDelta()),
        text(event.getOptionSymbol()),
        mapping.getPriceWideDecimal(cursor),
        mapping.getSizeWideDecimal(cursor),
        mapping.getBidPriceWideDecimal(cursor),
        mapping.getAskPriceWideDecimal(cursor),
        mapping.getUnderlyingPriceWideDecimal(cursor),
        mapping.getVolatilityWideDecimal(cursor),
        mapping.getDeltaWideDecimal(cursor),
        cursor.getTime());
  }

  private static void capture(
      Path directory, DataScheme scheme, DataRecord record, boolean fieldTime) throws Exception {
    OptionSaleDelegate delegate = historyDelegate(record);
    OptionSaleMapping mapping = delegate.getMapping();
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
    for (int index = 0; index < EVENT_FLAGS.length; ++index) {
      OptionSale event = new OptionSale(index % 2 == 0 ? "IBM" : "SPY&Q");
      event.setIndex(INDEX[index]);
      event.setEventFlags(EVENT_FLAGS[index]);
      event.setTimeSequence(((long) SECONDS[index] << 32) | (SEQUENCES[index] & 0xffffffffL));
      event.setTimeNanoPart(NANOS[index]);
      event.setExchangeCode(index % 2 == 0 ? 'Q' : 'N');
      event.setPrice(NUMBERS[index]);
      event.setSize(NUMBERS[(index + 1) % NUMBERS.length]);
      event.setBidPrice(NUMBERS[(index + 2) % NUMBERS.length]);
      event.setAskPrice(NUMBERS[(index + 3) % NUMBERS.length]);
      event.setExchangeSaleConditions(CONDITIONS[index]);
      event.setUnderlyingPrice(NUMBERS[(index + 4) % NUMBERS.length]);
      event.setVolatility(NUMBERS[(index + 5) % NUMBERS.length]);
      event.setDelta(NUMBERS[(index + 6) % NUMBERS.length]);
      event.setOptionSymbol(OPTIONS[index]);
      RecordCursor cursor = delegate.putEvent(event, input);
      mapping.setFlags(cursor, SALE_FLAGS[index]);
      cursor.setEventTimeSequence(((long) (index - 4) << 32) | 0x32100000L);
      if ((EVENT_FLAGS[index] & 2) != 0) {
        cursor.setTime(REMOVALS[index - 6]);
      }
    }
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    composer.setWriteEventTimeSequence(fieldTime);
    ByteArrayOutput bytes = new ByteArrayOutput();
    composer.setOutput(bytes);
    composer.visitData(input, MessageType.HISTORY_DATA);
    byte[] wire = Arrays.copyOf(bytes.getBuffer(), bytes.getPosition());
    Files.write(directory.resolve("option-sale.qtp"), wire);

    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve("oracle.tsv"), StandardCharsets.UTF_8))) {
      metadata(out, record);
      int[] seen = {0};
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parser.setReadEventTimeSequence(fieldTime);
      parser.setInput(new ByteArrayInput(wire));
      parser.parse(
          new MessageConsumerAdapter() {
            @Override
            protected void processData(DataIterator iterator, MessageType type) {
              if (type != MessageType.HISTORY_DATA) {
                throw new IllegalStateException("Unexpected data contract");
              }
              RecordCursor cursor;
              while ((cursor = ((RecordSource) iterator).next()) != null) {
                if (seen[0] >= EVENT_FLAGS.length
                    || cursor.getEventFlags() != EVENT_FLAGS[seen[0]]) {
                  throw new IllegalStateException("HISTORY event flags or record count differ");
                }
                observed(out, seen[0]++, cursor);
              }
            }

            @Override
            public void handleCorruptedStream() {
              throw new IllegalStateException("Corrupt stream");
            }

            @Override
            public void handleCorruptedMessage(int type) {
              throw new IllegalStateException("Corrupt message " + type);
            }

            @Override
            public void handleUnknownMessage(int type) {
              throw new IllegalStateException("Unknown message " + type);
            }
          });
      if (seen[0] != EVENT_FLAGS.length || out.checkError()) {
        throw new IllegalStateException("Incomplete capture");
      }
    }
  }

  private static void captureMissingFields(Path directory, DataRecord source) throws Exception {
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve("missing.tsv"), StandardCharsets.UTF_8))) {
      for (String omitted : REQUIRED) {
        List<DataIntField> integers = new ArrayList<>();
        List<DataObjField> objects = new ArrayList<>();
        for (DataIntField field : logicalFields(source)) {
          if (field.getLocalName().equals(omitted)) {
            continue;
          }
          integers.add(
              field.getSerialType().createDefaultIntInstance(integers.size(), field.getName()));
          if (field.getSerialType().isLong()) {
            integers.add(new VoidIntField(integers.size(), field.getName() + "$VoidTail"));
          }
        }
        for (int index = 0; index < source.getObjFieldCount(); ++index) {
          DataObjField field = source.getObjField(index);
          if (!field.getLocalName().equals(omitted)) {
            objects.add(
                field.getSerialType().createDefaultObjInstance(objects.size(), field.getName()));
          }
        }
        DataRecord candidate =
            new DefaultRecord(
                0,
                "OptionSale",
                false,
                integers.toArray(new DataIntField[0]),
                objects.toArray(new DataObjField[0]));
        try {
          new MarketFactoryImpl().createMapping(candidate);
          throw new IllegalStateException("Missing required property accepted: " + omitted);
        } catch (IllegalArgumentException expected) {
          row(out, "reject", omitted, expected.getClass().getSimpleName());
        }
      }
      if (out.checkError()) {
        throw new IllegalStateException("Incomplete missing-field observations");
      }
    }
  }

  /** Capture one profile to a fresh directory or show help without accessing QD. */
  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "OptionSaleOracle <новый-каталог> <standard|tiny|nano|field>: офлайн-наблюдения"
                  + " QD в QTP и TSV. Код 0 означает успех; ошибка завершается ненулевым кодом."
              : "OptionSaleOracle <new-directory> <standard|tiny|nano|field>: offline QD"
                  + " observations in QTP and TSV. Exit 0 on success, nonzero on failure.");
      return;
    }
    if (args.length != 2 || !Arrays.asList("standard", "tiny", "nano", "field").contains(args[1])) {
      throw new IllegalArgumentException(
          "Expected a new output directory and standard, tiny, nano or field");
    }
    DataScheme scheme = DXFeedScheme.getInstance();
    DataRecord record = scheme.findRecordByName("OptionSale");
    if (record == null) {
      throw new IllegalStateException("No production OptionSale record");
    }
    if ("nano".equals(args[1]) && record.findFieldByName("TimeNanoPart") == null) {
      throw new IllegalStateException(
          "Nano profile requires -Ddxscheme.enabled.TimeNanoPart=OptionSale");
    }
    if ("tiny".equals(args[1]) && record.findFieldByName("Price").getSerialType().isLong()) {
      throw new IllegalStateException("Tiny profile requires -Ddxscheme.wide=false");
    }
    Path directory = Files.createDirectory(Paths.get(args[0]));
    capture(directory, scheme, record, "field".equals(args[1]));
    captureMissingFields(directory, record);
  }
}
