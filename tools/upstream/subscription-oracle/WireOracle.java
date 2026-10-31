package qdcxx.oracle;

import com.devexperts.connector.proto.ApplicationConnection;
import com.devexperts.connector.proto.TransportConnection;
import com.devexperts.io.BufferedInput;
import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.Chunk;
import com.devexperts.io.ChunkList;
import com.devexperts.io.ChunkPool;
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
import com.devexperts.qd.kit.StringField;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.BinaryRecordDesc;
import com.devexperts.qd.qtp.MessageAdapter;
import com.devexperts.qd.qtp.MessageAdapterConnectionFactory;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.MessageVisitor;
import com.devexperts.qd.stats.QDStats;
import com.devexperts.util.TypedMap;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Offline caller: java -Xmx128m -cp <provider-and-driver-classes> qdcxx.oracle.WireOracle --capture
 * <fresh-directory>. Inputs are the fixed synthetic schemes and tuples below. Outputs are UTF-8/LF
 * observations.tsv, cases.tsv, raw .qtp files, and COMPLETE. No arguments and --help perform no
 * capture or factory construction. Invalid syntax exits 3; capture/I/O failures exit 2 and omit
 * COMPLETE. Run under an external 60-second timeout; this driver uses no sockets, sleeps, or worker
 * threads.
 */
public final class WireOracle {
  private static final String HELP =
      "Usage: WireOracle [--help | --capture FRESH_DIRECTORY]\n"
          + "Capture bounded offline Java QTP subscription and requested-layout observations.\n"
          + "No arguments and --help print this help without creating a factory or doing file"
          + " I/O.\n"
          + "--capture requires a nonexistent directory with an existing parent; no overwrite.\n"
          + "Outputs: observations.tsv, cases.tsv, raw .qtp files, COMPLETE; UTF-8 and LF.\n"
          + "Defaults: fixed synthetic inputs, PentaCodec, default QTP options, no field"
          + " replacer.\n"
          + "Limits: 64 cases, 64 tuples per parse, 65536 bytes per capture, 20000 rows.\n"
          + "Run with -Xmx128m and an external 60-second timeout. No network or native code.\n"
          + "Effects: creates the requested directory and files; may leave incomplete failure"
          + " files.\n"
          + "Exit codes: 0 help/capture success, 2 capture or output failure, 3 invalid"
          + " arguments.\n";
  private static final int MAX_CASES = 64;
  private static final int MAX_TUPLES = 64;
  private static final int MAX_BYTES = 65536;
  private static final int MAX_ROWS = 20000;
  private static final long MAX_NANOS = 45_000_000_000L;
  private static final MessageType[] TYPES = {
    MessageType.TICKER_ADD_SUBSCRIPTION,
    MessageType.TICKER_REMOVE_SUBSCRIPTION,
    MessageType.STREAM_ADD_SUBSCRIPTION,
    MessageType.STREAM_REMOVE_SUBSCRIPTION,
    MessageType.HISTORY_ADD_SUBSCRIPTION,
    MessageType.HISTORY_REMOVE_SUBSCRIPTION
  };
  private static final String[] SYMBOLS = {
    "ABC", "ABC", "", "*", "AB*", "A?", "\u0416\ud83d\ude80"
  };
  private static final long[] TIMES = {
    Long.MIN_VALUE, -4294967297L, -1, 0, 1, 4294967297L, Long.MAX_VALUE
  };

  private final Path directory;
  private final StringBuilder observations = new StringBuilder("case\tphase\tindex\tkey\tvalue\n");
  private final StringBuilder cases =
      new StringBuilder("case\tevidence_kind\tproduction_path\toutcome\n");
  private final Set<String> keys = new HashSet<>();
  private final Set<String> caseNames = new HashSet<>();
  private final long started = System.nanoTime();
  private int rows;

  private WireOracle(Path directory) {
    this.directory = directory;
  }

