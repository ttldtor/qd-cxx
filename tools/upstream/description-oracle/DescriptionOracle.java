package com.devexperts.qd.qtp;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import java.io.EOFException;
import java.io.IOException;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.StringJoiner;

/** Bounded P08 observations. Diagnostic raw projection is distinct from parser installation. */
public final class DescriptionOracle {
  private static String units(String value) {
    if (value == null) {
      return "null";
    }
    StringBuilder out = new StringBuilder();
    for (int i = 0; i < value.length(); i++) {
      out.append(String.format("%04x", (int) value.charAt(i)));
    }
    return out.toString();
  }

  private static String hex(byte[] value) {
    StringBuilder out = new StringBuilder();
    for (byte b : value) {
      out.append(String.format("%02x", b & 255));
    }
    return out.length() == 0 ? "-" : out.toString();
  }

  private static String joined(List<String> values) {
    return values.isEmpty() ? "-" : String.join(";", values);
  }

  private static final class Parser extends BinaryQTPParser {
    final List<String> installed = new ArrayList<>();

    Parser() {
      super(new DefaultScheme(PentaCodec.INSTANCE));
    }

    @Override
    protected void remapRecord(int id, BinaryRecordDesc desc) {
      super.remapRecord(id, desc);
      installed.add(Integer.toString(id));
    }

    String map() throws Exception {
      Field field = BinaryQTPParser.class.getDeclaredField("recordMap");
      field.setAccessible(true);
      BinaryRecordDesc[] map = (BinaryRecordDesc[]) field.get(this);
      List<String> values = new ArrayList<>();
      if (map != null) {
        for (int i = 0; i < map.length; i++) {
          if (map[i] != null) {
            Field names = BinaryRecordDesc.class.getDeclaredField("names");
            Field types = BinaryRecordDesc.class.getDeclaredField("types");
            Field count = BinaryRecordDesc.class.getDeclaredField("nDesc");
            names.setAccessible(true);
            types.setAccessible(true);
            count.setAccessible(true);
            StringJoiner fields = new StringJoiner(",");
            for (int j = 0; j < count.getInt(map[i]); j++) {
              fields.add(
                  units(((String[]) names.get(map[i]))[j]) + ":" + ((int[]) types.get(map[i]))[j]);
            }
            values.add(i + "/" + fields);
          }
        }
      }
      return joined(values);
    }
  }

  private static void observe(String id, byte[] payload) throws Exception {
    // Independent diagnostic walk uses locked Java primitives and source-checked validation order.
    // It is not a BinaryQTPParser callback or a local field constructor.
    ByteArrayInput raw = new ByteArrayInput(payload);
    List<String> descriptions = new ArrayList<>();
    String stage = "END";
    String status = "OK";
    String cause = "-";
    long offset = 0;
    try {
      while (raw.hasAvailable()) {
        stage = "SENDER_ID";
        offset = raw.totalPosition();
        int sender = raw.readCompactInt();
        stage = "RECORD_NAME";
        offset = raw.totalPosition();
        String name = raw.readUTFString();
        stage = "FIELD_COUNT";
        offset = raw.totalPosition();
        int count = raw.readCompactInt();
        stage = "RECORD_HEADER";
        offset = raw.totalPosition();
        if (sender < 0 || name == null || name.isEmpty() || count < 0) {
          status = "INVALID_RECORD";
          break;
        }
        if (sender > 1024 || count > 16) {
          throw new IllegalArgumentException("Probe safety bound");
        }
        StringJoiner fields = new StringJoiner(",");
        for (int i = 0; i < count; i++) {
          stage = "FIELD_NAME";
          offset = raw.totalPosition();
          String field = raw.readUTFString();
          stage = "FIELD_TYPE";
          offset = raw.totalPosition();
          int type = raw.readCompactInt();
          stage = "FIELD_HEADER";
          offset = raw.totalPosition();
          if (field == null || field.isEmpty() || type < 0 || type > 255) {
            status = "INVALID_FIELD";
            break;
          }
          fields.add(units(field) + ":" + type);
        }
        if (!status.equals("OK")) {
          break;
        }
        descriptions.add(sender + "/" + units(name) + "/" + fields);
        stage = "END";
        offset = raw.totalPosition();
      }
    } catch (EOFException e) {
      status = "TRUNCATED_PAYLOAD";
      cause = e.getClass().getName();
    } catch (IOException e) {
      status = "INVALID_STRING";
      cause = e.getClass().getName();
    }
    ByteArrayOutput out = new ByteArrayOutput();
    out.writeCompactLong(payload.length + 1);
    out.writeCompactInt(2);
    out.write(payload);
    byte[] frame = out.toByteArray();
    Parser parser = new Parser();
    ByteArrayInput input = new ByteArrayInput(frame);
    parser.setInput(input);
    List<String> errors = new ArrayList<>();
    parser.parse(
        new MessageConsumerAdapter() {
          @Override
          public void handleCorruptedMessage(int type) {
            errors.add("message:" + type);
          }

          @Override
          public void handleCorruptedStream() {
            errors.add("stream");
          }
        });
    System.out.println(
        String.join(
            "\t",
            "case",
            id,
            hex(payload),
            hex(frame),
            status,
            stage,
            Long.toString(raw.totalPosition()),
            Long.toString(offset),
            joined(descriptions),
            cause,
            joined(parser.installed),
            parser.map(),
            joined(errors),
            Long.toString(input.totalPosition())));
  }

  public static void main(String[] args) throws Exception {
    System.setProperty("com.devexperts.qd.qtp.maxMessageSize", "1024");
    for (String line : Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8)) {
      if (line.startsWith("#")) {
        continue;
      }
      String[] columns = line.split("\t", -1);
      if (columns.length != 3 || !columns[1].matches("-|(?:[0-9a-f]{2})+")) {
        throw new IllegalArgumentException("Invalid stimulus row");
      }
      String hex = columns[1].equals("-") ? "" : columns[1];
      if (hex.length() > 512) {
        throw new IllegalArgumentException("Probe payload bound");
      }
      byte[] payload = new byte[hex.length() / 2];
      for (int i = 0; i < payload.length; i++) {
        payload[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
      }
      observe(columns[0], payload);
    }
    System.out.println("complete\tdescriptions");
  }
}
