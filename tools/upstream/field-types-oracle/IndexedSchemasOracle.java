package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.LongField;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.TimeMillisField;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.kit.WideDecimalField;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.BinaryRecordDesc;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolOption;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.impl.DXFeedScheme;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import qdcxx.schemaexport.ResolvedSchemaExporter;

/**
 * Finite indexed-source and production writer/parser observations; no wire decoder is substituted.
 */
final class IndexedSchemasOracle {
  static PrintWriter output; // One explicitly owned capture sink; the oracle runs synchronously.

  /**
   * Write canonical LF at production time; never normalize captured observations afterward.
   *
   * @param value One complete canonical observation row.
   */
  static void line(String value) {
    output.print(value);
    output.print('\n');
  }

  /**
   * Capture finite original production inputs and deterministic observations into a new subtree.
   *
   * @param dir Caller-selected output directory.
   */
  static void capture(Path dir) throws Exception {
    Files.createDirectories(dir);
    try (PrintWriter sink =
        new PrintWriter(
            Files.newBufferedWriter(dir.resolve("observations.tsv"), StandardCharsets.UTF_8))) {
      output = sink;
      line("case\tindexed-schemas");
      run(dir);
      IndexedSessionObservations.capture(dir);
      line("complete\tindexed-schemas");
      require(!output.checkError(), "indexed observation output");
    } finally {
      output = null;
    }
  }

  static final long[] TIMES = {Long.MIN_VALUE, -1, 0, 0x1234567889abcdefL, Long.MAX_VALUE};
  static final int SENTINEL = 913;

  static void require(boolean ok, String message) {
    if (!ok) {
      throw new AssertionError(message);
    }
  }

  static void slots(DataRecord record) throws Exception {
    line(
        "schema\t"
            + record.getName()
            + "\t"
            + record.hasTime()
            + "\t"
            + record.getIntFieldCount()
            + "\t"
            + record.getObjFieldCount());
    for (int i = 0; i < record.getIntFieldCount(); ++i) {
      DataIntField f = record.getIntField(i);
      line(
          "slot\t"
              + record.getName()
              + "\t"
              + i
              + "\t"
              + f.getSerialType().getId()
              + "\t"
              + f.getSerialType().isLong()
              + "\t"
              + f.getName()
              + "\t"
              + f.getLocalName()
              + "\t"
              + f.getPropertyName());
    }
    // Diagnostic read-only projection of actual source identities, not exporter acceptance.
    IdentityHashMap<DataField, Integer> logical = new IdentityHashMap<>();
    IdentityHashMap<DataField, Integer> tails = new IdentityHashMap<>();
    int count = 0;
    for (int i = 0; i < record.getIntFieldCount(); ++i) {
      DataField value = record.getIntField(i);
      logical.put(value, count++);
      line(
          "source-map\t"
              + record.getName()
              + "\t"
              + value.getIndex()
              + "\t"
              + logical.get(value)
              + "\t"
              + value.getSerialType().getId());
      if (value.getSerialType().isLong()) {
        DataField tail = record.getIntField(++i);
        require(
            tail.getClass() == VoidIntField.class
                && tail.getSerialType().getId() == 0
                && tail.getIndex() == value.getIndex() + 1
                && tail.getRecord() == record
                && tail.getName().equals(value.getName() + "$VoidTail"),
            "actual tail identity");
        tails.put(tail, logical.get(value));
        line(
            "omitted-tail\t"
                + record.getName()
                + "\t"
                + tail.getIndex()
                + "\t"
                + logical.get(value));
      }
    }
    Field field = DefaultRecord.class.getDeclaredField("fieldsByName");
    field.setAccessible(true);
    Map<?, ?> actual = (Map<?, ?>) field.get(record);
    TreeMap<String, DataField> sorted = new TreeMap<>();
    for (Map.Entry<?, ?> entry : actual.entrySet()) {
      String key = (String) entry.getKey();
      DataField target = (DataField) entry.getValue();
      require(record.findFieldByName(key) == target, "lookup identity");
      sorted.put(key, target);
    }
    for (Map.Entry<String, DataField> entry : sorted.entrySet()) {
      DataField target = entry.getValue();
      require(logical.containsKey(target) || tails.containsKey(target), "lookup projection");
      line(
          "source-lookup\t"
              + record.getName()
              + "\t"
              + entry.getKey()
              + "\t"
              + (logical.containsKey(target)
                  ? "field:" + logical.get(target)
                  : "tail-owner:" + tails.get(target)));
    }
  }

