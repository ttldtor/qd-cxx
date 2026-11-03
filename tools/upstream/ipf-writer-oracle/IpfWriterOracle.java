// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.ipf.InstrumentProfile;
import com.dxfeed.ipf.InstrumentProfileField;
import com.dxfeed.ipf.InstrumentProfileReader;
import com.dxfeed.ipf.InstrumentProfileWriter;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Deterministic complete local writer bytes and actual reader projections. */
public final class IpfWriterOracle {
  private static final Map<String, List<InstrumentProfile>> CASES = new LinkedHashMap<>();
  private static final Map<String, String> STATUS = new LinkedHashMap<>();

  private static InstrumentProfile profile(String type, String symbol, String... fields) {
    InstrumentProfile p = new InstrumentProfile();
    p.setType(type);
    p.setSymbol(symbol);
    for (int i = 0; i < fields.length; i += 2) p.setField(fields[i], fields[i + 1]);
    return p;
  }

  private static void add(String id, String status, InstrumentProfile... p) {
    CASES.put(id, Arrays.asList(p));
    STATUS.put(id, status);
  }

  private static String hex(String s) {
    StringBuilder b = new StringBuilder();
    for (char c : s.toCharArray()) {
      String h = Integer.toHexString(c);
      for (int n = h.length(); n < 4; n++) b.append('0');
      b.append(h);
    }
    return b.toString();
  }

  private static String dump(InstrumentProfile p) {
    StringBuilder b = new StringBuilder("PROFILE");
    for (InstrumentProfileField f : InstrumentProfileField.values()) {
      if (!p.getField(f.name()).isEmpty())
        b.append('\t').append(hex(f.name())).append('=').append(hex(p.getField(f.name())));
    }
    TreeSet<String> names = new TreeSet<>();
    p.addNonEmptyCustomFieldNames(names);
    for (String name : names)
      b.append('\t').append(hex(name)).append('=').append(hex(p.getField(name)));
    return b.toString();
  }

  private static void cases() {
    add("empty", "OK");
    add("basic", "OK", profile("STOCK", "IBM"));
    add("empty-type", "OK", profile("", ""));
    add("empty-symbol", "OK", profile("STOCK", ""));
    InstrumentProfile all = profile("STOCK", "all");
    for (InstrumentProfileField f : InstrumentProfileField.values()) {
      if (f != InstrumentProfileField.TYPE
          && f != InstrumentProfileField.SYMBOL
          && !f.isNumericField()) all.setField(f.name(), "value:" + f.name());
    }
    add("all-text-fields", "OK", all);
    add(
        "typed",
        "OK",
        profile(
            "OPTION",
            "A",
            "ICB",
            "123",
            "SIC",
            "456",
            "MULTIPLIER",
            "100.5",
            "SPC",
            "0.125",
            "STRIKE",
            "0.000012345678901234005",
            "EXPIRATION",
            "2026-11-03",
            "LAST_TRADE",
            "1581-12-400"));
    add(
        "nonfinite",
        "OK",
        profile("OPTION", "A", "MULTIPLIER", "NaN", "SPC", "Infinity", "STRIKE", "-Infinity"));
    InstrumentProfile zero = profile("OPTION", "zero", "ICB", "0", "EXPIRATION", "1970-01-01");
    zero.setStrike(-0.0);
    zero.setMultiplier(0.0);
    add("zero-defaults", "OK", zero);
    add("custom-order", "OK", profile("STOCK", "A", "z", "last", "a", "first", "x", "middle"));
    add(
        "custom-case-empty-name",
        "OK",
        profile(
            "STOCK", "A", "", "empty", "symbol", "lower", "Symbol", "mixed", "TYPE ", "custom"));
    add("custom-quotes", "OK", profile("STOCK", "A", "a,\"\r\n", "v,\"\r\n", "x", "tab\tspace "));
    add(
        "escaping",
        "OK",
        profile("STOCK", "A,\"\r\n", "DESCRIPTION", "l1\nl2\rl3\r\nl4", "LOCAL_SYMBOL", "\"\""));
    add(
        "unicode",
        "OK",
        profile("STOCK", "\u20ac\ud83d\ude00", "\ud83d\ude00", "\ud800\udc00\udbff\udfff"));
    add("embedded-nul", "OK", profile("STOCK", "A\u0000B", "DESCRIPTION", "\u0000"));
    add("bom-value", "OK", profile("\ufeff", "\ufeff", "\ufeff", "\ufeff"));
    add(
        "types-order",
        "OK",
        profile("STOCK", "1"),
        profile("BOND", "2"),
        profile("ZZZ", "3"),
        profile("AAA", "4"));
    add(
        "union-fields",
        "OK",
        profile("STOCK", "A", "DESCRIPTION", "first", "x", "1"),
        profile("STOCK", "B", "COUNTRY", "US", "y", "2"));
    add(
        "duplicate-symbols",
        "OK",
        profile("STOCK", "A"),
        profile("STOCK", "A", "DESCRIPTION", "second"));
    add("removed", "OK", profile("REMOVED", "A"));
    add("removed-empty-extra", "OK", profile("REMOVED", "A", "DESCRIPTION", "", "x", ""));
    add("removed-standard", "REMOVED_FIELDS", profile("REMOVED", "A", "DESCRIPTION", "lost"));
    add("removed-custom", "REMOVED_FIELDS", profile("REMOVED", "A", "x", "lost"));
    add("reserved-flush", "INVALID_TYPE", profile("##", "A"));
    add("reserved-complete", "INVALID_TYPE", profile("##COMPLETE", "A"));
    add("reserved-header", "INVALID_TYPE", profile("#X::=TYPE", "A"));
    add("hash-type", "OK", profile("#comment", "A"));
    add("quoted-type", "OK", profile("odd,\"\r\n", "A"));
    add("unpaired-high", "UNREPRESENTABLE_UTF16", profile("STOCK", "A\ud800B"));
    add("unpaired-low-name", "UNREPRESENTABLE_UTF16", profile("STOCK", "A", "x\udc00", "v"));
    add(
        "paired-boundaries",
        "OK",
        profile("\ud800\udc00", "\udbff\udfff", "\ud800\udc00", "\udbff\udfff"));
    add("noncharacters", "OK", profile("STOCK", "\ufffe\uffff", "DESCRIPTION", "\ufdd0"));
    add(
        "typed-boundaries",
        "OK",
        profile(
            "OPTION",
            "limits",
            "ICB",
            "-2147483648",
            "SIC",
            "2147483647",
            "MULTIPLIER",
            "1.7976931348623157E308",
            "SPC",
            "4.9E-324",
            "EXPIRATION",
            "0001-01-01",
            "LAST_TRADE",
            "9999-12-31"));
  }

