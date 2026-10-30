package qdcxx.oracle;

import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
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
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.Quote;
import com.dxfeed.event.market.QuoteDelegate;
import java.io.IOException;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** Small production FileWriter/FileReader fixtures; this class does not decode wire bytes. */
public final class RealTapeCorpus {
  private static final int MAX_FILE_BYTES = 65536;
  private static final int RECORD_COUNT = 3;
  private static final long FIRST_TIME_MILLIS = 1600000000123L;
  private static final List<String> CASES =
      Arrays.asList("compact-none", "quote-none", "quote-message");
  private static final String TRUNCATED = "quote-truncated.qtp";
  private static final String HELP =
      "RealTapeCorpus: bounded deterministic production QD file fixtures\n"
          + "  --help (or no arguments): print help without file access\n"
          + "  generate NEW_DIRECTORY: write three fixed BINARY/NONE/STREAM_DATA tapes,\n"
          + "    production replay observations, and one derived truncated tape\n"
          + "  verify DIRECTORY EXPECTED: compare the exact seven-file artifact set and bytes\n"
          + "  negatives DIRECTORY EXPECTED NEW_WORK: verify five comparator rejections and\n"
          + "    production rejection of the truncated tape; retain bounded test copies\n"
          + "  help-tests NEW_DIRECTORY: assert child help/usage with separate file redirects\n"
          + "Limits: 65536 bytes/file, three expected records/case, no transport or sidecars.\n"
          + "Use Invoke-Corpus.ps1 for finite process time, heap and provenance.\n"
          + "Exit: 0 successful operation/help; 2 usage; 1 failed work/check.\n";

  private RealTapeCorpus() {}

  private static void require(boolean condition, String reason) {
    if (!condition) {
      throw new IllegalStateException(reason);
    }
  }

  private static String units(String value) {
    StringBuilder result = new StringBuilder();
    for (char c : value.toCharArray()) {
      result.append(String.format("%04x", (int) c));
    }
    return result.length() == 0 ? "-" : result.toString();
  }

  private static long time(int index) {
    return FIRST_TIME_MILLIS + index * 1000L;
  }

  private static DataScheme scheme(String name) {
    if (!name.equals("compact-none")) {
      return DXFeedScheme.getInstance();
    }
    DataIntField[] fields = {new CompactIntField(0, "Corpus.Value")};
    return new DefaultScheme(
        PentaCodec.INSTANCE, new DefaultRecord(0, "Corpus", false, fields, new DataObjField[0]));
  }

  private static RecordBuffer input(DataScheme scheme, String name, int index) {
    RecordBuffer buffer = new RecordBuffer(RecordMode.FLAGGED_DATA);
    if (name.equals("compact-none")) {
      String symbol = index == 1 ? "LONG-CORPUS-SYMBOL" : "IBM";
      RecordCursor cursor =
          buffer.add(scheme.getRecord(0), scheme.getCodec().encode(symbol), symbol);
      cursor.setInt(0, new int[] {Integer.MIN_VALUE, 0, Integer.MAX_VALUE}[index]);
    } else {
      Quote quote = new Quote(index == 1 ? "MSFT" : "IBM");
      quote.setBidTime(1700000000000L + index * 2000L);
      quote.setAskTime(1700000001000L + index * 2000L);
      quote.setBidExchangeCode('N');
      quote.setAskExchangeCode('Q');
      quote.setBidPrice(100.25 + index);
      quote.setAskPrice(100.5 + index);
      quote.setBidSizeAsDouble(10.5 + index);
      quote.setAskSizeAsDouble(12.25 + index);
      quote.setEventTime(time(index));
      QuoteDelegate delegate = null;
      for (EventDelegate<?> candidate :
          new MarketFactoryImpl().createStreamOnlyDelegates(scheme.findRecordByName("Quote"))) {
        if (candidate instanceof QuoteDelegate) {
          delegate = (QuoteDelegate) candidate;
        }
      }
      require(delegate != null, "production Quote delegate unavailable");
      delegate.putEvent(quote, buffer);
    }
    return buffer;
  }

