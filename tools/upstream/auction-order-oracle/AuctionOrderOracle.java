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
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
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
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.OrderBase;
import com.dxfeed.event.market.OrderImbalance;
import com.dxfeed.event.market.OrderImbalanceDelegate;
import com.dxfeed.event.market.OtcMarketsOrder;
import com.dxfeed.event.market.OtcMarketsOrderDelegate;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Captures standard OTC and imbalance records through the pinned binary parser and delegates. */
public final class AuctionOrderOracle {
  private static final String[] OTC_NAMES = {"OtcMarketsOrder", "OtcMarketsOrder#pink"};
  private static final String[] IMBALANCE_NAMES = {
    "OrderImbalance", "OrderImbalance#NTV", "OrderImbalance#ARCA", "OrderImbalance#NUAM"
  };
  private static final List<String> REQUIRED_IMBALANCE =
      Arrays.asList("RefPrice", "PairedSize", "ImbalanceSize", "Flags");
  private static final long[] RAW = {
    Long.MIN_VALUE,
    Long.MAX_VALUE,
    Integer.MIN_VALUE,
    Integer.MAX_VALUE,
    -1,
    0,
    1,
    0x0000000100000001L,
    -385,
    -384,
    -383,
    383,
    384,
    385,
    0x7fffffffffffff80L,
    0x8000000000000080L
  };

  private static void row(PrintWriter out, Object... values) {
    for (int i = 0; i < values.length; ++i) {
      if (i != 0) {
        out.print('\t');
      }
      out.print(values[i]);
    }
    out.print('\n');
  }

  private static String units(String text) {
    if (text == null) {
      return "null";
    }
    if (text.isEmpty()) {
      return "-";
    }
    StringBuilder result = new StringBuilder();
    for (char unit : text.toCharArray()) {
      result.append(String.format("%04x", (int) unit));
    }
    return result.toString();
  }

  private static List<DataIntField> logicalFields(DataRecord record) {
    List<DataIntField> result = new ArrayList<>();
    for (int i = 0; i < record.getIntFieldCount(); ++i) {
      DataIntField field = record.getIntField(i);
      if (field.getSerialType().getId() != 0) {
        result.add(field);
      }
    }
    return result;
  }

  private static DataRecord subset(DataRecord original, String missing, boolean otc) {
    String name = original.getName();
    List<DataIntField> fields = new ArrayList<>();
    if (otc) {
      fields.add(new VoidIntField(0, name + ".Void"));
    }
    for (DataIntField field : logicalFields(original)) {
      String local = field.getLocalName();
      if (local.equals(missing)
          || (missing.equals("optional") && !REQUIRED_IMBALANCE.contains(local))) {
        continue;
      }
      fields.add(field.getSerialType().createDefaultIntInstance(fields.size(), name + "." + local));
      if (field.getSerialType().isLong()) {
        fields.add(new VoidIntField(fields.size(), name + "." + local + "$VoidTail"));
      }
    }
    return new DefaultRecord(
        0, name, otc, fields.toArray(new DataIntField[0]), new DataObjField[0]);
  }

  private static EventDelegate<?> delegate(DataRecord record, boolean otc) {
    for (EventDelegate<?> candidate : new MarketFactoryImpl().createDelegates(record)) {
      if ((otc
              && candidate instanceof OtcMarketsOrderDelegate
              && candidate.getContract() == QDContract.HISTORY)
          || (!otc
              && candidate instanceof OrderImbalanceDelegate
              && candidate.getContract() == QDContract.TICKER)) {
        return candidate;
      }
    }
    throw new IllegalArgumentException("Missing production subscription delegate");
  }

  private static long raw(String name, int sample) {
    switch (name) {
      case "Index":
        return new int[] {0, -1, Integer.MIN_VALUE, Integer.MAX_VALUE}[sample % 4];
      case "Time":
        return sample % 2 == 0 ? sample - 7 : Integer.MIN_VALUE + sample;
      case "Sequence":
        return sample % 2 == 0 ? -1 : 0x7fffffff;
      case "Flags":
        return 0x87650000 | sample * 0x425;
      case "OtcMarketsFlags":
        return 0x98100000 | sample * 0x13;
      case "MMID":
        return new int[] {-1, 0, 0x41420043}[sample % 3];
      case "QuoteAccessPayment":
        return new int[] {-30, 0, 30, Integer.MIN_VALUE, Integer.MAX_VALUE}[sample % 5];
      default:
        return RAW[sample % RAW.length];
    }
  }