  private void row(String id, String phase, int index, String key, String value) {
    if (++rows > MAX_ROWS || System.nanoTime() - started > MAX_NANOS) {
      throw new IllegalStateException("capture budget exceeded");
    }
    String identity = id + "\t" + phase + "\t" + index + "\t" + key;
    if (!keys.add(identity) || value.indexOf('\t') >= 0 || value.indexOf('\n') >= 0) {
      throw new IllegalStateException("duplicate or invalid observation");
    }
    observations.append(identity).append('\t').append(value).append('\n');
  }

  private void caseRow(String id, String kind, String path, String outcome) {
    if (!caseNames.add(id) || caseNames.size() > MAX_CASES) {
      throw new IllegalStateException("duplicate case or case budget exceeded");
    }
    cases.append(id).append('\t').append(kind).append('\t').append(path).append('\t');
    cases.append(outcome).append('\n');
  }

  private static String integer(long value) {
    return "i:" + value;
  }

  private static String bool(boolean value) {
    return value ? "b:1" : "b:0";
  }

  private static String text(String value) {
    if (value == null) {
      return "n:null";
    }
    StringBuilder result = new StringBuilder("u:");
    for (int i = 0; i < value.length(); i++) {
      int unit = value.charAt(i);
      for (int shift = 12; shift >= 0; shift -= 4) {
        result.append("0123456789abcdef".charAt((unit >>> shift) & 15));
      }
    }
    return result.toString();
  }

  private static String hex(byte[] value) {
    StringBuilder result = new StringBuilder("x:");
    for (byte b : value) {
      result.append("0123456789abcdef".charAt((b >>> 4) & 15));
      result.append("0123456789abcdef".charAt(b & 15));
    }
    return result.toString();
  }

  private static DefaultRecord record(int id, String name, boolean time, String... fields) {
    DataIntField[] integers = new DataIntField[fields.length];
    for (int i = 0; i < fields.length; i++) {
      integers[i] = new CompactIntField(i, name + "." + fields[i]);
    }
    return new DefaultRecord(id, name, time, integers, new DataObjField[0]);
  }

  private static DataScheme scheme(DefaultRecord... records) {
    return new DefaultScheme(PentaCodec.INSTANCE, records);
  }

  private static DataScheme timedScheme(boolean reverse) {
    return reverse
        ? scheme(record(0, "S", false), record(1, "R", true, "TimeHi", "TimeLo", "x"))
        : scheme(record(0, "R", true, "TimeHi", "TimeLo", "x"), record(1, "S", false));
  }

  private RecordBuffer subscription(String id, String phase, DataScheme scheme) {
    RecordBuffer buffer = new RecordBuffer(RecordMode.HISTORY_SUBSCRIPTION);
    DataRecord record = scheme.findRecordByName("R");
    for (int i = 0; i < SYMBOLS.length; i++) {
      RecordCursor cursor = buffer.add(record, scheme.getCodec().encode(SYMBOLS[i]), SYMBOLS[i]);
      cursor.setTime(TIMES[i]);
      row(id, phase, i, "record_id", integer(record.getId()));
      row(id, phase, i, "symbol", text(SYMBOLS[i]));
      row(id, phase, i, "time", integer(cursor.getTime()));
    }
    return buffer;
  }

  private static RecordBuffer singleSubscription(DataScheme scheme, String symbol, long time) {
    RecordBuffer buffer = new RecordBuffer(RecordMode.HISTORY_SUBSCRIPTION);
    RecordCursor cursor =
        buffer.add(scheme.findRecordByName("R"), scheme.getCodec().encode(symbol), symbol);
    cursor.setTime(time);
    return buffer;
  }

  private static byte[] compose(DataScheme scheme, RecordBuffer buffer, MessageType type) {
    ByteArrayOutput output = new ByteArrayOutput();
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setOutput(output);
    composer.visitSubscription(buffer, type);
    return output.toByteArray();
  }

  private void save(String id, String phase, byte[] bytes) throws IOException {
    if (bytes.length > MAX_BYTES) {
      throw new IllegalStateException("byte budget exceeded");
    }
    Files.write(
        directory.resolve(id + "--" + phase + ".qtp"), bytes, StandardOpenOption.CREATE_NEW);
    row(id, phase + ".bytes", 0, "length", integer(bytes.length));
    row(id, phase + ".bytes", 0, "hex", hex(bytes));
  }