  private static String snapshot(RecordCursor cursor, DataScheme scheme) {
    StringBuilder result = new StringBuilder();
    DataRecord record = cursor.getRecord();
    result.append(units(record.getName())).append('\t');
    result.append(units(scheme.getCodec().decode(cursor.getCipher(), cursor.getSymbol())));
    result.append('\t').append(cursor.getEventFlags());
    require(record.getObjFieldCount() == 0, "these cases require an object-free full schema");
    for (int i = 0; i < record.getIntFieldCount(); i++) {
      result.append('\t').append(units(record.getIntField(i).getName()));
      result.append(':').append(record.getIntField(i).getSerialType().getId());
      result.append(':').append(cursor.getInt(i));
    }
    return result.toString();
  }

  private static List<String> expected(DataScheme scheme, String name) {
    List<String> result = new ArrayList<>();
    for (int i = 0; i < RECORD_COUNT; i++) {
      result.add(snapshot(input(scheme, name, i).next(), scheme));
    }
    return result;
  }

  private static void write(Path file, DataScheme scheme, String name) throws Exception {
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(name.equals("quote-message") ? TimestampsType.MESSAGE : TimestampsType.NONE);
    writer.setOpt("");
    writer.setSaveAs(MessageType.STREAM_DATA);
    try (FileWriter opened = writer.open(file.toString(), scheme)) {
      for (int i = 0; i < RECORD_COUNT; i++) {
        HeartbeatPayload heartbeat = new HeartbeatPayload();
        heartbeat.setTimeMillis(time(i));
        opened.visitHeartbeat(heartbeat);
        require(
            !opened.visitData(input(scheme, name, i), MessageType.STREAM_DATA), "writer capacity");
      }
    }
    require(Files.size(file) > 0 && Files.size(file) <= MAX_FILE_BYTES, "writer file budget");
  }

  private static final class Observer extends MessageConsumerAdapter {
    final DataScheme scheme;
    final String name;
    final List<String> expected;
    final PrintWriter output;
    int records;
    int callbacks;
    int errors;
    int descriptors;

    Observer(DataScheme scheme, String name, PrintWriter output) {
      this.scheme = scheme;
      this.name = name;
      this.expected = expected(scheme, name);
      this.output = output;
    }

    @Override
    public void processDescribeProtocol(ProtocolDescriptor descriptor, boolean log) {
      require(++descriptors == 1, "exactly one descriptor required");
      require(descriptor.getEndpointId() == null, "unexpected endpoint");
      require(descriptor.getReceiveSet().isEmpty(), "unexpected receive messages");
      require(
          descriptor.getSendSet().equals(java.util.EnumSet.of(MessageType.STREAM_DATA)),
          "unexpected send messages");
      require("".equals(descriptor.getProperty("opt")), "EMPTY_SET options required");
    }

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      require(++callbacks <= RECORD_COUNT, "callback budget");
      require(type == MessageType.STREAM_DATA, "unexpected message type");
      RecordCursor cursor;
      while ((cursor = ((RecordSource) iterator).next()) != null) {
        require(records < RECORD_COUNT, "delivery budget");
        String actual = snapshot(cursor, scheme);
        require(actual.equals(expected.get(records)), "record mismatch at " + records);
        long eventTime =
            name.equals("quote-message")
                ? TimeSequenceUtil.getTimeSequenceFromTimeMillis(time(records))
                : 0;
        require(cursor.getEventTimeSequence() == eventTime, "event time mismatch");
        output.print("record\t" + records + "\t" + eventTime + "\t" + actual + "\n");
        records++;
      }
    }

    @Override
    public void handleCorruptedStream() {
      errors++;
    }

    @Override
    public void handleCorruptedMessage(int messageType) {
      errors++;
    }