  static DefaultRecord pair(int type) {
    String name = "Pair" + type;
    SerialFieldType serial =
        type == 4
            ? SerialFieldType.INT
            : type == 8
                ? SerialFieldType.COMPACT_INT
                : type == 24
                    ? SerialFieldType.DECIMAL
                    : type == 40
                        ? SerialFieldType.SHORT_STRING
                        : type == 56
                            ? SerialFieldType.TIME_SECONDS
                            : type == 72 ? SerialFieldType.SEQUENCE : SerialFieldType.DATE;
    return new DefaultRecord(
        0,
        name,
        true,
        new DataIntField[] {
          serial.createDefaultIntInstance(0, name + ".High"),
          serial.createDefaultIntInstance(1, name + ".Low"),
          new CompactIntField(2, name + ".Sentinel")
        },
        new DataObjField[0]);
  }

  static DefaultRecord single(int type) {
    String name = "Single" + type;
    DataIntField value =
        type == 104
            ? new LongField(0, name + ".Index")
            : type == 120
                ? new WideDecimalField(0, name + ".Index")
                : new TimeMillisField(0, name + ".Index");
    return new DefaultRecord(
        0,
        name,
        true,
        new DataIntField[] {
          value,
          new VoidIntField(1, name + ".Index$VoidTail"),
          new CompactIntField(2, name + ".Sentinel")
        },
        new DataObjField[0]);
  }

  static RecordBuffer input(DataScheme scheme, DataRecord record, long time, int sentinelSlot) {
    RecordBuffer buffer = new RecordBuffer(RecordMode.DATA);
    RecordCursor cursor = buffer.add(record, scheme.getCodec().encode("INDEX"), null);
    cursor.setTime(time);
    require(cursor.getTime() == time, "set/getTime");
    require(cursor.getInt(0) == (int) (time >>> 32), "high word");
    require(cursor.getInt(1) == (int) time, "low word");
    if (sentinelSlot >= 0) {
      cursor.setInt(sentinelSlot, SENTINEL);
    }
    line(
        "source\t"
            + record.getName()
            + "\t"
            + time
            + "\t"
            + cursor.getInt(0)
            + "\t"
            + cursor.getInt(1)
            + "\t"
            + cursor.getEventTimeSequence());
    return buffer;
  }

  static class Observer extends MessageConsumerAdapter {
    final String name;
    final long[] times;
    final int sentinelSlot;
    int records;
    int errors;

    Observer(String name, long[] times, int sentinelSlot) {
      this.name = name;
      this.times = times;
      this.sentinelSlot = sentinelSlot;
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      RecordCursor cursor;
      while ((cursor = ((RecordSource) iterator).next()) != null) {
        require(records < times.length, "record bound");
        require(cursor.getTime() == times[records], name + " time mismatch " + records);
        require(cursor.getEventTimeSequence() == 0, "indexed time is not event time");
        if (sentinelSlot >= 0) {
          require(cursor.getInt(sentinelSlot) == SENTINEL, "sentinel alignment");
        }
        line(
            "delivered\t"
                + name
                + "\t"
                + type.getId()
                + "\t"
                + records
                + "\t"
                + cursor.getTime()
                + "\t"
                + cursor.getInt(0)
                + "\t"
                + cursor.getInt(1)
                + "\t"
                + cursor.getEventTimeSequence());
        ++records;
      }
    }

    @Override
    public void handleCorruptedMessage(int type) {
      ++errors;
    }

    @Override
    public void handleCorruptedStream() {
      ++errors;
    }

