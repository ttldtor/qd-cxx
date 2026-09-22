package qdcxx.oracle;

import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.HeartbeatPayload;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.transport.stats.ConnectionStats;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.TreeMap;

/**
 * Focused inputs replayed through the unchanged production FileReader, without resynchronization.
 */
public final class TapeReaderOracle {
  private static DefaultRecord record(int id, String name, String... fields) {
    DataIntField[] ints = new DataIntField[fields.length];
    for (int i = 0; i < fields.length; i++) {
      ints[i] = new CompactIntField(i, name + "." + fields[i]);
    }
    return new DefaultRecord(id, name, false, ints, new DataObjField[0]);
  }

  private static final DataScheme LOCAL =
      new DefaultScheme(
          PentaCodec.INSTANCE, record(0, "S", "v"), record(1, "R", "y", "x", "z"), record(2, "E"));

  private static byte[] bytes(ByteArrayOutput out) {
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  private static byte[] join(byte[]... arrays) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    for (byte[] a : arrays) {
      out.write(a);
    }
    return bytes(out);
  }

  private static byte[] frame(int type, byte[] payload) throws Exception {
    ByteArrayOutput body = new ByteArrayOutput();
    body.writeCompactInt(type);
    body.write(payload);
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(body.getPosition());
    out.write(bytes(body));
    return bytes(out);
  }

