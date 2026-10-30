package qdcxx.oracle;

import com.devexperts.io.BufferedInput;
import com.devexperts.io.BufferedInputPart;
import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.Marshalled;
import com.devexperts.io.Marshaller;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SymbolCodec;
import com.devexperts.qd.kit.ByteArrayField;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.MarshalledObjField;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.StringField;
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
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;

/**
 * Production byte/string/object observations; called by FieldTypesOracle into a new child
 * directory.
 */
final class ObjectFieldsOracle {
  static final int SENTINEL = 77;
  static Path root;
  static PrintWriter out;

  static byte[] bytes(ByteArrayOutput b) {
    return Arrays.copyOf(b.getBuffer(), b.getPosition());
  }

  static byte[] hex(String text) {
    byte[] b = new byte[text.length() / 2];
    for (int i = 0; i < b.length; i++) {
      b[i] = (byte) Integer.parseInt(text.substring(i * 2, i * 2 + 2), 16);
    }
    return b;
  }

  static String hex(byte[] b) {
    if (b == null) {
      return "null";
    }
    StringBuilder s = new StringBuilder();
    for (byte v : b) {
      s.append(String.format("%02x", v & 255));
    }
    return s.length() == 0 ? "empty" : s.toString();
  }

  static String value(Object v) {
    if (v == null) {
      return "null";
    }
    if (v instanceof Marshalled) {
      return "marshalled:" + hex(((Marshalled<?>) v).getBytes());
    }
    if (v instanceof byte[]) {
      return "bytes:" + hex((byte[]) v);
    }
    StringBuilder s = new StringBuilder("units:");
    for (char c : ((String) v).toCharArray()) {
      s.append(String.format("%04x", (int) c));
    }
    return s.toString();
  }

  static byte[] compact(long v) throws Exception {
    ByteArrayOutput b = new ByteArrayOutput();
    b.writeCompactLong(v);
    return bytes(b);
  }

  static byte[] concat(byte[]... inputs) throws Exception {
    ByteArrayOutput b = new ByteArrayOutput();
    for (byte[] v : inputs) {
      b.write(v);
    }
    return bytes(b);
  }

  static byte[] frame(int t, byte[] p) throws Exception {
    return concat(compact(p.length + 1), compact(t), p);
  }

  static DataRecord record(int type, boolean skip) {
    DataObjField field =
        type == 137
            ? new StringField(0, "Objects.Value", true)
            : type == 249
                ? new MarshalledObjField(0, "Objects.Value")
                : new ByteArrayField(0, "Objects.Value");
    DataRecord r =
        new DefaultRecord(
            0,
            "Objects",
            false,
            new DataIntField[] {new CompactIntField(0, "Objects.Sentinel")},
            skip ? new DataObjField[0] : new DataObjField[] {field});
    new DefaultScheme(PentaCodec.INSTANCE, r);
    return r;
  }

  static class Requested extends BinaryRecordDesc {
    Requested(DataRecord r, int type) throws Exception {
      super(r, 2, new String[] {"Value", "Sentinel"}, new int[] {type, 8}, false, DIR_READ);
    }
  }

  static class Trace extends BinaryRecordDesc {
    long examined;

    Trace(BinaryRecordDesc d) {
      super(d);
    }

    public RecordCursor readRecord(BufferedInput b, RecordBuffer r, int c, String s, int f)
        throws IOException {
      long start = b.totalPosition();
      try {
        return super.readRecord(b, r, c, s, f);
      } finally {
        examined = b.totalPosition() - start;
      }
    }
  }

  static class Seen extends MessageConsumerAdapter {
    int records, errors, sentinel;
    String observed = "-";

    protected void processData(DataIterator iterator, MessageType t) {
      RecordCursor c;
      while ((c = ((RecordSource) iterator).next()) != null) {
        records++;
        sentinel = c.getInt(0);
        observed = c.getRecord().getObjFieldCount() == 0 ? "skipped" : value(c.getObj(0));
      }
    }

    public void handleCorruptedStream() {
      errors++;
    }

