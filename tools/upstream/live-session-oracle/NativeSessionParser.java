package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.HeartbeatPayload;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageDescriptor;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

/**
 * Observes native control bytes through the unchanged production QD parser.
 *
 * <p>Caller: {@code java -Xmx256m ... qdcxx.oracle.NativeSessionParser --input FILE --output
 * NEW_FILE}. The caller supplies locked QD classes. A complete synthetic control-only file is
 * preflighted before SDK initialization; callbacks immediately produce bounded ASCII/LF TSV. No
 * arguments and standalone {@code --help} perform no file I/O or SDK initialization. Failures have
 * generic diagnostics to avoid disclosing input metadata. See {@code --help} for limits and exits.
 */
public final class NativeSessionParser {
  private static final int MAX_INPUT_BYTES = 1048576;
  private static final int MAX_FRAMES = 4096;
  private static final int MAX_ENTRIES = 256;
  private static final int MAX_TOTAL_ENTRIES = 4096;
  private static final int MAX_TEXT_BYTES = 16384;
  private static final int MAX_TEXT_UNITS = 4096;
  private static final int MAX_TOTAL_TEXT_UNITS = 65536;
  private static final int MAX_ROWS = 65536;
  private static final int MAX_OUTPUT_BYTES = 8 * 1024 * 1024;
  private static final String SYNTHETIC_AUTHORIZATION = "Basic c3ludGhldGljOnRva2Vu";
  private static final String HELP =
      "NativeSessionParser - observe native controls with the production QD parser\n"
          + "Usage: NativeSessionParser --input FILE --output NEW_FILE\n"
          + "       NativeSessionParser [--help]\n"
          + "Options (required together; no operational defaults; either order):\n"
          + "  --input FILE       Read one complete synthetic binary QTP file.\n"
          + "  --output NEW_FILE  Create an ASCII/LF TSV file; its parent must exist.\n"
          + "  --help             Show help without file I/O or QD initialization.\n"
          + "No arguments show identical help. Existing output paths are rejected.\n"
          + "Only Heartbeat (0), DescribeProtocol (1), and empty envelopes are accepted.\n"
          + "EndpointId trailers, other message types, unknown heartbeat bits, and malformed\n"
          + "or incomplete input fail before production parsing. Synthetic input only:\n"
          + "nonempty authorization must be exactly Basic c3ludGhldGljOnRva2Vu.\n"
          + "Authorization output is REDACTED; null and empty remain distinct.\n"
          + "Limits: 1048576 input bytes, 4096 frames, 256 entries per properties/message\n"
          + "list, 4096 total property/message entries, IDs -1..65535, 16384 wire bytes\n"
          + "and 4096 UTF-16 units per text, 65536 total text units, 65536 output rows,\n"
          + "8388608 output bytes. Run with -Xmx256m; heap is not a process RSS limit.\n"
          + "Output: kind/index/section/message_id/key/value TSV; text is N (null) or\n"
          + "U followed by four uppercase hexadecimal digits per UTF-16 unit (empty U).\n"
          + "The index is the zero-based global callback ordinal. Empty heartbeats are skipped.\n"
          + "No input is modified. A failed write may leave a partial NEW_FILE.\n"
          + "Exit codes: 0 success/help, 1 operational/parser/resource/help-output failure,\n"
          + "3 invalid, duplicate, missing or incomplete arguments (values are not echoed).\n";

  private NativeSessionParser() {}

  private static IOException invalid() {
    return new IOException("invalid synthetic control input");
  }

  private static byte[] readInput(Path path) throws IOException {
    if (!Files.isRegularFile(path)) {
      throw invalid();
    }
    try (InputStream stream = Files.newInputStream(path)) {
      ByteArrayOutputStream output = new ByteArrayOutputStream();
      byte[] chunk = new byte[8192];
      while (true) {
        int count =
            stream.read(chunk, 0, Math.min(chunk.length, MAX_INPUT_BYTES + 1 - output.size()));
        if (count < 0) {
          return output.toByteArray();
        }
        output.write(chunk, 0, count);
        if (output.size() > MAX_INPUT_BYTES) {
          throw invalid();
        }
      }
    }
  }

  /** Resource and syntax gate only; this is not the compatibility observation. */
  private static final class Preflight {
    private final byte[] bytes;
    private int position;
    private int limit;
    private int entries;
    private int textUnits;

    private Preflight(byte[] bytes) {
      this.bytes = bytes;
      limit = bytes.length;
    }

    private int octet() throws IOException {
      if (position == limit) {
        throw invalid();
      }
      return bytes[position++] & 255;
    }

    private long compact() throws IOException {
      int first = octet();
      int width = 1;
      for (int flag = 128; width < 9 && (first & flag) != 0; flag >>>= 1) {
        width++;
      }
      long value = first & (width == 9 ? 0 : (1 << (8 - width)) - 1);
      for (int i = 1; i < width; i++) {
        value = (value << 8) | octet();
      }
      int bits = width == 9 ? 64 : width * 7;
      return (value << (64 - bits)) >> (64 - bits);
    }