    @Override
    public void handleUnknownMessage(int type) {
      ++errors;
    }

    void done() {
      require(records == times.length && errors == 0, name + " completion");
      line("complete\t" + name + "\t" + records + "\t" + errors);
    }
  }

  static class ObservedDesc extends BinaryRecordDesc {
    ObservedDesc(BinaryRecordDesc desc) {
      super(desc);
      for (int i = 0; i < nDesc; ++i) {
        line(
            "bound\t"
                + (record == null ? "unknown" : record.getName())
                + "\t"
                + i
                + "\t"
                + names[i]
                + "\t"
                + types[i]
                + "\t"
                + (descs[i] & INDEX_MASK));
      }
    }
  }

  static class Parser extends BinaryQTPParser {
    String senderZeroName;

    Parser(DataScheme scheme) {
      super(scheme);
    }

    @Override
    protected BinaryRecordDesc wrapRecordDesc(BinaryRecordDesc desc) {
      return new ObservedDesc(desc);
    }

    @Override
    protected void remapRecord(int id, BinaryRecordDesc desc) {
      line(
          "remap\t"
              + id
              + "\t"
              + (desc.getRecord() == null ? "unknown" : desc.getRecord().getName()));
      super.remapRecord(id, desc);
      if (id == 0) {
        senderZeroName = desc.getRecord() == null ? null : desc.getRecord().getName();
      }
    }
  }

  static void file(Path dir, DataScheme scheme, DataRecord record, int sentinelSlot)
      throws Exception {
    String name = record.getName();
    slots(record);
    try {
      ResolvedSchemaExporter.export(
          scheme, Collections.singletonList(name), new ResolvedSchemaExporter.Limits());
      throw new AssertionError("ordinary native exporter admitted indexed record");
    } catch (IllegalArgumentException expected) {
      require("UNSUPPORTED_INDEXED".equals(expected.getMessage()), "actual exporter refusal");
      line("exporter\t" + name + "\t" + expected.getMessage());
    }
    Path file = dir.resolve(name + ".qtp");
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(TimestampsType.NONE);
    writer.setOpt("");
    writer.setSaveAs(MessageType.STREAM_DATA);
    try (FileWriter opened = writer.open(file.toString(), scheme)) {
      for (long time : TIMES) {
        require(
            !opened.visitData(input(scheme, record, time, sentinelSlot), MessageType.STREAM_DATA),
            "writer capacity");
      }
    }
    require(Files.size(file) < 65536, "finite tape bytes");
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    params.setResyncOn(null);
    FileReader reader = new FileReader(file.toString(), new ConnectionStats(), params);
    reader.setScheme(scheme);
    Observer observer = new Observer("file-" + name, TIMES, sentinelSlot);
    try {
      reader.readInto(observer);
    } finally {
      reader.close();
    }
    observer.done();
    Parser parser = new Parser(scheme);
    parser.setInput(new ByteArrayInput(Files.readAllBytes(file)));
    Observer binary = new Observer("binary-" + name, TIMES, sentinelSlot);
    parser.parse(binary);
    binary.done();
  }

  static class Requested extends BinaryRecordDesc {
    Requested(DataRecord record, String[] names, int[] types) throws Exception {
      super(record, names.length, names, types, false, DIR_READ);
    }
  }

  static void binding(Path dir, String label, String[] names, int[] types, long expected)
      throws Exception {
    DefaultRecord record = pair(8);
    record.addFieldAlias("High", "HighAlias");
    DataScheme scheme = new DefaultScheme(PentaCodec.INSTANCE, record);
    require(
        record.findFieldByName("High") == record.findFieldByName("HighAlias"), "alias identity");
    Requested requested = new Requested(record, names, types);
    BinaryQTPComposer composer =
        new BinaryQTPComposer(scheme, true) {
          @Override
          protected BinaryRecordDesc getRequestedRecordDesc(DataRecord ignored) {
            return requested;
          }
        };
    composer.setOptSet(ProtocolOption.EMPTY_SET);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.visitData(input(scheme, record, TIMES[3], 2), MessageType.STREAM_DATA);
    byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
    Files.write(dir.resolve(label + ".qtp"), wire);
    Parser parser = new Parser(scheme);
    parser.setInput(new ByteArrayInput(wire));
    Observer observer = new Observer(label, new long[] {expected}, 2);
    parser.parse(observer);
    observer.done();
  }