    public void handleCorruptedMessage(int t) {
      errors++;
    }

    public void handleUnknownMessage(int t) {
      errors++;
    }
  }

  static void row(Object... cells) {
    for (int i = 0; i < cells.length; i++) {
      if (i > 0) {
        out.print('\t');
      }
      out.print(cells[i]);
    }
    out.print('\n');
  }

  static byte[] description(int type) throws Exception {
    ByteArrayOutput b = new ByteArrayOutput();
    b.writeCompactInt(0);
    b.writeUTFString("Objects");
    b.writeCompactInt(2);
    b.writeUTFString("Value");
    b.writeCompactInt(type);
    b.writeUTFString("Sentinel");
    b.writeCompactInt(8);
    return frame(2, bytes(b));
  }

  static byte[] repeatedDescription(int type) throws Exception {
    ByteArrayOutput b = new ByteArrayOutput();
    b.writeCompactInt(0);
    b.writeUTFString("Objects");
    b.writeCompactInt(3);
    b.writeUTFString("Value");
    b.writeCompactInt(type);
    b.writeUTFString("Value");
    b.writeCompactInt(type);
    b.writeUTFString("Sentinel");
    b.writeCompactInt(8);
    return frame(2, bytes(b));
  }

  static byte[] payload(byte[] field, boolean sentinel) throws Exception {
    ByteArrayOutput b = new ByteArrayOutput();
    SymbolCodec.Writer s = PentaCodec.INSTANCE.createWriter();
    s.reset(ProtocolOption.EMPTY_SET);
    s.writeSymbol(b, PentaCodec.INSTANCE.encode("IBM"), null, 0);
    b.writeCompactInt(0);
    b.write(field);
    if (sentinel) {
      b.writeCompactInt(SENTINEL);
    }
    return bytes(b);
  }

  static void observe(int type, String id, byte[] body, boolean sentinel) throws Exception {
    for (boolean skip : new boolean[] {false, true}) {
      DataRecord r = record(type, skip);
      for (int family : new int[] {15, 5}) {
        final Trace[] trace = new Trace[1];
        BinaryQTPParser p =
            new BinaryQTPParser(r.getScheme()) {
              protected BinaryRecordDesc wrapRecordDesc(BinaryRecordDesc d) {
                return trace[0] = new Trace(d);
              }
            };
        byte[] wire = concat(description(type), frame(family, payload(body, sentinel)));
        p.setInput(new ByteArrayInput(wire));
        Seen seen = new Seen();
        p.parse(seen);
        row(
            "parser",
            type,
            id,
            skip ? "unknown-field" : "known",
            family,
            hex(body),
            seen.records,
            seen.errors,
            seen.observed,
            seen.sentinel,
            trace[0] == null ? -1 : trace[0].examined);
      }
      byte[] direct = sentinel ? concat(body, compact(SENTINEL)) : body;
      ByteArrayInput underlying = new ByteArrayInput(direct);
      underlying.mark();
      BufferedInputPart in = new BufferedInputPart(underlying, direct.length);
      String status = "OK", observed = "-";
      int sent = -1;
      try {
        RecordBuffer b = new RecordBuffer(RecordMode.DATA);
        RecordCursor c =
            new Requested(r, type).readRecord(in, b, PentaCodec.INSTANCE.encode("IBM"), null, 0);
        observed = skip ? "skipped" : value(c.getObj(0));
        sent = c.getInt(0);
      } catch (Exception e) {
        status = e.getClass().getSimpleName();
      }
      row(
          "FieldReader-message-part",
          type,
          id,
          skip ? "unknown-field" : "known",
          hex(body),
          status,
          observed,
          sent,
          in.totalPosition());
    }
    ByteArrayInput underlying = new ByteArrayInput(body);
    underlying.mark();
    BufferedInputPart in = new BufferedInputPart(underlying, body.length);
    String status = "OK", observed = "-";
    try {
      observed =
          value(
              type == 137
                  ? in.readUTFString()
                  : type == 249 ? in.readMarshalled(Marshaller.SERIALIZATION) : in.readByteArray());
    } catch (Exception e) {
      status = e.getClass().getSimpleName();
    }
    row("primitive-message-part", type, id, hex(body), status, observed, in.totalPosition());
    bindingParser(type, id, body, sentinel, false);
  }