  // A diagnostic envelope/descriptor walk uses upstream primitives. Tuple results come from
  // parse().
  private void frames(String id, String phase, byte[] bytes) throws IOException {
    ByteArrayInput input = new ByteArrayInput(bytes);
    int ordinal = 0;
    while (input.available() != 0) {
      int start = input.getPosition();
      long length = input.readCompactLong();
      int end = Math.toIntExact(input.getPosition() + length);
      int type = length == 0 ? 0 : input.readCompactInt();
      if (length < 0 || end > bytes.length || ordinal >= MAX_TUPLES) {
        throw new IOException("invalid diagnostic frame boundary");
      }
      row(id, phase + ".frame", ordinal, "start", integer(start));
      row(id, phase + ".frame", ordinal, "end", integer(end));
      row(id, phase + ".frame", ordinal, "type", integer(type));
      if (type == MessageType.DESCRIBE_RECORDS.getId()) {
        int described = 0;
        while (input.getPosition() < end) {
          if (++described > MAX_TUPLES) {
            throw new IOException("descriptor budget exceeded");
          }
          String descriptorPhase = phase + ".descriptor." + ordinal;
          int recordId = input.readCompactInt();
          row(id, descriptorPhase, described - 1, "record_id", integer(recordId));
          row(id, descriptorPhase, described - 1, "record_name", text(input.readUTFString()));
          int count = input.readCompactInt();
          if (count < 0 || count > MAX_TUPLES) {
            throw new IOException("field budget exceeded");
          }
          row(id, descriptorPhase, described - 1, "field_count", integer(count));
          for (int i = 0; i < count; i++) {
            String fieldPhase = descriptorPhase + ".fields." + (described - 1);
            row(id, fieldPhase, i, "name", text(input.readUTFString()));
            row(id, fieldPhase, i, "type", integer(input.readCompactInt()));
          }
        }
      }
      input.setPosition(end);
      ordinal++;
    }
  }

  private final class Observer extends MessageConsumerAdapter {
    final String id;
    final String phase;
    int tuples;
    int callbacks;
    int errors;

    Observer(String id, String phase) {
      this.id = id;
      this.phase = phase;
    }

    void take(RecordSource source, MessageType type) {
      int count = 0;
      RecordCursor cursor;
      while ((cursor = source.next()) != null) {
        if (tuples >= MAX_TUPLES) {
          throw new IllegalStateException("tuple budget exceeded");
        }
        DataRecord record = cursor.getRecord();
        String tuplePhase = phase + ".tuple";
        row(id, tuplePhase, tuples, "message_type", integer(type.getId()));
        row(id, tuplePhase, tuples, "record_id", integer(record.getId()));
        row(id, tuplePhase, tuples, "record_name", text(record.getName()));
        row(id, tuplePhase, tuples, "cipher", integer(cursor.getCipher()));
        row(id, tuplePhase, tuples, "stored_symbol", text(cursor.getSymbol()));
        row(id, tuplePhase, tuples, "symbol", text(cursor.getDecodedSymbol()));
        row(id, tuplePhase, tuples, "time", integer(cursor.getTime()));
        row(id, tuplePhase, tuples, "has_time", bool(cursor.hasTime()));
        row(id, tuplePhase, tuples, "event_flags", integer(cursor.getEventFlags()));
        if (type.isData()) {
          for (int i = 0; i < record.getIntFieldCount(); i++) {
            row(id, tuplePhase, tuples, "int." + i, integer(cursor.getInt(i)));
          }
          for (int i = 0; i < record.getObjFieldCount(); i++) {
            Object object = cursor.getObj(i);
            if (object != null && !(object instanceof String)) {
              throw new IllegalStateException("unexpected object observation");
            }
            row(id, tuplePhase, tuples, "obj." + i, text((String) object));
          }
        }
        tuples++;
        count++;
      }
      row(id, phase + ".callback", callbacks, "message_type", integer(type.getId()));
      row(id, phase + ".callback", callbacks, "tuples", integer(count));
      callbacks++;
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      take((RecordSource) iterator, type);
    }

    @Override
    protected void processSubscription(SubscriptionIterator iterator, MessageType type) {
      take((RecordSource) iterator, type);
    }

