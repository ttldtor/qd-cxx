package com.devexperts.qd.qtp;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SymbolCodec;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordMode;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;

/** Bounded observations of actual pinned symbol APIs and production composition/parsing. */
public final class SymbolOracle {
  private static byte[] bytes(String hex) {
    if (hex.equals("-")) {
      return new byte[0];
    }
    if (hex.length() > 4096 || hex.length() % 2 != 0) {
      throw new IllegalArgumentException();
    }
    byte[] result = new byte[hex.length() / 2];
    for (int i = 0; i < result.length; i++) {
      result[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
    }
    return result;
  }

  private static String units(String s) {
    if (s == null) {
      return "null";
    }
    StringBuilder out = new StringBuilder();
    for (int i = 0; i < s.length(); i++) {
      out.append(String.format("%04x", (int) s.charAt(i)));
    }
    return out.length() == 0 ? "-" : out.toString();
  }

  private static String hex(byte[] b) {
    return b.length == 0 ? "-" : ProtocolOracle.hex(b);
  }

  private static void read(String id, String phase, String seed, SymbolCodec.Reader r, byte[] b) {
    ByteArrayInput in = new ByteArrayInput(b);
    String status = "OK";
    try {
      r.readSymbol(in, null);
    } catch (Exception e) {
      status = e.getClass().getName() + ":" + e.getMessage();
    }
    ProtocolOracle.row(
        "read",
        id,
        phase,
        seed,
        hex(b),
        status,
        in.totalPosition(),
        r.getCipher(),
        units(r.getSymbol()),
        units(PentaCodec.INSTANCE.decode(r.getCipher(), r.getSymbol())),
        r.getEventFlags(),
        r.getEventFlagsBytes());
  }

  private static void probe(String id, String seed, byte[] b) throws Exception {
    SymbolCodec.Reader r = PentaCodec.INSTANCE.createReader();
    r.reset(ProtocolOption.EMPTY_SET);
    if (!seed.equals("-")) {
      r.readSymbol(new ByteArrayInput(bytes(seed)), null);
    }
    read(id, "main", seed, r, b);
    // Separate buffers deliberately observe primitive state, not parser recovery.
    read(id, "after-repeat", seed, r, bytes("ff"));
    read(id, "after-mru", seed, r, bytes("f8ff"));
    r.reset(ProtocolOption.EMPTY_SET);
    read(id, "after-reset", "-", r, bytes("f8ff"));
    read(id, "sentinel", "-", r, bytes("0443"));
  }

  private static void integration() throws Exception {
    DataScheme scheme = ProtocolOracle.scheme(ProtocolOracle.record(0, "R"));
    ByteArrayOutput out = new ByteArrayOutput();
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setOutput(out);
    composer.setOptSet(ProtocolOption.EMPTY_SET);
    for (String[] symbols : new String[][] {{"ABC", "ABC", "XYZ", "XYZ"}, {"XYZ", "XYZ", "ABC"}}) {
      RecordBuffer buffer = new RecordBuffer(RecordMode.DATA);
      for (String s : symbols) {
        buffer.add(scheme.getRecord(0), scheme.getCodec().encode(s), null);
      }
      composer.visitData(buffer, MessageType.STREAM_DATA);
    }
    ProtocolOracle.row("integration", hex(out.toByteArray()));
    ProtocolOracle.replay(scheme, out.toByteArray(), "production", out.getPosition());
  }

  public static void main(String[] args) throws Exception {
    ProtocolOracle.row("profile", "QD-3.355", "EMPTY_SET", "resolver=null", "bounded<=2048bytes");
    int count = 0;
    for (String line : Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8)) {
      if (line.isEmpty() || line.startsWith("#")) {
        continue;
      }
      String[] c = line.split("\t", -1);
      byte[] wire;
      if (c[0].equals("W")) {
        String s = null;
        if (!c[2].equals("null")) {
          byte[] raw = bytes(c[2]);
          StringBuilder value = new StringBuilder();
          for (int i = 0; i < raw.length; i += 2) {
            value.append((char) ((raw[i] & 255) * 256 + (raw[i + 1] & 255)));
          }
          s = value.toString();
        }
        int cipher = PentaCodec.INSTANCE.encode(s);
        SymbolCodec.Writer w = PentaCodec.INSTANCE.createWriter();
        w.reset(ProtocolOption.EMPTY_SET);
        ByteArrayOutput out = new ByteArrayOutput();
        try {
          w.writeSymbol(out, cipher, cipher == 0 ? s : null, 0);
        } catch (IllegalArgumentException e) {
          ProtocolOracle.row(
              "writer-error", c[1], units(s), cipher, e.getClass().getName(), out.getPosition());
          count++;
          continue;
        }
        wire = out.toByteArray();
        ProtocolOracle.row("writer", c[1], units(s), cipher, hex(wire));
        for (int flags : new int[] {1, 5, 5, 0, -1}) {
          w.writeSymbol(out, cipher, cipher == 0 ? s : null, flags);
        }
        ProtocolOracle.row("writer-sequence", c[1], hex(out.toByteArray()));
        SymbolCodec.Reader sequence = PentaCodec.INSTANCE.createReader();
        sequence.reset(ProtocolOption.EMPTY_SET);
        ByteArrayInput si = new ByteArrayInput(out.toByteArray());
        while (si.available() > 0) {
          long start = si.totalPosition();
          sequence.readSymbol(si, null);
          ProtocolOracle.row(
              "sequence",
              c[1],
              start,
              si.totalPosition(),
              units(PentaCodec.INSTANCE.decode(sequence.getCipher(), sequence.getSymbol())),
              sequence.getEventFlags());
        }
      } else {
        wire = bytes(c[2]);
      }
      probe(c[1], c[3], wire);
      for (int n = 0; n < wire.length; n++) {
        probe(c[1] + "/prefix-" + n, c[3], Arrays.copyOf(wire, n));
      }
      count++;
    }
    if (count < 100) {
      throw new AssertionError("Missing inputs");
    }
    integration();
    ProtocolOracle.row("complete", "symbols", count);
  }
}
