// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
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
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.custom.CustomFactoryImpl;
import com.dxfeed.event.custom.NuamOrder;
import com.dxfeed.event.custom.NuamOrderDelegate;
import com.dxfeed.event.market.Side;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

/** Captures the production Java indexed Nuam order delegate through binary QTP. */
public final class NuamOrderOracle {
  private static void row(PrintWriter out, Object... values) {
    for (int i = 0; i < values.length; ++i) {
      if (i != 0) out.print('\t');
      out.print(values[i]);
    }
    out.print('\n');
  }

  private static String text(String value) {
    return value == null ? "<null>" : value;
  }

  private static NuamOrderDelegate delegate(DataRecord record) {
    for (EventDelegate<?> candidate : new CustomFactoryImpl().createDelegates(record))
      if (candidate instanceof NuamOrderDelegate && candidate.getContract() == QDContract.HISTORY)
        return (NuamOrderDelegate) candidate;
    throw new IllegalStateException("Missing indexed delegate");
  }

  private static void capture(Path directory, DataScheme scheme, String name, String stem)
      throws Exception {
    DataRecord record = scheme.findRecordByName(name);
    if (record == null) throw new IllegalStateException("Missing default record: " + name);
    NuamOrderDelegate delegate = delegate(record);
    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
    NuamOrder event = new NuamOrder("AAPL");
    event.setIndex(7);
    event.setSource((com.dxfeed.event.market.OrderSource) delegate.getSource());
    event.setEventFlags(0);
    event.setTime(1701);
    event.setSequence(9);
    event.setOrderSide(Side.BUY);
    event.setPrice(30.25);
    event.setSizeAsDouble(12.5);
    event.setActorId(-42);
    event.setParticipantId(123);
    event.setSubmitterId(456);
    event.setOnBehalfOfSubmitterId(789);
    event.setClientOrderId("id");
    event.setCustomerAccount("");
    event.setCustomerInfo(null);
    event.setExchangeInfo("exchange");
    event.setTimeInForceData(-17);
    event.setTriggerOrderBookId(314);
    event.setTriggerPrice(-3.125);
    event.setTriggerSessionType(271);
    event.setOrderQuantity(15.5);
    event.setDisplayQuantity(8.25);
    event.setRefreshQuantity(4.125);
    event.setLeavesQuantity(3.5);
    event.setMatchedQuantity(12.0);
    RecordCursor cursor = delegate.putEvent(event, input);
    delegate.getMapping().setNuamFlags(cursor, 0xf6);
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.visitData(input, MessageType.HISTORY_DATA);
    byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
    Files.write(directory.resolve(stem + ".qtp"), wire);
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve(stem + ".tsv"), StandardCharsets.UTF_8))) {
      row(out, "record", record.getId(), record.getName(), record.hasTime());
      for (int i = 0; i < record.getIntFieldCount(); ++i) {
        DataIntField field = record.getIntField(i);
        row(
            out,
            "int-field",
            i,
            field.getSerialType().getId(),
            field.getLocalName(),
            field.getPropertyName());
      }
      for (int i = 0; i < record.getObjFieldCount(); ++i) {
        DataObjField field = record.getObjField(i);
        row(
            out,
            "object-field",
            i,
            field.getSerialType().getId(),
            field.getLocalName(),
            field.getPropertyName());
      }
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parser.setInput(new ByteArrayInput(wire));
      parser.parse(
          new MessageConsumerAdapter() {
            @Override
            protected void processData(DataIterator iterator, MessageType type) {
              if (type != MessageType.HISTORY_DATA)
                throw new IllegalStateException("wrong data type");
              RecordCursor c;
              while ((c = ((RecordSource) iterator).next()) != null) {
                NuamOrder value = delegate.createEvent(c);
                row(
                    out,
                    "event",
                    value.getEventSymbol(),
                    value.getEventFlags(),
                    value.getIndex(),
                    value.getSource().id(),
                    value.getTime(),
                    value.getSequence(),
                    value.getActorId(),
                    value.getParticipantId(),
                    value.getSubmitterId(),
                    value.getOnBehalfOfSubmitterId(),
                    text(value.getClientOrderId()),
                    text(value.getCustomerAccount()),
                    text(value.getCustomerInfo()),
                    text(value.getExchangeInfo()),
                    value.getTimeInForceData(),
                    value.getTriggerOrderBookId(),
                    value.getTriggerSessionType(),
                    delegate.getMapping().getNuamFlags(c),
                    value.getOrderType().getCode(),
                    value.getTimeInForce().getCode(),
                    Double.doubleToLongBits(value.getTriggerPrice()),
                    Double.doubleToLongBits(value.getOrderQuantity()),
                    Double.doubleToLongBits(value.getDisplayQuantity()),
                    Double.doubleToLongBits(value.getRefreshQuantity()),
                    Double.doubleToLongBits(value.getLeavesQuantity()),
                    Double.doubleToLongBits(value.getMatchedQuantity()));
              }
            }

            @Override
            public void handleCorruptedStream() {
              throw new IllegalStateException("Corrupted stream");
            }

            @Override
            public void handleCorruptedMessage(int type) {
              throw new IllegalStateException("Corrupted message");
            }
          });
      if (out.checkError()) throw new IllegalStateException("Cannot write observation");
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "NuamOrderOracle <новый-каталог>: офлайн QTP и TSV; код 0 при успехе, иначе"
                  + " ненулевой."
              : "NuamOrderOracle <new-directory>: offline QTP and TSV; exit 0 on success, nonzero"
                  + " on error.");
      return;
    }
    if (args.length != 1) throw new IllegalArgumentException("Expected one new output directory");
    Path output = Files.createDirectory(Paths.get(args[0]));
    DataScheme scheme = DXFeedScheme.getInstance();
    capture(output, scheme, "NuamOrder", "default");
    capture(output, scheme, "NuamOrder#NUAM", "upper");
    capture(output, scheme, "NuamOrder#nuam", "lower");
  }
}
