package qdcxx.schemaexport;

import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.impl.DXFeedScheme;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Collections;

/** Opt-in production observations; generated JSON is not the source of expected field values. */
public final class ExportCapture {
  private static void require(boolean condition) {
    if (!condition) {
      throw new IllegalStateException("capture invariant");
    }
  }

  private static void write(Path path, String text) throws Exception {
    Files.write(path, text.getBytes(StandardCharsets.UTF_8));
  }

  private static String units(String value) {
    if (value == null) {
      return "null";
    }
    if (value.isEmpty()) {
      return "-";
    }
    StringBuilder result = new StringBuilder();
    for (char unit : value.toCharArray()) {
      result.append(String.format("%04x", (int) unit));
    }
    return result.toString();
  }

  private static void export(
      Path root, DataScheme scheme, java.util.List<String> selected, String provenance)
      throws Exception {
    ResolvedSchemaExporter.Snapshot result =
        ResolvedSchemaExporter.export(scheme, selected, new ResolvedSchemaExporter.Limits());
    write(root.resolve("schema.json"), result.schemaJson);
    write(root.resolve("projection.json"), result.projectionJson + "\n");
    write(root.resolve("source.json"), provenance + "\n");
  }

  private static void observe(PrintWriter out, RecordCursor cursor) {
    DataRecord record = cursor.getRecord();
    out.print("value\t" + units(cursor.getDecodedSymbol()) + "\t" + cursor.getEventTimeSequence());
    for (int slot = 0; slot < record.getIntFieldCount(); slot++) {
      DataIntField field = record.getIntField(slot);
      if (field.getSerialType().getId() != 0) {
        long value = field.getSerialType().isLong() ? cursor.getLong(slot) : cursor.getInt(slot);
        out.print("\t" + field.getSerialType().getId() + ":" + value + ":null");
      }
    }
    for (int slot = 0; slot < record.getObjFieldCount(); slot++) {
      out.print(
          "\t"
              + record.getObjField(slot).getSerialType().getId()
              + ":0:"
              + units((String) cursor.getObj(slot)));
    }
    out.print('\n');
  }

  private static void custom(Path root, DataScheme scheme, boolean raw) throws Exception {
    String id = raw ? "raw" : "stream";
    DataRecord record = scheme.findRecordByName("Custom");
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(raw ? TimestampsType.MESSAGE : TimestampsType.NONE);
    writer.setOpt("");
    writer.setSaveAs(raw ? MessageType.RAW_DATA : MessageType.STREAM_DATA);
    Path tape = root.resolve(id + ".qtp");
    try (FileWriter opened = writer.open(tape.toString(), scheme)) {
      for (int index = 0; index < 3; index++) {
        RecordBuffer buffer = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
        RecordCursor cursor = buffer.add(record, scheme.getCodec().encode("TEST"), "TEST");
        cursor.setLong(record.findFieldByName("Wide").getIndex(), 1234567890123L + index);
        cursor.setInt(record.findFieldByName("Count").getIndex(), 42 + index);
        cursor.setInt(record.findFieldByName("Extra").getIndex(), -7 - index);
        cursor.setObj(
            record.findFieldByName("Text").getIndex(),
            index == 0 ? null : index == 1 ? "" : "A\u0000\ud800\ud83d\ude00");
        cursor.setEventTimeSequence((1700000000000L + index) << 22);
        require(!opened.visitData(buffer, MessageType.STREAM_DATA));
      }
    }
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    FileReader reader = new FileReader(tape.toString(), new ConnectionStats(), params);
    reader.setScheme(scheme);
    final int[] counts = {0, 0};
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(root.resolve(id + ".tsv"), StandardCharsets.UTF_8))) {
      try {
        reader.readInto(
            new MessageConsumerAdapter() {
              @Override
              protected void processData(DataIterator iterator, MessageType type) {
                RecordCursor cursor;
                while ((cursor = ((RecordSource) iterator).next()) != null) {
                  require(counts[1]++ < 16);
                  if (type == MessageType.STREAM_DATA) {
                    observe(out, cursor);
                    counts[0]++;
                  }
                }
              }

              @Override
              public void handleCorruptedStream() {
                throw new IllegalStateException("corrupted stream");
              }

              @Override
              public void handleCorruptedMessage(int type) {
                throw new IllegalStateException("corrupted message");
              }
            });
      } finally {
        reader.close();
      }
      require(counts[0] == 3 && counts[1] == (raw ? 9 : 3));
      require(!out.checkError());
    }
  }

  /** Captures one finite configured-event case and one local XML import/overlay case. */
  public static void main(String[] args) throws Exception {
    require(args.length == 2);
    Path root = Paths.get(args[0]);
    Path inputs = Paths.get(args[1]);
    Files.createDirectory(root);
    Path events = root.resolve("set-before");
    // Invoke the unchanged historical harness only through its public entry point.
    Class.forName("com.dxfeed.event.market.EventMappingOracle")
        .getMethod("main", String[].class)
        .invoke(null, (Object) new String[] {events.toString(), "set-before", inputs.toString()});
    export(
        events,
        DXFeedScheme.getInstance(),
        Arrays.asList("Quote", "Profile"),
        "{\"mode\":\"default\",\"startup\":{\"dxscheme.nanoTime\":\"true\"}}");
    Path custom = root.resolve("custom");
    Files.createDirectory(custom);
    try (SchemaSource source =
        SchemaSource.resolve(
            "xml",
            Collections.emptyMap(),
            null,
            Collections.emptyMap(),
            inputs,
            Arrays.asList("root.xml", "overlay.xml"),
            false,
            false,
            false,
            16,
            1048576,
            8)) {
      export(custom, source.scheme, Collections.emptyList(), source.provenanceJson);
      custom(custom, source.scheme, false);
      custom(custom, source.scheme, true);
    }
    Path names = Files.createDirectory(root.resolve("names"));
    String name = "Control\u0000\"\\\ud83d\ude00";
    DefaultRecord named =
        new DefaultRecord(
            0,
            name,
            false,
            new DataIntField[] {
              SerialFieldType.COMPACT_INT.createDefaultIntInstance(0, name + ".Value")
            },
            new DataObjField[0]);
    named.addFieldAlias("Value", "Alias\u0000\"\\\ud83d\ude00");
    export(
        names,
        new DefaultScheme(PentaCodec.INSTANCE, named),
        Collections.emptyList(),
        "{\"mode\":\"programmatic\"}");
    Path empty = Files.createDirectory(root.resolve("empty"));
    export(
        empty,
        new DefaultScheme(PentaCodec.INSTANCE),
        Collections.emptyList(),
        "{\"mode\":\"programmatic\"}");
    System.out.println("EXPORT CAPTURE COMPLETE");
  }
}
