// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.kit.CompactIntField;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.devexperts.transport.stats.ConnectionStats;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/**
 * Production FileWriter/FileReader evidence; Invoke-Oracle.ps1 supplies exact pinned class roots.
 */
public final class ContainerOracle {
  private static final int VALUE = 913;
  private static final String SYMBOL = "FLAGS";
  private static final DefaultRecord RECORD =
      new DefaultRecord(
          0,
          "Container",
          false,
          new DataIntField[] {new CompactIntField(0, "Container.F0")},
          new DataObjField[0]);
  private static final DataScheme SCHEME = new DefaultScheme(PentaCodec.INSTANCE, RECORD);
  private static final List<String> ROWS = new ArrayList<>();

  private static void require(boolean condition, String message) {
    if (!condition) {
      throw new AssertionError(message);
    }
  }

  private static String hash(byte[] bytes) throws Exception {
    StringBuilder result = new StringBuilder();
    for (byte value : MessageDigest.getInstance("SHA-256").digest(bytes)) {
      result.append(String.format("%02x", value & 255));
    }
    return result.toString();
  }

  private static byte[] concatenate(byte[] first, byte[] second) {
    byte[] result = Arrays.copyOf(first, first.length + second.length);
    System.arraycopy(second, 0, result, first.length, second.length);
    return result;
  }

  private static byte[] compress(StreamCompression kind, byte[] plain) throws IOException {
    ByteArrayOutputStream output = new ByteArrayOutputStream();
    try (OutputStream compressed = kind.compress(output, "tape.qtp")) {
      compressed.write(plain);
    }
    return output.toByteArray();
  }

  private static byte[] twoEntries(byte[] plain) throws IOException {
    ByteArrayOutputStream output = new ByteArrayOutputStream();
    try (ZipOutputStream zip = new ZipOutputStream(output)) {
      for (String name : new String[] {"first.qtp", "second.qtp"}) {
        ZipEntry entry = new ZipEntry(name);
        entry.setTime(0);
        zip.putNextEntry(entry);
        zip.write(plain);
        zip.closeEntry();
      }
    }
    return output.toByteArray();
  }

  private static byte[] write(Path path, StreamCompression kind) throws Exception {
    FileWriter configuration = new FileWriter();
    configuration.setFormat(FileFormat.BINARY);
    configuration.setCompression(kind);
    configuration.setTime(TimestampsType.NONE);
    configuration.setSaveAs(MessageType.STREAM_DATA);
    RecordBuffer records = new RecordBuffer(RecordMode.FLAGGED_DATA);
    RecordCursor cursor = records.add(RECORD, SCHEME.getCodec().encode(SYMBOL), null);
    cursor.setInt(0, VALUE);
    try (FileWriter writer = configuration.open(path.toString(), SCHEME)) {
      writer.visitData(records, MessageType.STREAM_DATA);
    }
    return Files.readAllBytes(path);
  }

  private static final class Seen extends MessageConsumerAdapter {
    int records;
    int errors;

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      require(type == MessageType.STREAM_DATA, "unexpected callback family");
      RecordCursor cursor;
      while ((cursor = ((RecordSource) iterator).next()) != null) {
        require(cursor.getRecord().getName().equals("Container"), "record name");
        require(cursor.getInt(0) == VALUE, "field value");
        records++;
      }
    }

    @Override
    public void handleCorruptedMessage(int type) {
      errors++;
    }

