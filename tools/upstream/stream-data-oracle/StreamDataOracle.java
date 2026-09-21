package com.devexperts.qd.qtp;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordSource;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Bounded complete-message probes; only production callbacks supply observed records. */
public final class StreamDataOracle {
  private static byte[] hex(String text) {
    if (text.equals("-")) {
      return new byte[0];
    }
    if (!text.matches("(?:[0-9a-f]{2})+")) {
      throw new IllegalArgumentException("wire hex");
    }
    byte[] bytes = new byte[text.length() / 2];
    for (int i = 0; i < bytes.length; i++) {
      bytes[i] = (byte) Integer.parseInt(text.substring(i * 2, i * 2 + 2), 16);
    }
    return bytes;
  }

  private static String units(String value) {
    StringBuilder out = new StringBuilder();
    for (int i = 0; i < value.length(); i++) {
      out.append(String.format("%04x", (int) value.charAt(i)));
    }
    return out.length() == 0 ? "-" : out.toString();
  }

  private static String bytes(byte[] value) {
    return value.length == 0 ? "-" : ProtocolOracle.hex(value);
  }

  private static void frame(ByteArrayOutput stream, int type, byte[] payload) throws Exception {
    stream.writeCompactLong(payload.length + 1);
    stream.writeCompactInt(type);
    stream.write(payload);
  }

  private static byte[] descriptions() throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    String[][] entries = {
      {"7", "R", "x", "unknown", "y", "xdot", "x"},
      {"9", "S", "v"},
      {"31", "Unknown", "a", "b"},
      {"4096", "E"},
      {"11", "R"},
      {"12", "R", "x"}
    };
    for (String[] e : entries) {
      out.writeCompactInt(Integer.parseInt(e[0]));
      out.writeUTFString(e[1]);
      out.writeCompactInt(e.length - 2);
      for (int i = 2; i < e.length; i++) {
        out.writeUTFString(e[i]);
        out.writeCompactInt(8);
      }
    }
    return out.toByteArray();
  }

  private static final class Observer extends MessageConsumerAdapter {
    int records, callbacks, errors;

    protected void processData(DataIterator it, MessageType type) {
      int count = 0;
      RecordCursor c;
      while ((c = ((RecordSource) it).next()) != null) {
        StringBuilder values = new StringBuilder();
        for (int i = 0; i < c.getRecord().getIntFieldCount(); i++) {
          if (i != 0) {
            values.append(',');
          }
          values.append(c.getInt(i));
        }
        ProtocolOracle.row(
            "record",
            records++,
            c.getRecord().getId(),
            units(c.getRecord().getScheme().getCodec().decode(c.getCipher(), c.getSymbol())),
            values.length() == 0 ? "-" : values.toString(),
            c.getEventFlags());
        count++;
      }
      ProtocolOracle.row("callback", callbacks++, type.getId(), count);
    }

    public void handleCorruptedMessage(int type) {
      errors++;
      ProtocolOracle.row("error", "message", type);
    }

    public void handleCorruptedStream() {
      errors++;
      ProtocolOracle.row("error", "stream", -1);
    }

    public void handleUnknownMessage(int type) {
      errors++;
      ProtocolOracle.row("error", "unknown", type);
    }
  }

  private static void observe(String name, byte[][] payloads) throws Exception {
    DefaultRecord r = ProtocolOracle.record(1, "R", "y", "x", "z");
    r.addFieldAlias("x", "xdot");
    DataScheme local =
        ProtocolOracle.scheme(ProtocolOracle.record(0, "S", "v"), r, ProtocolOracle.record(2, "E"));
    ProtocolOracle.row("case", name);
    byte[] desc = descriptions();
    ProtocolOracle.row("descriptions", bytes(desc));
    ByteArrayOutput stream = new ByteArrayOutput();
    frame(stream, 2, desc);
    for (int i = 0; i < payloads.length; i++) {
      ProtocolOracle.row("payload", i, bytes(payloads[i]));
      frame(stream, 15, payloads[i]);
    }
    ProtocolOracle.row("wire", bytes(stream.toByteArray()));
    BinaryQTPParser parser = new BinaryQTPParser(local);
    parser.setReadEventTimeSequence(false);
    parser.setEventTimeSequence(0);
    parser.setFieldReplacers(null);
    ByteArrayInput input = new ByteArrayInput(stream.toByteArray());
    parser.setInput(input);
    Observer observer = new Observer();
    parser.parse(observer);
    ProtocolOracle.row(
        "end",
        input.totalPosition(),
        input.available(),
        observer.records,
        observer.callbacks,
        observer.errors);
    if (input.available() != 0) {
      throw new AssertionError("Complete frames left pending");
    }
  }

  public static void main(String[] args) throws Exception {
    ProtocolOracle.row(
        "profile",
        "QD-3.355",
        "ordinary-type-8",
        "metadata=false",
        "replacers=null",
        "event-time=0",
        "complete-frames");
    Set<String> names = new HashSet<>();
    for (String line : Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8)) {
      if (line.isEmpty() || line.startsWith("#")) {
        continue;
      }
      String[] row = line.split("\t", -1);
      if (row.length != 3
          || !names.add(row[0])
          || (!row[2].equals("whole") && !row[2].equals("prefix"))) {
        throw new IllegalArgumentException("input row");
      }
      String[] parts = row[1].split(";", -1);
      byte[][] payloads = new byte[parts.length][];
      for (int i = 0; i < parts.length; i++) {
        payloads[i] = hex(parts[i]);
      }
      observe(row[0], payloads);
      if (row[2].equals("prefix")) {
        if (parts.length != 1) {
          throw new IllegalArgumentException("prefix messages");
        }
        for (int n = 0; n < payloads[0].length; n++) {
          observe(row[0] + "/prefix-" + n, new byte[][] {Arrays.copyOf(payloads[0], n)});
        }
      }
    }
    if (names.isEmpty()) {
      throw new IllegalArgumentException("empty inputs");
    }
    ProtocolOracle.row("complete", "stream-data");
  }
}