    private void error(String kind, int type) {
      row(id, phase + ".error", errors, "kind", "t:" + kind);
      row(id, phase + ".error", errors, "message_type", integer(type));
      errors++;
    }

    @Override
    public void handleCorruptedStream() {
      error("stream", -1);
    }

    @Override
    public void handleCorruptedMessage(int type) {
      error("message", type);
    }

    @Override
    public void handleUnknownMessage(int type) {
      error("unknown", type);
    }
  }

  private final class Parser extends BinaryQTPParser {
    final String id;
    final String phase;
    int ids;
    int times;

    Parser(DataScheme scheme, String id, String phase) {
      super(scheme);
      this.id = id;
      this.phase = phase;
    }

    @Override
    protected int readRecordId(BufferedInput input) throws IOException {
      int value = super.readRecordId(input);
      row(id, phase + ".wire_id", ids++, "raw", integer(value));
      return value;
    }

    @Override
    protected long readSubscriptionTime(BufferedInput input) throws IOException {
      long value = super.readSubscriptionTime(input);
      row(id, phase + ".wire_time", times++, "raw", integer(value));
      return value;
    }
  }

  private Observer parse(String id, String phase, DataScheme scheme, byte[] bytes)
      throws IOException {
    ByteArrayInput input = new ByteArrayInput(bytes);
    Parser parser = new Parser(scheme, id, phase);
    Observer observer = new Observer(id, phase);
    parser.setInput(input);
    parser.parse(observer);
    row(id, phase + ".summary", 0, "consumed", integer(input.totalPosition()));
    row(id, phase + ".summary", 0, "remaining", integer(input.available()));
    row(id, phase + ".summary", 0, "tuples", integer(observer.tuples));
    row(id, phase + ".summary", 0, "callbacks", integer(observer.callbacks));
    row(id, phase + ".summary", 0, "errors", integer(observer.errors));
    row(id, phase + ".summary", 0, "wire_id_reads", integer(parser.ids));
    row(id, phase + ".summary", 0, "wire_time_reads", integer(parser.times));
    return observer;
  }

  private void sixMessages() throws IOException {
    for (int direction = 0; direction < 2; direction++) {
      DataScheme sender = timedScheme(direction != 0);
      DataScheme receiver = timedScheme(direction == 0);
      for (MessageType type : TYPES) {
        String id = "wire-" + type.getId() + (direction == 0 ? "-a-b" : "-b-a");
        byte[] bytes = compose(sender, subscription(id, "submitted", sender), type);
        save(id, "composed", bytes);
        frames(id, "diagnostic", bytes);
        Observer observer = parse(id, "parsed", receiver, bytes);
        caseRow(
            id,
            "production",
            "BinaryQTPComposer.visitSubscription>BinaryQTPParser.parseSubscription",
            observer.errors == 0 ? "delivered" : "parser-error");
      }
    }
  }

  private final class Adapter extends MessageAdapter {
    final DataScheme scheme;
    final Observer observer;
    RecordBuffer pending;

    Adapter(DataScheme scheme, String id) {
      super(QDStats.VOID);
      this.scheme = scheme;
      observer = new Observer(id, "connection.request");
    }

    @Override
    public DataScheme getScheme() {
      return scheme;
    }

    @Override
    public boolean retrieveMessages(MessageVisitor visitor) {
      if (pending != null) {
        visitor.visitData(pending, MessageType.STREAM_DATA);
        pending = null;
      }
      return false;
    }

    @Override
    protected void processSubscription(SubscriptionIterator iterator, MessageType type) {
      observer.take((RecordSource) iterator, type);
    }

    @Override
    public void handleCorruptedStream() {
      observer.handleCorruptedStream();
      super.handleCorruptedStream();
    }

    @Override
    public void handleCorruptedMessage(int type) {
      observer.handleCorruptedMessage(type);
      super.handleCorruptedMessage(type);
    }

    @Override
    public void handleUnknownMessage(int type) {
      observer.handleUnknownMessage(type);
      super.handleUnknownMessage(type);
    }
  }

  private static final class Transport implements TransportConnection {
    private final TypedMap variables = new TypedMap();

