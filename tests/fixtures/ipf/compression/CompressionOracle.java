// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal fixture generator; compile/run with JDK8 and the pinned QD classpath from README.md.
// Inputs: --help/--help-ru or one new output directory. Outputs: bounded .bin files and
// outcomes.tsv.
// Errors stop with nonzero exit; existing files are never overwritten. No network or native SDK
// work.
import com.devexperts.io.StreamCompression;
import com.dxfeed.ipf.InstrumentProfileReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.zip.CRC32;
import java.util.zip.Deflater;
import java.util.zip.GZIPOutputStream;

public final class CompressionOracle {
  private static final byte[] IPF =
      "#STOCK::=TYPE,SYMBOL\nSTOCK,IBM\n##COMPLETE\n".getBytes(StandardCharsets.UTF_8);

  private static byte[] concat(byte[]... parts) throws Exception {
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    for (byte[] part : parts) {
      out.write(part);
    }
    return out.toByteArray();
  }

  private static void number(ByteArrayOutputStream out, long n, int bytes) {
    for (int i = 0; i < bytes; i++) {
      out.write((int) (n >>> (i * 8)) & 255);
    }
  }

  private static long crc(byte[] data) {
    CRC32 crc = new CRC32();
    crc.update(data);
    return crc.getValue();
  }

  private static byte[] raw(byte[] data) {
    Deflater def = new Deflater(6, true);
    def.setInput(data);
    def.finish();
    byte[] buf = new byte[4096];
    int size = def.deflate(buf);
    if (!def.finished()) {
      throw new AssertionError("synthetic compressed fixture too large");
    }
    def.end();
    return Arrays.copyOf(buf, size);
  }

  private static byte[] gzip(byte[] data) throws Exception {
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    GZIPOutputStream gzip = new GZIPOutputStream(out);
    gzip.write(data);
    gzip.close();
    return out.toByteArray();
  }

  private static byte[] optionalGzip(byte[] data) throws Exception {
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    out.write(new byte[] {31, (byte) 139, 8, 30, 0, 0, 0, 0, 0, 3});
    out.write(new byte[] {3, 0, 4, 5, 6, 'n', 0, 'c', 0});
    number(out, crc(out.toByteArray()), 2);
    out.write(raw(data));
    number(out, crc(data), 4);
    number(out, data.length, 4);
    return out.toByteArray();
  }

  private static byte[] zip(String name, byte[] data, int method, int descriptor, boolean zip64)
      throws Exception {
    byte[] compressed = method == 0 ? data : raw(data);
    byte[] filename = name.getBytes(StandardCharsets.UTF_8);
    ByteArrayOutputStream extra = new ByteArrayOutputStream();
    if (zip64) {
      number(extra, 1, 2);
      number(extra, 16, 2);
      number(extra, data.length, 8);
      number(extra, compressed.length, 8);
    }
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    number(out, 0x04034b50L, 4);
    number(out, zip64 ? 45 : 20, 2);
    number(out, descriptor != 0 ? 8 : 0, 2);
    number(out, method, 2);
    number(out, 0, 4);
    number(out, descriptor == 0 ? crc(data) : 0, 4);
    number(out, zip64 ? 0xffffffffL : descriptor == 0 ? compressed.length : 0, 4);
    number(out, zip64 ? 0xffffffffL : descriptor == 0 ? data.length : 0, 4);
    number(out, filename.length, 2);
    number(out, extra.size(), 2);
    out.write(filename);
    out.write(extra.toByteArray());
    out.write(compressed);
    if (descriptor != 0) {
      if (descriptor != 2) {
        number(out, 0x08074b50L, 4);
      }
      number(out, crc(data), 4);
      number(out, compressed.length, descriptor == 3 ? 8 : 4);
      number(out, data.length, descriptor == 3 ? 8 : 4);
    }
    return out.toByteArray();
  }

  private static byte[] damaged(byte[] value, int fromEnd) {
    byte[] out = value.clone();
    out[out.length - fromEnd] ^= 1;
    return out;
  }

