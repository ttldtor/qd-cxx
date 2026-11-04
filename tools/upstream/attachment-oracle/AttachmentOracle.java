package com.dxfeed.event.misc;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.Marshalled;
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
import java.io.Serializable;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.EnumSet;

/** Production event/delegate/standard-record/wire witness, called only by Invoke-Oracle.ps1. */
public final class AttachmentOracle {
  /** Benign application class with a stable serialization identity. */
  public static final class Payload implements Serializable {
    private static final long serialVersionUID = 1L;
    final int number;
    final String text;

    Payload() {
      number = 7;
      text = "payload";
    }
  }

  private static void require(boolean condition, String message) {
    if (!condition) {
      throw new IllegalStateException(message);
    }
  }

  private static String hex(byte[] bytes) {
    if (bytes == null) {
      return "null";
    }
    if (bytes.length == 0) {
      return "empty";
    }
    StringBuilder result = new StringBuilder();
    for (byte value : bytes) {
      result.append(String.format("%02x", value & 255));
    }
    return result.toString();
  }

  private static byte[] body(Marshalled<?> value) {
    return value == null ? null : value.getBytes();
  }

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) {
        out.print('\t');
      }
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static String decoded(Object value) {
    if (value == null) {
      return "null";
    }
    if (value instanceof byte[]) {
      return "byte[]:" + hex((byte[]) value);
    }
    if (value instanceof Payload) {
      Payload p = (Payload) value;
      require(p.number == 7 && p.text.equals("payload"), "custom object state");
      return "Payload:7:payload";
    }
    require(value instanceof String, "unexpected attachment class");
    return "String:" + value;
  }

  private static void capture(
      Path root,
      PrintWriter out,
      DataScheme scheme,
      String recordName,
      String id,
      Object input,
      boolean raw)
      throws Exception {
    DataRecord record = scheme.findRecordByName(recordName);
    require(record != null, "standard record missing");
    String fieldName = recordName.equals("Message") ? "Message" : "Configuration";
    require(record.findFieldByName(fieldName).getSerialType().getId() == 249, "SERIAL_OBJECT");
    MessageDelegate md = null;
    ConfigurationDelegate cd = null;
    RecordBuffer buffer = new RecordBuffer(RecordMode.DATA);
    RecordCursor cursor;
    if (recordName.equals("Message")) {
      md = new MessageDelegate(record, QDContract.STREAM, EnumSet.of(EventDelegateFlags.PUB));
      Message event = new Message("ATTACH");
      if (raw) {
        event.setMarshalledAttachment(Marshalled.forBytes((byte[]) input));
      } else {
        event.setAttachment(input);
      }
      cursor = md.putEvent(event, buffer);
    } else {
      cd = new ConfigurationDelegate(record, QDContract.STREAM, EnumSet.of(EventDelegateFlags.PUB));
      Configuration event = new Configuration("ATTACH");
      event.setVersion(-7);
      if (raw) {
        event.setMarshalledAttachment(Marshalled.forBytes((byte[]) input));
      } else {
        event.setAttachment(input);
      }
      cursor = cd.putEvent(event, buffer);
    }
    byte[] expected =
        body((Marshalled<?>) cursor.getObj(record.findFieldByName(fieldName).getIndex()));
    if (!raw && input instanceof byte[]) {
      require(!Arrays.equals((byte[]) input, expected), "Java array must be serialized");
      require(
          expected.length >= 4 && hex(Arrays.copyOf(expected, 4)).equals("aced0005"),
          "Java stream header");
    }
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    ByteArrayOutput bytes = new ByteArrayOutput();
    composer.setOptSet(ProtocolOption.EMPTY_SET);
    composer.setOutput(bytes);
    composer.visitData(buffer, MessageType.STREAM_DATA);
    byte[] wire = Arrays.copyOf(bytes.getBuffer(), bytes.getPosition());
    String stem = recordName + "-" + id;
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
    final MessageDelegate messageDelegate = md;
    final ConfigurationDelegate configurationDelegate = cd;
    final int[] seen = new int[2];
    BinaryQTPParser parser = new BinaryQTPParser(scheme);
    parser.setInput(new ByteArrayInput(wire));
    parser.parse(
        new MessageConsumerAdapter() {
          protected void processData(DataIterator iterator, MessageType type) {
            RecordCursor c;
            while ((c = ((RecordSource) iterator).next()) != null) {
              ++seen[0];
              Marshalled<?> marshalled;
              Object decoded = null;
              String decodeResult;
              try {
                if (messageDelegate != null) {
                  Message event = messageDelegate.getEvent(new Message(), c);
                  marshalled = event.getMarshalledAttachment();
                  require(Arrays.equals(expected, body(marshalled)), "parser/delegate bytes");
                  decoded = event.getAttachment();
                } else {
                  Configuration event = configurationDelegate.getEvent(new Configuration(), c);
                  require(event.getVersion() == -7, "version passthrough");
                  marshalled = event.getMarshalledAttachment();
                  require(Arrays.equals(expected, body(marshalled)), "parser/delegate bytes");
                  decoded = event.getAttachment();
                }
                decodeResult = decoded(decoded);
                require(!raw || input == null, "malformed body unexpectedly decoded");
              } catch (com.devexperts.io.MarshallingException e) {
                require(raw && input != null, "valid object failed decoding");
                decodeResult = "MarshallingException";
              }
              row(
                  out,
                  "event",
                  stem,
                  record.getId(),
                  fieldName,
                  249,
                  hex(expected),
                  decodeResult,
                  configurationDelegate == null ? 0 : -7,
                  hex(wire));
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
    require(seen[0] == 1 && seen[1] == 0, "one real parser/delegate callback");
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 1, "fresh output directory required; use launcher help");
    Path root = Paths.get(args[0]);
    Files.createDirectory(root);
    DataScheme scheme = DXFeedScheme.getInstance();
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(root.resolve("oracle.tsv"), StandardCharsets.UTF_8))) {
      for (String name : new String[] {"Message", "Configuration"}) {
        capture(root, out, scheme, name, "null", null, false);
        capture(root, out, scheme, name, "string", "hello", false);
        capture(root, out, scheme, name, "bytes-empty", new byte[0], false);
        capture(root, out, scheme, name, "bytes-content", new byte[] {0, 1, -1}, false);
        capture(root, out, scheme, name, "custom", new Payload(), false);
        capture(root, out, scheme, name, "raw-empty", new byte[0], true);
        capture(root, out, scheme, name, "raw-malformed", new byte[] {1, 2, 3}, true);
      }
      for (String name : new String[] {"TextMessage", "TextConfiguration"}) {
        DataRecord record = scheme.findRecordByName(name);
        row(
            out,
            "text-field",
            name,
            "Text",
            record.findFieldByName("Text").getSerialType().getId());
      }
      require(!out.checkError(), "oracle output failure");
    }
  }
}