  private static Object readInternal(Class<?> type, Object event, String name) throws Exception {
    Method method = type.getDeclaredMethod(name);
    method.setAccessible(true);
    return method.invoke(event);
  }

  private static void observed(
      PrintWriter out, String scenario, int sample, EventDelegate<?> delegate, RecordCursor cursor)
      throws Exception {
    row(
        out,
        "symbol",
        scenario,
        sample,
        units(cursor.getDecodedSymbol()),
        cursor.getEventTimeSequence());
    List<DataIntField> logical = logicalFields(cursor.getRecord());
    for (int i = 0; i < logical.size(); ++i) {
      DataIntField field = logical.get(i);
      long value =
          field.getSerialType().isLong()
              ? cursor.getLong(field.getIndex())
              : cursor.getInt(field.getIndex());
      row(out, "value", scenario, sample, i, value);
    }
    if (delegate instanceof OtcMarketsOrderDelegate) {
      OtcMarketsOrder event = ((OtcMarketsOrderDelegate) delegate).createEvent(cursor);
      row(
          out,
          "otc",
          scenario,
          sample,
          units(event.getEventSymbol()),
          event.getEventTime(),
          event.getIndex(),
          event.getSource().id(),
          event.getEventFlags(),
          event.getTimeSequence(),
          event.getTime(),
          event.getTimeNanos(),
          event.getSequence(),
          event.getTimeNanoPart(),
          event.getActionTime(),
          event.getOrderId(),
          event.getAuxOrderId(),
          event.getCount(),
          readInternal(OrderBase.class, event, "getFlags"),
          event.getTradeId(),
          units(event.getMarketMaker()),
          (int) event.getExchangeCode(),
          event.getOrderSide().getCode(),
          event.getScope().getCode(),
          event.getAction().getCode(),
          event.getSize(),
          event.hasSize(),
          Double.doubleToLongBits(event.getPrice()),
          Double.doubleToLongBits(event.getSizeAsDouble()),
          Double.doubleToLongBits(event.getExecutedSize()),
          Double.doubleToLongBits(event.getTradePrice()),
          Double.doubleToLongBits(event.getTradeSize()),
          event.getQuoteAccessPayment(),
          readInternal(OtcMarketsOrder.class, event, "getOtcMarketsFlags"),
          event.isOpen(),
          event.isUnsolicited(),
          event.getOtcMarketsPriceType().getCode(),
          event.isSaturated(),
          event.isAutoExecution(),
          event.isNmsConditional());
    } else {
      OrderImbalance event = ((OrderImbalanceDelegate) delegate).createEvent(cursor);
      row(
          out,
          "imbalance",
          scenario,
          sample,
          units(event.getEventSymbol()),
          event.getEventTime(),
          event.getSource().id(),
          readInternal(OrderImbalance.class, event, "getTimeSequence"),
          event.getTime(),
          event.getSequence(),
          Double.doubleToLongBits(event.getRefPrice()),
          Double.doubleToLongBits(event.getPairedSize()),
          Double.doubleToLongBits(event.getImbalanceSize()),
          Double.doubleToLongBits(event.getNearPrice()),
          Double.doubleToLongBits(event.getFarPrice()),
          readInternal(OrderImbalance.class, event, "getFlags"),
          event.getImbalanceSide().getCode(),
          event.getAuctionType().getCode());
    }
  }