    @Override
    public TypedMap variables() {
      return variables;
    }

    @Override
    public void markForImmediateRestart() {}

    @Override
    public void connectionClosed() {}

    @Override
    public void chunksAvailable() {}

    @Override
    public void readyToProcessChunks() {}
  }

  private static Field field(Object target, String name) throws ReflectiveOperationException {
    Field result = target.getClass().getDeclaredField(name);
    result.setAccessible(true);
    return result;
  }

  private ApplicationConnection<?> connection(final Adapter adapter) throws Exception {
    // The real factory constructor is package-private. Reflection changes access, not behavior.
    Constructor<MessageAdapterConnectionFactory> constructor =
        MessageAdapterConnectionFactory.class.getDeclaredConstructor(
            MessageAdapter.ConfigurableFactory.class);
    constructor.setAccessible(true);
    MessageAdapterConnectionFactory factory =
        constructor.newInstance(
            new MessageAdapter.ConfigurableFactory() {
              @Override
              public MessageAdapter createAdapter(QDStats stats) {
                return adapter;
              }
            });
    ApplicationConnection<?> result = factory.createConnection(new Transport());
    result.start();
    // Suppress clock-bearing heartbeat output exactly as in P05; no protocol bytes are filtered.
    field(result, "nextHeartbeatTime").setLong(result, Long.MAX_VALUE);
    field(result, "bytesToNextHeartbeat").setLong(result, Long.MAX_VALUE);
    return result;
  }

  private static DataScheme requestScheme(String layout) {
    DefaultRecord requested;
    if (layout.equals("object-x")) {
      requested =
          new DefaultRecord(
              1, "R", false, new DataIntField[0], new DataObjField[] {new StringField(0, "R.x")});
    } else {
      String[] fields =
          layout.equals("empty") ? new String[0] : new String[] {layout.equals("z") ? "z" : "x"};
      requested = record(1, "R", false, fields);
    }
    return scheme(record(0, "S", false), requested);
  }

  private void request(
      String id, String phase, ApplicationConnection<?> connection, Adapter adapter, String layout)
      throws Exception {
    DataScheme peer = requestScheme(layout);
    byte[] bytes =
        compose(peer, singleSubscription(peer, "ABC", 0), MessageType.STREAM_ADD_SUBSCRIPTION);
    save(id, phase, bytes);
    frames(id, phase, bytes);
    boolean ready =
        connection.processChunks(
            ChunkPool.DEFAULT.copyToChunkList(bytes, 0, bytes.length, adapter), adapter);
    row(id, phase + ".state", 0, "ready", bool(ready));
    Method method =
        connection.getClass().getDeclaredMethod("getRequestedRecordDesc", DataRecord.class);
    method.setAccessible(true);
    BinaryRecordDesc descriptor =
        (BinaryRecordDesc) method.invoke(connection, adapter.scheme.findRecordByName("R"));
    row(id, phase + ".state", 0, "descriptor_present", bool(descriptor != null));
    row(
        id,
        phase + ".state",
        0,
        "descriptor_originally_empty",
        descriptor == null ? "n:null" : bool(descriptor.isEmpty()));
  }

  private byte[] reply(
      String id, String phase, ApplicationConnection<?> connection, Adapter adapter)
      throws Exception {
    RecordBuffer data = new RecordBuffer(RecordMode.DATA);
    RecordCursor cursor =
        data.add(adapter.scheme.getRecord(0), adapter.scheme.getCodec().encode("ABC"), null);
    cursor.setInt(0, 11);
    cursor.setInt(1, 22);
    adapter.pending = data;
    ChunkList chunks = connection.retrieveChunks(adapter);
    ByteArrayOutput output = new ByteArrayOutput();
    if (chunks != null) {
      try {
        Chunk chunk;
        while ((chunk = chunks.poll(adapter)) != null) {
          output.write(chunk.getBytes(), chunk.getOffset(), chunk.getLength());
          chunk.recycle(adapter);
        }
      } finally {
        chunks.recycle(adapter);
      }
    }
    byte[] bytes = output.toByteArray();
    save(id, phase, bytes);
    frames(id, phase, bytes);
    return bytes;
  }

