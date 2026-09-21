package com.devexperts.qd.qtp;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

/** Offline production composition and observable parser delivery, without a tape container. */
public final class OfflineInspectionOracle {
  private static String units(String value) {
    StringBuilder out = new StringBuilder();
    for (int i = 0; i < value.length(); i++) {
      out.append(String.format("%04x", (int) value.charAt(i)));
    }
    return out.length() == 0 ? "-" : out.toString();
  }

  private static String symbol(String hex) {
    if (hex.equals("-")) {
      return "";
    }
    if (!hex.matches("(?:[0-9a-f]{4})+")) {
      throw new IllegalArgumentException("symbol units");
    }
    StringBuilder out = new StringBuilder();
    for (int i = 0; i < hex.length(); i += 4) {
      out.append((char) Integer.parseInt(hex.substring(i, i + 4), 16));
    }
    return out.toString();
  }

  private static final class Observer extends MessageConsumerAdapter {
    int records;
    int callbacks;

    @Override
    protected void processData(DataIterator iterator, MessageType type) {
      RecordCursor cursor;
      while ((cursor = ((RecordSource) iterator).next()) != null) {
        StringBuilder values = new StringBuilder();
        for (int i = 0; i < cursor.getIntCount(); i++) {
          if (i != 0) {
            values.append(',');
          }
          values.append(cursor.getInt(i));
        }
        ProtocolOracle.row(
            "record",
            records++,
            cursor.getRecord().getId(),
            units(cursor.getRecord().getName()),
            units(
                cursor
                    .getRecord()
                    .getScheme()
                    .getCodec()
                    .decode(cursor.getCipher(), cursor.getSymbol())),
            values.length() == 0 ? "-" : values.toString(),
            cursor.getEventFlags());
      }
      callbacks++;
    }

    @Override
    public void handleCorruptedMessage(int type) {
      throw new AssertionError("Corrupted message " + type);
    }

    @Override
    public void handleCorruptedStream() {
      throw new AssertionError("Corrupted stream");
    }

    @Override
    public void handleUnknownMessage(int type) {
      throw new AssertionError("Unknown message " + type);
    }
  }

  public static void main(String[] args) throws Exception {
    DataScheme sender =
        ProtocolOracle.scheme(
            ProtocolOracle.record(0, "R", "x", "unknown", "y", "xdot"),
            ProtocolOracle.record(1, "S", "v"),
            ProtocolOracle.record(2, "Unknown", "a", "b"),
            ProtocolOracle.record(3, "E"));
    DefaultRecord r = ProtocolOracle.record(1, "R", "y", "x", "z");
    r.addFieldAlias("x", "xdot");
    DataScheme local =
        ProtocolOracle.scheme(ProtocolOracle.record(0, "S", "v"), r, ProtocolOracle.record(2, "E"));
    ByteArrayOutput out = new ByteArrayOutput();
    BinaryQTPComposer composer = new BinaryQTPComposer(sender, false);
    composer.setOutput(out);
    composer.setOptSet(ProtocolOption.EMPTY_SET);
    // Exercise the actual production description writer before data, without automatic
    // interleaving.
    composer.beginMessage(MessageType.STREAM_DATA);
    for (int i = 0; i < sender.getRecordCount(); i++) {
      composer.describeRecord(sender.getRecord(i));
    }
    composer.endMessage();
    List<String> inputs = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
    int message = 0;
    int inputRecords = 0;
    RecordBuffer buffer = new RecordBuffer(RecordMode.DATA);
    for (String line : inputs) {
      if (line.isEmpty() || line.startsWith("#")) {
        continue;
      }
      String[] cells = line.split("\t", -1);
      if (cells.length != 4) {
        throw new IllegalArgumentException("input arity");
      }
      int next = Integer.parseInt(cells[0]);
      if (next != message) {
        if (next != message + 1 || buffer.size() == 0) {
          throw new IllegalArgumentException("message sequence");
        }
        composer.visitData(buffer, MessageType.STREAM_DATA);
        composer.visitHeartbeat(new HeartbeatPayload());
        buffer = new RecordBuffer(RecordMode.DATA);
        message = next;
      }
      String s = symbol(cells[2]);
      int cipher = sender.getCodec().encode(s);
      RecordCursor cursor =
          buffer.add(sender.findRecordByName(cells[1]), cipher, cipher == 0 ? s : null);
      String[] values = cells[3].equals("-") ? new String[0] : cells[3].split(",", -1);
      if (values.length != cursor.getIntCount()) {
        throw new IllegalArgumentException("field count");
      }
      for (int i = 0; i < values.length; i++) {
        cursor.setInt(i, Integer.parseInt(values[i]));
      }
      inputRecords++;
    }
    if (inputRecords == 0 || message == 0) {
      throw new IllegalArgumentException("empty or single-message input");
    }
    composer.visitData(buffer, MessageType.STREAM_DATA);
    byte[] wire = out.toByteArray();
    Files.write(Paths.get(args[1], "composed.qtp"), wire);
    ProtocolOracle.row("profile", "QD-3.355", "oracle-sre-v1", "EMPTY_SET", "raw-QTP");
    ProtocolOracle.row("wire", ProtocolOracle.hex(wire));
    BinaryQTPParser parser = new BinaryQTPParser(local);
    parser.setReadEventTimeSequence(false);
    parser.setEventTimeSequence(0);
    parser.setFieldReplacers(null);
    ByteArrayInput input = new ByteArrayInput(wire);
    parser.setInput(input);
    Observer observer = new Observer();
    parser.parse(observer);
    if (input.available() != 0 || observer.records == 0) {
      throw new AssertionError("Incomplete or empty observation");
    }
    ProtocolOracle.row("end", input.totalPosition(), observer.records, observer.callbacks);
    ProtocolOracle.row("complete", "offline-inspection");
  }
}
