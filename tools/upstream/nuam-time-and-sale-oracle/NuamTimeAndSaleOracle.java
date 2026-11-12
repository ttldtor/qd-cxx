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
import com.dxfeed.event.IndexedEvent;
import com.dxfeed.event.custom.CustomFactoryImpl;
import com.dxfeed.event.custom.NuamTimeAndSale;
import com.dxfeed.event.custom.NuamTimeAndSaleDelegate;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

/** Captures pinned QD NuamTimeAndSale schema, native QTP and typed delegate observations. */
public final class NuamTimeAndSaleOracle {
  private static final long[] TIMES = {2001, 1005, 500, 0};
  private static final int[] SEQUENCES = {2, 7, 3, 0};
  private static final int[] BUSINESS_FLAGS = {
    ('Q' << 8) | (2 << 5) | (1 << 4) | (1 << 3) | (1 << 2),
    ('A' << 8) | (1 << 5) | (1 << 2) | 1,
    ('Z' << 8) | 2,
    (3 << 5) | 3
  };
  private static final int[] EVENT_FLAGS = {
    IndexedEvent.SNAPSHOT_BEGIN,
    IndexedEvent.TX_PENDING,
    IndexedEvent.TX_PENDING | IndexedEvent.REMOVE_EVENT,
    IndexedEvent.SNAPSHOT_END | IndexedEvent.REMOVE_EVENT
  };
  private static final double[] PRICES = {30.25, -3.125, 40.5, Double.NaN};

  private static void row(PrintWriter out, Object... values) {
    for (int index = 0; index < values.length; ++index) {
      if (index != 0) {
        out.print('\t');
      }
      out.print(values[index]);
    }
    out.print('\n');
  }

  private static NuamTimeAndSaleDelegate delegate(DataRecord record, QDContract contract) {
    for (EventDelegate<?> candidate : new CustomFactoryImpl().createDelegates(record)) {
      if (candidate instanceof NuamTimeAndSaleDelegate && candidate.getContract() == contract) {
        return (NuamTimeAndSaleDelegate) candidate;
      }
    }
    throw new IllegalStateException("Missing NuamTimeAndSale delegate " + contract);
  }

  private static NuamTimeAndSale sample(int index, boolean history, boolean regional) {
    NuamTimeAndSale event = new NuamTimeAndSale(regional ? "AAPL&Q" : "AAPL");
    event.setTime(TIMES[index]);
    event.setSequence(SEQUENCES[index]);
    event.setEventFlags(history ? EVENT_FLAGS[index] : 0);
    event.setTimeNanoPart(765432);
    event.setExchangeCode('X');
    event.setTradeId(9007199254740993L + index);
    event.setMatchId(index == 3 ? Long.MIN_VALUE : 9223372036854775800L + index);
    event.setPrice(PRICES[index]);
    event.setSizeAsDouble(index == 0 ? 100.5 : 2.25);
    event.setBidPrice(29.5);
    event.setAskPrice(31.125);
    event.setExchangeSaleConditions(index == 0 ? "AB" : null);
    event.setBuyer("buyer");
    event.setSeller(index == 0 ? "" : "seller");
    return event;
  }

  private static String nullable(String value) {
    return value == null ? "<null>" : value;
  }

  private static void observe(
      PrintWriter out, int index, NuamTimeAndSaleDelegate delegate, RecordCursor cursor) {
    NuamTimeAndSale event = delegate.createEvent(cursor);
    row(
        out,
        "event",
        index,
        event.getEventSymbol(),
        event.getEventTime(),
        event.getEventFlags(),
        event.getSource().id(),
        event.getIndex(),
        event.getTime(),
        event.getTimeNanos(),
        event.getTimeNanoPart(),
        event.getSequence(),
        (int) event.getExchangeCode(),
        event.getTradeId(),
        Double.doubleToLongBits(event.getPrice()),
        Double.doubleToLongBits(event.getSizeAsDouble()),
        event.getSize(),
        Double.doubleToLongBits(event.getBidPrice()),
        Double.doubleToLongBits(event.getAskPrice()),
        nullable(event.getExchangeSaleConditions()),
        delegate.getMapping().getFlags(cursor),
        (int) event.getTradeThroughExempt(),
        event.getAggressorSide().getCode(),
        event.isSpreadLeg(),
        event.isExtendedTradingHours(),
        event.isValidTick(),
        event.getType().getCode(),
        event.isNew(),
        event.isCorrection(),
        event.isCancel(),
        nullable(event.getBuyer()),
        nullable(event.getSeller()),
        event.getMatchId());
    DataRecord record = cursor.getRecord();
    for (int fieldIndex = 0; fieldIndex < record.getIntFieldCount(); ++fieldIndex) {
      DataIntField field = record.getIntField(fieldIndex);
      if (field.getSerialType().getId() != 0) {
        long raw =
            field.getSerialType().isLong() ? cursor.getLong(fieldIndex) : cursor.getInt(fieldIndex);
        row(out, "int", index, field.getLocalName(), raw);
      }
    }
    for (int fieldIndex = 0; fieldIndex < record.getObjFieldCount(); ++fieldIndex) {
      DataObjField field = record.getObjField(fieldIndex);
      Object value = cursor.getObj(fieldIndex);
      row(out, "object", index, field.getLocalName(), value == null ? "<null>" : value);
    }
  }

