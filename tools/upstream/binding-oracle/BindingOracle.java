package com.devexperts.qd.qtp;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SymbolCodec;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.PentaCodec;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

/** Production-parser binding observations; Java writers generate all primitive and symbol bytes. */
public final class BindingOracle {
  private static void frame(ByteArrayOutput stream, int type, ByteArrayOutput body)
      throws Exception {
    stream.writeCompactLong(body.getPosition() + 1);
    stream.writeCompactInt(type);
    stream.write(body.toByteArray());
  }

  private static void observe(String name, List<String[]> rows) throws Exception {
    DefaultRecord r = ProtocolOracle.record(1, "R", "y", "x", "z");
    r.addFieldAlias("x", "xdot");
    DataScheme local =
        ProtocolOracle.scheme(ProtocolOracle.record(0, "S", "v"), r, ProtocolOracle.record(2, "E"));
    ProtocolOracle.row("case", name);
    ProtocolOracle.describe("local", local);
    ByteArrayOutput descriptions = new ByteArrayOutput();
    ByteArrayOutput data = new ByteArrayOutput();
    SymbolCodec.Writer symbols = PentaCodec.INSTANCE.createWriter();
    symbols.reset(ProtocolOption.EMPTY_SET);
    for (String[] row : rows) {
      int id = Integer.parseInt(row[1]);
      String[] fields = row[3].equals("-") ? new String[0] : row[3].split(",");
      String[] values = row[4].equals("-") ? new String[0] : row[4].split(",");
      if (id < 0 || id > 4096 || fields.length != values.length) {
        throw new IllegalArgumentException("Unsafe or inconsistent probe input");
      }
      descriptions.writeCompactInt(id);
      descriptions.writeUTFString(row[2]);
      descriptions.writeCompactInt(fields.length);
      for (String field : fields) {
        descriptions.writeUTFString(field);
        descriptions.writeCompactInt(8);
      }
      symbols.writeSymbol(data, PentaCodec.INSTANCE.encode("ABC"), null, 0);
      data.writeCompactInt(id);
      for (String value : values) {
        data.writeCompactInt(Integer.parseInt(value));
      }
    }
    ByteArrayOutput stream = new ByteArrayOutput();
    if (!name.equals("undescribed")) {
      frame(stream, 2, descriptions);
    }
    frame(stream, MessageType.STREAM_DATA.getId(), data);
    ProtocolOracle.row("payload", ProtocolOracle.hex(descriptions.toByteArray()));
    ProtocolOracle.row("wire", ProtocolOracle.hex(stream.toByteArray()));
    BinaryQTPParser parser = new BinaryQTPParser(local);
    parser.setReadEventTimeSequence(false);
    parser.setEventTimeSequence(0);
    parser.setFieldReplacers(null);
    ByteArrayInput input = new ByteArrayInput(stream.toByteArray());
    parser.setInput(input);
    ProtocolOracle.Observer observer = new ProtocolOracle.Observer();
    parser.parse(observer);
    ProtocolOracle.row(
        "end", input.totalPosition(), input.available(), observer.records.size(), observer.errors);
    if (input.available() != 0 || observer.errors != (name.equals("undescribed") ? 1 : 0)) {
      throw new AssertionError("Production parse failed: " + name);
    }
  }

  public static void main(String[] args) throws Exception {
    ProtocolOracle.row(
        "profile",
        "QD-3.355",
        "builtin-metadata=false",
        "field-replacers=null",
        "event-time=0",
        "programmatic-scheme",
        "optional-properties=disabled");
    String current = "";
    List<String[]> group = new ArrayList<>();
    for (String line : Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8)) {
      if (line.startsWith("#") || line.isEmpty()) {
        continue;
      }
      String[] cells = line.split("\t", -1);
      if (!current.isEmpty() && !current.equals(cells[0])) {
        observe(current, group);
        group.clear();
      }
      current = cells[0];
      group.add(cells);
    }
    if (group.isEmpty()) {
      throw new IllegalArgumentException("Empty fixture");
    }
    observe(current, group);
    ProtocolOracle.row("complete", "binding");
  }
}