  static byte[] composed(DataScheme scheme, DataRecord record, Requested requested, long time)
      throws Exception {
    ByteArrayOutput output = new ByteArrayOutput();
    BinaryQTPComposer composer =
        new BinaryQTPComposer(scheme, true) {
          @Override
          protected BinaryRecordDesc getRequestedRecordDesc(DataRecord ignored) {
            return requested;
          }
        };
    composer.setOptSet(ProtocolOption.EMPTY_SET);
    composer.setOutput(output);
    composer.visitData(input(scheme, record, time, 2), MessageType.STREAM_DATA);
    return Arrays.copyOf(output.getBuffer(), output.getPosition());
  }

  static byte[] frame(int type, byte[] payload) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(1 + payload.length);
    out.writeCompactInt(type);
    out.write(payload);
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  static byte[] dataOnly(byte[] wire) throws Exception {
    ByteArrayInput in = new ByteArrayInput(wire);
    int size = in.readCompactInt();
    return Arrays.copyOfRange(wire, in.getPosition() + size, wire.length);
  }

  static void session(Path dir) throws Exception {
    DefaultRecord pair = pair(8);
    pair.addFieldAlias("High", "HighAlias");
    DefaultRecord other =
        new DefaultRecord(
            1,
            "Other",
            true,
            new DataIntField[] {
              new CompactIntField(0, "Other.High"),
              new CompactIntField(1, "Other.Low"),
              new CompactIntField(2, "Other.Sentinel")
            },
            new DataObjField[0]);
    DataScheme scheme = new DefaultScheme(PentaCodec.INSTANCE, pair, other);
    byte[] ordinary = composed(scheme, pair, null, TIMES[3]);
    byte[] reordered =
        composed(
            scheme,
            pair,
            new Requested(pair, new String[] {"Low", "HighAlias", "Sentinel"}, new int[] {8, 8, 8}),
            TIMES[3]);
    byte[] subset =
        composed(
            scheme,
            pair,
            new Requested(pair, new String[] {"Low", "Sentinel"}, new int[] {8, 8}),
            TIMES[3]);
    byte[] retyped =
        composed(
            scheme,
            pair,
            new Requested(pair, new String[] {"HighAlias", "Low", "Sentinel"}, new int[] {4, 4, 8}),
            TIMES[3]);
    byte[] newSender = composed(scheme, other, null, TIMES[3]);
    ByteArrayOutput concat = new ByteArrayOutput();
    for (byte[] wire : new byte[][] {ordinary, ordinary, reordered, subset, retyped, newSender}) {
      concat.write(wire);
    }
    byte[] session = Arrays.copyOf(concat.getBuffer(), concat.getPosition());
    Files.write(dir.resolve("session-replacements.qtp"), session);
    Parser parser = new Parser(scheme);
    parser.setInput(new ByteArrayInput(session));
    Observer observer =
        new Observer(
            "session",
            new long[] {TIMES[3], TIMES[3], TIMES[3], TIMES[3] & 0xffffffffL, TIMES[3], TIMES[3]},
            2);
    parser.parse(observer);
    observer.done();

    // Deliberately assembled descriptor mutation: source id0 is renamed to actual local Other.
    ByteArrayOutput desc = new ByteArrayOutput();
    desc.writeCompactInt(0);
    desc.writeUTFString("Other");
    desc.writeCompactInt(3);
    for (String field : new String[] {"High", "Low", "Sentinel"}) {
      desc.writeUTFString(field);
      desc.writeCompactInt(8);
    }
    byte[] rename = frame(2, Arrays.copyOf(desc.getBuffer(), desc.getPosition()));
    ByteArrayOutput renamed = new ByteArrayOutput();
    renamed.write(rename);
    renamed.write(dataOnly(ordinary));
    byte[] renameWire = Arrays.copyOf(renamed.getBuffer(), renamed.getPosition());
    Files.write(dir.resolve("session-renamed.qtp"), renameWire);
    parser.setInput(new ByteArrayInput(renameWire));
    Observer renamedObserver = new Observer("renamed", new long[] {TIMES[3]}, 2);
    parser.parse(renamedObserver);
    renamedObserver.done();

    // Java installs complete entries before a later entry fails; C++ deliberately promises more.
    ByteArrayOutput partial = new ByteArrayOutput();
    partial.write(Arrays.copyOf(desc.getBuffer(), desc.getPosition()));
    partial.writeCompactInt(7);
    partial.writeUTFString("Broken");
    partial.writeCompactInt(1);
    partial.writeUTFString("Invalid");
    partial.writeCompactInt(256);
    byte[] partialWire = frame(2, Arrays.copyOf(partial.getBuffer(), partial.getPosition()));
    Files.write(dir.resolve("session-partial-failure.qtp"), partialWire);
    Parser partialParser = new Parser(scheme);
    partialParser.setInput(new ByteArrayInput(ordinary));
    Observer beforePartial = new Observer("before-partial", new long[] {TIMES[3]}, 2);
    partialParser.parse(beforePartial);
    beforePartial.done();
    partialParser.setInput(new ByteArrayInput(partialWire));
    Observer partialObserver = new Observer("partial", new long[0], 2);
    partialParser.parse(partialObserver);
    require(
        partialObserver.errors > 0 && "Other".equals(partialParser.senderZeroName),
        "Java complete-entry installation survives later descriptor error");
    line("partial-failure\t" + partialObserver.errors + "\t" + partialParser.senderZeroName);
    partialParser.setInput(new ByteArrayInput(dataOnly(ordinary)));
    Observer afterPartial = new Observer("after-partial", new long[] {TIMES[3]}, 2);
    partialParser.parse(afterPartial);
    afterPartial.done();

    // Reset is an actual production operation. A data-only next source loses the old map.
    parser.resetSession();
    parser.setInput(new ByteArrayInput(dataOnly(ordinary)));
    Observer reset = new Observer("reset-data-only", new long[0], 2);
    parser.parse(reset);
    require(reset.records == 0 && reset.errors > 0, "reset must discard sender descriptions");
    line("reset\t" + reset.records + "\t" + reset.errors);
    Parser fresh = new Parser(scheme);
    fresh.setInput(new ByteArrayInput(ordinary));
    Observer freshObserver = new Observer("fresh-source", new long[] {TIMES[3]}, 2);
    fresh.parse(freshObserver);
    freshObserver.done();
  }