  private static void capture(
      Path directory, DataScheme scheme, DataRecord record, boolean history, boolean regional)
      throws Exception {
    String stem = (regional ? "regional-" : "") + (history ? "history" : "stream");
    QDContract contract = history ? QDContract.HISTORY : QDContract.STREAM;
    MessageType type = history ? MessageType.HISTORY_DATA : MessageType.STREAM_DATA;
    NuamTimeAndSaleDelegate delegate = delegate(record, contract);
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
    int count = history ? TIMES.length : 3;
    for (int index = 0; index < count; ++index) {
      int sampleIndex = !history && index == 2 ? 3 : index;
      NuamTimeAndSale event = sample(sampleIndex, history, regional);
      RecordCursor cursor = delegate.putEvent(event, input);
      delegate.getMapping().setFlags(cursor, BUSINESS_FLAGS[sampleIndex]);
      if (index == 0) {
        cursor.setEventTimeSequence((7L << 32) | 0x32100000L);
      }
      if (history && (EVENT_FLAGS[index] & IndexedEvent.REMOVE_EVENT) != 0) {
        cursor.setTime(event.getIndex());
      }
    }

    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    composer.setWriteEventTimeSequence(true);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.visitData(input, type);
    byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
    Files.write(directory.resolve(stem + ".qtp"), wire);

    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve(stem + ".tsv"), StandardCharsets.UTF_8))) {
      row(out, "record", record.getId(), record.getName(), record.hasTime());
      for (int index = 0; index < record.getIntFieldCount(); ++index) {
        DataIntField field = record.getIntField(index);
        row(
            out,
            "int-field",
            index,
            field.getSerialType().getId(),
            field.getLocalName(),
            field.getPropertyName());
      }
      for (int index = 0; index < record.getObjFieldCount(); ++index) {
        DataObjField field = record.getObjField(index);
        row(
            out,
            "object-field",
            index,
            field.getSerialType().getId(),
            field.getLocalName(),
            field.getPropertyName());
      }
      int[] seen = {0};
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parser.setReadEventTimeSequence(true);
      parser.setInput(new ByteArrayInput(wire));
      parser.parse(
          new MessageConsumerAdapter() {
            @Override
            protected void processData(DataIterator iterator, MessageType messageType) {
              if (messageType != type) {
                throw new IllegalStateException("Unexpected QTP message " + messageType);
              }
              RecordCursor cursor;
              while ((cursor = ((RecordSource) iterator).next()) != null) {
                observe(out, seen[0]++, delegate, cursor);
              }
            }

            @Override
            public void handleCorruptedStream() {
              throw new IllegalStateException("Corrupt QTP stream");
            }

            @Override
            public void handleCorruptedMessage(int messageType) {
              throw new IllegalStateException("Corrupt QTP message " + messageType);
            }
          });
      if (seen[0] != count || out.checkError()) {
        throw new IllegalStateException("Incomplete NuamTimeAndSale capture");
      }
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "NuamTimeAndSaleOracle <новый-каталог>: офлайн-захват QD в QTP и TSV."
                  + " Без аргументов выводится справка. Успех: 0; ошибка: ненулевой код."
              : "NuamTimeAndSaleOracle <new-directory>: offline QD QTP and TSV capture."
                  + " No arguments prints help. Exit 0 on success, nonzero on failure.");
      return;
    }
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected one new output directory");
    }
    DataScheme scheme = DXFeedScheme.getInstance();
    DataRecord composite = scheme.findRecordByName("NuamTimeAndSale");
    DataRecord regional = scheme.findRecordByName("NuamTimeAndSale&Q");
    if (composite == null || regional == null) {
      throw new IllegalStateException(
          "Enable regional schema using -Ddxscheme.exchanges.NuamTimeAndSale=Q");
    }
    Path directory = Files.createDirectory(Paths.get(args[0]));
    capture(directory, scheme, composite, false, false);
    capture(directory, scheme, composite, true, false);
    capture(directory, scheme, regional, false, true);
    capture(directory, scheme, regional, true, true);
  }
}
