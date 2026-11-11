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
import com.dxfeed.event.option.OptionFactoryImpl;
import com.dxfeed.event.option.Series;
import com.dxfeed.event.option.SeriesDelegate;
import com.dxfeed.event.option.impl.SeriesMapping;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Captures production Series schema, HISTORY wire records, mapped values and event text. */
public final class SeriesOracle {
  private static final String[] REQUIRED = {
    "Expiration", "Volatility", "PutCallRatio", "ForwardPrice"
  };
  private static final int[] INDICES = {Integer.MIN_VALUE, -1, 0, 1, Integer.MAX_VALUE, 7, -7, 17};
  private static final int[] FLAGS = {4, 1, 0, 8, 16, 64, 2, 3};
  private static final int[] SECONDS = {-1, Integer.MIN_VALUE, 0, 1, Integer.MAX_VALUE, -7, 23, 55};
  private static final int[] SEQUENCES = {-1, 0, 1, 0x7fffffff, 0xffc00001, 17, 1, 0};
  private static final double[] NUMBERS = {
    0, 1.25, -8.5, Double.NaN, Double.POSITIVE_INFINITY, 3.125, Double.NEGATIVE_INFINITY, -2
  };
  private static final long[] REMOVALS = {0x00000000fffffff9L, 0x00000001ffffffffL};

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

  private static SeriesDelegate historyDelegate(DataRecord record) {
    for (EventDelegate<?> delegate : new OptionFactoryImpl().createDelegates(record)) {
      if (delegate instanceof SeriesDelegate && delegate.getContract() == QDContract.HISTORY) {
        return (SeriesDelegate) delegate;
      }
    }
    throw new IllegalStateException("No production HISTORY Series delegate");
  }

  private static DefaultRecord variant(
      DataRecord standard, List<String> included, boolean hasTime) {
    List<DataIntField> fields = new ArrayList<>();
    if (hasTime) {
      fields.add(new VoidIntField(0, "Series.Void"));
    }
    for (DataIntField original : logicalFields(standard)) {
      if (!included.contains(original.getLocalName())) {
        continue;
      }
      fields.add(
          original.getSerialType().createDefaultIntInstance(fields.size(), original.getName()));
      if (original.getSerialType().isLong()) {
        fields.add(new VoidIntField(fields.size(), original.getName() + "$VoidTail"));
      }
    }
    return new DefaultRecord(
        0, "Series", hasTime, fields.toArray(new DataIntField[0]), new DataObjField[0]);
  }

  private static void writeMetadata(PrintWriter out, DataRecord record) {
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

  private static void observed(PrintWriter out, PrintWriter text, int id, RecordCursor cursor) {
    SeriesDelegate delegate = historyDelegate(cursor.getRecord());
    SeriesMapping mapping = delegate.getMapping();
    Series series = delegate.createEvent(cursor);
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
        series.getEventSymbol(),
        series.getEventTime(),
        series.getEventFlags(),
        series.getSource().id(),
        series.getIndex(),
        series.getTimeSequence(),
        series.getTime(),
        series.getSequence(),
        series.getExpiration(),
        Double.doubleToLongBits(series.getVolatility()),
        Double.doubleToLongBits(series.getCallVolume()),
        Double.doubleToLongBits(series.getPutVolume()),
        Double.doubleToLongBits(series.getOptionVolume()),
        Double.doubleToLongBits(series.getPutCallRatio()),
        Double.doubleToLongBits(series.getForwardPrice()),
        Double.doubleToLongBits(series.getDividend()),
        Double.doubleToLongBits(series.getInterest()),
        mapping.getVolatilityWideDecimal(cursor),
        mapping.getCallVolumeWideDecimal(cursor),
        mapping.getPutVolumeWideDecimal(cursor),
        mapping.getPutCallRatioWideDecimal(cursor),
        mapping.getForwardPriceWideDecimal(cursor),
        mapping.getDividendWideDecimal(cursor),
        mapping.getInterestWideDecimal(cursor),
        cursor.getTime());
    row(text, id, series);
  }

