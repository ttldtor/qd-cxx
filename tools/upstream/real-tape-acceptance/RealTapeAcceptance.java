package qdcxx.oracle;

import com.devexperts.io.BufferedInput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.SymbolCodec;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.AbstractQTPParser;
import com.devexperts.qd.qtp.BinaryRecordDesc;
import com.devexperts.qd.qtp.HeartbeatPayload;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.file.BinaryFileQTPParser;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.qd.util.TimeSequenceUtil;
import com.devexperts.transport.stats.ConnectionStats;
import java.io.BufferedReader;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Compares ordinary wire records with the unmodified locked production parser. */
public final class RealTapeAcceptance {
  private static final String HELP =
      "RealTapeAcceptance --input PATH --cpp-output PATH --tape-mode legacy|raw-message-v1\n"
          + "  [--max-input N] [--max-records N] [--max-output N]\n"
          + "  [--cpp-projection ordinary-v1|binary-catalog-v1|self-contained-segments-v1]\n"
          + "  (default ordinary-v1; source profile: one uncompressed MESSAGE ordinary source)\n"
          + "  [--inject-late-io] [--inject-close-io]\n"
          + "Read-only two-pass Java production oracle, then literal-file cross-check.\n"
          + "Supported full types: 2/8/10/40/56/72/88/120, ordinary schemas, zero flags.\n"
          + "C++ projection selection never infers Java NONE/MESSAGE source timing.\n"
          + "Version 8 accepts only that ordinary subset, absent bytes/removal and canonical"
          + " timing.\n"
          + "Defaults: input 8388608 bytes, records 1000000, C++ output 268435456 bytes.\n"
          + "Hard maxima: input 67108864, records 10000000, output 1073741824; row 4194304 bytes.\n"
          + "External launcher must bound child heap/time. Output contains aggregate results"
          + " only.\n"
          + "--inject-late-io rejects a read error after all source bytes, before EOF.\n"
          + "--inject-close-io rejects an error after the input handle is actually closed.\n"
          + "No args/--help: identical help, no input/output I/O.\n"
          + "Exit 0: full equality and EOF; 1: rejected/setup/I/O/resource/comparison failure.\n";
  private static final int MAX_ROW = 4194304;
  private static final int MAX_DESCRIPTIONS = 4096;
  private static final int MAX_FIELDS = 65536;
  private static final int MAX_NAMES = 1048576;
  private static final char[] HEX = "0123456789abcdef".toCharArray();

  private final Path input;
  private final Path cpp;
  private final boolean timed;
  private final boolean binaryProjection;
  private final boolean sourceProjection;
  private final boolean injectLateIo;
  private final boolean injectCloseIo;
  private final long maxInput;
  private final long maxRecords;
  private final long maxOutput;
  private final List<Description> discovered = new ArrayList<>();
  private String discoveryName;
  private int discoveryFields;
  private int discoveryNames;

  private RealTapeAcceptance(Map<String, String> args) {
    input = Paths.get(required(args, "--input"));
    cpp = Paths.get(required(args, "--cpp-output"));
    String mode = required(args, "--tape-mode");
    require(mode.equals("legacy") || mode.equals("raw-message-v1"), "tape-mode");
    timed = mode.equals("raw-message-v1");
    String projection =
        args.containsKey("--cpp-projection") ? args.get("--cpp-projection") : "ordinary-v1";
    require(
        projection.equals("ordinary-v1")
            || projection.equals("binary-catalog-v1")
            || projection.equals("self-contained-segments-v1"),
        "cpp-projection");
    sourceProjection = projection.equals("self-contained-segments-v1");
    require(!sourceProjection || timed, "v9-message-only");
    binaryProjection = projection.equals("binary-catalog-v1") || sourceProjection;
    injectLateIo = args.containsKey("--inject-late-io");
    injectCloseIo = args.containsKey("--inject-close-io");
    maxInput = limit(args, "--max-input", 8388608, 67108864);
    maxRecords = limit(args, "--max-records", 1000000, 10000000);
    maxOutput = limit(args, "--max-output", 268435456, 1073741824);
  }

  private static String required(Map<String, String> args, String key) {
    String value = args.get(key);
    require(value != null && !value.isEmpty(), "required-option");
    return value;
  }

  private static long limit(Map<String, String> args, String key, long fallback, long maximum) {
    if (!args.containsKey(key)) {
      return fallback;
    }
    String text = args.get(key);
    require(text.matches("[0-9]+"), "unsigned-limit");
    long value = Long.parseLong(text);
    require(value > 0 && value <= maximum, "limit-range");
    return value;
  }

  private static void require(boolean condition, String reason) {
    if (!condition) {
      throw new IllegalStateException(reason);
    }
  }

  private static SerialFieldType serial(int id) {
    switch (id) {
      case 2:
        return SerialFieldType.UTF_CHAR;
      case 8:
        return SerialFieldType.COMPACT_INT;
      case 10:
        return SerialFieldType.UTF_CHAR_ARRAY;
      case 40:
        return SerialFieldType.SHORT_STRING;
      case 56:
        return SerialFieldType.TIME_SECONDS;
      case 72:
        return SerialFieldType.SEQUENCE;
      case 88:
        return SerialFieldType.DATE;
      case 120:
        return SerialFieldType.WIDE_DECIMAL;
      default:
        throw new IllegalStateException("unsupported-full-type-" + id);
    }
  }

  private static String units(String text) {
    if (text.isEmpty()) {
      return "-";
    }
    require(text.length() <= MAX_ROW / 4, "units-budget");
    StringBuilder out = new StringBuilder(text.length() * 4);
    for (int i = 0; i < text.length(); i++) {
      char c = text.charAt(i);
      for (int shift = 12; shift >= 0; shift -= 4) {
        out.append(HEX[(c >>> shift) & 15]);
      }
    }
    return out.toString();
  }

