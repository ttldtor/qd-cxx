package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import java.util.Arrays;

/** Independently authored stimuli and observation only; no codec implementation. */
public final class EncodingOracle {
  static void row(Object... cells) {
    StringBuilder b = new StringBuilder();
    for (Object c : cells) {
      if (b.length() != 0) {
        b.append('\t');
      }
      b.append(
          c == null
              ? "\\N"
              : c.toString()
                  .replace("\\", "\\\\")
                  .replace("\t", "\\t")
                  .replace("\r", "\\r")
                  .replace("\n", "\\n"));
    }
    System.out.println(b);
  }

  static String hex(byte[] a) {
    StringBuilder b = new StringBuilder();
    for (byte x : a) {
      b.append(String.format("%02x", x & 255));
    }
    return b.toString();
  }

  static byte[] bytes(String h) {
    if (h.length() > 2048 || (h.length() & 1) != 0) {
      throw new IllegalArgumentException();
    }
    byte[] a = new byte[h.length() / 2];
    for (int i = 0; i < a.length; i++) {
      a[i] = (byte) Integer.parseInt(h.substring(i * 2, i * 2 + 2), 16);
    }
    return a;
  }

  static String units(String s) {
    if (s == null) {
      return null;
    }
    StringBuilder b = new StringBuilder();
    for (int i = 0; i < s.length(); i++) {
      b.append(String.format("%04x", (int) s.charAt(i)));
    }
    return b.toString();
  }

  static void read(String id, String kind, byte[] a) {
    ByteArrayInput in = new ByteArrayInput(a);
    String method =
        "read" + (kind.equals("S") ? "UTFString" : kind.equals("I") ? "CompactInt" : "CompactLong");
    try {
      String value =
          kind.equals("S")
              ? units(in.readUTFString())
              : kind.equals("I")
                  ? Integer.toString(in.readCompactInt())
                  : Long.toString(in.readCompactLong());
      row(
          "reader",
          id,
          method,
          hex(a),
          "ok",
          value,
          in.totalPosition(),
          a.length - in.totalPosition());
    } catch (java.io.IOException e) {
      row(
          "reader",
          id,
          method,
          hex(a),
          "error",
          e.getClass().getName(),
          in.totalPosition(),
          a.length - in.totalPosition(),
          e.getMessage());
    }
  }

  static void prefixes(String id, String kind, byte[] a) {
    for (int n = 0; n < a.length; n++) {
      read(id + "/prefix-" + n, kind, Arrays.copyOf(a, n));
    }
  }

  static byte[] number(String id, String kind, long v, boolean partial) throws Exception {
    ByteArrayOutput out = new ByteArrayOutput();
    if (kind.equals("I")) {
      out.writeCompactInt((int) v);
    } else {
      out.writeCompactLong(v);
    }
    byte[] a = out.toByteArray();
    row(
        "writer",
        id,
        "writeCompact" + (kind.equals("I") ? "Int" : "Long"),
        Long.toString(v),
        hex(a));
    read(id, kind, a);
    if (partial) {
      prefixes(id, kind, a);
    }
    return a;
  }

  static void integers() throws Exception {
    for (String k : new String[] {"I", "L"}) {
      number(k + "-zero", k, 0, true);
      number(k + "-minus1", k, -1, true);
      number(k + "-plus1", k, 1, true);
      number(k + "-minus2", k, -2, true);
      number(k + "-min", k, k.equals("I") ? Integer.MIN_VALUE : Long.MIN_VALUE, true);
      number(k + "-max", k, k.equals("I") ? Integer.MAX_VALUE : Long.MAX_VALUE, true);
      for (int bit : new int[] {6, 13, 20, 27, 34, 41, 48, 55}) {
        if (k.equals("I") && bit > 27) {
          continue;
        }
        for (int sign : new int[] {-1, 1}) {
          for (int d = -1; d <= 1; d++) {
            long v = sign * (1L << bit) + d;
            number(k + "-b" + bit + "-" + (sign < 0 ? "neg" : "pos") + "-d" + d, k, v, true);
          }
        }
      }
    }

    long[] cross = {
      Integer.MIN_VALUE,
      Integer.MAX_VALUE,
      -2147483649L,
      2147483648L,
      4294967296L,
      -4294967297L,
      Long.MIN_VALUE,
      Long.MAX_VALUE
    };
    for (int i = 0; i < cross.length; i++) {
      byte[] a = number("X" + i, "L", cross[i], false);
      read("X" + i + "/as-int", "I", a);
      prefixes("X" + i + "/as-int", "I", a);
    }
    for (long v : new long[] {Integer.MIN_VALUE, -1, 0, Integer.MAX_VALUE}) {
      read("XI" + v + "/as-long", "L", number("XI" + v, "I", v, false));
    }
  }

