package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.impl.DXFeedScheme;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Map;
import java.util.TreeMap;

/** Bounded structural inventory; opaque data bodies are never interpreted as record payloads. */
public final class RawTapeInventory {
  private static final int MAX_FILE_BYTES = 8 * 1024 * 1024;
  private static final int MAX_FRAME_BYTES = 1024 * 1024;
  private static final int MAX_NAME_BYTES = 4096;
  private static final int MAX_NAME_UNITS = 1024 * 1024;
  private static final int MAX_FRAMES = 100000;
  private static final int MAX_DESCRIPTIONS = 10000;
  private static final int MAX_FIELDS = 100000;
  private static final int MAX_REPLAY_RECORDS = 1000000;

  private static final class Stop extends IOException {
    Stop(String reason) {
      super(reason);
    }
  }

  private static final class Result {
    String status = "complete";
    int boundary;
    int examined;
    int frames;
    int descriptions;
    int fields;
    int nameUnits;
    final Map<Integer, Integer> messages = new TreeMap<>();
    final Map<Integer, Integer> types = new TreeMap<>();
    final Map<String, Integer> records = new TreeMap<>();
  }

  private static void count(Map<Integer, Integer> counts, int key) {
    counts.put(key, counts.containsKey(key) ? counts.get(key) + 1 : 1);
  }

  private static String name(ByteArrayInput in, Result result) throws IOException {
    int start = in.getPosition();
    long length = in.readCompactLong();
    if (length < -1) {
      throw new Stop("invalid-string");
    }
    if (length > MAX_NAME_BYTES) {
      throw new Stop("name-budget");
    }
    if (length > in.available()) {
      throw new EOFException();
    }
    in.setPosition(start);
    String value = in.readUTFString();
    if (value != null) {
      if (value.length() > MAX_NAME_UNITS - result.nameUnits) {
        throw new Stop("name-units-budget");
      }
      result.nameUnits += value.length();
    }
    return value;
  }

  private static Result inventory(
      byte[] bytes, int frameLimit, int descriptionLimit, int fieldLimit) {
    Result result = new Result();
    if (bytes.length > MAX_FILE_BYTES) {
      result.status = "file-budget";
      return result;
    }
    ByteArrayInput input = new ByteArrayInput(bytes);
    ByteArrayInput active = input;
    try {
      while (input.available() > 0) {
        result.boundary = input.getPosition();
        active = input;
        if (result.frames == frameLimit) {
          throw new Stop("frame-budget");
        }
        long wireLength = input.readCompactLong();
        if (wireLength < 0) {
          throw new Stop("invalid-frame-length");
        }
        if (wireLength > MAX_FRAME_BYTES) {
          throw new Stop("frame-bytes-budget");
        }
        int length = (int) wireLength;
        if (length > input.available()) {
          throw new EOFException();
        }
        int end = input.getPosition() + length;
        active = new ByteArrayInput(bytes, input.getPosition(), length);
        long wireType = length == 0 ? -1 : active.readCompactLong();
        if (length != 0 && (wireType < 0 || wireType > Integer.MAX_VALUE)) {
          throw new Stop("invalid-message-type");
        }
        int type = (int) wireType;
        count(result.messages, type);
        result.frames++;
        if (type == MessageType.DESCRIBE_RECORDS.getId()) {
          while (active.available() > 0) {
            result.boundary = active.getPosition();
            if (result.descriptions == descriptionLimit) {
              throw new Stop("description-budget");
            }
            int id = active.readCompactInt();
            String record = name(active, result);
            int fields = active.readCompactInt();
            if (id < 0 || record == null || record.isEmpty() || fields < 0) {
              throw new Stop("invalid-record");
            }
            if (fields > fieldLimit - result.fields) {
              throw new Stop("field-budget");
            }
            Map<Integer, Integer> types = new TreeMap<>();
            for (int i = 0; i < fields; i++) {
              String field = name(active, result);
              int serialType = active.readCompactInt();
              if (field == null || field.isEmpty() || serialType < 0 || serialType > 255) {
                throw new Stop("invalid-field");
              }
              count(types, serialType);
            }
            result.descriptions++;
            result.fields += fields;
            result.records.put(
                record, result.records.containsKey(record) ? result.records.get(record) + 1 : 1);
            for (Map.Entry<Integer, Integer> entry : types.entrySet()) {
              int key = entry.getKey();
              result.types.put(
                  key,
                  (result.types.containsKey(key) ? result.types.get(key) : 0) + entry.getValue());
            }
          }
        }
        input.setPosition(end);
        result.boundary = end;
      }
    } catch (Stop failure) {
      result.status = failure.getMessage();
    } catch (EOFException failure) {
      result.status = "truncated";
    } catch (IOException failure) {
      result.status = "invalid-primitive";
    }
    result.examined = active.getPosition();
    if (result.status.equals("complete")) {
      result.examined = result.boundary;
    }
    return result;
  }