  private static byte[] write(List<InstrumentProfile> profiles) throws IOException {
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    new InstrumentProfileWriter().write(out, profiles);
    return out.toByteArray();
  }

  private static List<String> read(byte[] bytes) {
    List<String> lines = new ArrayList<>();
    InstrumentProfileReader r = new InstrumentProfileReader();
    try {
      for (InstrumentProfile p : r.read(new ByteArrayInputStream(bytes))) lines.add(dump(p));
      lines.add("EOF\t" + r.wasComplete());
    } catch (Exception e) {
      lines.add("ERROR\t" + hex(e.getClass().getSimpleName() + ":" + e.getMessage()));
    }
    return lines;
  }

  private static void lines(Path path, List<String> text) throws IOException {
    try (PrintWriter out = new PrintWriter(path.toFile(), "UTF-8")) {
      for (String s : text) out.println(s);
    }
  }

  private static final class FailingOutput extends OutputStream {
    final ByteArrayOutputStream bytes = new ByteArrayOutputStream();
    final int limit;
    final boolean failFlush;
    int flushes;

    FailingOutput(int limit, boolean failFlush) {
      this.limit = limit;
      this.failFlush = failFlush;
    }

    @Override
    public void write(int value) throws IOException {
      if (bytes.size() == limit) throw new IOException("owned injected output failure");
      bytes.write(value);
    }

    @Override
    public void write(byte[] b, int offset, int length) throws IOException {
      for (int i = 0; i < length; i++) write(b[offset + i]);
    }

    @Override
    public void flush() throws IOException {
      if (++flushes == 2 && failFlush) throw new IOException("owned injected final flush failure");
    }
  }

  public static void main(String[] args) throws Exception {
    cases();
    if (args.length == 3 && args[0].equals("--read-native")) {
      Path nativeDir = Paths.get(args[1]), output = Paths.get(args[2]);
      List<String> results = new ArrayList<>();
      Set<String> expected = new TreeSet<>();
      for (String id : CASES.keySet())
        if (STATUS.get(id).equals("OK")) {
          expected.add(id + ".ipf");
          byte[] bytes = Files.readAllBytes(nativeDir.resolve(id + ".ipf"));
          if (!Arrays.equals(bytes, write(CASES.get(id))))
            throw new IOException("Native writer bytes differ: " + id);
          if (!read(bytes).equals(read(write(CASES.get(id)))))
            throw new IOException("Native reader projection differs: " + id);
          results.add(id + "\tPASS\t" + bytes.length);
        }
      Set<String> actual = new TreeSet<>();
      try (DirectoryStream<Path> files = Files.newDirectoryStream(nativeDir)) {
        for (Path p : files) actual.add(p.getFileName().toString());
      }
      if (!actual.equals(expected))
        throw new IOException("Native output exact file inventory differs");
      lines(output, results);
      System.out.println(
          "Native writer output read by pinned Java: " + results.size() + " cases PASS");
      return;
    }
    if (args.length != 1)
      throw new IllegalArgumentException(
          "Expected output directory or --read-native directory result");
    Path out = Paths.get(args[0]);
    Files.createDirectories(out);
    List<String> index = new ArrayList<>();
    for (String id : CASES.keySet()) {
      List<String> input = new ArrayList<>();
      for (InstrumentProfile p : CASES.get(id)) input.add(dump(p));
      lines(out.resolve(id + ".input.tsv"), input);
      byte[] bytes = write(CASES.get(id));
      Files.write(out.resolve(id + ".ipf"), bytes);
      lines(out.resolve(id + ".read.tsv"), read(bytes));
      index.add(id + "\t" + STATUS.get(id) + "\t" + CASES.get(id).size() + "\t" + bytes.length);
    }
    lines(out.resolve("cases.tsv"), index);
    List<String> failures = new ArrayList<>();
    int length = write(CASES.get("basic")).length;
    for (int mode = 0; mode < 3; mode++) {
      FailingOutput target =
          new FailingOutput(mode == 0 ? 5 : mode == 1 ? length - 3 : Integer.MAX_VALUE, mode == 2);
      boolean failed = false;
      try {
        new InstrumentProfileWriter().write(target, CASES.get("basic"));
      } catch (IOException expected) {
        failed = true;
      }
      String text = new String(target.bytes.toByteArray(), StandardCharsets.UTF_8);
      failures.add(
          (mode == 0 ? "body" : mode == 1 ? "marker" : "final-flush")
              + "\t"
              + failed
              + "\t"
              + target.bytes.size()
              + "\t"
              + text.contains("##COMPLETE\r\n"));
    }
    lines(out.resolve("failures.tsv"), failures);
    System.out.println("Writer observations: " + CASES.size() + " named cases");
  }
}
