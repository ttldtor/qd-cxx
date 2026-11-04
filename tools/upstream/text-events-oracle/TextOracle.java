package com.dxfeed.event.misc;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.StreamCompression;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolOption;
import com.devexperts.qd.qtp.file.FileFormat;
import com.devexperts.qd.qtp.file.FileWriter;
import com.devexperts.qd.qtp.file.TimestampsType;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegateFlags;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.EnumSet;

/** Finite production delegate, composer, parser and file witness called by Invoke-Oracle.ps1. */
public final class TextOracle {
  private static void require(boolean value, String message) {
    if (!value) throw new IllegalStateException(message);
  }

  private static String units(String value) {
    if (value == null) return "null";
    if (value.isEmpty()) return "empty";
    StringBuilder result = new StringBuilder();
    for (char c : value.toCharArray()) result.append(String.format("%04x", (int) c));
    return result.toString();
  }

  private static String hex(byte[] value) {
    StringBuilder result = new StringBuilder();
    for (byte b : value) result.append(String.format("%02x", b & 255));
    return result.toString();
  }

  private static void capture(
      Path root, PrintWriter out, DataScheme scheme, boolean configuration, String id, String text)
      throws Exception {
    String name = configuration ? "TextConfiguration" : "TextMessage";
    DataRecord record = scheme.findRecordByName(name);
    require(
        record != null && record.findFieldByName("Text").getSerialType().getId() == 10,
        "field identity");
    TextMessageDelegate md =
        configuration
            ? null
            : new TextMessageDelegate(
                record, QDContract.STREAM, EnumSet.of(EventDelegateFlags.PUB));
    TextConfigurationDelegate cd =
        configuration
            ? new TextConfigurationDelegate(
                record, QDContract.STREAM, EnumSet.of(EventDelegateFlags.PUB))
            : null;
    RecordBuffer buffer = new RecordBuffer(RecordMode.DATA);
    if (configuration) {
      TextConfiguration event = new TextConfiguration("TEXT");
      event.setTime(-877);
      event.setSequence(9);
      event.setVersion(-7);
      event.setText(text);
      cd.putEvent(event, buffer);
    } else {
      TextMessage event = new TextMessage("TEXT");
      event.setTime(-877);
      event.setSequence(9);
      event.setText(text);
      md.putEvent(event, buffer);
    }
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOptSet(ProtocolOption.EMPTY_SET);
    composer.setOutput(output);
    composer.visitData(buffer, MessageType.STREAM_DATA);
    byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
    String stem = name + "-" + id;
    Files.write(root.resolve(stem + ".qtp"), wire);
    FileWriter writer = new FileWriter();
    writer.setFormat(FileFormat.BINARY);
    writer.setCompression(StreamCompression.NONE);
    writer.setTime(TimestampsType.NONE);
    writer.setSaveAs(MessageType.STREAM_DATA);
    writer.setOpt("");
    try (FileWriter opened = writer.open(root.resolve(stem + ".tape").toString(), scheme)) {
      buffer.rewind();
      opened.visitData(buffer, MessageType.STREAM_DATA);
    }
    final int[] seen = new int[2];
    BinaryQTPParser parser = new BinaryQTPParser(scheme);
    parser.setInput(new ByteArrayInput(wire));
    parser.parse(
        new MessageConsumerAdapter() {
          protected void processData(DataIterator iterator, MessageType type) {
            RecordCursor cursor;
            while ((cursor = ((RecordSource) iterator).next()) != null) {
              ++seen[0];
              String actual;
              long time;
              int sequence;
              int version = 0;
              if (configuration) {
                TextConfiguration event = cd.getEvent(new TextConfiguration(), cursor);
                actual = event.getText();
                time = event.getTime();
                sequence = event.getSequence();
                version = event.getVersion();
              } else {
                TextMessage event = md.getEvent(new TextMessage(), cursor);
                actual = event.getText();
                time = event.getTime();
                sequence = event.getSequence();
              }
              require(
                  java.util.Objects.equals(text, actual)
                      && time == -877
                      && sequence == 9
                      && (!configuration || version == -7),
                  "delegate result");
              out.printf(
                  "event\t%s\t%d\tText\t10\t%s\t%d\t%d\t%d\t%s%n",
                  stem, record.getId(), units(actual), time, sequence, version, hex(wire));
            }
          }

          public void handleCorruptedStream() {
            ++seen[1];
          }

          public void handleCorruptedMessage(int type) {
            ++seen[1];
          }

          public void handleUnknownMessage(int type) {
            ++seen[1];
          }
        });
    require(seen[0] == 1 && seen[1] == 0, "one parser callback");
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 1, "fresh output directory required; use launcher help");
    Path root = Paths.get(args[0]);
    Files.createDirectory(root);
    DataScheme scheme = DXFeedScheme.getInstance();
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(root.resolve("oracle.tsv"), StandardCharsets.UTF_8))) {
      for (boolean configuration : new boolean[] {false, true}) {
        capture(root, out, scheme, configuration, "null", null);
        capture(root, out, scheme, configuration, "empty", "");
        capture(
            root,
            out,
            scheme,
            configuration,
            "units",
            new String(new char[] {'A', 0, '\ud800', '\udc00', '\ud800'}));
      }
      require(!out.checkError(), "output failure");
    }
  }
}
