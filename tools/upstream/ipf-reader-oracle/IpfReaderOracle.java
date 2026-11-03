// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.dxfeed.ipf.InstrumentProfile;
import com.dxfeed.ipf.InstrumentProfileField;
import com.dxfeed.ipf.InstrumentProfileReader;
import com.dxfeed.ipf.impl.InstrumentProfileParser;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

/** Deterministic plain local IPF grammar/value observations. */
public final class IpfReaderOracle {
  private static String hex(String text) {
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < text.length(); i++) {
      String unit = Integer.toHexString(text.charAt(i));
      for (int padding = unit.length(); padding < 4; padding++) {
        result.append('0');
      }
      result.append(unit);
    }
    return result.toString();
  }

  private static byte[] bytes(String text) {
    return text.getBytes(StandardCharsets.UTF_8);
  }

  private static byte[] malformed(int... middle) {
    byte[] prefix = bytes("#STOCK::=TYPE,SYMBOL,DESCRIPTION\nSTOCK,A,");
    byte[] result = new byte[prefix.length + middle.length + 12];
    System.arraycopy(prefix, 0, result, 0, prefix.length);
    for (int index = 0; index < middle.length; index++) {
      result[prefix.length + index] = (byte) middle[index];
    }
    byte[] suffix = bytes("\n##COMPLETE\n");
    System.arraycopy(suffix, 0, result, prefix.length + middle.length, suffix.length);
    return result;
  }

  private static String profile(InstrumentProfile profile) {
    StringBuilder result = new StringBuilder("PROFILE");
    for (InstrumentProfileField field : InstrumentProfileField.values()) {
      if (!profile.getField(field.name()).isEmpty()) {
        result
            .append('\t')
            .append(hex(field.name()))
            .append('=')
            .append(hex(profile.getField(field.name())));
      }
    }
    TreeSet<String> custom = new TreeSet<>();
    profile.addNonEmptyCustomFieldNames(custom);
    for (String name : custom) {
      result.append('\t').append(hex(name)).append('=').append(hex(profile.getField(name)));
    }
    return result.toString();
  }

  /** Write bounded named inputs and observations under one output directory. */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output directory");
    }
    File output = new File(args[0]);
    Files.createDirectories(output.toPath());
    Map<String, byte[]> cases = new LinkedHashMap<>();
    cases.put("empty", bytes(""));
    cases.put("blank-lines", bytes("\n\r\r\n\n"));
    cases.put(
        "basic",
        bytes(
            "#STOCK::=TYPE,SYMBOL,DESCRIPTION,ICB,X\nSTOCK,IBM,IBM Corp,123,custom\n##COMPLETE\n"));
    cases.put("header-only-eof", bytes("#STOCK::=TYPE,SYMBOL"));
    cases.put("header-only-newline", bytes("#STOCK::=TYPE,SYMBOL\n"));
    cases.put("profile-eof", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,A"));
    cases.put("complete-eof", bytes("##COMPLETE"));
    cases.put("unknown-type", bytes("STOCK,A\n"));
    cases.put("unknown-after-prefix", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,A\nNOFORMAT,B\n"));
    cases.put(
        "comments-controls",
        bytes(
            "# comment,anything\n"
                + "##,ignored\n"
                + "##COMPLETE,ignored\n"
                + "#STOCK::=TYPE,SYMBOL\n"
                + "STOCK,A\n"
                + "##COMPLETE\n"));
    cases.put(
        "complete-then-error", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,A\n##COMPLETE\nBROKEN,A\n"));
    cases.put(
        "quoted",
        bytes(
            "#STOCK::=TYPE,SYMBOL,DESCRIPTION\n"
                + "STOCK,\"A,B\",\"He said \"\"hello\"\"\"\n"
                + "##COMPLETE\n"));
    cases.put(
        "multiline",
        bytes(
            "#STOCK::=TYPE,SYMBOL,DESCRIPTION\r\n"
                + "STOCK,A,\"line1\n"
                + "line2\r"
                + "line3\r\n"
                + "line4\"\r\n"
                + "##COMPLETE\r\n"));
    cases.put("unquoted-quote", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,A\nSTOCK,A\"B\n"));
    cases.put("quoted-suffix", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,\"A\"x\n"));
    cases.put("quoted-space", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,\"A\" \n"));
    cases.put("quote-unfinished", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,A\nSTOCK,\"B\nC"));
    cases.put("short-row", bytes("#STOCK::=TYPE,SYMBOL,DESCRIPTION\nSTOCK,A\n"));
    cases.put("extra-row", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,A,\n"));
    cases.put("extra-bad-quote", bytes("#STOCK::=TYPE,SYMBOL\nSTOCK,A,un\"quoted\n"));
    cases.put("invalid-number-first", bytes("#STOCK::=TYPE,STRIKE,SYMBOL\nSTOCK,bad,un\"quoted\n"));
    cases.put("invalid-date", bytes("#STOCK::=TYPE,SYMBOL,EXPIRATION\nSTOCK,A,not-date\n"));
    cases.put(
        "header-redefine",
        bytes(
            "#STOCK::=TYPE,SYMBOL,X\n"
                + "STOCK,A,first\n"
                + "#STOCK::=TYPE,X,SYMBOL\n"
                + "STOCK,second,B\n"
                + "##COMPLETE\n"));
    cases.put(
        "duplicate-fields",
        bytes(
            "#STOCK::=TYPE,SYMBOL,SYMBOL,X,X,TYPE\n"
                + "STOCK,A,,first,,OTHER\n"
                + "STOCK,A,B,first,second,\n"
                + "##COMPLETE\n"));
    cases.put(
        "empty-custom-name",
        bytes("#STOCK::=TYPE,SYMBOL,,symbol\nSTOCK,A,value,custom\n##COMPLETE\n"));
    cases.put("empty-type", bytes("#::=TYPE,SYMBOL\n,A\n\n##COMPLETE\n"));
    cases.put("zero-fields", bytes("#STOCK::=TYPE\nSTOCK\n##COMPLETE\n"));
    cases.put("empty-format", bytes("#::=TYPE\n\n##COMPLETE\n"));
    cases.put("forbidden-flush", bytes("###::=TYPE,SYMBOL\n"));
    cases.put("forbidden-complete", bytes("###COMPLETE::=TYPE,SYMBOL\n"));
    cases.put("forbidden-header", bytes("##STOCK::=TYPE::=TYPE,SYMBOL\n"));
    cases.put("metadata-as-type", bytes("##comment::=TYPE,SYMBOL\n#comment,A\n##COMPLETE\n"));
    cases.put("control-malformed-tail", bytes("##COMPLETE,un\"quoted\n"));
    cases.put("bom", bytes("\ufeff#STOCK::=TYPE,SYMBOL\nSTOCK,A\n"));
    cases.put(
        "unicode",
        bytes(
            "#STOCK::=TYPE,SYMBOL,DESCRIPTION\n"
                + "STOCK,\ud83d\ude00,\u0000\u20ac\ud83d\ude42\n"
                + "##COMPLETE\n"));
    cases.put("utf8-stray", malformed(0x80));
    cases.put("utf8-overlong", malformed(0xc0, 0xaf));
    cases.put("utf8-surrogate", malformed(0xed, 0xa0, 0x80));
    cases.put("utf8-bad-middle", malformed(0xe2, 0x28, 0xa1));
    cases.put("utf8-bad-third", malformed(0xe2, 0x82, 0x28));
    cases.put("utf8-out-of-range", malformed(0xf4, 0x90, 0x80, 0x80));
    byte[] truncated = bytes("#STOCK::=TYPE,SYMBOL,DESCRIPTION\nSTOCK,A,");
    byte[] incomplete = new byte[truncated.length + 2];
    System.arraycopy(truncated, 0, incomplete, 0, truncated.length);
    incomplete[truncated.length] = (byte) 0xe2;
    incomplete[truncated.length + 1] = (byte) 0x82;
    cases.put("utf8-truncated", incomplete);
    try (PrintWriter index = new PrintWriter(new File(output, "cases.tsv"), "UTF-8")) {
      for (Map.Entry<String, byte[]> entry : cases.entrySet()) {
        String id = entry.getKey();
        Files.write(new File(output, id + ".ipf").toPath(), entry.getValue());
        List<String> events = new ArrayList<>();
        String error = "";
        try (InstrumentProfileParser parser =
            new InstrumentProfileParser(new ByteArrayInputStream(entry.getValue()))
                .whenFlush(() -> events.add("FLUSH"))
                .whenComplete(() -> events.add("COMPLETE"))) {
          InstrumentProfile value;
          while ((value = parser.next()) != null) {
            events.add(profile(value));
          }
        } catch (Exception failure) {
          error = failure.getClass().getSimpleName() + ":" + failure.getMessage();
        }
        try (PrintWriter observed = new PrintWriter(new File(output, id + ".tsv"), "UTF-8")) {
          for (String event : events) {
            observed.println(event);
          }
          observed.println(error.isEmpty() ? "EOF" : "ERROR\t" + hex(error));
          InstrumentProfileReader reader = new InstrumentProfileReader();
          try {
            List<InstrumentProfile> values =
                reader.read(new ByteArrayInputStream(entry.getValue()), id);
            observed.println("READER\t" + values.size() + "\t" + reader.wasComplete());
          } catch (Exception failure) {
            observed.println(
                "READER_ERROR\t"
                    + reader.wasComplete()
                    + "\t"
                    + hex(failure.getClass().getSimpleName() + ":" + failure.getMessage()));
          }
        }
        index.println(id + "\t" + entry.getValue().length + "\t" + events.size());
      }
    }
  }
}