  private void layouts() throws Exception {
    for (String layout : new String[] {"absent", "empty", "x", "z", "object-x"}) {
      String id = "layout-" + layout;
      DataScheme publisher = scheme(record(0, "R", false, "x", "y"));
      Adapter adapter = new Adapter(publisher, id);
      ApplicationConnection<?> connection = connection(adapter);
      try {
        row(id, "instrumentation", 0, "heartbeat_suppressed", "b:1");
        if (!layout.equals("absent")) {
          request(id, "request", connection, adapter, layout);
        }
        byte[] bytes = reply(id, "reply", connection, adapter);
        DataScheme audit = scheme(record(0, "S", false), record(1, "R", false, "x", "y", "z"));
        Observer observer = parse(id, "audit", audit, bytes);
        if (layout.equals("object-x")) {
          parse(id, "peer", requestScheme(layout), bytes);
        }
        row(id, "state", 0, "adapter_alive", bool(adapter.isAlive()));
        row(id, "state", 0, "adapter_closed", bool(adapter.isClosed()));
        caseRow(
            id,
            "production-instrumented",
            (layout.equals("absent")
                    ? ""
                    : "MessageAdapterConnection.processChunks>ConnectionQTPParser.remapRecord>")
                + "MessageAdapterConnection.retrieveChunks>ConnectionQTPComposer.getRequestedRecordDesc>"
                + "BinaryQTPParser.parse",
            observer.errors == 0 ? "delivered" : "parser-error");
      } finally {
        connection.close();
      }
    }
  }

  private void layoutBeforeFirstUse() throws Exception {
    String id = "layout-before-first-use";
    DataScheme publisher = scheme(record(0, "R", false, "x", "y"));
    Adapter adapter = new Adapter(publisher, id);
    ApplicationConnection<?> connection = connection(adapter);
    try {
      request(id, "request-x", connection, adapter, "x");
      request(id, "request-z", connection, adapter, "z");
      byte[] bytes = reply(id, "reply", connection, adapter);
      Observer observer = parse(id, "audit", scheme(record(0, "R", false, "x", "y", "z")), bytes);
      caseRow(
          id,
          "production-instrumented",
          "MessageAdapterConnection.processChunks>ConnectionQTPParser.remapRecord>"
              + "MessageAdapterConnection.retrieveChunks>BinaryQTPComposer.getRecordDesc>"
              + "ConnectionQTPComposer.getRequestedRecordDesc>BinaryQTPParser.parse",
          observer.errors == 0 ? "delivered" : "parser-error");
    } finally {
      connection.close();
    }
  }

  private void cachedLayout(boolean reset) throws Exception {
    String id = reset ? "layout-after-reset" : "layout-after-first-use";
    DataScheme publisher = scheme(record(0, "R", false, "x", "y"));
    DataScheme audit = scheme(record(0, "R", false, "x", "y", "z"));
    Adapter adapter = new Adapter(publisher, id);
    ApplicationConnection<?> connection = connection(adapter);
    try {
      request(id, "request-x", connection, adapter, "x");
      byte[] first = reply(id, "reply-first", connection, adapter);
      parse(id, "audit-first", audit, first);
      if (reset) {
        BinaryQTPParser parser = (BinaryQTPParser) field(connection, "parser").get(connection);
        BinaryQTPComposer composer =
            (BinaryQTPComposer) field(connection, "composer").get(connection);
        parser.resetSession();
        composer.resetSession();
        row(id, "instrumentation", 0, "reset_parser_and_composer", "b:1");
      }
      request(id, "request-z", connection, adapter, "z");
      byte[] second = reply(id, "reply-second", connection, adapter);
      // The no-reset reply has no description: use the exact earlier bytes as retained session
      // state.
      parse(id, "audit-session", audit, reset ? second : concatenate(first, second));
      row(id, "state", 0, "adapter_alive", bool(adapter.isAlive()));
      caseRow(
          id,
          "production-instrumented",
          "ConnectionQTPParser.remapRecord>BinaryQTPComposer.getRecordDesc>"
              + (reset ? "BinaryQTPParser.resetSession>BinaryQTPComposer.resetSession>" : "")
              + "ConnectionQTPComposer.getRequestedRecordDesc>BinaryQTPParser.parse",
          "captured");
    } finally {
      connection.close();
    }
  }