  static void bindingParser(int type, String id, byte[] body, boolean sentinel, boolean repeated)
      throws Exception {
    DataScheme scheme =
        repeated ? record(type, false).getScheme() : new DefaultScheme(PentaCodec.INSTANCE);
    for (int family : new int[] {15, 5}) {
      final Trace[] trace = new Trace[1];
      BinaryQTPParser parser =
          new BinaryQTPParser(scheme) {
            protected BinaryRecordDesc wrapRecordDesc(BinaryRecordDesc d) {
              return trace[0] = new Trace(d);
            }
          };
      byte[] fields = repeated ? concat(body, body) : body;
      byte[] wire =
          concat(
              repeated ? repeatedDescription(type) : description(type),
              frame(family, payload(fields, sentinel)));
      parser.setInput(new ByteArrayInput(wire));
      Seen seen = new Seen();
      parser.parse(seen);
      row(
          "binding-parser",
          type,
          id,
          repeated ? "repeated-target" : "unknown-record",
          family,
          hex(fields),
          seen.records,
          seen.errors,
          seen.observed,
          seen.sentinel,
          trace[0] == null ? -1 : trace[0].examined);
    }
  }

  static void composed(int type, String id, Object value) throws Exception {
    final DataRecord r = record(type, false);
    final Requested requested = new Requested(r, type);
    for (MessageType family : new MessageType[] {MessageType.STREAM_DATA, MessageType.RAW_DATA}) {
      String suffix = family == MessageType.RAW_DATA ? "-raw" : "";
      BinaryQTPComposer c =
          new BinaryQTPComposer(r.getScheme(), true) {
            protected BinaryRecordDesc getRequestedRecordDesc(DataRecord ignored) {
              return requested;
            }
          };
      ByteArrayOutput encoded = new ByteArrayOutput();
      c.setOptSet(ProtocolOption.EMPTY_SET);
      c.setOutput(encoded);
      RecordBuffer b = new RecordBuffer(RecordMode.TIMESTAMPED_DATA);
      RecordCursor cursor = b.add(r, PentaCodec.INSTANCE.encode("IBM"), null);
      cursor.setObj(0, value);
      cursor.setInt(0, SENTINEL);
      cursor.setEventTimeSequence((1600000000L << 32) + (123L << 22));
      c.visitData(b, family);
      byte[] wire = bytes(encoded);
      Files.write(root.resolve("composer-" + type + "-" + id + suffix + ".qtp"), wire);
      BinaryQTPParser parser = new BinaryQTPParser(r.getScheme());
      parser.setInput(new ByteArrayInput(wire));
      Seen seen = new Seen();
      parser.parse(seen);
      if (seen.records != (family == MessageType.RAW_DATA ? 3 : 1)
          || seen.errors != 0
          || seen.sentinel != SENTINEL) {
        throw new IllegalStateException("requested object composer callback");
      }
      row(
          "composer",
          type,
          id,
          family.getId(),
          r.getObjField(0).getSerialType().getId(),
          hex(wire),
          seen.records,
          seen.errors,
          seen.observed,
          seen.sentinel);
      FileWriter writer = new FileWriter();
      writer.setFormat(FileFormat.BINARY);
      writer.setCompression(StreamCompression.NONE);
      writer.setTime(family == MessageType.RAW_DATA ? TimestampsType.MESSAGE : TimestampsType.NONE);
      writer.setSaveAs(family);
      writer.setOpt("");
      Path path = root.resolve("file-" + type + "-" + id + suffix + ".qtp");
      try (FileWriter opened = writer.open(path.toString(), r.getScheme())) {
        b.rewind();
        opened.visitData(b, MessageType.STREAM_DATA);
      }
      FileReaderParams.Default params = new FileReaderParams.Default();
      params.setSpeed(FileReaderParams.MAX_SPEED);
      params.setCycle(false);
      FileReader reader = new FileReader(path.toString(), new ConnectionStats(), params);
      reader.setScheme(r.getScheme());
      Seen delivered = new Seen();
      try {
        reader.readInto(delivered);
      } finally {
        reader.close();
      }
      if (delivered.records != (family == MessageType.RAW_DATA ? 3 : 1)
          || delivered.errors != 0
          || delivered.sentinel != SENTINEL) {
        throw new IllegalStateException("object FileWriter/FileReader callback");
      }
      row(
          "FileWriter-FileReader",
          type,
          id,
          family.getId(),
          "actual-local-type",
          r.getObjField(0).getSerialType().getId(),
          delivered.records,
          delivered.errors,
          delivered.observed,
          delivered.sentinel);
    }
  }