  private static void run(Path dir) throws Exception {
    Files.createDirectories(dir);
    for (int type : new int[] {4, 8, 24, 40, 56, 72, 88}) {
      DefaultRecord record = pair(type);
      file(dir, new DefaultScheme(PentaCodec.INSTANCE, record), record, 2);
    }
    for (int type : new int[] {104, 120, 152}) {
      DefaultRecord record = single(type);
      file(dir, new DefaultScheme(PentaCodec.INSTANCE, record), record, 2);
    }
    DataScheme standard = DXFeedScheme.getInstance();
    DataRecord indexed = standard.findRecordByName("TimeAndSale");
    require(indexed != null && indexed.hasTime(), "standard indexed record");
    file(dir, standard, indexed, -1);
    binding(
        dir,
        "reordered-alias",
        new String[] {"Low", "HighAlias", "Sentinel"},
        new int[] {8, 8, 8},
        TIMES[3]);
    binding(
        dir,
        "missing-high",
        new String[] {"Low", "Sentinel"},
        new int[] {8, 8},
        TIMES[3] & 0xffffffffL);
    session(dir);
    line("all\tPASS");
  }
}

/** Same-message active sender observations through the actual production parser. */
final class IndexedSessionObservations {
  static final String[] BASE_NAMES = {"HighAlias", "Low", "Sentinel"};
  static final int[] BASE_TYPES = {8, 8, 8};
  static final String[] REORDERED_NAMES = {"Low", "HighAlias", "Sentinel"};
  static final String[] SUBSET_NAMES = {"Low", "Sentinel"};
  static final int[] SUBSET_TYPES = {8, 8};
  static final int[] RETYPED_TYPES = {4, 4, 8};
  static final String[] OTHER_NAMES = {"High", "Low", "Sentinel"};
  static final long TIME = IndexedSchemasOracle.TIMES[3];

