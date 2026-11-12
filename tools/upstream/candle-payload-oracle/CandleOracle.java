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
import com.devexperts.util.WideDecimal;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.candle.Candle;
import com.dxfeed.event.candle.CandleByTradeHistoryDelegate;
import com.dxfeed.event.candle.CandleDelegate;
import com.dxfeed.event.candle.CandleEventDelegateImpl;
import com.dxfeed.event.candle.CandleFactoryImpl;
import com.dxfeed.event.candle.CandleSymbol;
import com.dxfeed.event.candle.impl.CandleMapping;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Captures the pinned production candle delegate through a QTP HISTORY round trip. */
public final class CandleOracle {
  private static final List<String> REQUIRED_CANDLE =
      Arrays.asList("Time", "Sequence", "Open", "High", "Low", "Close");
  private static final List<String> REQUIRED_TRADE =
      Arrays.asList("Time", "Sequence", "Price", "Size");
  private static final int[] FLAGS = {4, 1, 0, 8, 16, 64, 2, 3};
  private static final int[] SECONDS = {-1, Integer.MIN_VALUE, 0, 1, Integer.MAX_VALUE, -7, 23, 55};
  private static final int[] SEQUENCES = {-1, 0, 1, 0x7fffffff, 0xffc00001, 17, 1, 0};
  private static final double[] NUMBERS = {
    0, 1.25, -8.5, Double.NaN, Double.POSITIVE_INFINITY, 3.125, Double.NEGATIVE_INFINITY, -2
  };
  private static final long[] REMOVALS = {0x00000000fffffff9L, 0x00000001ffffffffL};
  private static final long[] COUNTS = {
    WideDecimal.composeWide(15, 1),
    WideDecimal.composeWide(-15, 1),
    WideDecimal.composeWide(9007199254740993L),
    WideDecimal.composeWide(1, -19),
    WideDecimal.composeWide(-1, -19),
    WideDecimal.NaN
  };