    private int compactInt() throws IOException {
      // Production readCompactInt consumes every encoded byte and retains the low 32 bits.
      return (int) compact();
    }

    private int count(boolean longCount) throws IOException {
      long count = longCount ? compact() : compactInt();
      if (count < 0 || count > MAX_ENTRIES || count > MAX_TOTAL_ENTRIES - entries) {
        throw invalid();
      }
      entries += (int) count;
      return (int) count;
    }

    private String text() throws IOException {
      long length = compact();
      if (length == -1) {
        return null;
      }
      if (length < 0 || length > MAX_TEXT_BYTES || length > limit - position) {
        throw invalid();
      }
      int end = position + (int) length;
      StringBuilder output = new StringBuilder();
      while (position < end) {
        int first = octet();
        int continuation;
        int codepoint;
        if (first < 128) {
          continuation = 0;
          codepoint = first;
        } else if ((first & 224) == 192) {
          continuation = 1;
          codepoint = first & 31;
        } else if ((first & 240) == 224) {
          continuation = 2;
          codepoint = first & 15;
        } else if ((first & 248) == 240) {
          continuation = 3;
          codepoint = first & 7;
        } else {
          throw invalid();
        }
        if (continuation > end - position) {
          throw invalid();
        }
        for (int i = 0; i < continuation; i++) {
          int next = octet();
          if ((next & 192) != 128) {
            throw invalid();
          }
          codepoint = (codepoint << 6) | (next & 63);
        }
        // The production reader accepts overlong forms and CESU-8 surrogates.
        if (codepoint > 0x10FFFF) {
          throw invalid();
        }
        int units = codepoint >= 65536 ? 2 : 1;
        if (units > MAX_TEXT_UNITS - output.length() || units > MAX_TOTAL_TEXT_UNITS - textUnits) {
          throw invalid();
        }
        output.appendCodePoint(codepoint);
        textUnits += units;
      }
      return output.toString();
    }

    private void properties() throws IOException {
      int count = count(true);
      for (int i = 0; i < count; i++) {
        String key = text();
        String value = text();
        if ("authorization".equals(key)
            && value != null
            && !value.isEmpty()
            && !SYNTHETIC_AUTHORIZATION.equals(value)) {
          throw invalid();
        }
      }
    }

    private void messages() throws IOException {
      int count = count(false);
      for (int i = 0; i < count; i++) {
        int id = compactInt();
        if (id < -1 || id > 65535) {
          throw invalid();
        }
        text();
        properties();
      }
    }

    private void protocol() throws IOException {
      for (int expected : new int[] {'D', 'X', 'P', '3'}) {
        if (octet() != expected) {
          throw invalid();
        }
      }
      properties();
      messages();
      messages();
    }

    private void heartbeat() throws IOException {
      if (position == limit) {
        return;
      }
      int mask = compactInt();
      if ((mask & ~15) != 0) {
        throw invalid();
      }
      if ((mask & 1) != 0) {
        compact();
      }
      for (int flag = 2; flag <= 8; flag <<= 1) {
        if ((mask & flag) != 0) {
          compactInt();
        }
      }
    }

    private void validate() throws IOException {
      int frames = 0;
      while (position < bytes.length) {
        if (++frames > MAX_FRAMES) {
          throw invalid();
        }
        limit = bytes.length;
        long length = compact();
        if (length < 0 || length > bytes.length - position) {
          throw invalid();
        }
        limit = position + (int) length;
        if (length != 0) {
          long type = compact();
          if (type == 0) {
            heartbeat();
          } else if (type == 1) {
            protocol();
          } else {
            throw invalid();
          }
        }
        if (position != limit) {
          throw invalid();
        }
      }
    }
  }

  private static String text(String value) {
    if (value == null) {
      return "N";
    }
    StringBuilder output = new StringBuilder("U");
    for (int i = 0; i < value.length(); i++) {
      for (int shift = 12; shift >= 0; shift -= 4) {
        output.append("0123456789ABCDEF".charAt((value.charAt(i) >>> shift) & 15));
      }
    }
    return output.toString();
  }

  // Keep every SDK-linked type behind this lazy boundary: JDK 8 eagerly verifies signatures.
  private static final class Engine {
    private static final class Observation extends MessageConsumerAdapter {
      private final StringBuilder output =
          new StringBuilder("kind\tindex\tsection\tmessage_id\tkey\tvalue\n");
      private int index;
      private int rows = 1;

      private void row(
          String kind, int ordinal, String section, String id, String key, String value) {
        int size =
            kind.length()
                + Integer.toString(ordinal).length()
                + section.length()
                + id.length()
                + key.length()
                + value.length()
                + 6;
        if (rows >= MAX_ROWS || size > MAX_OUTPUT_BYTES - output.length()) {
          throw new IllegalStateException("output budget exceeded");
        }
        rows++;
        output
            .append(kind)
            .append('\t')
            .append(ordinal)
            .append('\t')
            .append(section)
            .append('\t')
            .append(id)
            .append('\t')
            .append(key)
            .append('\t')
            .append(value)
            .append('\n');
      }