    @Override
    public void handleCorruptedStream() {
      errors++;
    }
  }

  private static void read(Path path, String label, int expectedRecords) throws Exception {
    FileReaderParams.Default parameters = new FileReaderParams.Default();
    parameters.setTime(TimestampsType.NONE);
    parameters.setSpeed(FileReaderParams.MAX_SPEED);
    parameters.setCycle(false);
    parameters.setResyncOn(null);
    FileReader reader = new FileReader(path.toString(), new ConnectionStats(), parameters);
    reader.setScheme(SCHEME);
    Seen seen = new Seen();
    try {
      reader.readInto(seen);
    } finally {
      reader.close();
    }
    require(seen.records == expectedRecords && seen.errors == 0, label + " FileReader observation");
    ROWS.add("file\t" + label + "\t" + seen.records + "\t" + seen.errors);
  }

  private static void direct(
      String label,
      byte[] source,
      StreamCompression kind,
      boolean shortRead,
      int expectedBytes,
      boolean expectedFailure)
      throws Exception {
    ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    InputStream raw =
        new FilterInputStream(new ByteArrayInputStream(source)) {
          @Override
          public int read(byte[] target, int offset, int size) throws IOException {
            return super.read(target, offset, shortRead ? Math.min(size, 1) : size);
          }

          @Override
          public int available() {
            return 0;
          }
        };
    boolean failed = false;
    String outcome = "OK";
    try (InputStream ownedRaw = raw;
        InputStream input = kind.decompress(ownedRaw)) {
      byte[] buffer = new byte[31];
      int count;
      while ((count = input.read(buffer)) != -1) {
        bytes.write(buffer, 0, count);
      }
    } catch (IOException failure) {
      failed = true;
      outcome = failure.getClass().getSimpleName();
    }
    require(
        bytes.size() == expectedBytes && failed == expectedFailure, label + " direct observation");
    ROWS.add(
        "direct\t"
            + label
            + "\t"
            + (shortRead ? 1 : 0)
            + "\t"
            + outcome
            + "\t"
            + bytes.size()
            + "\t"
            + hash(bytes.toByteArray()));
  }

  /** Invoke with one private output directory; runtime logs may contain its path. */
  public static void main(String[] args) throws Exception {
    require(args.length == 1, "one output directory required");
    Path root = Paths.get(args[0]);
    Files.createDirectories(root);
    byte[] plain = write(root.resolve("none.qtp"), StreamCompression.NONE);
    read(root.resolve("none.qtp"), "none", 1);
    for (StreamCompression kind :
        new StreamCompression[] {StreamCompression.GZIP, StreamCompression.ZIP}) {
      String suffix = kind.getExtension();
      byte[] valid = write(root.resolve("tape.qtp" + suffix), kind);
      Map<String, byte[]> cases = new LinkedHashMap<>();
      cases.put("valid", valid);
      cases.put("header-short", Arrays.copyOf(valid, 3));
      cases.put("trailer-short", Arrays.copyOf(valid, valid.length - 4));
      cases.put("trailing-junk", concatenate(valid, new byte[] {11, 22, 33}));
      byte[] crc = valid.clone();
      if (kind == StreamCompression.GZIP) {
        crc[crc.length - 8] ^= 1;
        cases.put("members-two", concatenate(valid, compress(kind, plain)));
      } else {
        for (int index = 0; index + 8 < crc.length; index++) {
          if (crc[index] == 'P'
              && crc[index + 1] == 'K'
              && crc[index + 2] == 7
              && crc[index + 3] == 8) {
            crc[index + 4] ^= 1;
            break;
          }
        }
        cases.put("entries-two", twoEntries(plain));
      }
      cases.put("crc", crc);
      for (Map.Entry<String, byte[]> entry : cases.entrySet()) {
        String label = kind + "-" + entry.getKey();
        Path file = root.resolve(label + ".qtp" + suffix);
        Files.write(file, entry.getValue());
        int records =
            entry.getKey().equals("header-short")
                ? 0
                : entry.getKey().equals("members-two") ? 2 : 1;
        read(file, label, records);
        for (boolean shortRead : new boolean[] {false, true}) {
          int expected = entry.getKey().equals("header-short") ? 0 : plain.length;
          if (entry.getKey().equals("members-two") && !shortRead) {
            expected *= 2;
          }
          boolean failure =
              entry.getKey().equals("header-short")
                  || entry.getKey().equals("crc")
                  || (kind == StreamCompression.GZIP && entry.getKey().equals("trailer-short"));
          direct(label, entry.getValue(), kind, shortRead, expected, failure);
        }
      }
    }
    Files.write(root.resolve("results.tsv"), ROWS, StandardCharsets.UTF_8);
    System.out.println("container-oracle\tPASS");
  }
}