  static void strings() throws Exception {
    String[] ids = {
      "null",
      "empty",
      "ascii",
      "nul",
      "bmp",
      "supplementary",
      "high",
      "low",
      "reversed",
      "high-ascii",
      "utf-boundaries",
      "len63",
      "len64"
    };
    String[] values = {
      null,
      "",
      "ABC",
      "A\u0000B",
      "\u0416\u20ac",
      "\ud83d\ude00",
      "\ud800",
      "\udc00",
      "\udc00\ud800",
      "\ud800A",
      "\u007f\u0080\u07ff\u0800\uffff\udbff\udfff",
      repeat(63),
      repeat(64)
    };
    for (int i = 0; i < ids.length; i++) {
      ByteArrayOutput out = new ByteArrayOutput();
      out.writeUTFString(values[i]);
      byte[] a = out.toByteArray();
      row("writer", "S-" + ids[i], "writeUTFString", units(values[i]), hex(a));
      read("S-" + ids[i], "S", a);
      prefixes("S-" + ids[i], "S", a);
    }
  }

  static String repeat(int n) {
    char[] a = new char[n];
    Arrays.fill(a, 'a');
    return new String(a);
  }

  static void forms() {
    String[] nums = {
      "8000",
      "bfff",
      "c00000",
      "dfffff",
      "e0000000",
      "efffffff",
      "f000000000",
      "f7ffffffff",
      "f80000000000",
      "fbffffffffff",
      "fc000000000000",
      "fdffffffffffff",
      "fe00000000000000",
      "feffffffffffffff",
      "ff0000000000000000",
      "ffffffffffffffffff",
      "f100000000",
      "f600000000"
    };
    for (int i = 0; i < nums.length; i++) {
      for (String k : new String[] {"I", "L"}) {
        read("N" + i, k, bytes(nums[i]));
        prefixes("N" + i, k, bytes(nums[i]));
      }
    }

    String[][] strings = {
      {"overlong2", "02c080"},
      {"overlong3", "03e08080"},
      {"overlong4", "04f0808080"},
      {"cesu-pair", "06eda0bdedb880"},
      {"surrogate4", "04f08da080"},
      {"bad-cont2", "02c241"},
      {"bad-cont3", "03e24180"},
      {"bad-cont4", "04f0418080"},
      {"continuation", "0180"},
      {"lead-f8", "01f8"},
      {"lead-ff", "01ff"},
      {"too-large", "04f4908080"},
      {"short-declared", "01c280"},
      {"short-declared-eof", "01c2"},
      {"long-declared", "0241"},
      {"trailing", "014142"},
      {"negative-length", "7e"},
      {"overlong-null", "bfff"},
      {"overlong-empty", "8000"},
      {"overlong-length", "800141"},
      {"limit-plus1", "f1fffffffd"},
      {"longmax-length", "ff7fffffffffffffff"}
    };
    for (String[] s : strings) {
      read("F-" + s[0], "S", bytes(s[1]));
      prefixes("F-" + s[0], "S", bytes(s[1]));
    }
  }

  public static void main(String[] args) throws Exception {
    row(
        "profile",
        "QD-3.355",
        "ByteArrayInput",
        "fresh input per read",
        "UTF-16 hex units; exact decimal integers");
    if (args[0].equals("integers")) {
      integers();
    } else if (args[0].equals("strings")) {
      strings();
    } else if (args[0].equals("forms")) {
      forms();
    } else {
      throw new IllegalArgumentException();
    }
    row("complete", args[0]);
  }
}