      private void properties(int ordinal, String section, String id, Map<String, String> values) {
        List<String> keys = new ArrayList<>(values.keySet());
        keys.sort(Comparator.nullsFirst(Comparator.naturalOrder()));
        for (String key : keys) {
          String value = values.get(key);
          row(
              "protocol",
              ordinal,
              section,
              id,
              text(key),
              "authorization".equals(key) && value != null && !value.isEmpty()
                  ? "REDACTED"
                  : text(value));
        }
      }

      private void messages(int ordinal, String section, Collection<MessageDescriptor> values) {
        List<MessageDescriptor> messages = new ArrayList<>(values);
        messages.sort(
            Comparator.comparingInt(MessageDescriptor::getId)
                .thenComparing(
                    MessageDescriptor::getName, Comparator.nullsFirst(Comparator.naturalOrder())));
        for (MessageDescriptor message : messages) {
          String id = Integer.toString(message.getId());
          row("protocol", ordinal, section, id, text(message.getName()), "-");
          properties(ordinal, section + "-property", id, message.getProperties());
        }
      }

      @Override
      public void processDescribeProtocol(ProtocolDescriptor descriptor, boolean logging) {
        int ordinal = index++;
        properties(ordinal, "property", "-", descriptor.getProperties());
        messages(ordinal, "send", descriptor.getSendMessages());
        messages(ordinal, "receive", descriptor.getReceiveMessages());
      }

      @Override
      public void processHeartbeat(HeartbeatPayload heartbeat) {
        int ordinal = index++;
        int mask =
            (heartbeat.hasTimeMillis() ? 1 : 0)
                | (heartbeat.hasTimeMark() ? 2 : 0)
                | (heartbeat.hasDeltaMark() ? 4 : 0)
                | (heartbeat.hasLagMark() ? 8 : 0);
        row("heartbeat", ordinal, "mask", "-", "-", Integer.toString(mask));
        if (heartbeat.hasTimeMillis()) {
          row(
              "heartbeat",
              ordinal,
              "field",
              "-",
              "timeMillis",
              Long.toString(heartbeat.getTimeMillis()));
        }
        if (heartbeat.hasTimeMark()) {
          row(
              "heartbeat",
              ordinal,
              "field",
              "-",
              "timeMark",
              Integer.toString(heartbeat.getTimeMark()));
        }
        if (heartbeat.hasDeltaMark()) {
          row(
              "heartbeat",
              ordinal,
              "field",
              "-",
              "deltaMark",
              Integer.toString(heartbeat.getDeltaMark()));
        }
        if (heartbeat.hasLagMark()) {
          row(
              "heartbeat",
              ordinal,
              "field",
              "-",
              "lagMark",
              Integer.toString(heartbeat.getLagMark()));
        }
      }

      @Override
      public void handleCorruptedStream() {
        throw new IllegalStateException("corrupted control stream");
      }

      @Override
      public void handleCorruptedMessage(int type) {
        throw new IllegalStateException("corrupted control message");
      }

      @Override
      public void handleUnknownMessage(int type) {
        throw new IllegalStateException("unknown control message");
      }
    }

    private static void observe(byte[] bytes, Path output) throws IOException {
      ByteArrayInput input = new ByteArrayInput(bytes);
      BinaryQTPParser parser =
          new BinaryQTPParser(new DefaultScheme(PentaCodec.INSTANCE, new DataRecord[0]));
      Observation observation = new Observation();
      parser.setInput(input);
      parser.parse(observation);
      if (input.available() != 0) {
        throw invalid();
      }
      Files.write(
          output,
          observation.output.toString().getBytes(StandardCharsets.US_ASCII),
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
    boolean valid = args.length == 4;
    if (valid) {
      for (int i = 0; i < args.length; i += 2) {
        if (args[i + 1].startsWith("--")) {
          valid = false;
        }
        if (args[i].equals("--input") && input == null) {
          input = args[i + 1];
        } else if (args[i].equals("--output") && output == null) {
          output = args[i + 1];
        } else {
          valid = false;
        }
      }
    }
    if (!valid || input == null || input.isEmpty() || output == null || output.isEmpty()) {
      System.err.println("NativeSessionParser: invalid arguments; use --help");
      return 3;
    }
    Path outputPath = Paths.get(output);
    if (Files.exists(outputPath) || !Files.isDirectory(outputPath.toAbsolutePath().getParent())) {
      throw invalid();
    }
    byte[] bytes = readInput(Paths.get(input));
    new Preflight(bytes).validate();
    Engine.observe(bytes, outputPath);
    return 0;
  }

  public static void main(String[] args) {
    int result;
    try {
      result = run(args);
    } catch (Exception | LinkageError | OutOfMemoryError failure) {
      System.err.println("NativeSessionParser: operation failed");
      result = 1;
    }
    if (result != 0) {
      System.exit(result);
    }
  }
}