  static DefaultRecord other(int id) {
    return new DefaultRecord(
        id,
        "Other",
        true,
        new DataIntField[] {
          new CompactIntField(0, "Other.High"),
          new CompactIntField(1, "Other.Low"),
          new CompactIntField(2, "Other.Sentinel")
        },
        new DataObjField[0]);
  }

  static byte[] entry(String name, String[] fields, int[] types) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(0);
    out.writeUTFString(name);
    out.writeCompactInt(fields.length);
    for (int i = 0; i < fields.length; ++i) {
      out.writeUTFString(fields[i]);
      out.writeCompactInt(types[i]);
    }
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  static byte[] concatenate(byte[]... parts) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    for (byte[] part : parts) {
      out.write(part);
    }
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  static class Description extends BinaryRecordDesc {
    Description(BinaryRecordDesc desc) {
      super(desc);
    }

    String signature() {
      StringBuilder text = new StringBuilder(record == null ? "unknown" : record.getName());
      for (int i = 0; i < nDesc; ++i) {
        text.append('|')
            .append(names[i])
            .append(':')
            .append(types[i])
            .append('@')
            .append(descs[i] & INDEX_MASK);
      }
      return text.toString();
    }
  }

  static class Parser extends BinaryQTPParser {
    final List<String> installs = new ArrayList<>();
    String lastRecord;
    String lastSignature;

    Parser(DataScheme scheme) {
      super(scheme);
    }

    @Override
    protected BinaryRecordDesc wrapRecordDesc(BinaryRecordDesc desc) {
      return new Description(desc);
    }

    @Override
    protected void remapRecord(int sender, BinaryRecordDesc desc) {
      IndexedSchemasOracle.require(sender == 0, "single repeated source sender");
      super.remapRecord(sender, desc);
      lastRecord = desc.getRecord() == null ? "unknown" : desc.getRecord().getName();
      lastSignature = ((Description) desc).signature();
      installs.add(lastSignature);
      IndexedSchemasOracle.line(
          "batch-remap\t" + (installs.size() - 1) + "\t" + sender + "\t" + lastSignature);
    }
  }

  static class Observer extends MessageConsumerAdapter {
    final String label;
    final String expectedRecord;
    final long[] expectedTimes;
    int records;
    int errors;

    Observer(String label, String expectedRecord, long... expectedTimes) {
      this.label = label;
      this.expectedRecord = expectedRecord;
      this.expectedTimes = expectedTimes;
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      RecordCursor cursor;
      while ((cursor = ((RecordSource) iterator).next()) != null) {
        IndexedSchemasOracle.require(records < expectedTimes.length, "finite callback count");
        IndexedSchemasOracle.require(
            expectedRecord.equals(cursor.getRecord().getName()), "actual delivered record name");
        IndexedSchemasOracle.require(
            cursor.getTime() == expectedTimes[records], "actual delivered time");
        IndexedSchemasOracle.require(
            cursor.getInt(2) == IndexedSchemasOracle.SENTINEL, "actual sentinel");
        IndexedSchemasOracle.require(cursor.getEventTimeSequence() == 0, "no implicit event time");
        IndexedSchemasOracle.line(
            "batch-delivered\t"
                + label
                + "\t"
                + records
                + "\t"
                + cursor.getRecord().getName()
                + "\t"
                + cursor.getTime()
                + "\t"
                + cursor.getInt(0)
                + "\t"
                + cursor.getInt(1)
                + "\t"
                + cursor.getInt(2));
        ++records;
      }
    }