  private static String hash(MessageDigest digest) {
    StringBuilder result = new StringBuilder(64);
    for (byte b : digest.digest()) {
      result.append(HEX[(b >>> 4) & 15]).append(HEX[b & 15]);
    }
    return result.toString();
  }

  private static final class Field {
    final String name;
    final int type;

    Field(String name, int type) {
      this.name = name;
      this.type = type;
    }
  }

  private static final class Description {
    final int sender;
    final String name;
    final List<Field> fields;

    Description(int sender, String name, String[] names, int[] types) {
      this.sender = sender;
      this.name = name;
      fields = new ArrayList<>();
      for (int i = 0; i < names.length; i++) {
        serial(types[i]);
        require(
            !Arrays.asList("EventSymbol", "EventFlags", "EventTime", "EventSequence")
                .contains(names[i]),
            "unsupported-event-field");
        fields.add(new Field(names[i], types[i]));
      }
    }
  }

  private static final class LocalRecord {
    final int id;
    final String name;
    final LinkedHashMap<String, Field> byName = new LinkedHashMap<>();
    final List<Field> fields = new ArrayList<>();

    LocalRecord(int id, String name) {
      this.id = id;
      this.name = name;
    }

    void append(Description description) {
      for (Field field : description.fields) {
        Field prior = byName.get(field.name);
        if (prior == null) {
          byName.put(field.name, field);
          fields.add(field);
        } else {
          require(prior.type == field.type, "field-type-change");
        }
      }
    }

    int fieldId(String name) {
      for (int i = 0; i < fields.size(); i++) {
        if (fields.get(i).name.equals(name)) {
          return i;
        }
      }
      throw new IllegalStateException("missing-local-field");
    }
  }

  private static final class Schema {
    final LinkedHashMap<String, LocalRecord> records = new LinkedHashMap<>();
    final LinkedHashMap<Integer, Description> descriptions = new LinkedHashMap<>();

    LocalRecord add(Description description, boolean allowReplacement) {
      require(
          allowReplacement || !descriptions.containsKey(description.sender),
          "unsupported-description-replacement");
      descriptions.put(description.sender, description);
      LocalRecord record = records.get(description.name);
      if (record == null) {
        record = new LocalRecord(records.size(), description.name);
        records.put(description.name, record);
      }
      record.append(description);
      return record;
    }

    DataScheme owned() {
      List<DataRecord> result = new ArrayList<>();
      for (LocalRecord record : records.values()) {
        List<DataIntField> ints = new ArrayList<>();
        List<DataObjField> objs = new ArrayList<>();
        for (Field field : record.fields) {
          SerialFieldType type = serial(field.type);
          String name = record.name + "." + field.name;
          if (type.isObject()) {
            objs.add(type.createDefaultObjInstance(objs.size(), name));
          } else {
            ints.add(type.createDefaultIntInstance(ints.size(), name));
            if (type.isLong()) {
              ints.add(new VoidIntField(ints.size(), name + "$VoidTail"));
            }
          }
        }
        result.add(
            new DefaultRecord(
                result.size(),
                record.name,
                false,
                ints.toArray(new DataIntField[0]),
                objs.toArray(new DataObjField[0])));
      }
      return new DefaultScheme(PentaCodec.INSTANCE, result.toArray(new DataRecord[0]));
    }
  }

  private final class EmptyScheme implements DataScheme {
    private final DataScheme delegate = new DefaultScheme(PentaCodec.INSTANCE);

    public SymbolCodec getCodec() {
      return delegate.getCodec();
    }

    public int getRecordCount() {
      return 0;
    }

    public DataRecord getRecord(int index) {
      return delegate.getRecord(index);
    }

    public DataRecord findRecordByName(String name) {
      discoveryName = name;
      return null;
    }

    public DataIntField findIntFieldByName(String name) {
      return null;
    }

    public DataObjField findObjFieldByName(String name) {
      return null;
    }

    public <T> T getService(Class<T> type) {
      return delegate.getService(type);
    }
  }

  /** Fixed single input owned by a child-JVM-local URL connection, never a network endpoint. */
  private final class Source {
    long bytes;
    int opens;
    int closes;
    int errors;
    boolean eof;

    InputStream open() throws IOException {
      require(++opens == 1, "source-open-budget");
      return new FilterInputStream(Files.newInputStream(input)) {
        public int read(byte[] data, int offset, int length) throws IOException {
          try {
            if (injectLateIo && bytes == Files.size(input)) {
              throw new IOException("injected-late-io");
            }
            int count = in.read(data, offset, length);
            observe(count);
            return count;
          } catch (IOException failure) {
            errors++;
            throw failure;
          }
        }

        public int read() throws IOException {
          try {
            if (injectLateIo && bytes == Files.size(input)) {
              throw new IOException("injected-late-io");
            }
            int value = in.read();
            observe(value < 0 ? -1 : 1);
            return value;
          } catch (IOException failure) {
            errors++;
            throw failure;
          }
        }

        public void close() throws IOException {
          try {
            in.close();
            closes++;
            if (injectCloseIo) {
              throw new IOException("injected-close-io");
            }
          } catch (IOException failure) {
            errors++;
            throw failure;
          }
        }
      };
    }

    void observe(int count) throws IOException {
      if (count < 0) {
        eof = true;
      } else {
        bytes += count;
        if (bytes > maxInput) {
          errors++;
          throw new IOException("source-byte-budget");
        }
      }
    }

    void complete() throws IOException {
      require(
          opens == 1 && closes == 1 && eof && errors == 0 && bytes == Files.size(input),
          "source-incomplete");
    }
  }

  private Source activeSource;

  private void installSource() {
    URL.setURLStreamHandlerFactory(
        protocol ->
            !protocol.equals("qdcxx-accept")
                ? null
                : new URLStreamHandler() {
                  protected URLConnection openConnection(URL url) {
                    return new URLConnection(url) {
                      public void connect() {}

                      public InputStream getInputStream() throws IOException {
                        return activeSource.open();
                      }
                    };
                  }
                });
  }

