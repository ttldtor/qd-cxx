// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Oracle caller: Invoke-FieldTimeOracle.ps1. Inputs: fresh TSV path and field profile.
// Outputs: production composer/parser/delegate FIELD precision observations and two QTP files.
package qdcxx.oracle;

import com.devexperts.io.*;
import com.devexperts.qd.*;
import com.devexperts.qd.kit.*;
import com.devexperts.qd.ng.*;
import com.devexperts.qd.qtp.*;
import com.devexperts.qd.util.TimeSequenceUtil;
import com.dxfeed.api.impl.*;
import com.dxfeed.event.market.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

public final class OrderVariantsFieldTimeOracle {
  private static final long[] PACKED = {
    0,
    1,
    1L << 22,
    -1,
    Long.MIN_VALUE,
    Long.MAX_VALUE,
    (-1L << 32) | (999L << 22) | 0x3fffffL,
    (123L << 32) | (456L << 22) | 321
  };

  private static void require(boolean v, String why) {
    if (!v) throw new IllegalStateException(why);
  }

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; i++) {
      if (i != 0) out.print('\t');
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static int slot(DataRecord r, String name) {
    for (int i = 0; i < r.getIntFieldCount(); i++)
      if (r.getIntField(i).getLocalName().equals(name)) return i;
    throw new IllegalArgumentException(name);
  }

  private static DataRecord cloneRecord(DataRecord r, int id) {
    DataIntField[] ints = new DataIntField[r.getIntFieldCount()];
    DataObjField[] objs = new DataObjField[r.getObjFieldCount()];
    for (int i = 0; i < ints.length; i++) {
      DataIntField f = r.getIntField(i);
      ints[i] =
          f.getSerialType().getId() == 0
              ? new VoidIntField(i, r.getName() + "." + f.getLocalName())
              : f.getSerialType().createDefaultIntInstance(i, r.getName() + "." + f.getLocalName());
    }
    for (int i = 0; i < objs.length; i++) {
      DataObjField f = r.getObjField(i);
      objs[i] = f.getSerialType().createDefaultObjInstance(i, r.getName() + "." + f.getLocalName());
    }
    return new DefaultRecord(id, r.getName(), true, ints, objs);
  }

  private static EventDelegate<?> delegate(DataRecord r) {
    for (EventDelegate<?> d : new MarketFactoryImpl().createDelegates(r))
      if ((d instanceof SpreadOrderDelegate || d instanceof AnalyticOrderDelegate)
          && d.getContract().toString().equals("History")) return d;
    throw new IllegalArgumentException("variantdelegate");
  }

  private static byte[] compose(DataScheme scheme, boolean write) {
    RecordBuffer input = new RecordBuffer(RecordMode.TIMESTAMPED_DATA.withEventFlags());
    for (int id = 0; id < 2; id++)
      for (int sample = 0; sample < PACKED.length; sample++) {
        DataRecord r = scheme.getRecord(id);
        RecordCursor c = input.add(r, 0, "AAPL");
        c.setInt(slot(r, "Index"), sample);
        c.setInt(slot(r, "Time"), 7);
        c.setInt(slot(r, "Sequence"), (123 << 22) | 17);
        c.setInt(slot(r, "Flags"), 4);
        c.setEventFlags(sample % 2);
        c.setEventTimeSequence(PACKED[sample]);
        for (int i = 0; i < r.getObjFieldCount(); i++) c.setObj(i, "SPREAD");
      }
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setWriteEventTimeSequence(write);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    ByteArrayOutput out = new ByteArrayOutput();
    composer.setOutput(out);
    require(!composer.visitData(input, MessageType.HISTORY_DATA), "composer capacity");
    return Arrays.copyOf(out.getBuffer(), out.getPosition());
  }

  private static void parse(
      PrintWriter out, DataScheme scheme, byte[] wire, boolean write, boolean read, String mode) {
    final int[] seen = {0};
    BinaryQTPParser parser = new BinaryQTPParser(scheme);
    parser.setReadEventTimeSequence(read);
    parser.setEventTimeSequence(0);
    parser.setInput(new ByteArrayInput(wire));
    parser.parse(
        new MessageConsumerAdapter() {
          protected void processData(DataIterator iterator, MessageType kind) {
            require(kind == MessageType.HISTORY_DATA, "family");
            RecordCursor c;
            while ((c = ((RecordSource) iterator).next()) != null) {
              DataRecord r = c.getRecord();
              int sample = c.getInt(slot(r, "Index"));
              require(sample >= 0 && sample < PACKED.length, "sample index");
              long expected = write && read ? PACKED[sample] : 0;
              require(c.getEventTimeSequence() == expected, "full signed FIELD precision");
              OrderBase e = (OrderBase) delegate(r).createEvent(c);
              long millis = TimeSequenceUtil.getTimeMillisFromTimeSequence(expected);
              require(e.getEventTime() == millis, "delegate FIELD milliseconds");
              require(
                  e.getTime() == 7123 && e.getSequence() == 17,
                  "FIELD changed business Time/Sequence");
              row(
                  out,
                  "fieldTime",
                  mode,
                  r.getName(),
                  sample,
                  PACKED[sample],
                  c.getEventTimeSequence(),
                  millis,
                  e.getEventTime(),
                  e.getTimeSequence(),
                  e.getTime(),
                  e.getSequence());
              seen[0]++;
            }
          }

          public void handleCorruptedStream() {
            throw new IllegalStateException("corruptedstream");
          }

          public void handleCorruptedMessage(int type) {
            throw new IllegalStateException("corruptedmessage");
          }

          public void handleUnknownMessage(int type) {
            throw new IllegalStateException("unknownmessage");
          }
        });
    require(seen[0] == 16, "complete typed FIELD observations");
    row(out, "parsed", mode, seen[0]);
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 2 && args[1].equals("field"), "fresh TSV and field profile required");
    DataScheme standard = DXFeedScheme.getInstance();
    DataRecord[] r = {
      cloneRecord(standard.findRecordByName("SpreadOrder"), 0),
      cloneRecord(standard.findRecordByName("AnalyticOrder"), 1)
    };
    DataScheme scheme = new DefaultScheme(PentaCodec.INSTANCE, r);
    Path dir = Paths.get(args[0] + ".wire");
    Files.createDirectory(dir);
    byte[] on = compose(scheme, true), off = compose(scheme, false);
    Files.write(dir.resolve("with-time.qtp"), on);
    Files.write(dir.resolve("without-time.qtp"), off);
    PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8));
    try {
      parse(out, scheme, on, true, true, "on");
      parse(out, scheme, on, true, false, "read-off");
      parse(out, scheme, off, false, true, "write-off");
      out.flush();
      if (out.checkError()) throw new IOException("FIELD flush");
    } finally {
      out.close();
    }
    if (out.checkError()) throw new IOException("FIELD close");
  }
}