    @Override
    public void handleCorruptedMessage(int type) {
      ++errors;
    }

    @Override
    public void handleCorruptedStream() {
      ++errors;
    }

    @Override
    public void handleUnknownMessage(int type) {
      ++errors;
    }

    void done() {
      IndexedSchemasOracle.require(
          records == expectedTimes.length && errors == 0, "batch delivery completion");
      IndexedSchemasOracle.line(
          "batch-delivery-complete\t" + label + "\t" + records + "\t" + errors);
    }
  }

  static void parseCase(
      Path dir,
      String name,
      DataScheme scheme,
      byte[][] entries,
      byte[] productionData,
      String expectedRecord,
      String expectedSignature,
      long expectedTime)
      throws Exception {
    byte[] description = IndexedSchemasOracle.frame(2, concatenate(entries));
    byte[] wire = concatenate(description, productionData);
    Files.write(dir.resolve(name + ".qtp"), wire);
    // Only the descriptor batch is assembled; the following data is unchanged composer output.
    IndexedSchemasOracle.require(wire.length < 4096, "finite batch wire");
    Parser parser = new Parser(scheme);
    parser.setInput(new ByteArrayInput(wire));
    Observer observer = new Observer(name, expectedRecord, expectedTime);
    IndexedSchemasOracle.line(
        "batch-begin\t" + name + "\t" + entries.length + "\t1-description-frame");
    parser.parse(observer);
    observer.done();
    IndexedSchemasOracle.require(
        parser.installs.size() == entries.length, "every same-frame remap observed");
    IndexedSchemasOracle.require(
        expectedRecord.equals(parser.lastRecord), "last record mapping wins");
    IndexedSchemasOracle.require(
        expectedSignature.equals(parser.lastSignature), "last exact description wins");
    IndexedSchemasOracle.line(
        "batch-complete\t"
            + name
            + "\t"
            + parser.installs.size()
            + "\t"
            + parser.lastSignature
            + "\t"
            + expectedTime);
  }