  /** Create repeatable synthetic ordinary archive metadata without ambient clock input. */
  private static java.util.zip.ZipEntry entry(String name) {
    java.util.zip.ZipEntry entry = new java.util.zip.ZipEntry(name);
    entry.setTime(315532800000L);
    return entry;
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      System.out.println("CompressionOracle OUTPUT_DIRECTORY | --help | --help-ru");
      System.out.println(
          "Writes finite synthetic IPF .bin fixtures and actual JDK8/QD outcomes.tsv.");
      System.out.println(
          "No network or SDK initialization. Existing files are refused. Exit0 success, otherwise"
              + " failure.");
      return;
    }
    if (args.length == 1 && args[0].equals("--help-ru")) {
      System.out.println("CompressionOracle OUTPUT_DIRECTORY | --help | --help-ru");
      System.out.println(
          "Записывает конечные синтетические IPF .bin fixtures и фактические JDK8/QD"
              + " outcomes.tsv.");
      System.out.println(
          "Без сети/SDK. Существующие файлы отклоняются. Код0: успех; иной: ошибка.");
      return;
    }
    if (args.length != 1) {
      throw new IllegalArgumentException("one new output directory required; use --help");
    }
    Path dir = Paths.get(args[0]);
    Files.createDirectories(dir);
    Map<String, byte[]> cases = new LinkedHashMap<String, byte[]>();
    byte[] gzip = gzip(IPF);
    byte[] zip = zip("first.ipf", IPF, 8, 0, false);
    cases.put("plain", IPF);
    cases.put("gzip", gzip);
    cases.put("gzip-optional", optionalGzip(IPF));
    cases.put(
        "gzip-concatenated",
        concat(gzip(Arrays.copyOf(IPF, 12)), gzip(Arrays.copyOfRange(IPF, 12, IPF.length))));
    cases.put("gzip-trailing-junk", concat(gzip, new byte[] {1, 2, 3}));
    cases.put("gzip-next-header-truncated", concat(gzip, new byte[] {31, (byte) 139, 8}));
    cases.put("gzip-truncated", Arrays.copyOf(gzip, gzip.length - 4));
    cases.put("gzip-bad-crc", damaged(gzip, 8));
    cases.put("gzip-missing-marker", gzip(Arrays.copyOf(IPF, IPF.length - 11)));
    cases.put("zip-stored", zip("first.ipf", IPF, 0, 0, false));
    cases.put("zip-deflate", zip);
    cases.put("zip-descriptor", zip("first.ipf", IPF, 8, 1, false));
    cases.put("zip-descriptor-nosignature", zip("first.ipf", IPF, 8, 2, false));
    cases.put("zip64-small", zip("first.ipf", IPF, 8, 0, true));
    cases.put("zip64-descriptor-small", zip("first.ipf", IPF, 8, 1, true));
    cases.put("zip64-descriptor64-small", zip("first.ipf", IPF, 8, 3, true));
    cases.put("zip-stored-descriptor", zip("first.ipf", IPF, 0, 1, false));
    cases.put(
        "zip-directory-first", concat(zip("directory/", new byte[] {1, 2}, 8, 1, false), zip));
    cases.put(
        "zip-followed-file", concat(zip, zip("ignored.ipf", new byte[] {0, 1, 2}, 8, 1, false)));
    cases.put("zip-followed-corruption", concat(zip, new byte[] {80, 75, 3, 4, -1}));
    cases.put("zip-directory-only", zip("directory/", new byte[0], 0, 0, false));
    cases.put("zip-bad-crc", damaged(zip, zip.length - 14));
    cases.put("zip-truncated", Arrays.copyOf(zip, zip.length - 3));
    byte[] descriptor = zip("first.ipf", IPF, 8, 1, false);
    cases.put("zip-truncated-descriptor", Arrays.copyOf(descriptor, descriptor.length - 3));
    cases.put("zip-empty-eocd", new byte[] {80, 75, 5, 6, 0, 0, 0, 0});
    cases.put(
        "zip-descriptor-nosignature-followed",
        concat(zip("first.ipf", IPF, 8, 2, false), new byte[] {80, 75, 5, 6}));
    cases.put(
        "zip-directory-nosignature-first",
        concat(zip("directory/", new byte[] {1, 2}, 8, 2, false), zip));
    cases.put("zip64-stored-small", zip("first.ipf", IPF, 0, 0, true));
    cases.put("gzip-nested", gzip(gzip));
    cases.put("gzip-concat-second-bad", concat(gzip, damaged(gzip, 8)));
    cases.put("gzip-bad-size", damaged(gzip, 1));
    byte[] encrypted = zip.clone();
    encrypted[6] = 1;
    cases.put("zip-encrypted", encrypted);
    byte[] sizeMismatch = zip.clone();
    sizeMismatch[22]++;
    cases.put("zip-bad-size", sizeMismatch);
    ByteArrayOutputStream archiveBytes = new ByteArrayOutputStream();
    java.util.zip.ZipOutputStream archive = new java.util.zip.ZipOutputStream(archiveBytes);
    archive.putNextEntry(entry("directory/"));
    archive.closeEntry();
    archive.putNextEntry(new java.util.zip.ZipEntry("directory/first.ipf"));
    archive.write(IPF);
    archive.closeEntry();
    archive.putNextEntry(new java.util.zip.ZipEntry("ignored.ipf"));
    archive.write(new byte[] {0, 1, 2});
    archive.close();
    cases.put("zip-full-archive", archiveBytes.toByteArray());
    StringBuilder expanded = new StringBuilder("#STOCK::=TYPE,SYMBOL\n");
    for (int n = 0; n < 10000; ++n) {
      expanded.append("STOCK,IBM\n");
    }
    expanded.append("##COMPLETE\n");
    cases.put("gzip-expansion", gzip(expanded.toString().getBytes(StandardCharsets.UTF_8)));
    byte[] wide = zip("first.ipf", IPF, 8, 3, true);
    cases.put(
        "zip64-descriptor64-nosignature-small",
        concat(
            Arrays.copyOf(wide, wide.length - 24),
            Arrays.copyOfRange(wide, wide.length - 20, wide.length)));
    byte[] wideDirectory = zip("directory/", new byte[] {1, 2}, 8, 3, true);
    byte[] unsignedDirectory =
        concat(
            Arrays.copyOf(wideDirectory, wideDirectory.length - 24),
            Arrays.copyOfRange(wideDirectory, wideDirectory.length - 20, wideDirectory.length));
    cases.put("zip64-directory-nosignature-first", concat(unsignedDirectory, zip));
    cases.put(
        "gzip-next-header-invalid",
        concat(gzip, new byte[] {31, (byte) 139, 7, 0, 0, 0, 0, 0, 0, 3}));
    cases.put("gzip-header-truncated", Arrays.copyOf(gzip, 8));
    cases.put("zip-header-truncated", Arrays.copyOf(zip, 12));
    StringBuilder report = new StringBuilder("case\tdecodedBytes\tdecodeOutcome\tipfOutcome\n");
    for (Map.Entry<String, byte[]> entry : cases.entrySet()) {
      Path path = dir.resolve(entry.getKey() + ".bin");
      if (Files.exists(path)) {
        throw new IllegalArgumentException("refuse overwrite " + path);
      }
      Files.write(path, entry.getValue());
      int count = 0;
      String decode = "OK";
      try (InputStream in =
          StreamCompression.detectCompressionByHeaderAndDecompress(
              new ByteArrayInputStream(entry.getValue()))) {
        byte[] buf = new byte[7];
        int n;
        while ((n = in.read(buf)) != -1) {
          count += n;
          if (count > 1048576) {
            throw new AssertionError("probe expansion limit");
          }
        }
      } catch (java.io.IOException e) {
        decode = e.getClass().getSimpleName() + ":" + e.getMessage();
      }
      String ipf;
      try {
        ipf =
            "OK:"
                + new InstrumentProfileReader()
                    .read(new ByteArrayInputStream(entry.getValue()), "synthetic.ipf")
                    .size();
      } catch (java.io.IOException e) {
        ipf = e.getClass().getSimpleName() + ":" + e.getMessage();
      }
      String row = entry.getKey() + "\t" + count + "\t" + decode + "\t" + ipf + "\n";
      report.append(row);
      System.out.print(row);
    }
    Files.write(dir.resolve("outcomes.tsv"), report.toString().getBytes(StandardCharsets.UTF_8));
  }
}