  /** Bounded ASCII TSV reader; source-dependent mismatch text is deliberately not reported. */
  private static final class Projection implements AutoCloseable {
    final BufferedReader reader;
    final boolean metadata;
    final MessageDigest digest;
    final boolean binary;
    final boolean timed;
    final boolean source;
    final Path input;
    boolean identity;
    boolean started;
    boolean policy;
    boolean sourceEnded;
    long sourceRecords;
    String sourceComplete;
    final Set<String> resources = new HashSet<>();
    final Map<String, String> resourceObservations = new HashMap<>();
    final MessageDigest sourceTimingDigest;
    private static final Set<String> EXPECTED_RESOURCES = expectedResources();

    private static Set<String> expectedResources() {
      Set<String> result = new HashSet<>();
      for (String name :
          ("physicalFetched compressedConsumed logicalProduced headerBytes workBytes "
                  + "inflateCalls physicalProbes inflatedProbes")
              .split(" ")) {
        result.add("data:" + name);
      }
      for (String name :
          ("parserAccepted frames markers emittedRecords processedRecords wireFields").split(" ")) {
        result.add("parser:" + name);
      }
      for (String name :
          ("path-native-units path-encoded-bytes path-storage-bytes occurrences source-handles"
                  + " diagnostic-bytes terminal-bytes output-charged-bytes closed-sources"
                  + " accepted-bytes frames emitted-records processed-records wire-fields")
              .split(" ")) {
        result.add("sequence:" + name);
      }
      for (String name :
          ("frame-maximumBodyBytes frame-maximumPendingBytes inputBytes frames"
               + " descriptor-properties descriptor-messages descriptor-text-bodyBytes"
               + " descriptor-text-codeUnits descriptor-textUnits context-schema-records"
               + " context-schema-fields context-schema-lookups context-schema-nameUnits"
               + " context-schema-totalNameUnits context-descriptions context-bindings-fields"
               + " context-bindings-nameUnits context-bindings-totalNameUnits constructionUnits"
               + " data-payloadBytes data-processedRecords data-wireFields data-emittedRecords"
               + " data-emittedValues data-ownedSymbolUnits data-symbol-inputBytes"
               + " data-symbol-codeUnits data-symbol-stateUnits data-fieldString-encodedBytes"
               + " data-fieldString-codeUnits data-fieldBytes data-fieldUnits data-ownedFieldUnits"
               + " data-fieldPayloadEncodedBytes data-fieldPayloadBytes data-payloadEncodedBytes"
               + " data-payloadWorkBytes data-ownedPayloadBytes data-ownedFieldTimeRecords"
               + " timingEvents snapshotUnits")
              .split(" ")) {
        result.add("sequence:tape-" + name);
      }
      for (String name :
          ("compressedBytes inflatedBytes expansionRatio ratioSlack gzipMembers zipEntries"
               + " headerBytes totalHeaderBytes workBytes inflateCalls retainedBytes inflaterBytes"
               + " chunkBytes")
              .split(" ")) {
        result.add("sequence:dataContainer-" + name);
        result.add("sequence:companionContainer-" + name);
      }
      for (String name : "inputBytes lineBytes lines marks workBytes retainedBytes".split(" ")) {
        result.add("sequence:scanner-" + name);
      }
      for (String name :
          "chunkBytes outputBytes eventUnits physicalProbes inflatedProbes".split(" ")) {
        result.add("sequence:" + name);
      }
      return result;
    }

    long row;
    long frames;
    long visibleGeneration;
    Long pendingGeneration;
    String complete;
    boolean ended;

    Projection(
        Path path,
        boolean metadata,
        String mode,
        boolean binary,
        boolean timed,
        boolean source,
        Path input)
        throws Exception {
      reader =
          new BufferedReader(
              new InputStreamReader(Files.newInputStream(path), StandardCharsets.UTF_8));
      this.metadata = metadata;
      this.binary = binary;
      this.timed = timed;
      this.source = source;
      this.input = input;
      digest = MessageDigest.getInstance("SHA-256");
      sourceTimingDigest = MessageDigest.getInstance("SHA-256");
      try {
        String header =
            source
                ? "format\tqdcxx-tape-inspect\t9\tbinary-catalog-v1\tself-contained-segments-v1"
                : binary
                    ? "format\tqdcxx-tape-inspect\t8\tbinary-catalog-v1\tbinary-catalog-v1"
                    : "format\tqdcxx-tape-inspect\t7\tordinary-v1\t" + mode;
        require(header.equals(line()), "cpp-format");
      } catch (Throwable failure) {
        reader.close();
        throw failure;
      }
    }

    private static long decimal(String text) {
      require(text.matches("0|-?[1-9][0-9]*"), "v8-decimal");
      try {
        return Long.parseLong(text);
      } catch (NumberFormatException failure) {
        throw new IllegalStateException("v8-long-range");
      }
    }

    private static void hexUnits(String text, boolean nullable) {
      require(text.length() <= MAX_NAMES * 4, "v8-text-budget");
      if (nullable && (text.equals("null") || text.equals("empty"))
          || !nullable && text.equals("-")) {
        return;
      }
      require(!text.isEmpty() && text.length() % 4 == 0, "v8-utf16");
      for (int i = 0; i < text.length(); i++) {
        char c = text.charAt(i);
        require(c >= '0' && c <= '9' || c >= 'a' && c <= 'f', "v8-utf16");
      }
    }

    private static void presence(String bit, String text) {
      require(bit.equals("0") || bit.equals("1"), "v8-presence");
      hexUnits(text, false);
      require(!bit.equals("0") || text.equals("-"), "v8-absent-text");
    }