  private static void row(PrintWriter out, Object... values) {
    for (int index = 0; index < values.length; ++index) {
      if (index != 0) {
        out.print('\t');
      }
      out.print(values[index]);
    }
    out.print('\n');
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

  @SuppressWarnings("unchecked")
  private static CandleEventDelegateImpl<Candle> historyDelegate(DataRecord record) {
    for (EventDelegate<?> delegate : new CandleFactoryImpl().createDelegates(record)) {
      if ((delegate instanceof CandleDelegate || delegate instanceof CandleByTradeHistoryDelegate)
          && delegate.getContract() == QDContract.HISTORY) {
        return (CandleEventDelegateImpl<Candle>) delegate;
      }
    }
    throw new IllegalStateException(
        "No production HISTORY Candle delegate for " + record.getName());
  }

  private static DefaultRecord variant(
      DataRecord standard, List<String> included, boolean hasTime, String longField) {
    List<DataIntField> fields = new ArrayList<>();
    for (DataIntField original : logicalFields(standard)) {
      if (!included.contains(original.getLocalName())) {
        continue;
      }
      SerialFieldType type =
          original.getLocalName().equals(longField)
              ? SerialFieldType.LONG
              : original.getSerialType();
      fields.add(type.createDefaultIntInstance(fields.size(), original.getName()));
      if (type.isLong()) {
        fields.add(new VoidIntField(fields.size(), original.getName() + "$VoidTail"));
      }
    }
    return new DefaultRecord(
        0, standard.getName(), hasTime, fields.toArray(new DataIntField[0]), new DataObjField[0]);
  }

  private static List<String> names(DataRecord record) {
    List<String> result = new ArrayList<>();
    for (DataIntField field : logicalFields(record)) {
      result.add(field.getLocalName());
    }
    return result;
  }

  private static void metadata(PrintWriter out, DataRecord record) {
    row(out, "record", record.getId(), record.getName(), record.hasTime());
    for (int index = 0; index < record.getIntFieldCount(); ++index) {
      DataIntField field = record.getIntField(index);
      row(out, "physical", index, field.getSerialType().getId(), field.getLocalName());
    }
    for (DataIntField field : logicalFields(record)) {
      row(
          out,
          "field",
          field.getSerialType().getId(),
          field.getLocalName(),
          field.getPropertyName());
    }
  }

  private static void observed(PrintWriter out, int id, RecordCursor cursor) {
    CandleEventDelegateImpl<Candle> delegate = historyDelegate(cursor.getRecord());
    Candle event = delegate.createEvent(cursor);
    for (DataIntField field : logicalFields(cursor.getRecord())) {
      long raw =
          field.getSerialType().isLong()
              ? cursor.getLong(field.getIndex())
              : cursor.getInt(field.getIndex());
      row(out, "value", id, raw);
    }
    row(
        out,
        "event",
        id,
        event.getEventSymbol(),
        event.getEventTime(),
        event.getEventFlags(),
        event.getSource().id(),
        event.getIndex(),
        event.getTime(),
        event.getSequence(),
        event.getCount(),
        Double.doubleToLongBits(event.getOpen()),
        Double.doubleToLongBits(event.getHigh()),
        Double.doubleToLongBits(event.getLow()),
        Double.doubleToLongBits(event.getClose()),
        Double.doubleToLongBits(event.getVolumeAsDouble()),
        Double.doubleToLongBits(event.getVWAP()),
        Double.doubleToLongBits(event.getBidVolumeAsDouble()),
        Double.doubleToLongBits(event.getAskVolumeAsDouble()),
        Double.doubleToLongBits(event.getImpVolatility()),
        Double.doubleToLongBits(event.getOpenInterestAsDouble()),
        event.getVolume(),
        event.getBidVolume(),
        event.getAskVolume(),
        event.getOpenInterest(),
        cursor.getTime());
  }

  private static void capture(Path directory, DataScheme scheme, DataRecord record)
      throws Exception {
    CandleEventDelegateImpl<Candle> delegate = historyDelegate(record);
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
    for (int index = 0; index < FLAGS.length; ++index) {
      Candle event = new Candle(CandleSymbol.valueOf(index % 2 == 0 ? "AAPL{=1m}" : "SPY{=1m}"));
      event.setIndex(((long) SECONDS[index] << 32) | (SEQUENCES[index] & 0xffffffffL));
      event.setEventFlags(FLAGS[index]);
      event.setCount(index + 1);
      event.setOpen(NUMBERS[index]);
      event.setHigh(NUMBERS[(index + 1) % NUMBERS.length]);
      event.setLow(NUMBERS[(index + 2) % NUMBERS.length]);
      event.setClose(NUMBERS[(index + 3) % NUMBERS.length]);
      event.setVolumeAsDouble(NUMBERS[(index + 4) % NUMBERS.length]);
      event.setVWAP(NUMBERS[(index + 5) % NUMBERS.length]);
      event.setBidVolumeAsDouble(NUMBERS[(index + 6) % NUMBERS.length]);
      event.setAskVolumeAsDouble(NUMBERS[(index + 7) % NUMBERS.length]);
      event.setImpVolatility(NUMBERS[index]);
      event.setOpenInterestAsDouble(NUMBERS[(index + 1) % NUMBERS.length]);
      RecordCursor cursor = delegate.putEvent(event, input);
      cursor.setEventTimeSequence(((long) (index - 4) << 32) | 0x32100000L);

      if (delegate instanceof CandleDelegate && index < COUNTS.length) {
        CandleMapping mapping = ((CandleDelegate) delegate).getMapping();
        mapping.setCountWideDecimal(cursor, COUNTS[index]);
      }

      if (index == 0) {
        for (DataIntField field : logicalFields(record)) {
          if (field.getLocalName().equals("Open")
              && field.getSerialType() == SerialFieldType.LONG) {
            cursor.setLong(field.getIndex(), 9007199254740993L);
          }
        }
      }

      if ((FLAGS[index] & 2) != 0) {
        cursor.setTime(REMOVALS[index - 6]);
      }
    }

    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    composer.setWriteEventTimeSequence(true);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.visitData(input, MessageType.HISTORY_DATA);
    byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
    Files.write(directory.resolve("candle.qtp"), wire);

    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve("oracle.tsv"), StandardCharsets.UTF_8))) {
      metadata(out, record);
      int[] seen = {0};
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parser.setReadEventTimeSequence(true);
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
                if (cursor.getEventFlags() != FLAGS[seen[0]]) {
                  throw new IllegalStateException("HISTORY lost event flags");
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
      if (seen[0] != FLAGS.length || out.checkError()) {
        throw new IllegalStateException("Incomplete capture");
      }
    }
  }

  private static void captureRequiredFields(
      Path directory, DataRecord record, List<String> required) throws Exception {
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve("required.tsv"), StandardCharsets.UTF_8))) {
      for (String name : required) {
        List<String> remaining = new ArrayList<>(required);
        remaining.remove(name);
        try {
          new CandleFactoryImpl().createMapping(variant(record, remaining, false, ""));
          throw new IllegalStateException("Missing required field accepted: " + name);
        } catch (IllegalArgumentException expected) {
          row(out, "reject", name, expected.getClass().getSimpleName());
        }
      }
      if (out.checkError()) {
        throw new IllegalStateException("Incomplete missing-field observations");
      }
    }
  }

  /** Capture one selected profile into a fresh directory. */
  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "CandleOracle <новый-каталог>"
                  + " <standard|tiny|optional|long|numeric-long|trade|period>: офлайн QD, QTP и"
                  + " TSV. Успех: 0; ошибка: ненулевой код."
              : "CandleOracle <new-directory>"
                  + " <standard|tiny|optional|long|numeric-long|trade|period>: offline QD, QTP"
                  + " and TSV. Exit 0 on success and nonzero on failure.");
      return;
    }
    if (args.length != 2
        || !Arrays.asList("standard", "tiny", "optional", "long", "numeric-long", "trade", "period")
            .contains(args[1])) {
      throw new IllegalArgumentException("Expected a new directory and a documented profile");
    }

    DataScheme scheme = DXFeedScheme.getInstance();
    boolean trade = args[1].equals("trade");
    String recordName = trade ? "TradeHistory" : args[1].equals("period") ? "Trade.1min" : "Candle";
    DataRecord standard = scheme.findRecordByName(recordName);
    if (standard == null) {
      throw new IllegalStateException("Missing production record " + recordName);
    }
    DataRecord record = standard;

    if (args[1].equals("optional") || args[1].equals("long") || args[1].equals("numeric-long")) {
      List<String> selected = args[1].equals("optional") ? REQUIRED_CANDLE : names(standard);
      String longField =
          args[1].equals("long") ? "Count" : args[1].equals("numeric-long") ? "Open" : "";
      record = variant(standard, selected, true, longField);
      scheme = new DefaultScheme(PentaCodec.INSTANCE, record);
    }

    Path directory = Files.createDirectory(Paths.get(args[0]));
    capture(directory, scheme, record);
    if (args[1].equals("optional") || trade) {
      captureRequiredFields(directory, standard, trade ? REQUIRED_TRADE : REQUIRED_CANDLE);
    }
  }
}