  private static void capture(
      Path directory, DataScheme scheme, DataRecord record, boolean fieldTime) throws Exception {
    SeriesDelegate delegate = historyDelegate(record);
    List<DataIntField> fields = logicalFields(record);
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
    for (int index = 0; index < FLAGS.length; ++index) {
      Series series = new Series(index % 2 == 0 ? "AAPL" : "SPY");
      series.setIndex(INDICES[index]);
      series.setEventFlags(FLAGS[index]);
      series.setTimeSequence(((long) SECONDS[index] << 32) | (SEQUENCES[index] & 0xffffffffL));
      series.setExpiration(index % 2 == 0 ? 20452 : -1);
      series.setVolatility(NUMBERS[index]);
      series.setCallVolume(NUMBERS[(index + 2) % NUMBERS.length]);
      series.setPutVolume(NUMBERS[(index + 3) % NUMBERS.length]);
      series.setPutCallRatio(NUMBERS[(index + 4) % NUMBERS.length]);
      series.setForwardPrice(NUMBERS[(index + 5) % NUMBERS.length]);
      series.setDividend(NUMBERS[(index + 6) % NUMBERS.length]);
      series.setInterest(NUMBERS[(index + 7) % NUMBERS.length]);
      RecordCursor cursor = delegate.putEvent(series, input);
      cursor.setEventTimeSequence(((long) (index - 4) << 32) | 0x32100000L);
      if ((FLAGS[index] & 2) != 0) {
        cursor.setTime(REMOVALS[index - 6]);
      }
      if (index == 0 && fields.get(4).getSerialType().isLong()) {
        cursor.setLong(fields.get(4).getIndex(), 0x7fffffffffffff80L);
      }
    }
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    composer.setWriteEventTimeSequence(fieldTime);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.visitData(input, MessageType.HISTORY_DATA);
    byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
    Files.write(directory.resolve("series.qtp"), wire);

    try (PrintWriter out =
            new PrintWriter(
                Files.newBufferedWriter(directory.resolve("oracle.tsv"), StandardCharsets.UTF_8));
        PrintWriter text =
            new PrintWriter(
                Files.newBufferedWriter(directory.resolve("text.tsv"), StandardCharsets.UTF_8))) {
      writeMetadata(out, record);
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
                if (cursor.getEventFlags() != FLAGS[seen[0]]) {
                  throw new IllegalStateException("HISTORY hs lost event flags");
                }
                observed(out, text, seen[0]++, cursor);
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
      if (seen[0] != FLAGS.length || out.checkError() || text.checkError()) {
        throw new IllegalStateException("Incomplete capture");
      }
    }
  }

  private static void captureOptionalFields(Path directory, DataRecord standard) throws Exception {
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve("optional.tsv"), StandardCharsets.UTF_8))) {
      List<String> names = Arrays.asList(REQUIRED);
      DataRecord minimal =
          new DefaultScheme(PentaCodec.INSTANCE, variant(standard, names, false)).getRecord(0);
      SeriesDelegate delegate = historyDelegate(minimal);
      RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA);
      RecordCursor cursor = input.add(minimal, 0, "AAPL");
      Series series = delegate.createEvent(cursor);
      row(
          out,
          "absent",
          series.getIndex(),
          series.getTimeSequence(),
          Double.doubleToLongBits(series.getCallVolume()),
          Double.doubleToLongBits(series.getPutVolume()),
          Double.doubleToLongBits(series.getDividend()),
          Double.doubleToLongBits(series.getInterest()));
      for (String name : names) {
        List<String> remaining = new ArrayList<>(names);
        remaining.remove(name);
        try {
          new OptionFactoryImpl().createMapping(variant(standard, remaining, false));
          throw new IllegalStateException("Missing field was accepted: " + name);
        } catch (IllegalArgumentException expected) {
          row(out, "reject", name, expected.getClass().getSimpleName());
        }
      }
      if (out.checkError()) {
        throw new IllegalStateException("Incomplete optional-field observations");
      }
    }
  }

  /** Run with an explicit new output directory and one of the documented profiles. */
  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "SeriesOracle <новый-каталог> <standard|tiny|optional|field>: офлайн-наблюдения QD"
                  + " в QTP и TSV. Успех: 0; ошибка: ненулевой код. По умолчанию выводится"
                  + " справка."
              : "SeriesOracle <new-directory> <standard|tiny|optional|field>: offline QD"
                  + " observations in QTP and TSV. Exit 0 on success, nonzero on failure. With no"
                  + " arguments, print help.");
      return;
    }
    if (args.length != 2
        || !Arrays.asList("standard", "tiny", "optional", "field").contains(args[1])) {
      throw new IllegalArgumentException(
          "Expected a new output directory and standard, tiny, optional or field");
    }
    DataScheme scheme = DXFeedScheme.getInstance();
    DataRecord standard = scheme.findRecordByName("Series");
    DataRecord record = standard;
    if ("optional".equals(args[1])) {
      List<String> names = new ArrayList<>(Arrays.asList(REQUIRED));
      names.addAll(Arrays.asList("Index", "Time", "Sequence", "PutVolume"));
      record = variant(standard, names, true);
      scheme = new DefaultScheme(PentaCodec.INSTANCE, record);
    }
    Path directory = Files.createDirectory(Paths.get(args[0]));
    capture(directory, scheme, record, "field".equals(args[1]));
    if ("optional".equals(args[1])) {
      captureOptionalFields(directory, standard);
    }
  }
}