  static void capture(Path dir) throws Exception {

    DefaultRecord pair = IndexedSchemasOracle.pair(8);
    pair.addFieldAlias("High", "HighAlias");
    DataScheme consumer = new DefaultScheme(PentaCodec.INSTANCE, pair, other(1));
    byte[] ordinary = IndexedSchemasOracle.composed(consumer, pair, null, TIME);
    byte[] subsetData =
        IndexedSchemasOracle.dataOnly(
            IndexedSchemasOracle.composed(
                consumer,
                pair,
                new IndexedSchemasOracle.Requested(pair, SUBSET_NAMES, SUBSET_TYPES),
                TIME));
    byte[] retypedData =
        IndexedSchemasOracle.dataOnly(
            IndexedSchemasOracle.composed(
                consumer,
                pair,
                new IndexedSchemasOracle.Requested(pair, BASE_NAMES, RETYPED_TYPES),
                TIME));
    DefaultRecord sourceOther = other(0);
    DataScheme otherSource = new DefaultScheme(PentaCodec.INSTANCE, sourceOther);
    byte[] otherData =
        IndexedSchemasOracle.dataOnly(
            IndexedSchemasOracle.composed(otherSource, sourceOther, null, TIME));
    byte[] base = entry("Pair8", BASE_NAMES, BASE_TYPES);
    byte[] reordered = entry("Pair8", REORDERED_NAMES, BASE_TYPES);
    byte[] subset = entry("Pair8", SUBSET_NAMES, SUBSET_TYPES);
    byte[] retyped = entry("Pair8", BASE_NAMES, RETYPED_TYPES);
    byte[] renamed = entry("Other", OTHER_NAMES, BASE_TYPES);

    parseCase(
        dir,
        "batch-subset-last",
        consumer,
        new byte[][] {base, base, reordered, subset},
        subsetData,
        "Pair8",
        "Pair8|Low:8@1|Sentinel:8@2",
        TIME & 0xffffffffL);
    parseCase(
        dir,
        "batch-retyped-last",
        consumer,
        new byte[][] {base, base, retyped},
        retypedData,
        "Pair8",
        "Pair8|HighAlias:4@0|Low:4@1|Sentinel:8@2",
        TIME);
    parseCase(
        dir,
        "batch-renamed-last",
        consumer,
        new byte[][] {base, base, retyped, renamed},
        otherData,
        "Other",
        "Other|High:8@0|Low:8@1|Sentinel:8@2",
        TIME);
    parseCase(
        dir,
        "batch-renamed-exact-last",
        consumer,
        new byte[][] {base, base, reordered, renamed},
        otherData,
        "Other",
        "Other|High:8@0|Low:8@1|Sentinel:8@2",
        TIME);

    Parser parser = new Parser(consumer);
    parser.setInput(new ByteArrayInput(ordinary));
    Observer initial = new Observer("late-invalid-initial", "Pair8", TIME);
    parser.parse(initial);
    initial.done();
    int installedBefore = parser.installs.size();
    byte[] invalid = entry("Broken", new String[] {"Bad"}, new int[] {256});
    byte[] invalidWire = IndexedSchemasOracle.frame(2, concatenate(reordered, renamed, invalid));
    Files.write(dir.resolve("batch-late-invalid.qtp"), invalidWire);
    Files.write(dir.resolve("batch-late-invalid-prelude.qtp"), ordinary);
    Files.write(dir.resolve("batch-late-invalid-following-data.qtp"), otherData);
    parser.setInput(new ByteArrayInput(invalidWire));
    Observer rejected = new Observer("late-invalid", null);
    parser.parse(rejected);
    IndexedSchemasOracle.require(
        rejected.errors == 1 && rejected.records == 0, "late error observed");
    IndexedSchemasOracle.require(
        parser.installs.size() == installedBefore + 2, "two valid prefix remaps installed");
    IndexedSchemasOracle.require("Other".equals(parser.lastRecord), "Java retains prefix mutation");
    parser.setInput(new ByteArrayInput(otherData));
    Observer following = new Observer("late-invalid-following", "Other", TIME);
    parser.parse(following);
    following.done();
    IndexedSchemasOracle.line("batch-invalid-complete\t1\t2-prefix-installs\tOther\t" + TIME);
    Parser middleParser = new Parser(consumer);
    middleParser.setInput(new ByteArrayInput(ordinary));
    Observer middleInitial = new Observer("middle-invalid-initial", "Pair8", TIME);
    middleParser.parse(middleInitial);
    middleInitial.done();
    byte[] middleWire = IndexedSchemasOracle.frame(2, concatenate(reordered, invalid, renamed));
    Files.write(dir.resolve("batch-middle-invalid.qtp"), middleWire);
    middleParser.setInput(new ByteArrayInput(middleWire));
    Observer middleRejected = new Observer("middle-invalid", null);
    middleParser.parse(middleRejected);
    IndexedSchemasOracle.require(
        middleRejected.errors == 1 && middleRejected.records == 0,
        "middle invalid entry stops later remap");
    IndexedSchemasOracle.require(
        middleParser.installs.size() == 2 && "Pair8".equals(middleParser.lastRecord),
        "valid following entry was not installed");
    byte[] reorderedData =
        IndexedSchemasOracle.dataOnly(
            IndexedSchemasOracle.composed(
                consumer,
                pair,
                new IndexedSchemasOracle.Requested(pair, REORDERED_NAMES, BASE_TYPES),
                TIME));
    Files.write(dir.resolve("batch-middle-invalid-following-data.qtp"), reorderedData);
    middleParser.setInput(new ByteArrayInput(reorderedData));
    Observer middleFollowing = new Observer("middle-invalid-following", "Pair8", TIME);
    middleParser.parse(middleFollowing);
    middleFollowing.done();
    IndexedSchemasOracle.line("batch-middle-invalid-complete\t1\t1-prefix-install\tPair8\t" + TIME);
    IndexedSchemasOracle.line("batch-all\tPASS");
  }
}