    @Override
    public void handleUnknownMessage(int messageType) {
      errors++;
    }
  }

  private static void read(Path file, Path observation, String name, boolean reject)
      throws Exception {
    require(Files.size(file) > 0 && Files.size(file) <= MAX_FILE_BYTES, "reader file budget");
    DataScheme scheme = scheme(name);
    ConnectionStats stats = new ConnectionStats();
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    params.setResyncOn(null);
    FileReader reader = new FileReader(file.toString(), stats, params);
    reader.setScheme(scheme);
    try (PrintWriter output =
        new PrintWriter(Files.newBufferedWriter(observation, StandardCharsets.UTF_8))) {
      output.print(
          "profile\t"
              + name
              + "\tBINARY\tNONE\tEMPTY_SET\tSTREAM_DATA\t"
              + (name.equals("quote-message") ? "MESSAGE" : "NONE")
              + "\n");
      Observer observer = new Observer(scheme, name, output);
      try {
        reader.readInto(observer);
      } finally {
        reader.close();
      }
      if (reject) {
        require(observer.errors > 0, "truncated tape must report a production parser error");
        require(observer.records < RECORD_COUNT, "truncated tape must omit the last record");
        output.print("rejected\t" + observer.records + "\t" + observer.errors + "\n");
      } else {
        require(observer.descriptors == 1, "missing descriptor");
        require(observer.records == RECORD_COUNT && observer.errors == 0, "incomplete replay");
        require(stats.getReadBytes() == Files.size(file), "physical-byte mismatch");
        output.print("complete\t" + observer.records + "\t" + stats.getReadBytes() + "\n");
      }
      output.close();
      require(!output.checkError(), "observation flush/close failure");
    }
  }

  private static Set<String> names() {
    Set<String> result = new TreeSet<>();
    for (String name : CASES) {
      result.add(name + ".qtp");
      result.add(name + ".tsv");
    }
    result.add(TRUNCATED);
    return result;
  }

  private static void verify(Path directory, Path expected) throws IOException {
    for (Path root : Arrays.asList(directory, expected)) {
      try (Stream<Path> paths = Files.list(root)) {
        Set<String> actual = paths.map(p -> p.getFileName().toString()).collect(Collectors.toSet());
        require(actual.equals(names()), "unexpected artifact set");
      }
    }
    for (String name : names()) {
      Path actual = directory.resolve(name);
      Path golden = expected.resolve(name);
      require(Files.isRegularFile(actual) && Files.isRegularFile(golden), "regular files required");
      require(Files.size(actual) > 0 && Files.size(actual) <= MAX_FILE_BYTES, "artifact budget");
      require(Files.size(golden) > 0 && Files.size(golden) <= MAX_FILE_BYTES, "expectation budget");
      require(
          Arrays.equals(Files.readAllBytes(actual), Files.readAllBytes(golden)),
          "artifact bytes differ: " + name);
    }
  }

  private static void generate(Path directory) throws Exception {
    Files.createDirectory(directory);
    for (String name : CASES) {
      Path file = directory.resolve(name + ".qtp");
      write(file, scheme(name), name);
      read(file, directory.resolve(name + ".tsv"), name, false);
    }
    byte[] bytes = Files.readAllBytes(directory.resolve("quote-message.qtp"));
    Files.write(directory.resolve(TRUNCATED), Arrays.copyOf(bytes, bytes.length - 2));
    System.out.println(
        "GENERATED: three production writer/reader cases; negative tape unqualified");
  }

  private static void negatives(Path directory, Path expected, Path work) throws Exception {
    verify(directory, expected);
    Files.createDirectory(work);
    for (String mutation : Arrays.asList("missing", "extra", "empty", "wire-byte", "observation")) {
      Path copy = Files.createDirectory(work.resolve(mutation));
      for (String name : names()) {
        Files.copy(directory.resolve(name), copy.resolve(name));
      }
      Path victim = copy.resolve("quote-none.qtp");
      if (mutation.equals("missing")) {
        Files.delete(victim);
      } else if (mutation.equals("extra")) {
        Files.write(copy.resolve("extra"), new byte[] {1});
      } else if (mutation.equals("empty")) {
        Files.write(victim, new byte[0]);
      } else {
        if (mutation.equals("observation")) {
          victim = copy.resolve("quote-none.tsv");
        }
        byte[] bytes = Files.readAllBytes(victim);
        bytes[bytes.length / 2] ^= 1;
        Files.write(victim, bytes);
      }
      boolean rejected = false;
      try {
        verify(copy, expected);
      } catch (IllegalStateException e) {
        rejected = true;
      }
      require(rejected, "mutation accepted: " + mutation);
      System.out.println("rejected\t" + mutation);
    }
    read(directory.resolve(TRUNCATED), work.resolve("truncated.tsv"), "quote-message", true);
    System.out.println("rejected\tproduction-truncation");
    System.out.println("NEGATIVE CHECKS PASSED: five artifact mutations and production truncation");
  }

  private static void helpTests(Path work) throws Exception {
    Files.createDirectory(work);
    String java =
        Paths.get(
                System.getProperty("java.home"),
                "bin",
                System.getProperty("os.name").startsWith("Windows") ? "java.exe" : "java")
            .toString();
    for (String mode : Arrays.asList("noargs", "help", "invalid")) {
      List<String> command =
          new ArrayList<>(
              Arrays.asList(
                  java,
                  "-Xmx32m",
                  "-Dfile.encoding=UTF-8",
                  "-cp",
                  System.getProperty("java.class.path"),
                  RealTapeCorpus.class.getName()));
      if (!mode.equals("noargs")) {
        command.add(mode.equals("help") ? "--help" : "invalid");
      }
      Path stdout = work.resolve(mode + ".stdout");
      Path stderr = work.resolve(mode + ".stderr");
      // File redirects avoid descendant-held pipes. The outer native runner owns all children.
      Process process =
          new ProcessBuilder(command)
              .redirectOutput(stdout.toFile())
              .redirectError(stderr.toFile())
              .start();
      try {
        require(process.waitFor(5, TimeUnit.SECONDS), "child help timeout");
        require(process.exitValue() == (mode.equals("invalid") ? 2 : 0), "child usage exit");
      } finally {
        if (process.isAlive()) {
          process.destroyForcibly();
          require(process.waitFor(1, TimeUnit.SECONDS), "child termination unconfirmed");
        }
      }
      require(Files.size(stdout) <= MAX_FILE_BYTES, "help stdout budget");
      require(Files.size(stderr) <= MAX_FILE_BYTES, "help stderr budget");
      if (!mode.equals("invalid")) {
        require(Files.size(stderr) == 0, "help stderr must be empty");
        require(
            Arrays.equals(Files.readAllBytes(stdout), HELP.getBytes(StandardCharsets.UTF_8)),
            "help output mismatch");
      } else {
        require(Files.size(stdout) == 0 && Files.size(stderr) > 0, "usage stream contract");
      }
    }
    System.out.println("HELP CHECKS PASSED: exact stdout, empty stderr, invalid arguments exit 2");
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      System.out.print(HELP);
    } else if (args.length == 2 && args[0].equals("generate")) {
      generate(Paths.get(args[1]));
    } else if (args.length == 3 && args[0].equals("verify")) {
      verify(Paths.get(args[1]), Paths.get(args[2]));
      System.out.println("VERIFIED: exact artifact set and bytes");
    } else if (args.length == 4 && args[0].equals("negatives")) {
      negatives(Paths.get(args[1]), Paths.get(args[2]), Paths.get(args[3]));
    } else if (args.length == 2 && args[0].equals("help-tests")) {
      helpTests(Paths.get(args[1]));
    } else {
      System.err.println("Invalid arguments; use --help");
      System.exit(2);
    }
  }
}