    private void descriptor(String text) {
      String[] cells = text.split("\t", -1);
      require(
          cells.length == 9
              && frames > 0
              && pendingGeneration == null
              && decimal(cells[1]) == frames - 1,
          "v8-descriptor-context");
      presence(cells[2], cells[3]);
      presence(cells[4], cells[5]);
      presence(cells[6], cells[7]);
      long mask = decimal(cells[8]);
      long known = (1L << 5) | (1L << 10) | (1L << 15) | (1L << 20);
      require(mask >= 0 && (mask & ~known) == 0, "v8-send-mask");
      if (source) {
        require(
            cells[6].equals("1")
                && (cells[7].equals(units("message")) || cells[7].equals(units("MESSAGE"))),
            "v9-physical-time-property");
      }
    }

    /** Validate the full lossless row before forming the intentionally narrow comparison view. */
    private String ordinary(String text) {
      String[] cells = text.split("\t", -1);
      require(cells.length == 14, "v8-data-columns");
      for (int i = 1; i <= 5; i++) {
        require(decimal(cells[i]) >= 0, "v8-source-identity");
      }
      hexUnits(cells[6], false);
      hexUnits(cells[7], false);
      require(cells[12].equals("0"), "v8-event-flags");
      require(cells[13].equals("-"), "v8-removal-time");
      StringBuilder values = new StringBuilder();
      if (!cells[8].isEmpty()) {
        int count = 1;
        for (int i = 0; i < cells[8].length(); i++) {
          if (cells[8].charAt(i) == ',') {
            count++;
          }
        }
        require(count <= MAX_FIELDS, "v8-value-budget");
        for (String token : cells[8].split(",", -1)) {
          String[] value = token.split(":", -1);
          require(value.length == 4, "v8-value-columns");
          long id = decimal(value[0]);
          require(id >= 0 && id <= Integer.MAX_VALUE, "v8-full-type");
          serial((int) id);
          long raw = decimal(value[1]);
          require(value[3].equals("-"), "v8-bytes-payload");
          if (id == 10) {
            require(value[1].equals("0"), "v8-text-raw");
            hexUnits(value[2], true);
          } else {
            require(value[2].equals("-"), "v8-scalar-payload");
            if (id == 2) {
              require(raw >= 0 && raw <= 0x10ffff, "v8-char-range");
            } else if (id != 120) {
              require(raw >= Integer.MIN_VALUE && raw <= Integer.MAX_VALUE, "v8-int-range");
            }
          }
          if (values.length() != 0) {
            values.append(',');
          }
          values.append(value[0]).append(':').append(value[1]).append(':').append(value[2]);
        }
      }
      if (!cells[9].equals("-")) {
        decimal(cells[9]);
      }
      decimal(cells[10]);
      decimal(cells[11]);
      if (!timed) {
        require(
            cells[9].equals("-") && cells[10].equals("0") && cells[11].equals("0"),
            "v8-none-timing");
      }
      StringBuilder result = new StringBuilder("wire-data");
      for (int i = 1; i <= 7; i++) {
        result.append('\t').append(cells[i]);
      }
      result.append('\t').append(values);
      if (timed) {
        for (int i = 9; i <= 11; i++) {
          result.append('\t').append(cells[i]);
        }
      }
      return result.toString();
    }

    String line() throws IOException {
      StringBuilder text = new StringBuilder();
      for (; ; ) {
        int c = reader.read();
        if (c < 0) {
          require(text.length() == 0, "cpp-unterminated-row");
          return null;
        }
        if (c == '\n') {
          return text.toString();
        }
        require(c >= 32 && c < 127 || c == '\t', "cpp-non-ascii-row");
        require(text.length() < MAX_ROW, "cpp-row-budget");
        text.append((char) c);
      }
    }

    /**
     * Narrow original-corpus admission; broader catalog/time families remain explicit exclusions.
     */
    private String sourceData(String text) {
      String[] c = text.split("\t", -1);
      require(c.length == 27 && policy && !sourceEnded, "v9-data-context");
      require(
          c[1].equals("0") && decimal(c[3]) == sourceRecords && decimal(c[4]) == sourceRecords,
          "v9-record-order");
      require(c[16].equals("0"), "v9-field-not-in-original-profile");
      for (int i = 17; i <= 21; i++) {
        require(c[i].equals("-"), "v9-field-absent");
      }
      require(
          c[22].equals("message") && !c[11].equals("-") && c[23].equals("message"),
          "v9-message-assignment");
      long packed = decimal(c[12]);
      require(
          c[24].equals(packed == 0 ? "zero" : "external")
              && c[25].equals(c[12])
              && c[26].equals(c[13]),
          "v9-effective-time");
      StringBuilder legacy = new StringBuilder("wire-data-v8");
      for (int index : new int[] {2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}) {
        legacy.append('\t').append(c[index]);
      }
      String result = ordinary(legacy.toString());
      if (!metadata) {
        StringBuilder timing = new StringBuilder("source-time");
        for (int index :
            new int[] {1, 3, 4, 11, 12, 13, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26}) {
          timing.append('\t').append(c[index]);
        }
        sourceTimingDigest.update((timing + "\n").getBytes(StandardCharsets.US_ASCII));
      }
      sourceRecords++;
      return result;
    }

    /** Validate occurrence identity without printing private native path bytes. */
    private void identity(String text) {
      String[] c = text.split("\t", -1);
      require(
          c.length == 5 && c[1].equals("0") && c[4].equals("-") && !identity && !started,
          "v9-source-identity");
      byte[] bytes;
      if (c[2].equals("utf16le")) {
        bytes = input.toString().getBytes(StandardCharsets.UTF_16LE);
      } else {
        require(c[2].equals("bytes"), "v9-path-encoding");
        bytes = input.toString().getBytes(StandardCharsets.UTF_8);
      }
      StringBuilder expected = new StringBuilder();
      for (byte b : bytes) {
        expected.append(HEX[(b >>> 4) & 15]).append(HEX[b & 15]);
      }
      require(c[3].equals(expected.toString()), "v9-native-path");
      identity = true;
    }