  private static byte[] concatenate(byte[] first, byte[] second) {
    byte[] result = Arrays.copyOf(first, first.length + second.length);
    System.arraycopy(second, 0, result, first.length, second.length);
    return result;
  }

  private static byte[] lastFrame(byte[] bytes) throws IOException {
    ByteArrayInput input = new ByteArrayInput(bytes);
    int last = 0;
    while (input.available() != 0) {
      last = input.getPosition();
      int length = input.readCompactInt();
      input.setPosition(input.getPosition() + length);
    }
    return Arrays.copyOfRange(bytes, last, bytes.length);
  }

  private void malformed(boolean batch) throws IOException {
    DataScheme sender = timedScheme(false);
    DataScheme receiver = timedScheme(true);
    byte[] valid =
        compose(
            sender, singleSubscription(sender, "PREFIX", -1), MessageType.HISTORY_ADD_SUBSCRIPTION);
    RecordBuffer malformedBuffer =
        singleSubscription(sender, batch ? "CURRENT" : "BROKEN", batch ? -2 : Long.MAX_VALUE);
    if (batch) {
      malformedBuffer
          .add(sender.findRecordByName("R"), sender.getCodec().encode("BROKEN"), null)
          .setTime(Long.MAX_VALUE);
    }
    byte[] current = compose(sender, malformedBuffer, MessageType.HISTORY_ADD_SUBSCRIPTION);
    byte[] frame = lastFrame(current);
    ByteArrayInput input = new ByteArrayInput(frame);
    int bodyLength = input.readCompactInt();
    int bodyStart = input.getPosition();
    ByteArrayOutput injection = new ByteArrayOutput();
    injection.writeCompactInt(bodyLength - 1);
    injection.write(frame, bodyStart, bodyLength - 1);
    String id = batch ? "malformed-history-batch" : "malformed-history-bound";
    byte[] bytes = concatenate(valid, injection.toByteArray());
    save(id, "injected", bytes);
    row(id, "injection", 0, "operation", "t:drop-final-bound-byte-and-reduce-frame-length");
    frames(id, "diagnostic", bytes);
    Observer observer = parse(id, "parsed", receiver, bytes);
    caseRow(
        id,
        "diagnostic-injection-production-parser",
        "BinaryQTPComposer.visitSubscription>BinaryQTPParser.parseSubscription",
        observer.errors == 0 ? "accepted" : "parser-error");
  }

  private void run() throws Exception {
    sixMessages();
    layouts();
    layoutBeforeFirstUse();
    cachedLayout(false);
    cachedLayout(true);
    malformed(false);
    malformed(true);
    Files.write(
        directory.resolve("observations.tsv"),
        observations.toString().getBytes(StandardCharsets.UTF_8),
        StandardOpenOption.CREATE_NEW);
    Files.write(
        directory.resolve("cases.tsv"),
        cases.toString().getBytes(StandardCharsets.UTF_8),
        StandardOpenOption.CREATE_NEW);
    Files.write(
        directory.resolve("COMPLETE"),
        ("wire-oracle-v1\ncases=" + caseNames.size() + "\nrows=" + rows + "\n")
            .getBytes(StandardCharsets.UTF_8),
        StandardOpenOption.CREATE_NEW);
  }

  public static void main(String[] args) {
    try {
      if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
        System.out.write(HELP.getBytes(StandardCharsets.UTF_8));
        System.out.flush();
        if (System.out.checkError()) {
          System.exit(2);
        }
        return;
      }
      if (args.length != 2 || !args[0].equals("--capture") || args[1].isEmpty()) {
        System.err.write(
            "WireOracle: invalid arguments; use --help\n".getBytes(StandardCharsets.UTF_8));
        System.exit(3);
        return;
      }
      Path directory = Paths.get(args[1]);
      Files.createDirectory(directory);
      new WireOracle(directory).run();
    } catch (Exception exception) {
      exception.printStackTrace(System.err);
      System.exit(2);
    }
  }
}