  private static void capture(
      PrintWriter out,
      Path wireRoot,
      DataScheme scheme,
      DataRecord record,
      String scenario,
      boolean otc,
      boolean reject)
      throws Exception {
    row(out, "record", scenario, record.getName(), otc ? "history" : "ticker");
    List<DataIntField> fields = logicalFields(record);
    for (int i = 0; i < fields.size(); ++i) {
      DataIntField field = fields.get(i);
      row(
          out,
          "field",
          scenario,
          i,
          field.getSerialType().getId(),
          field.getLocalName(),
          field.getPropertyName());
    }
    EventDelegate<?> projection;
    try {
      projection = delegate(record, otc);
    } catch (IllegalArgumentException failure) {
      if (!reject) {
        throw failure;
      }
      row(out, "reject", scenario);
      return;
    }
    if (reject) {
      throw new IllegalStateException("Incomplete record selected a delegate");
    }

    RecordBuffer input = new RecordBuffer(RecordMode.FLAGGED_DATA.withEventTimeSequence());
    for (int sample = 0; sample < RAW.length; ++sample) {
      RecordCursor cursor = input.add(record, 0, sample % 2 == 0 ? "AAPL" : "BRK.B");
      cursor.setEventTimeSequence(((long) (sample - 3) << 32) | 0x7fffffffL);
      for (DataIntField field : fields) {
        long value = raw(field.getLocalName(), sample);
        if (field.getSerialType().isLong()) {
          cursor.setLong(field.getIndex(), value);
        } else {
          cursor.setInt(field.getIndex(), (int) value);
        }
      }
      if (otc) {
        cursor.setEventFlags(new int[] {0, 1, 4, 8, 16, 32, 64, 2}[sample % 8]);
      }
    }
    MessageType data = otc ? MessageType.HISTORY_DATA : MessageType.TICKER_DATA;
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOptSet(ProtocolOption.parseProtocolOptions("hs"));
    composer.setOutput(output);
    composer.visitData(input, data);
    byte[] wire = Arrays.copyOf(output.getBuffer(), output.getPosition());
    Files.write(wireRoot.resolve(scenario + ".qtp"), wire);

    BinaryQTPParser parser = new BinaryQTPParser(scheme);
    parser.setInput(new ByteArrayInput(wire));
    int[] seen = new int[1];
    parser.parse(
        new MessageConsumerAdapter() {
          @Override
          protected void processData(DataIterator iterator, MessageType messageType) {
            if (messageType != data) {
              throw new IllegalStateException("Unexpected data contract");
            }
            RecordCursor cursor;
            while ((cursor = ((RecordSource) iterator).next()) != null) {
              if (cursor.getRecord() != record) {
                throw new IllegalStateException("Unexpected projected record identity");
              }
              try {
                observed(out, scenario, seen[0]++, projection, cursor);
              } catch (Exception failure) {
                throw new IllegalStateException(failure);
              }
            }
          }

          @Override
          public void handleCorruptedStream() {
            throw new IllegalStateException("Corrupted binary fixture stream");
          }

          @Override
          public void handleCorruptedMessage(int type) {
            throw new IllegalStateException("Corrupted binary fixture message " + type);
          }

          @Override
          public void handleUnknownMessage(int type) {
            throw new IllegalStateException("Unknown binary fixture message " + type);
          }
        });
    if (seen[0] != RAW.length) {
      throw new IllegalStateException("Incomplete binary round trip");
    }
    row(out, "parsed", scenario, seen[0]);
  }

  /** Inputs: a fresh TSV path and one explicit scheme profile. */
  public static void main(String[] args) throws Exception {
    if (args.length != 2 || !Arrays.asList("default", "tiny", "int", "fob").contains(args[1])) {
      throw new IllegalArgumentException("Expected output TSV and scheme profile");
    }
    String profile = args[1];
    Path root = Files.createDirectory(Paths.get(args[0] + ".wire"));
    DataScheme scheme = DXFeedScheme.getInstance();
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8))) {
      for (String name : OTC_NAMES) {
        capture(
            out,
            root,
            scheme,
            scheme.findRecordByName(name),
            profile + "-" + name.replace('#', '-'),
            true,
            false);
      }
      for (String name : IMBALANCE_NAMES) {
        capture(
            out,
            root,
            scheme,
            scheme.findRecordByName(name),
            profile + "-" + name.replace('#', '-'),
            false,
            false);
      }
      if (profile.equals("default")) {
        for (String missing : Arrays.asList("QuoteAccessPayment", "OtcMarketsFlags")) {
          DataRecord record = subset(scheme.findRecordByName(OTC_NAMES[1]), missing, true);
          capture(
              out,
              root,
              new DefaultScheme(PentaCodec.INSTANCE, new DataRecord[] {record}),
              record,
              "missing-" + missing,
              true,
              true);
        }
        for (String missing :
            Arrays.asList("optional", "RefPrice", "PairedSize", "ImbalanceSize", "Flags")) {
          DataRecord record = subset(scheme.findRecordByName(IMBALANCE_NAMES[1]), missing, false);
          capture(
              out,
              root,
              new DefaultScheme(PentaCodec.INSTANCE, new DataRecord[] {record}),
              record,
              "missing-" + missing,
              false,
              !missing.equals("optional"));
        }
      }
      if (out.checkError()) {
        throw new IOException("Could not write observations");
      }
    }
  }
}
