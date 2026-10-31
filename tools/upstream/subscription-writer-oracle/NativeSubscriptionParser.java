package qdcxx.oracle;

import com.devexperts.io.BufferedInput;
import com.devexperts.io.ByteArrayInput;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SubscriptionIterator;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.BinaryRecordDesc;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;

/**
 * Observes native subscription bytes through the unchanged production QD parser.
 *
 * <p>Invoke with {@code java -Xmx256m ... qdcxx.oracle.NativeSubscriptionParser --input FILE
 * --output NEW_DIRECTORY}. The caller supplies the locked QD classpath. No arguments and standalone
 * {@code --help} print help without constructing a QD scheme or touching files. Operational input
 * is a bounded binary QTP file; output is two UTF-8/LF TSV files in a newly created directory.
 * Failures return a nonzero status and may leave partial output files, which must never be treated
 * as success. Existing files and directories are never replaced. See {@code --help} for limits and
 * exit codes.
 */
public final class NativeSubscriptionParser {
  private static final int MAX_INPUT_BYTES = 1024 * 1024;
  private static final int MAX_TUPLES = 4096;
  private static final int MAX_SYMBOL_UNITS = 4096;
  private static final int MAX_TOTAL_SYMBOL_UNITS = 65536;
  private static final int MAX_DESCRIPTIONS = 64;
  private static final int MAX_WIRE_RECORD_ID = 65535;
  private static final int MAX_MESSAGES = 4096;
  private static final int DESCRIBE_RECORDS = 2;
  private static final String HELP =
      "NativeSubscriptionParser - observe native subscriptions with the production QD parser\n"
          + "\n"
          + "Usage: NativeSubscriptionParser --input FILE --output NEW_DIRECTORY\n"
          + "       NativeSubscriptionParser [--help]\n"
          + "\n"
          + "Options (required together; no operational defaults):\n"
          + "  --input FILE            Read one complete binary QTP file.\n"
          + "  --output NEW_DIRECTORY  Create a directory; existing paths are rejected.\n"
          + "  --help                  Show this help without file I/O or QD initialization.\n"
          + "No arguments show the same help. Options may appear in either order.\n"
          + "\n"
          + "Profile: DESCRIBE_RECORDS and subscription messages 11/12/16/17/21/22 only.\n"
          + "Receiver order: Indexed=0 (timed hi/lo/x), Beta=1 (x/y), Alpha=2 (x).\n"
          + "Limits: 1048576 input bytes, 4096 messages, 4096 tuples/time reads,\n"
          + "64 descriptions, wire IDs 0..65535, 4096 UTF-16 units per symbol and\n"
          + "65536 symbol units total. Run with -Xmx256m to bound production allocations.\n"
          + "Unknown records/messages, corruption and incomplete final frames fail.\n"
          + "\n"
          + "Outputs (UTF-8, LF, no BOM):\n"
          + "  observations.tsv  Actual callback type/index/receiver ID/name/symbol/raw time.\n"
          + "                    Names and symbols use four lowercase hex digits per UTF-16 unit.\n"
          + "  time-reads.tsv    Actual production readSubscriptionTime index/value calls.\n"
          + "No input or existing output is overwritten. Failed writes may leave partial files.\n"
          + "Exit codes: 0 success/help; 1 I/O, observation, parser or resource failure;\n"
          + "3 invalid arguments. Help output failure also returns 1.\n";

  private NativeSubscriptionParser() {}

  private static boolean subscriptionType(int type) {
    return type == 11 || type == 12 || type == 16 || type == 17 || type == 21 || type == 22;
  }

  private static void appendUtf16(StringBuilder output, String text) {
    if (text == null) {
      throw new IllegalStateException("unexpected null text");
    }
    for (int i = 0; i < text.length(); i++) {
      int unit = text.charAt(i);
      for (int shift = 12; shift >= 0; shift -= 4) {
        output.append("0123456789abcdef".charAt((unit >>> shift) & 15));
      }
    }
  }

  private static final class Observation extends MessageConsumerAdapter {
    private final StringBuilder tuples =
        new StringBuilder("message\tindex\treceiver_id\trecord_utf16\tsymbol_utf16\traw_time\n");
    private final StringBuilder times = new StringBuilder("index\tvalue\n");
    private int tupleCount;
    private int timeCount;
    private int symbolUnits;

    @Override
    protected void processSubscription(SubscriptionIterator iterator, MessageType type) {
      if (!subscriptionType(type.getId()) || !(iterator instanceof RecordSource)) {
        throw new IllegalStateException("unexpected subscription callback");
      }
      RecordCursor cursor;
      while ((cursor = ((RecordSource) iterator).next()) != null) {
        if (tupleCount >= MAX_TUPLES) {
          throw new IllegalStateException("tuple limit exceeded");
        }
        DataRecord record = cursor.getRecord();
        String symbol = cursor.getDecodedSymbol();
        if (symbol == null) {
          throw new IllegalStateException("unexpected null symbol");
        }
        if (symbol.length() > MAX_SYMBOL_UNITS
            || symbol.length() > MAX_TOTAL_SYMBOL_UNITS - symbolUnits) {
          throw new IllegalStateException("symbol limit exceeded");
        }
        symbolUnits += symbol.length();
        tuples.append(type.getId()).append('\t').append(tupleCount++).append('\t');
        tuples.append(record.getId()).append('\t');
        appendUtf16(tuples, record.getName());
        tuples.append('\t');
        appendUtf16(tuples, symbol);
        tuples.append('\t').append(cursor.getTime()).append('\n');
      }
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      throw new IllegalStateException("unexpected data message " + type.getId());
    }

