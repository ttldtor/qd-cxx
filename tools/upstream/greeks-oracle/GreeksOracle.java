// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
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
import com.dxfeed.event.option.Greeks;
import com.dxfeed.event.option.GreeksDelegate;
import com.dxfeed.event.option.OptionFactoryImpl;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

/** Captures the production Greeks schema, QTP packets and delegate values. */
public final class GreeksOracle {
  private static void row(PrintWriter out, Object... columns) {
    for (int index = 0; index < columns.length; ++index) {
      if (index != 0) {
        out.print('\t');
      }
      out.print(columns[index]);
    }
    out.print('\n');
  }

  private static GreeksDelegate delegate(DataRecord record, QDContract contract) {
    for (EventDelegate<?> candidate : new OptionFactoryImpl().createDelegates(record)) {
      if (candidate instanceof GreeksDelegate && candidate.getContract() == contract) {
        return (GreeksDelegate) candidate;
      }
    }
    throw new IllegalStateException("Missing Greeks delegate " + contract);
  }

  private static Greeks sample(long time, int sequence, int flags, double price) {
    Greeks event = new Greeks("AAPL");
    event.setTime(time);
    event.setSequence(sequence);
    event.setEventFlags(flags);
    event.setPrice(price);
    event.setVolatility(0.25);
    event.setDelta(-0.5);
    event.setGamma(Double.NaN);
    event.setTheta(-1.25);
    event.setRho(Double.POSITIVE_INFINITY);
    event.setVega(Double.NEGATIVE_INFINITY);
    return event;
  }

  private static long bits(double value) {
    return Double.doubleToLongBits(value);
  }

  private static void observe(PrintWriter out, GreeksDelegate delegate, RecordCursor cursor) {
    Greeks event = delegate.createEvent(cursor);
    row(
        out,
        "event",
        event.getEventSymbol(),
        event.getEventTime(),
        event.getEventFlags(),
        event.getSource().id(),
        event.getIndex(),
        event.getTime(),
        event.getSequence(),
        bits(event.getPrice()),
        bits(event.getVolatility()),
        bits(event.getDelta()),
        bits(event.getGamma()),
        bits(event.getTheta()),
        bits(event.getRho()),
        bits(event.getVega()));
    DataRecord record = cursor.getRecord();
    for (int index = 0; index < record.getIntFieldCount(); ++index) {
      DataIntField field = record.getIntField(index);
      if (field.getSerialType().getId() != 0) {
        row(
            out,
            "raw",
            field.getLocalName(),
            field.getSerialType().isLong() ? cursor.getLong(index) : cursor.getInt(index));
      }
    }
  }

  private static void capture(Path directory, DataScheme scheme, DataRecord record, boolean history)
      throws Exception {
    String stem = history ? "history" : "ticker";
    QDContract contract = history ? QDContract.HISTORY : QDContract.TICKER;
    MessageType type = history ? MessageType.HISTORY_DATA : MessageType.TICKER_DATA;
    GreeksDelegate delegate = delegate(record, contract);
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
    RecordCursor first =
        delegate.putEvent(
            sample(history ? 1250 : -1, 4194303, history ? IndexedEvent.SNAPSHOT_BEGIN : 0, 12.75),
            input);
    first.setEventTimeSequence((7L << 32) | 0x32100000L);
    int expected = history ? 3 : 1;
    if (history) {
      delegate.putEvent(sample(750, 1, IndexedEvent.TX_PENDING, -3.125), input);
      RecordCursor end =
          delegate.putEvent(
              sample(0, 0, IndexedEvent.REMOVE_EVENT | IndexedEvent.SNAPSHOT_END, Double.NaN),
              input);
      end.setTime(0);
    }
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    composer.setWriteEventTimeSequence(true);
    ByteArrayOutput bytes = new ByteArrayOutput();
    composer.setOutput(bytes);
    composer.visitData(input, type);
    byte[] wire = Arrays.copyOf(bytes.getBuffer(), bytes.getPosition());
    Files.write(directory.resolve(stem + ".qtp"), wire);

    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve(stem + ".tsv"), StandardCharsets.UTF_8))) {
      row(out, "record", record.getId(), record.getName(), record.hasTime());
      for (int index = 0; index < record.getIntFieldCount(); ++index) {
        DataIntField field = record.getIntField(index);
        row(
            out,
            "field",
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
                throw new IllegalStateException("Unexpected message " + messageType);
              }
              RecordCursor cursor;
              while ((cursor = ((RecordSource) iterator).next()) != null) {
                observe(out, delegate, cursor);
                ++seen[0];
              }
            }

            @Override
            public void handleCorruptedStream() {
              throw new IllegalStateException("Corrupt stream");
            }

            @Override
            public void handleCorruptedMessage(int messageType) {
              throw new IllegalStateException("Corrupt message " + messageType);
            }
          });
      if (seen[0] != expected || out.checkError()) {
        throw new IllegalStateException("Incomplete Greeks capture");
      }
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "GreeksOracle <новый-каталог>: офлайн-захват Greeks TICKER/HISTORY в QTP и TSV."
                  + " Без аргументов выводится справка. Успех: 0; ошибка: ненулевой код."
              : "GreeksOracle <new-directory>: offline Greeks TICKER/HISTORY QTP and TSV."
                  + " No arguments prints help. Exit 0 on success, nonzero on failure.");
      return;
    }
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected exactly one new directory");
    }
    DataScheme scheme = DXFeedScheme.getInstance();
    DataRecord record = scheme.findRecordByName("Greeks");
    Path directory = Files.createDirectory(Paths.get(args[0]));
    capture(directory, scheme, record, false);
    capture(directory, scheme, record, true);
  }
}
