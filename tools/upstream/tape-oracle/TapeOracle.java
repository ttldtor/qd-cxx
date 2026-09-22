package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.HeartbeatPayload;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.qd.util.TimeSequenceUtil;
import com.devexperts.transport.stats.ConnectionStats;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** Bounded production file writer/reader evidence; envelope scanning is diagnostic only. */
public final class TapeOracle {
  private static final int MAX_BYTES = 65536;
  private static final DataScheme SENDER =
      new DefaultScheme(
          PentaCodec.INSTANCE, record(0, "R", "x", "y", "z"), record(1, "S", "v"), record(2, "E"));
  private static final DataScheme LOCAL =
      new DefaultScheme(
          PentaCodec.INSTANCE, record(0, "S", "v"), record(1, "R", "y", "x", "z"), record(2, "E"));

  private static DefaultRecord record(int id, String name, String... fields) {
    DataIntField[] ints = new DataIntField[fields.length];
    for (int i = 0; i < fields.length; i++) {
      ints[i] = new CompactIntField(i, name + "." + fields[i]);
    }
    return new DefaultRecord(id, name, false, ints, new DataObjField[0]);
  }

  private static void require(boolean ok, String reason) {
    if (!ok) {
      throw new IllegalArgumentException(reason);
    }
  }

  private static String units(String s) {
    if (s == null || s.isEmpty()) {
      return "-";
    }
    StringBuilder result = new StringBuilder();
    for (char c : s.toCharArray()) {
      result.append(String.format("%04x", (int) c));
    }
    return result.toString();
  }