    @Override
    public void handleCorruptedStream() {
      throw new IllegalStateException("corrupted QTP stream");
    }

    @Override
    public void handleCorruptedMessage(int type) {
      throw new IllegalStateException("corrupted QTP message " + type);
    }

    @Override
    public void handleUnknownMessage(int type) {
      throw new IllegalStateException("unknown QTP message " + type);
    }

    private void time(long value) {
      if (timeCount >= MAX_TUPLES) {
        throw new IllegalStateException("time-read limit exceeded");
      }
      times.append(timeCount++).append('\t').append(value).append('\n');
    }
  }

  private static final class Parser extends BinaryQTPParser {
    private final Observation observation;
    private int descriptions;
    private int messages;

    private Parser(DataScheme scheme, Observation observation) {
      super(scheme);
      this.observation = observation;
    }

    @Override
    protected void remapRecord(int id, BinaryRecordDesc descriptor) {
      if (++descriptions > MAX_DESCRIPTIONS || id < 0 || id > MAX_WIRE_RECORD_ID) {
        throw new IllegalStateException("record-description limit exceeded");
      }
      if (descriptor.getRecord() == null) {
        throw new IllegalStateException("description has no receiver record");
      }
      super.remapRecord(id, descriptor);
    }

    @Override
    protected void doAfterMessageBody(BufferedInput input, int type) {
      if (++messages > MAX_MESSAGES) {
        throw new IllegalStateException("message limit exceeded");
      }
      if (type != DESCRIBE_RECORDS && !subscriptionType(type)) {
        throw new IllegalStateException("unexpected QTP message " + type);
      }
    }

    @Override
    protected long readSubscriptionTime(BufferedInput input) throws IOException {
      long value = super.readSubscriptionTime(input);
      observation.time(value);
      return value;
    }
  }

  private static byte[] readInput(Path input) throws IOException {
    if (!Files.isRegularFile(input)) {
      throw new IOException("input must be a regular file");
    }
    try (InputStream stream = Files.newInputStream(input)) {
      ByteArrayOutputStream bytes = new ByteArrayOutputStream();
      byte[] block = new byte[8192];
      int count;
      while ((count = stream.read(block)) != -1) {
        if (count > MAX_INPUT_BYTES - bytes.size()) {
          throw new IOException("input-byte limit exceeded");
        }
        bytes.write(block, 0, count);
      }
      return bytes.toByteArray();
    }
  }

  // Keep QD-dependent method signatures out of the entry class: even JDK 8's main-method
  // lookup must work for help when the operational provider classpath is unavailable.
  private static final class Engine {
    private static DefaultRecord record(int id, String name, boolean time, String... fields) {
      DataIntField[] integers = new DataIntField[fields.length];
      for (int i = 0; i < fields.length; i++) {
        integers[i] = new CompactIntField(i, name + "." + fields[i]);
      }
      return new DefaultRecord(id, name, time, integers, new DataObjField[0]);
    }

    private static DataScheme receiver() {
      return new DefaultScheme(
          PentaCodec.INSTANCE,
          record(0, "Indexed", true, "hi", "lo", "x"),
          record(1, "Beta", false, "x", "y"),
          record(2, "Alpha", false, "x"));
    }

    private static void observe(Path inputPath, Path outputPath) throws IOException {
      if (Files.exists(outputPath)) {
        throw new IOException("output path already exists");
      }
      ByteArrayInput input = new ByteArrayInput(readInput(inputPath));
      Observation observation = new Observation();
      Parser parser = new Parser(receiver(), observation);
      parser.setInput(input);
      parser.parse(observation);
      // Production parsing waits for more input instead of diagnosing an incomplete outer frame.
      if (input.available() != 0) {
        throw new IOException("incomplete final QTP frame");
      }
      if (observation.tupleCount == 0) {
        throw new IOException("no subscription tuples observed");
      }
      Files.createDirectory(outputPath);
      Files.write(
          outputPath.resolve("observations.tsv"),
          observation.tuples.toString().getBytes(StandardCharsets.UTF_8),
          StandardOpenOption.CREATE_NEW);
      Files.write(
          outputPath.resolve("time-reads.tsv"),
          observation.times.toString().getBytes(StandardCharsets.UTF_8),
          StandardOpenOption.CREATE_NEW);
    }
  }

  private static int run(String[] args) throws IOException {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      System.out.write(HELP.getBytes(StandardCharsets.UTF_8));
      System.out.flush();
      return System.out.checkError() ? 1 : 0;
    }
    String input = null;
    String output = null;
    if (args.length == 4) {
      for (int i = 0; i < args.length; i += 2) {
        if (args[i].equals("--input") && input == null) {
          input = args[i + 1];
        } else if (args[i].equals("--output") && output == null) {
          output = args[i + 1];
        } else {
          break;
        }
      }
    }
    if (input == null || input.isEmpty() || output == null || output.isEmpty()) {
      System.err.println("NativeSubscriptionParser: invalid arguments; use --help");
      return 3;
    }
    Engine.observe(Paths.get(input), Paths.get(output));
    return 0;
  }

  public static void main(String[] args) {
    int result;
    try {
      result = run(args);
    } catch (Exception | OutOfMemoryError failure) {
      System.err.println("NativeSubscriptionParser: " + failure);
      result = 1;
    }
    if (result != 0) {
      System.exit(result);
    }
  }
}