    private void resource(String text) {
      String[] c = text.split("\t", -1);
      require(c.length == 6 && policy, "v9-resource-columns");
      boolean sequence = c[1].equals("sequence");
      require(
          sequence
              ? sourceEnded && c[2].equals("-")
              : !sourceEnded && (c[1].equals("data") || c[1].equals("parser")) && c[2].equals("0"),
          "v9-resource-context");
      require(
          c[3].matches("[A-Za-z][A-Za-z0-9-]*") && resources.add(c[1] + ":" + c[3]),
          "v9-resource-identity");
      long reserved = decimal(c[4]);
      require(reserved >= 0, "v9-resource-reserved");
      if (!c[5].equals("-")) {
        long observed = decimal(c[5]);
        require(observed >= 0 && observed <= reserved, "v9-resource-observed");
      }
      require(EXPECTED_RESOURCES.contains(c[1] + ":" + c[3]), "v9-unknown-resource");
      resourceObservations.put(c[1] + ":" + c[3], c[5]);
    }

    private void sourceEnd(String text) throws IOException {
      String[] c = text.split("\t", -1);
      require(
          c.length == 9
              && policy
              && !sourceEnded
              && pendingGeneration == null
              && c[1].equals("0")
              && decimal(c[2]) == Files.size(input)
              && decimal(c[3]) == frames
              && decimal(c[4]) == sourceRecords
              && c[5].equals(c[4])
              && c[7].equals(c[2])
              && c[8].equals(c[2]),
          "v9-source-end");
      require(decimal(c[6]) >= 0, "v9-source-fields");
      sourceComplete = "complete";
      for (int i = 2; i <= 6; i++) {
        sourceComplete += "\t" + c[i];
      }
      sourceEnded = true;
    }

    String next() throws IOException {
      String text;
      while ((text = line()) != null) {
        require(!ended, "cpp-extra-after-complete");
        if (source) {
          if (text.startsWith("source\t")) {
            identity(text);
            continue;
          }
          if (text.equals("source-begin\t0")) {
            require(identity && !started, "v9-source-begin");
            started = true;
            require("source-policy\tmessage\tnone\t-".equals(line()), "v9-source-policy");
            policy = true;
            continue;
          }
          require(policy, "v9-before-source");
          if (text.startsWith("resource\t")) {
            resource(text);
            continue;
          }
          if (text.startsWith("source-end\t")) {
            sourceEnd(text);
            continue;
          }
          if (text.startsWith("complete-v9\t")) {
            String[] c = text.split("\t", -1);
            require(
                c.length == 7 && c[1].equals("1") && sourceEnded && resources.size() > 0,
                "v9-complete-context");
            String summary = "complete";
            for (int i = 2; i <= 6; i++) {
              summary += "\t" + c[i];
            }
            require(summary.equals(sourceComplete), "v9-complete-counts");
            require(resources.equals(EXPECTED_RESOURCES), "v9-resource-inventory");
            for (String key :
                new String[] {
                  "data:physicalFetched",
                  "data:compressedConsumed",
                  "data:logicalProduced",
                  "parser:parserAccepted",
                  "sequence:accepted-bytes"
                }) {
              require(c[2].equals(resourceObservations.get(key)), "v9-resource-byte-count");
            }
            for (String[] pair :
                new String[][] {
                  {"parser:frames", "3"},
                  {"sequence:frames", "3"},
                  {"parser:emittedRecords", "4"},
                  {"sequence:emitted-records", "4"},
                  {"parser:processedRecords", "5"},
                  {"sequence:processed-records", "5"},
                  {"parser:wireFields", "6"},
                  {"sequence:wire-fields", "6"}
                }) {
              require(
                  c[Integer.parseInt(pair[1])].equals(resourceObservations.get(pair[0])),
                  "v9-resource-parser-count");
            }
            for (String key :
                new String[] {
                  "sequence:occurrences", "sequence:closed-sources", "sequence:source-handles"
                }) {
              require("1".equals(resourceObservations.get(key)), "v9-resource-source-count");
            }
            text = summary;
          } else {
            require(!sourceEnded, "v9-row-after-source-end");
            require(
                !text.startsWith("wire-data-v8\t")
                    && !text.startsWith("wire-data\t")
                    && !text.startsWith("complete\t"),
                "v9-old-row");
            if (text.startsWith("wire-data-v9\t")) {
              text = sourceData(text);
            }
          }
        }
        if (binary) {
          require(
              !(text.startsWith("wire-data\t") && !source)
                  && !text.startsWith("descriptor\t")
                  && !text.startsWith("descriptor-v3\t"),
              "v8-old-row");
          if (text.startsWith("wire-data-v8\t")) {
            text = ordinary(text);
          } else if (text.startsWith("descriptor-v8\t")) {
            descriptor(text);
            continue;
          }
        }
        if (text.startsWith("complete\t")) {
          require(complete == null, "cpp-duplicate-complete");
          require(pendingGeneration == null, "cpp-unclosed-schema");
          String[] cells = text.split("\t", -1);
          require(cells.length == 6 && Long.parseLong(cells[2]) == frames, "cpp-frame-counts");
          complete = text;
          ended = true;
          continue;
        }
        boolean isMetadata =
            text.startsWith("schema\t")
                || text.startsWith("schema-record\t")
                || text.startsWith("schema-field\t")
                || text.startsWith("schema-end\t")
                || text.startsWith("wire-record\t")
                || text.startsWith("wire-field\t");
        boolean isData = text.startsWith("wire-data\t");
        if (text.startsWith("frame\t")) {
          String[] cells = text.split("\t", -1);
          require(
              cells.length == 7
                  && Long.parseLong(cells[1]) == frames++
                  && pendingGeneration == null,
              "cpp-frame-order");
        } else if (text.startsWith("schema\t")) {
          String[] cells = text.split("\t", -1);
          require(
              cells.length == 5
                  && Long.parseLong(cells[1]) == frames - 1
                  && pendingGeneration == null,
              "cpp-schema-frame");
          pendingGeneration = Long.parseLong(cells[2]);
          require(pendingGeneration == visibleGeneration + 1, "cpp-schema-generation");
        } else if (text.startsWith("schema-end\t")) {
          String[] cells = text.split("\t", -1);
          require(
              cells.length == 2
                  && pendingGeneration != null
                  && Long.parseLong(cells[1]) == pendingGeneration,
              "cpp-schema-end");
          visibleGeneration = pendingGeneration;
          pendingGeneration = null;
        } else if (isData) {
          String[] cells = text.split("\t", -1);
          require(
              (cells.length == 9 || cells.length == 12)
                  && pendingGeneration == null
                  && Long.parseLong(cells[1]) == frames - 1
                  && Long.parseLong(cells[3]) == visibleGeneration,
              "cpp-data-context");
        }
        require(
            isMetadata
                || isData
                || text.startsWith("frame\t")
                || text.startsWith("descriptor\t")
                || text.startsWith("descriptor-v3\t")
                || text.startsWith("heartbeat\t")
                || text.startsWith("timing\t")
                || text.startsWith("markers\t"),
            "cpp-unknown-row");
        if (metadata ? isMetadata : isData) {
          return text;
        }
      }
      return null;
    }