  private static String decode(String s) {
    require(s.equals("-") || s.matches("(?:[0-9a-f]{4})+"), "symbol units");
    StringBuilder result = new StringBuilder();
    if (!s.equals("-")) {
      for (int i = 0; i < s.length(); i += 4) {
        result.append((char) Integer.parseInt(s.substring(i, i + 4), 16));
      }
    }
    return result.toString();
  }

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; i++) {
      if (i != 0) {
        out.print('\t');
      }
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static List<String[]> inputs(Path path) throws Exception {
    require(Files.size(path) <= MAX_BYTES, "input size");
    List<String[]> result = new ArrayList<>();
    int previous = 0;
    long time = 0;
    for (String line : Files.readAllLines(path, StandardCharsets.UTF_8)) {
      if (line.startsWith("#")) {
        continue;
      }
      String[] c = line.split("\t", -1);
      require(c.length == 6, "input arity");
      int message = Integer.parseInt(c[0]);
      long nextTime = Long.parseLong(c[1]);
      require(message == previous || message == previous + 1, "message order");
      require(nextTime > 0 && (message != previous || time == 0 || time == nextTime), "time");
      require(SENDER.findRecordByName(c[2]) != null, "record name");
      decode(c[3]);
      String[] values = c[4].equals("-") ? new String[0] : c[4].split(",", -1);
      require(values.length == SENDER.findRecordByName(c[2]).getIntFieldCount(), "values");
      for (String value : values) {
        Integer.parseInt(value);
      }
      require(c[5].equals("0"), "this input profile uses zero flags");
      result.add(c);
      previous = message;
      time = nextTime;
    }
    require(result.size() == 7 && previous == 2, "seven records / three messages required");
    return result;
  }

  private static void write(Path file, String profile, List<String[]> input) throws Exception {
    FileWriter writer = new FileWriter();
    if (profile.equals("constrained")) {
      writer.setFormat(FileFormat.BINARY);
      writer.setCompression(StreamCompression.NONE);
      writer.setTime(TimestampsType.NONE);
      writer.setOpt("");
      writer.setSaveAs(MessageType.STREAM_DATA);
    } else if (profile.equals("text")) {
      writer.setFormat(FileFormat.TEXT);
    }
    try (FileWriter opened = writer.open(file.toString(), SENDER)) {
      for (int message = 0; message < 3; message++) {
        RecordBuffer buffer = new RecordBuffer(RecordMode.FLAGGED_DATA);
        long time = 0;
        for (String[] c : input) {
          if (Integer.parseInt(c[0]) != message) {
            continue;
          }
          time = Long.parseLong(c[1]);
          String symbol = decode(c[3]);
          int cipher = SENDER.getCodec().encode(symbol);
          RecordCursor cursor =
              buffer.add(SENDER.findRecordByName(c[2]), cipher, cipher == 0 ? symbol : null);
          if (!c[4].equals("-")) {
            String[] values = c[4].split(",");
            for (int i = 0; i < values.length; i++) {
              cursor.setInt(i, Integer.parseInt(values[i]));
            }
          }
          cursor.setEventFlags(0);
        }
        HeartbeatPayload heartbeat = new HeartbeatPayload();
        heartbeat.setTimeMillis(time);
        opened.visitHeartbeat(heartbeat);
        require(!opened.visitData(buffer, MessageType.STREAM_DATA), "writer capacity");
      }
    }
    require(Files.size(file) > 0 && Files.size(file) <= MAX_BYTES, "output size");
  }

  private static final class Observer extends MessageConsumerAdapter {
    final PrintWriter out;
    final List<String[]> input;
    final String profile;
    int records;
    int callbacks;
    int errors;

    Observer(PrintWriter out, List<String[]> input, String profile) {
      this.out = out;
      this.input = input;
      this.profile = profile;
    }

    @Override
    public void processDescribeProtocol(ProtocolDescriptor descriptor, boolean log) {
      require(
          descriptor.getEndpointId() == null && descriptor.getReceiveSet().isEmpty(),
          "unexpected descriptor endpoint/receive set");
      require(
          descriptor.getSendSet().isEmpty()
              || descriptor.getSendSet().equals(java.util.EnumSet.of(MessageType.STREAM_DATA)),
          "unexpected send set");
      row(out, "descriptor", descriptor.getSendSet().isEmpty() ? "-" : "15", "-", "-");
      for (Map.Entry<String, String> entry : new TreeMap<>(descriptor.getProperties()).entrySet()) {
        row(out, "property", units(entry.getKey()), units(entry.getValue()));
      }
    }

    @Override
    public void processHeartbeat(HeartbeatPayload heartbeat) {
      row(
          out,
          "heartbeat",
          heartbeat.hasTimeMillis() ? heartbeat.getTimeMillis() : "-",
          heartbeat.isEmpty());
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      RecordCursor cursor;
      while ((cursor = ((RecordSource) iterator).next()) != null) {
        require(records < input.size(), "extra delivery");
        String[] expected = input.get(records);
        String symbol = LOCAL.getCodec().decode(cursor.getCipher(), cursor.getSymbol());
        StringBuilder values = new StringBuilder();
        for (int i = 0; i < cursor.getIntCount(); i++) {
          if (i != 0) {
            values.append(',');
          }
          values.append(cursor.getInt(i));
        }
        String actualValues = values.length() == 0 ? "-" : values.toString();
        String expectedValues = expected[4];
        if (expected[2].equals("R")) {
          String[] v = expectedValues.split(",");
          expectedValues = v[1] + "," + v[0] + "," + v[2];
        }
        long expectedTime =
            profile.equals("constrained") || profile.equals("truncated")
                ? 0
                : TimeSequenceUtil.getTimeSequenceFromTimeMillis(Long.parseLong(expected[1]));
        require(
            type == MessageType.STREAM_DATA
                && cursor.getRecord() == LOCAL.findRecordByName(expected[2])
                && units(symbol).equals(expected[3])
                && actualValues.equals(expectedValues)
                && cursor.getEventFlags() == 0
                && cursor.getEventTimeSequence() == expectedTime,
            "delivery mismatch at " + records);
        row(
            out,
            "record",
            records++,
            callbacks,
            type.getId(),
            cursor.getRecord().getId(),
            units(cursor.getRecord().getName()),
            units(symbol),
            actualValues,
            cursor.getEventFlags(),
            cursor.getEventTimeSequence());
      }
      callbacks++;
    }

    @Override
    public void handleCorruptedStream() {
      row(out, "error", "stream", "-");
      errors++;
    }

    @Override
    public void handleCorruptedMessage(int type) {
      row(out, "error", "message", type);
      errors++;
    }

    @Override
    public void handleUnknownMessage(int type) {
      row(out, "error", "unknown", type);
      errors++;
    }
  }

  private static void replay(Path file, Path observation, String profile, List<String[]> input)
      throws Exception {
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    // Keep format/time/compression autodetection; a literal single filename disables follow.
    FileReader reader = new FileReader(file.toString(), new ConnectionStats(), params);
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(observation, StandardCharsets.UTF_8))) {
      row(out, "profile", profile, "single-file", "MAX_SPEED", "auto");
      reader.setScheme(LOCAL);
      Observer observer = new Observer(out, input, profile);
      reader.readInto(observer);
      if (profile.equals("truncated")) {
        require(
            observer.errors == 1 && observer.records > 0 && observer.records < input.size(),
            "truncation must report partial deliveries and one error");
      } else {
        require(observer.errors == 0 && observer.records == input.size(), "incomplete replay");
      }
      row(out, "end", observer.records, observer.callbacks, observer.errors, "returned");
      row(out, "complete", profile);
      require(!out.checkError(), "observation I/O");
    } finally {
      reader.close();
    }
  }

  private static void envelopes(Path file, Path output) throws Exception {
    byte[] bytes = Files.readAllBytes(file);
    ByteArrayInput in = new ByteArrayInput(bytes);
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(output, StandardCharsets.UTF_8))) {
      int index = 0;
      while (in.available() > 0) {
        long start = in.totalPosition();
        int length = in.readCompactInt();
        long end = in.totalPosition() + length;
        require(length >= 0 && end <= bytes.length, "envelope length");
        int type = length == 0 ? -1 : in.readCompactInt();
        row(out, "frame", index++, start, end, length, type);
        in.skipBytes((int) (end - in.totalPosition()));
      }
      row(out, "complete", "envelopes", index, bytes.length);
      require(!out.checkError(), "envelope output I/O");
    }
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 2, "input TSV and new output directory required");
    List<String[]> input = inputs(Paths.get(args[0]));
    Path output = Paths.get(args[1]);
    Files.createDirectory(output);
    for (String profile : Arrays.asList("default", "constrained", "text")) {
      Path file = output.resolve(profile + (profile.equals("text") ? ".txt" : ".qtp"));
      write(file, profile, input);
      replay(file, output.resolve(profile + ".tsv"), profile, input);
      if (!profile.equals("text")) {
        envelopes(file, output.resolve(profile + "-frames.tsv"));
      }
    }
    byte[] bytes = Files.readAllBytes(output.resolve("constrained.qtp"));
    // Remove the footer and the final data byte: an incomplete complete-file probe, not replay
    // input repair.
    Files.write(output.resolve("truncated.qtp"), Arrays.copyOf(bytes, bytes.length - 2));
    replay(output.resolve("truncated.qtp"), output.resolve("truncated.tsv"), "truncated", input);
    System.out.println(
        "CAPTURE COMPLETE: production FileWriter/FileReader; 3 files and truncated probe");
  }
}