  private static byte[] descriptor(String[] props, int send, int receive) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    out.write(new byte[] {68, 88, 80, 51});
    out.writeCompactLong(props.length / 2);
    for (String p : props) {
      out.writeUTFString(p);
    }
    for (int id : new int[] {send, receive}) {
      out.writeCompactInt(id == 0 ? 0 : 1);
      if (id != 0) {
        out.writeCompactInt(id);
        out.writeUTFString(id == 15 ? "STREAM_DATA" : "RAW_DATA");
        out.writeCompactInt(0);
      }
    }
    return bytes(out);
  }

  private static byte[] desc(int id, String name) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(id);
    out.writeUTFString(name);
    out.writeCompactInt(1);
    out.writeUTFString("v");
    out.writeCompactInt(8);
    return frame(2, bytes(out));
  }

  private static String units(String value) {
    if (value == null) {
      return "null";
    }
    if (value.isEmpty()) {
      return "-";
    }
    StringBuilder out = new StringBuilder();
    for (char c : value.toCharArray()) {
      out.append(String.format("%04x", (int) c));
    }
    return out.toString();
  }

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; i++) {
      if (i > 0) {
        out.print('\t');
      }
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static final class Observer extends MessageConsumerAdapter {
    private final PrintWriter out;
    private int records;
    private int errors;

    Observer(PrintWriter out) {
      this.out = out;
    }

    @Override
    public void processDescribeProtocol(ProtocolDescriptor d, boolean log) {
      row(out, "descriptor", units(d.getSendSet().toString()), units(d.getReceiveSet().toString()));
      // Null keys are retained by Java; sort their exact observation representation instead.
      Map<String, String> props = new TreeMap<>();
      for (Map.Entry<String, String> p : d.getProperties().entrySet()) {
        props.put(units(p.getKey()), units(p.getValue()));
      }
      for (Map.Entry<String, String> p : props.entrySet()) {
        row(out, "property", p.getKey(), p.getValue());
      }
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      RecordCursor c;
      while ((c = ((RecordSource) iterator).next()) != null) {
        StringBuilder values = new StringBuilder();
        for (int i = 0; i < c.getIntCount(); i++) {
          if (i > 0) {
            values.append(',');
          }
          values.append(c.getInt(i));
        }
        row(
            out,
            "record",
            records++,
            type.getId(),
            c.getRecord().getId(),
            units(LOCAL.getCodec().decode(c.getCipher(), c.getSymbol())),
            values.length() == 0 ? "-" : values,
            c.getEventFlags(),
            c.getEventTimeSequence());
      }
    }

    @Override
    public void processHeartbeat(HeartbeatPayload h) {
      row(out, "heartbeat", h.isEmpty());
    }

    @Override
    public void handleCorruptedStream() {
      errors++;
      row(out, "error", "stream", "-");
    }

    @Override
    public void handleCorruptedMessage(int type) {
      errors++;
      row(out, "error", "message", type);
    }

    @Override
    public void handleUnknownMessage(int type) {
      errors++;
      row(out, "error", "unknown", type);
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 2) {
      throw new IllegalArgumentException("original constrained file and new directory required");
    }
    byte[] original = Files.readAllBytes(Paths.get(args[0]));
    if (original.length != 137) {
      throw new IllegalArgumentException("original fixture size");
    }
    Path root = Paths.get(args[1]);
    Files.createDirectory(root);
    Map<String, byte[]> cases = new LinkedHashMap<>();
    String[] base = {"type", "tape", "opt", "", "version", "probe"};
    byte[] header = frame(1, descriptor(base, 15, 0));
    byte[] tail = Arrays.copyOfRange(original, 58, original.length);
    cases.put("original", original);
    cases.put("empty", new byte[0]);
    cases.put("header-only", header);
    cases.put("no-footer", Arrays.copyOf(original, 136));
    cases.put("missing-header", tail);
    cases.put("repeated-header", join(header, original));
    cases.put("duplicate-id", join(header, desc(1, "S"), tail));
    cases.put("redefined-id", join(header, desc(1, "Unknown"), tail));
    cases.put(
        "unknown-record", join(header, desc(1, "Unknown"), Arrays.copyOfRange(original, 67, 76)));
    cases.put("undescribed-id", join(header, Arrays.copyOfRange(original, 67, 76)));
    cases.put(
        "partial-update",
        join(
            header,
            frame(2, new byte[] {1, 1, 83, 1, 1, 118, 8, 2}),
            Arrays.copyOfRange(original, 67, 76)));
    cases.put("footer-then-data", join(original, Arrays.copyOfRange(original, 67, 76)));
    cases.put("explicit-zero", join(header, new byte[] {1, 0}, tail));
    String[][] variants = {
      {"type", "tape"},
      {"opt", ""},
      {"type", "tape", "opt", ""},
      {"type", "tape", "opt", "hs"},
      {"type", "tape", "opt", "", "time", "NONE"},
      {"type", "tape", "opt", "", "time", ""},
      {"type", "tape", "opt", "", "time", "MESSAGE"},
      {"type", "tape", "opt", "", "unknown", "x"},
      {"type", "wrong", "type", "tape", "opt", ""},
      {"type", "tape", "opt", "", "opt", "hs"},
      {"type", "tape", "opt", null},
      {"type", "tape", "opt", "", "version", ""},
      {"type", "", "opt", ""}
    };
    for (int i = 0; i < variants.length; i++) {
      cases.put("properties-" + i, join(frame(1, descriptor(variants[i], 15, 0)), tail));
    }
    cases.put("send-empty", join(frame(1, descriptor(base, 0, 0)), tail));
    cases.put("send-raw", join(frame(1, descriptor(base, 5, 0)), tail));
    cases.put("receive-stream", join(frame(1, descriptor(base, 15, 15)), tail));
    byte[] payload = descriptor(base, 15, 0);
    cases.put("extension", join(frame(1, join(payload, new byte[] {0})), tail));
    cases.put("negative-properties", frame(1, new byte[] {68, 88, 80, 51, 127}));
    cases.put("negative-send", frame(1, new byte[] {68, 88, 80, 51, 0, 127, 0}));
    cases.put("negative-receive", frame(1, new byte[] {68, 88, 80, 51, 0, 0, 127}));
    cases.put("bad-magic", frame(1, new byte[] {68, 88, 80, 50, 0, 0, 0}));
    for (int n : new int[] {1, 2, 20, 57, 59, 65, 68, 74, 80, 100, 135}) {
      cases.put("truncated-" + n, Arrays.copyOf(original, n));
    }
    for (int n : new int[] {0, 1, 4, 5, payload.length - 1}) {
      cases.put("malformed-header-" + n, frame(1, Arrays.copyOf(payload, n)));
    }
    cases.put("malformed-data", join(header, desc(1, "S"), frame(15, new byte[] {(byte) 0xfe, 1})));
    for (Map.Entry<String, byte[]> test : cases.entrySet()) {
      Path file = root.resolve(test.getKey() + ".qtp");
      Files.write(file, test.getValue());
      FileReaderParams.Default params = new FileReaderParams.Default();
      params.setSpeed(FileReaderParams.MAX_SPEED);
      params.setCycle(false);
      params.setFormat(FileFormat.BINARY);
      params.setCompression(StreamCompression.NONE);
      FileReader reader = new FileReader(file.toString(), new ConnectionStats(), params);
      try (PrintWriter out =
          new PrintWriter(
              Files.newBufferedWriter(
                  root.resolve(test.getKey() + ".tsv"), StandardCharsets.UTF_8))) {
        row(out, "case", test.getKey(), test.getValue().length);
        Observer observer = new Observer(out);
        reader.setScheme(LOCAL);
        String result = "returned";
        try {
          reader.readInto(observer);
        } catch (Exception e) {
          result = e.getClass().getName();
          row(out, "exception", result);
        }
        row(out, "end", observer.records, observer.errors, result);
        row(out, "complete", test.getKey());
        if (out.checkError()) {
          throw new IllegalStateException("output failed");
        }
      } finally {
        reader.close();
      }
    }
    System.out.println("CAPTURE COMPLETE: " + cases.size() + " production FileReader probes");
  }
}