    void compare(String expected) throws IOException {
      require(expected.length() <= MAX_ROW, "expected-row-budget");
      require(expected.equals(next()), (metadata ? "schema" : "record") + "-mismatch-row-" + row);
      digest.update(expected.getBytes(StandardCharsets.US_ASCII));
      digest.update((byte) '\n');
      row++;
    }

    void finish(String expectedComplete) throws IOException {
      require(next() == null, "cpp-extra-projection-row");
      require(expectedComplete.equals(complete), "cpp-completion-counts");
    }

    public void close() throws IOException {
      reader.close();
    }
  }

  private final class Run {
    final boolean discovery;
    final Schema schema = new Schema();
    final ArrayDeque<Capture> pending = new ArrayDeque<>();
    final Projection metadata;
    final Projection data;
    long frames;
    long generation;
    long wireRecords;
    long wireFields;
    long values;
    long streamRecords;
    long skips;
    int errors;
    boolean recordLimitExceeded;
    int descriptionsInFrame;
    Long millis;
    boolean sourceMessageDeclared;
    final MessageDigest expectedTimingDigest;

    Run(boolean discovery) throws Exception {
      this.discovery = discovery;
      expectedTimingDigest = MessageDigest.getInstance("SHA-256");
      String mode = timed ? "raw-message-v1" : "legacy";
      metadata =
          discovery
              ? null
              : new Projection(cpp, true, mode, binaryProjection, timed, sourceProjection, input);
      data =
          discovery
              ? null
              : new Projection(cpp, false, mode, binaryProjection, timed, sourceProjection, input);
    }

    void snapshot() throws IOException {
      generation++;
      if (discovery) {
        return;
      }
      metadata.compare(
          "schema\t"
              + frames
              + "\t"
              + generation
              + "\t"
              + schema.records.size()
              + "\t"
              + schema.descriptions.size());
      for (LocalRecord record : schema.records.values()) {
        metadata.compare(
            "schema-record\t"
                + generation
                + "\t"
                + record.id
                + "\t"
                + units(record.name)
                + "\t0\t"
                + record.fields.size());
        for (int i = 0; i < record.fields.size(); i++) {
          Field field = record.fields.get(i);
          metadata.compare(
              "schema-field\t"
                  + generation
                  + "\t"
                  + record.id
                  + "\t"
                  + i
                  + "\t"
                  + units(field.name)
                  + "\t"
                  + field.type);
        }
      }
      int index = 0;
      for (Description description : schema.descriptions.values()) {
        LocalRecord record = schema.records.get(description.name);
        metadata.compare(
            "wire-record\t"
                + generation
                + "\t"
                + index++
                + "\t"
                + description.sender
                + "\t"
                + record.id
                + "\t"
                + units(description.name)
                + "\t"
                + description.fields.size());
        for (int i = 0; i < description.fields.size(); i++) {
          Field field = description.fields.get(i);
          metadata.compare(
              "wire-field\t"
                  + generation
                  + "\t"
                  + description.sender
                  + "\t"
                  + i
                  + "\t"
                  + units(field.name)
                  + "\t"
                  + field.type
                  + "\t"
                  + record.fieldId(field.name));
        }
      }
      metadata.compare("schema-end\t" + generation);
    }

    String complete() throws IOException {
      require(!recordLimitExceeded, "wire-record-budget");
      require(errors == 0 && pending.isEmpty(), "parser-incomplete");
      require(
          discovery || wireRecords == streamRecords && wireRecords > 0 && skips == 0,
          "record-coverage");
      String row =
          "complete\t"
              + Files.size(input)
              + "\t"
              + frames
              + "\t"
              + streamRecords
              + "\t"
              + wireRecords
              + "\t"
              + wireFields;
      if (discovery) {
        return "discovery";
      }
      metadata.finish(row);
      data.finish(row);
      if (sourceProjection) {
        String measuredTiming = hash(data.sourceTimingDigest);
        require(measuredTiming.equals(hash(expectedTimingDigest)), "v9-timing-digest");
        System.out.println(
            "source-timing\trecords\t" + streamRecords + "\tsha256\t" + measuredTiming);
      }
      return hash(metadata.digest) + ":" + hash(data.digest);
    }

    void close() throws IOException {
      if (metadata != null) {
        try {
          metadata.close();
        } finally {
          data.close();
        }
      }
    }
  }