  static void capture(Path directory) throws Exception {
    root = directory;
    Files.createDirectories(root);
    out = new PrintWriter(Files.newBufferedWriter(root.resolve("samples.tsv")));
    try {
      row("case", "object-samples");
      for (int t : new int[] {9, 137, 233, 249}) {
        composed(t, "null", null);
        composed(
            t, "empty", t == 137 ? "" : t == 249 ? Marshalled.forBytes(new byte[0]) : new byte[0]);
        composed(
            t,
            "content",
            t == 137
                ? "A\u0000\uD83D\uDE00"
                : t == 249 ? Marshalled.forBytes(hex("aced00057f")) : hex("007fff80"));
        observe(t, "null", compact(-1), true);
        observe(t, "empty", compact(0), true);
        bindingParser(t, "null", compact(-1), true, true);
        bindingParser(t, "empty", compact(0), true, true);
        bindingParser(t, "content", concat(compact(3), hex("414243")), true, true);
        observe(t, "content", concat(compact(3), hex("0041ff")), true);
        observe(t, "negative2", compact(-2), true);
        observe(t, "high32zero", compact(1L << 32), true);
        observe(t, "high32null", compact(0xffffffffL), true);
        observe(t, "high32one", concat(compact((1L << 32) + 1), hex("41")), true);
        observe(t, "overflow32", compact(1L << 31), true);
        observe(t, "overlong-one", hex("800141"), true);
        observe(t, "truncated-length", hex("f0"), false);
        observe(t, "truncated-body", hex("0341"), false);
        byte[] large = new byte[4096];
        Arrays.fill(large, (byte) 65);
        observe(t, "bounded-large", concat(compact(large.length), large), true);
        if (t == 249) {
          observe(t, "known-string-bytes", concat(compact(8), hex("aced000574000141")), true);
          observe(t, "unknown-opaque-tag", concat(compact(5), hex("aced00057f")), true);
        }
        byte[] finite = concat(compact(3), hex("414243"));
        for (int n = 0; n < finite.length; n++) {
          observe(t, "body-prefix-" + n, Arrays.copyOf(finite, n), false);
        }
        if (t == 137) {
          String[][] cases = {
            {"nul", "0100"},
            {"supplementary", "04f09f9880"},
            {"cesu-pair", "06eda0bdedb880"},
            {"surrogate", "03eda080"},
            {"overlong2", "02c080"},
            {"overlong3", "03e08080"},
            {"overlong4", "04f0808080"},
            {"bad-tail", "02c241"},
            {"bad-head", "0180"},
            {"too-large", "04f4908080"},
            {"split", "01c280"},
            {"split4", "01f0908080"},
            {"bad-tail4", "04f0418080"},
            {"truncated-utf", "02c2"}
          };
          for (String[] x : cases) {
            observe(t, x[0], hex(x[1]), true);
          }
          observe(t, "utf-tail-exhaustion", hex("01c2"), false);
        }
      }
      row("complete", "object-samples");
      if (out.checkError()) {
        throw new IOException("object samples output");
      }
    } finally {
      out.close();
    }
  }
}