  private static byte[] bytes(ByteArrayOutput out) {
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  private static byte[] frame(int type, byte[] payload) throws IOException {
    ByteArrayOutput body = new ByteArrayOutput();
    body.writeCompactInt(type);
    body.write(payload);
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(body.getPosition());
    out.write(bytes(body));
    return bytes(out);
  }

  private static byte[] description(String name, int type) throws IOException {
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactInt(0);
    out.writeUTFString(name);
    out.writeCompactInt(1);
    out.writeUTFString("value");
    out.writeCompactInt(type);
    return frame(2, bytes(out));
  }

  private static byte[] join(byte[]... parts) throws IOException {
    ByteArrayOutput out = new ByteArrayOutput();
    for (byte[] part : parts) {
      out.write(part);
    }
    return bytes(out);
  }

  private static void require(boolean condition, String name) {
    if (!condition) {
      throw new AssertionError(name);
    }
  }

  private static void selftest() throws Exception {
    byte[] unknown = frame(117, new byte[] {(byte) 255, 0, 81});
    byte[] first = description("Profile", 255);
    byte[] late = description("Quote", 120);
    byte[] complete = join(first, unknown, late);
    Result result = inventory(complete, 3, 2, 2);
    require(
        result.status.equals("complete") && result.boundary == complete.length,
        "complete boundary");
    require(
        result.frames == 3 && result.descriptions == 2 && result.fields == 2, "complete counts");
    require(
        result.messages.get(117) == 1
            && result.types.get(255) == 1
            && result.types.get(120) == 1
            && result.records.containsKey("Profile"),
        "opaque unknown and late description");
    result = inventory(complete, 2, 2, 2);
    require(
        result.status.equals("frame-budget")
            && result.boundary == first.length + unknown.length
            && result.descriptions == 1,
        "frame budget prefix");
    result = inventory(complete, 3, 1, 2);
    require(
        result.status.equals("description-budget")
            && result.frames == 3
            && result.descriptions == 1,
        "description budget");
    result = inventory(complete, 3, 2, 1);
    require(result.status.equals("field-budget") && result.descriptions == 1, "field budget");
    result = inventory(complete, 0, 0, 0);
    require(result.status.equals("frame-budget") && result.boundary == 0, "zero budget");
    result = inventory(complete, 3, 0, 2);
    require(
        result.status.equals("description-budget") && result.descriptions == 0,
        "zero description budget");
    result = inventory(complete, 3, 2, 0);
    require(result.status.equals("field-budget") && result.descriptions == 0, "zero field budget");
    for (int i = 1; i < first.length; i++) {
      result = inventory(Arrays.copyOf(first, i), 3, 2, 2);
      require(
          result.status.equals("truncated") && result.boundary == 0 && result.descriptions == 0,
          "frame truncation " + i);
    }
    for (int i = 3; i < first.length; i++) {
      result = inventory(frame(2, Arrays.copyOfRange(first, 2, i)), 3, 2, 2);
      require(
          result.status.equals("truncated") && result.descriptions == 0,
          "complete frame with truncated description " + i);
    }
    char[] oversizedName = new char[MAX_NAME_BYTES + 1];
    Arrays.fill(oversizedName, 'x');
    require(
        inventory(description(new String(oversizedName), 8), 3, 2, 2).status.equals("name-budget"),
        "name byte budget");
    byte[] maximumName = description(new String(oversizedName, 0, MAX_NAME_BYTES), 8);
    ByteArrayOutput manyNames = new ByteArrayOutput();
    for (int i = 0; i < 257; i++) {
      manyNames.write(maximumName);
    }
    result = inventory(bytes(manyNames), 300, 300, 300);
    require(
        result.status.equals("name-units-budget") && result.descriptions > 0,
        "aggregate name budget");
    result = inventory(frame(2, new byte[] {0, 1, (byte) 255, 0}), 3, 2, 2);
    require(
        result.status.equals("invalid-primitive") && result.descriptions == 0,
        "invalid UTF primitive");
    byte[] malformed = description("", 8);
    result = inventory(join(first, malformed, late), 3, 3, 3);
    require(
        result.status.equals("invalid-record")
            && result.descriptions == 1
            && result.boundary == first.length + 2
            && result.frames == 2,
        "malformed description no resync");
    result = inventory(join(first, description("Bad", 256), late), 3, 3, 3);
    require(
        result.status.equals("invalid-field")
            && result.descriptions == 1
            && result.types.size() == 1,
        "atomic invalid field");
    result = inventory(frame(2, new byte[] {0}), 3, 3, 3);
    require(
        result.status.equals("truncated") && result.boundary == 2 && result.examined == 3,
        "description truncation");
    ByteArrayOutput negative = new ByteArrayOutput();
    negative.writeCompactInt(-1);
    result = inventory(bytes(negative), 3, 3, 3);
    require(result.status.equals("invalid-frame-length") && result.boundary == 0, "invalid length");
    result = inventory(frame(-1, new byte[0]), 3, 3, 3);
    require(result.status.equals("invalid-message-type") && result.frames == 0, "invalid type");
    ByteArrayOutput large = new ByteArrayOutput();
    large.writeCompactLong(MAX_FRAME_BYTES + 1);
    require(
        inventory(bytes(large), 3, 3, 3).status.equals("frame-bytes-budget"), "frame byte budget");
    ByteArrayOutput crossWidth = new ByteArrayOutput();
    crossWidth.writeCompactLong(0x100000001L);
    require(
        inventory(bytes(crossWidth), 3, 3, 3).status.equals("frame-bytes-budget"),
        "wide frame length must not narrow");
    ByteArrayOutput wideType = new ByteArrayOutput();
    wideType.writeCompactLong(crossWidth.getPosition());
    wideType.write(bytes(crossWidth));
    require(
        inventory(bytes(wideType), 3, 3, 3).status.equals("invalid-message-type"),
        "wide message kind must not narrow");
    require(
        inventory(new byte[MAX_FILE_BYTES + 1], 3, 3, 3).status.equals("file-budget"),
        "file budget");
    result = inventory(new byte[] {0, 1, 0}, 2, 0, 0);
    require(
        result.status.equals("complete")
            && result.messages.get(-1) == 1
            && result.messages.get(0) == 1,
        "marker versus heartbeat");
    System.out.println("PASS bounded inventory generated cases, including every frame prefix");
  }

  private static final class ReplayObserver extends MessageConsumerAdapter {
    final int deliveryLimit;
    int attemptedDeliveries;
    int acceptedDeliveries;
    int streamDeliveries;
    int errors;
    final Map<Integer, Integer> callbacks = new TreeMap<>();

    ReplayObserver(int deliveryLimit) {
      this.deliveryLimit = deliveryLimit;
    }

    @Override
    public void processDescribeProtocol(ProtocolDescriptor descriptor, boolean log) {
      System.out.println("descriptor\t" + descriptor.getProperties());
      System.out.println("send\t" + descriptor.getSendSet());
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      count(callbacks, type.getId());
      RecordSource source = (RecordSource) iterator;
      while (source.next() != null) {
        if (++attemptedDeliveries > deliveryLimit) {
          throw new IllegalStateException("replay-record-budget");
        }
        acceptedDeliveries++;
        if (type == MessageType.STREAM_DATA) {
          streamDeliveries++;
        }
      }
    }

    @Override
    public void handleCorruptedStream() {
      errors++;
      System.out.println("error\tstream");
    }

    @Override
    public void handleCorruptedMessage(int type) {
      errors++;
      System.out.println("error\tmessage\t" + type);
    }

    @Override
    public void handleUnknownMessage(int type) {
      errors++;
      System.out.println("error\tunknown\t" + type);
    }
  }

  private static void replay(Path file, int deliveryLimit) throws Exception {
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    System.out.println(
        "settings\tdefault-DXFeedScheme\tsingle-file\tMAX_SPEED\tauto-format-time-compression"
            + "\tno-resync\tno-cycle");
    ReplayObserver observer = new ReplayObserver(deliveryLimit);
    ConnectionStats stats = new ConnectionStats();
    long fileBytes = Files.size(file);
    FileReader reader = new FileReader(file.toString(), stats, params);
    boolean returned = false;
    try {
      reader.setScheme(DXFeedScheme.getInstance());
      reader.readInto(observer);
      returned = true;
    } finally {
      reader.close();
      System.out.println(
          "replay-observed\t"
              + observer.attemptedDeliveries
              + "\t"
              + observer.errors
              + "\t"
              + observer.callbacks);
      System.out.println(
          "replay-result\t"
              + (returned && observer.errors == 0 && stats.getReadBytes() == fileBytes
                  ? "returned-byte-match"
                  : "incomplete")
              + "\tdelivery-limit\t"
              + deliveryLimit
              + "\taccepted-deliveries\t"
              + observer.acceptedDeliveries
              + "\tstream-projection-deliveries\t"
              + observer.streamDeliveries
              + "\tprocessed-bytes\t"
              + stats.getReadBytes()
              + "\tfile-bytes\t"
              + fileBytes);
    }
  }

  /** Runs generated tests, structural inventory, or separately bounded production replay. */
  public static void main(String[] args) throws Exception {
    if (args.length == 1 && args[0].equals("--help")) {
      System.out.println(
          "selftest | inventory FILE | replay FILE [DELIVERY_LIMIT]\n"
              + "Read-only diagnostic inventory or production FileReader replay; max file 8 MiB.\n"
              + "Replay delivery limit: 1..100000000, default 1000000; counts callback fanout.\n"
              + "STREAM_DATA deliveries are a wire projection, not distinct business events.\n"
              + "Output: TSV diagnostics; exit 0 on return, nonzero on invalid input/exception.\n"
              + "A nonzero corruption count means incomplete even if the process exits 0.\n"
              + "Returned-byte-match also requires processed bytes equal physical file size;\n"
              + "compressed/paired/sequence sources need separate evidence and are not qualified.\n"
              + "Use the launcher for finite wall time and heap; no input is modified.");
      return;
    }
    if (args.length == 1 && args[0].equals("selftest")) {
      selftest();
      return;
    }
    if ((args.length != 2 && args.length != 3)
        || (!args[0].equals("inventory") && !args[0].equals("replay"))
        || (args.length == 3 && !args[0].equals("replay"))) {
      throw new IllegalArgumentException(
          "selftest | inventory FILE | replay FILE [DELIVERY_LIMIT]");
    }
    int deliveryLimit = args.length == 3 ? Integer.parseInt(args[2]) : MAX_REPLAY_RECORDS;
    if (deliveryLimit < 1 || deliveryLimit > 100000000) {
      throw new IllegalArgumentException("delivery limit must be 1..100000000");
    }
    Path file = Paths.get(args[1]);
    if (Files.size(file) > MAX_FILE_BYTES) {
      throw new IOException("file-budget");
    }
    if (args[0].equals("replay")) {
      replay(file, deliveryLimit);
      return;
    }
    ByteArrayOutput bounded = new ByteArrayOutput();
    try (InputStream stream = Files.newInputStream(file)) {
      byte[] chunk = new byte[8192];
      int count;
      while ((count = stream.read(chunk)) != -1) {
        if (count > MAX_FILE_BYTES - bounded.getPosition()) {
          throw new IOException("file-budget");
        }
        bounded.write(chunk, 0, count);
      }
    }
    Result result = inventory(bytes(bounded), MAX_FRAMES, MAX_DESCRIPTIONS, MAX_FIELDS);
    System.out.println(
        "status\t"
            + result.status
            + "\tboundary\t"
            + result.boundary
            + "\texamined\t"
            + result.examined);
    System.out.println(
        "counts\t"
            + result.frames
            + "\t"
            + result.descriptions
            + "\t"
            + result.fields
            + "\t"
            + result.nameUnits);
    System.out.println("messages\t" + result.messages);
    System.out.println("full-types\t" + result.types);
    System.out.println("record-descriptions\t" + result.records);
  }
}