  /** Descriptor and scalars are owned; no borrowed cursor survives its callback. */
  private static final class Capture {
    final Description description;
    final LocalRecord record;
    final long frame;
    final long generation;
    final int fieldCount;

    Capture(Description description, LocalRecord record, long frame, long generation) {
      this.description = description;
      this.record = record;
      this.frame = frame;
      this.generation = generation;
      fieldCount = record.fields.size();
    }
  }

  private final class Descriptor extends BinaryRecordDesc {
    final Run run;
    final Description description;
    final LocalRecord local;

    Descriptor(BinaryRecordDesc source, int sender, Run run) {
      super(source);
      this.run = run;
      description =
          new Description(sender, run.discovery ? discoveryName : record.getName(), names, types);
      if (run.discovery) {
        require(discovered.size() < MAX_DESCRIPTIONS, "description-budget");
        discoveryFields += names.length;
        discoveryNames += description.name.length();
        for (String name : names) {
          discoveryNames += name.length();
        }
        require(discoveryFields <= MAX_FIELDS && discoveryNames <= MAX_NAMES, "schema-budget");
        discovered.add(description);
      } else {
        for (int d : descs) {
          int operation = d >> FLD_SHIFT;
          require(
              operation == FLD_INT || operation == FLD_LONG || operation == FLD_OBJ,
              "unsupported-conversion-or-event-field");
        }
      }
      local = run.schema.add(description, binaryProjection);
      run.descriptionsInFrame++;
    }

    public RecordCursor readRecord(
        BufferedInput in, RecordBuffer buffer, int cipher, String symbol, int eventFlags)
        throws IOException {
      require(!sourceProjection || run.sourceMessageDeclared, "v9-java-header-before-data");
      require(eventFlags == 0, "unsupported-event-flags");
      if (run.wireRecords == maxRecords) {
        run.recordLimitExceeded = true;
        throw new IllegalStateException("wire-record-budget");
      }
      RecordCursor cursor = super.readRecord(in, buffer, cipher, symbol, eventFlags);
      run.wireRecords++;
      run.wireFields += names.length;
      if (cursor == null) {
        run.skips++;
      } else {
        run.pending.add(new Capture(description, local, run.frames, run.generation));
      }
      return cursor;
    }
  }

  private final class Parser extends BinaryFileQTPParser {
    final Run run;

    Parser(DataScheme scheme, Run run) {
      super(scheme);
      this.run = run;
    }

    protected void remapRecord(int id, BinaryRecordDesc source) {
      super.remapRecord(id, new Descriptor(source, id, run));
    }

    protected void doAfterMessageBody(BufferedInput in, int type) {
      try {
        if (type == 2 && (!binaryProjection || run.descriptionsInFrame != 0)) {
          run.snapshot();
        }
        if (type == 2) {
          run.descriptionsInFrame = 0;
        }
        require(
            type == 0
                || type == 1
                || type == 2
                || type == (timed ? 5 : 15)
                || binaryProjection && (type == 5 || type == 15),
            "unsupported-message");
        run.frames++;
      } catch (IOException failure) {
        throw new IllegalStateException("projection-io", failure);
      }
    }
  }

  private final class Reader extends FileReader {
    final Run run;

    Reader(
        DataScheme scheme,
        Run run,
        ConnectionStats stats,
        FileReaderParams params,
        boolean literal) {
      super(literal ? input.toString() : "qdcxx-accept://input?single", stats, params);
      this.run = run;
      setScheme(scheme);
    }

    protected AbstractQTPParser createParser(FileFormat format, DataScheme scheme) {
      require(format == FileFormat.BINARY, "format-not-binary");
      return new Parser(scheme, run);
    }
  }

  private final class Observer extends MessageConsumerAdapter {
    final Run run;

    Observer(Run run) {
      this.run = run;
    }

    public void processDescribeProtocol(ProtocolDescriptor descriptor, boolean logDescriptor) {
      if (sourceProjection) {
        String time = descriptor.getProperty(ProtocolDescriptor.TIME_PROPERTY);
        require("message".equals(time) || "MESSAGE".equals(time), "v9-java-physical-time-property");
        run.sourceMessageDeclared = true;
      }
    }

    public void processHeartbeat(HeartbeatPayload heartbeat) {
      if (heartbeat.hasTimeMillis()) {
        run.millis = heartbeat.getTimeMillis();
      }
    }

    protected void processData(DataIterator iterator, MessageType type) {
      RecordCursor cursor;
      while ((cursor = ((RecordSource) iterator).next()) != null) {
        if (type != MessageType.STREAM_DATA) {
          continue;
        }
        require(!run.pending.isEmpty(), "source-id-queue-underflow");
        Capture capture = run.pending.remove();
        require(
            cursor.getRecord().getName().equals(capture.description.name), "callback-record-order");
        require(cursor.getEventFlags() == 0, "callback-event-flags");
        StringBuilder row =
            new StringBuilder("wire-data\t")
                .append(capture.frame)
                .append('\t')
                .append(run.streamRecords++)
                .append('\t')
                .append(capture.generation)
                .append('\t')
                .append(capture.description.sender)
                .append('\t')
                .append(capture.record.id)
                .append('\t')
                .append(units(capture.record.name))
                .append('\t')
                .append(units(cursor.getDecodedSymbol()))
                .append('\t');
        for (int i = 0; i < capture.fieldCount; i++) {
          Field field = capture.record.fields.get(i);
          DataField local = cursor.getRecord().findFieldByName(field.name);
          require(
              local != null && local.getSerialType().getId() == field.type, "raw-field-mapping");
          if (i != 0) {
            row.append(',');
          }
          row.append(field.type).append(':');
          if (local instanceof DataObjField) {
            Object value = cursor.getObj(local.getIndex());
            require(value == null || value instanceof String, "unsupported-object");
            String text = (String) value;
            row.append("0:").append(text == null ? "null" : text.isEmpty() ? "empty" : units(text));
          } else {
            long value =
                local.getSerialType().isLong()
                    ? cursor.getLong(local.getIndex())
                    : cursor.getInt(local.getIndex());
            row.append(value).append(":-");
          }
          run.values++;
          require(row.length() <= MAX_ROW, "expected-row-budget");
        }
        if (timed) {
          row.append('\t')
              .append(run.millis == null ? "-" : run.millis)
              .append('\t')
              .append(cursor.getEventTimeSequence())
              .append('\t')
              .append(
                  TimeSequenceUtil.getTimeMillisFromTimeSequence(cursor.getEventTimeSequence()));
        }
        if (sourceProjection) {
          long packed = cursor.getEventTimeSequence();
          long eventMillis = TimeSequenceUtil.getTimeMillisFromTimeSequence(packed);
          // millis comes from the production heartbeat callback; packed/eventMillis from its
          // cursor.
          String timing =
              "source-time\t0\t"
                  + (run.streamRecords - 1)
                  + "\t"
                  + (run.streamRecords - 1)
                  + "\t"
                  + run.millis
                  + "\t"
                  + packed
                  + "\t"
                  + eventMillis
                  + "\t0\t-\t-\t-\t-\t-\tmessage\tmessage\t"
                  + (packed == 0 ? "zero" : "external")
                  + "\t"
                  + packed
                  + "\t"
                  + eventMillis
                  + "\n";
          run.expectedTimingDigest.update(timing.getBytes(StandardCharsets.US_ASCII));
        }
        try {
          run.data.compare(row.toString());
        } catch (IOException failure) {
          throw new IllegalStateException("projection-io", failure);
        }
      }
    }

    public void handleCorruptedStream() {
      run.errors++;
    }

    public void handleCorruptedMessage(int type) {
      run.errors++;
    }

    public void handleUnknownMessage(int type) {
      run.errors++;
    }
  }

  private String run(DataScheme scheme, boolean discovery, boolean literal) throws Exception {
    activeSource = new Source();
    Run run = new Run(discovery);
    ConnectionStats stats = new ConnectionStats();
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    params.setFormat(FileFormat.BINARY);
    params.setCompression(StreamCompression.NONE);
    params.setTime(!discovery && timed ? TimestampsType.MESSAGE : TimestampsType.NONE);
    Reader reader = new Reader(scheme, run, stats, params, literal);
    try {
      try {
        reader.readInto(new Observer(run));
      } finally {
        reader.close();
      }
      if (!literal) {
        activeSource.complete();
      }
      if (sourceProjection) {
        System.out.println(
            literal
                ? "source-closure\tliteral\tinstrumented\tfalse"
                : "source-closure\tinstrumented\topens\t"
                    + activeSource.opens
                    + "\tcloses\t"
                    + activeSource.closes
                    + "\tio-errors\t"
                    + activeSource.errors
                    + "\teof\t"
                    + activeSource.eof);
      }
      require(stats.getReadBytes() == Files.size(input), "processed-byte-mismatch");
      String digest = run.complete();
      System.out.println(
          "observation\t"
              + (discovery
                  ? "discovery-instrumented"
                  : literal ? "mapped-literal" : "mapped-instrumented")
              + "\tbytes\t"
              + stats.getReadBytes()
              + "\teof-observed\t"
              + (!literal && activeSource.eof)
              + "\twire-records\t"
              + run.wireRecords
              + "\tstream-records\t"
              + run.streamRecords
              + "\tvalues\t"
              + run.values
              + "\tskips\t"
              + run.skips
              + "\tflags\t0"
              + "\tdescriptions\t"
              + run.schema.descriptions.size()
              + "\tgenerations\t"
              + run.generation);
      return digest;
    } finally {
      run.close();
    }
  }

  private void execute() throws Exception {
    require(Files.size(input) > 0 && Files.size(input) <= maxInput, "input-byte-budget");
    require(Files.size(cpp) > 0 && Files.size(cpp) <= maxOutput, "cpp-output-byte-budget");
    installSource();
    run(new EmptyScheme(), true, false);
    Schema schema = new Schema();
    for (Description description : discovered) {
      schema.add(description, binaryProjection);
    }
    DataScheme owned = schema.owned();
    String measured = run(owned, false, false);
    String literal = run(owned, false, true);
    require(measured.equals(literal), "literal-instrumented-digest-mismatch");
    System.out.println("acceptance\tcomplete\tschema-sha256:data-sha256\t" + measured);
  }

  /** Runs help without file work, or one finite production comparison. */
  public static void main(String[] args) {
    if (args.length == 0 || args.length == 1 && args[0].equals("--help")) {
      System.out.print(HELP);
      if (System.out.checkError()) {
        System.exit(1);
      }
      return;
    }
    try {
      Map<String, String> options = new LinkedHashMap<>();
      List<String> keys =
          Arrays.asList(
              "--input",
              "--cpp-output",
              "--tape-mode",
              "--cpp-projection",
              "--max-input",
              "--max-records",
              "--max-output");
      for (int i = 0; i < args.length; i++) {
        String key = args[i];
        require(!options.containsKey(key), "duplicate-option");
        if (key.equals("--inject-late-io") || key.equals("--inject-close-io")) {
          options.put(key, "true");
        } else {
          require(keys.contains(key) && i + 1 < args.length, "unknown-or-missing-option");
          options.put(key, args[++i]);
        }
      }
      new RealTapeAcceptance(options).execute();
    } catch (Throwable failure) {
      // Production parser logs remain private; do not copy source-dependent exception text.
      System.err.println("acceptance\trejected\t" + failure.getClass().getSimpleName());
      if (failure instanceof IllegalStateException
          && failure.getMessage() != null
          && failure.getMessage().matches("[a-z0-9-]+")) {
        System.err.println("reason\t" + failure.getMessage());
      }
      System.exit(1);
    }
  }
}
