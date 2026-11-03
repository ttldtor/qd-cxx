// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.api.impl.EventDelegateSet;
import com.dxfeed.event.market.MarketEvent;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.Profile;
import com.dxfeed.event.market.ProfileDelegate;
import com.dxfeed.event.market.Quote;
import com.dxfeed.event.market.QuoteDelegate;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Observes actual generated production schemas, mappings, delegates and value getters offline. */
public final class EventTimeOracle {
  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) {
        out.print('\t');
      }
      out.print(cells[i]);
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

  private static EventDelegate<?> delegate(DataRecord record) {
    for (EventDelegate<?> value : new MarketFactoryImpl().createStreamOnlyDelegates(record)) {
      if (value instanceof QuoteDelegate || value instanceof ProfileDelegate) {
        return value;
      }
    }
    throw new IllegalArgumentException("No Quote/Profile delegate: " + record.getName());
  }

  @SuppressWarnings("unchecked")
  private static void routes(PrintWriter out, DataScheme scheme) {
    QuoteDelegate composite = (QuoteDelegate) delegate(scheme.findRecordByName("Quote"));
    EventDelegateSet<Quote, QuoteDelegate> mixed =
        (EventDelegateSet<Quote, QuoteDelegate>) composite.createDelegateSet();
    EventDelegateSet<Quote, QuoteDelegate> fallback =
        (EventDelegateSet<Quote, QuoteDelegate>) composite.createDelegateSet();
    for (String name : new String[] {"Quote", "Quote&Q", "Quote&z"}) {
      mixed.add((QuoteDelegate) delegate(scheme.findRecordByName(name)));
    }
    fallback.add(composite);
    mixed.completeConstruction();
    fallback.completeConstruction();
    for (String symbol :
        new String[] {
          "AAPL", "AAPL&Q", "AAPL&z", "AAPL&N", "AAPL{x=y}", "AAPL&Q{x=y}", "AAPL&N{x=y}"
        }) {
      for (EventDelegateSet<Quote, QuoteDelegate> set : new EventDelegateSet[] {mixed, fallback}) {
        List<QuoteDelegate> selected = set.getSubDelegatesBySubscriptionSymbol(symbol, 0);
        if (selected.size() != 1) {
          throw new IllegalStateException(
              "Expected a sole production regional/composite selection");
        }
        QuoteDelegate chosen = selected.get(0);
        row(
            out,
            "selection",
            "default",
            set == mixed ? 0 : 1,
            units(symbol),
            chosen.getRecord().getName(),
            units(chosen.getQDSymbolByEventSymbol(symbol)));
      }
      for (int id = 0; id < 3; ++id) {
        EventDelegate<?> mapper = delegate(scheme.getRecord(id));
        row(
            out,
            "route",
            "default",
            id,
            units(symbol),
            units(mapper.getQDSymbolByEventSymbol(symbol)),
            units((String) mapper.getEventSymbolByQDSymbol(symbol)));
      }
    }
  }

  private static void representability(Path output, DataScheme scheme) throws Exception {
    EventDelegate<?> mapper = delegate(scheme.findRecordByName("Quote&Q"));
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(output, StandardCharsets.UTF_8))) {
      for (String logical :
          new String[] {"AAPL&N&Q", "AAPL{x=y}&Q", "AAPL&Q{x=y}", "AAPL{x=y}&Q{a=b}"}) {
        String physical = mapper.getQDSymbolByEventSymbol(logical);
        String restored = (String) mapper.getEventSymbolByQDSymbol(physical);
        row(
            out,
            "roundtrip",
            units(logical),
            units(physical),
            units(restored),
            logical.equals(restored));
      }
      if (out.checkError()) {
        throw new IllegalStateException("Cannot write representability observations");
      }
    }
  }

  private static DataRecord variant(DataRecord source, int id, String mode) {
    List<DataField> fields = new ArrayList<>();
    for (int i = 0; i < source.getIntFieldCount(); ++i) {
      if (source.getIntField(i).getSerialType().getId() != 0) {
        fields.add(source.getIntField(i));
      }
    }
    for (int i = 0; i < source.getObjFieldCount(); ++i) {
      fields.add(source.getObjField(i));
    }
    if (mode.equals("reordered")) {
      Collections.reverse(fields);
    }
    List<DataIntField> ints = new ArrayList<>();
    List<DataObjField> objects = new ArrayList<>();
    for (DataField field : fields) {
      boolean required =
          source.getName().equals("Profile")
              ? field.getLocalName().equals("Description")
              : field.getLocalName().matches("(Bid|Ask)\\.(Price|Size)");
      if (mode.equals("minimal") && !required || mode.equals("missing") && required) {
        continue;
      }
      SerialFieldType type = field.getSerialType();
      if (field instanceof DataObjField) {
        objects.add(type.createDefaultObjInstance(objects.size(), field.getName()));
      } else {
        ints.add(type.createDefaultIntInstance(ints.size(), field.getName()));
        if (type.isLong()) {
          ints.add(new VoidIntField(ints.size(), field.getName() + "$VoidTail"));
        }
      }
    }
    return new DefaultRecord(
        id,
        source.getName(),
        false,
        ints.toArray(new DataIntField[0]),
        objects.toArray(new DataObjField[0]));
  }

  private static void observe(PrintWriter out, DataRecord record, String scenario) {
    row(out, "record", scenario, record.getId(), record.getName());
    List<DataField> fields = new ArrayList<>();
    for (int i = 0; i < record.getIntFieldCount(); ++i) {
      if (record.getIntField(i).getSerialType().getId() != 0) {
        fields.add(record.getIntField(i));
      }
    }
    for (int i = 0; i < record.getObjFieldCount(); ++i) {
      fields.add(record.getObjField(i));
    }
    for (int i = 0; i < fields.size(); ++i) {
      DataField field = fields.get(i);
      row(
          out,
          "field",
          scenario,
          record.getId(),
          i,
          field.getSerialType().getId(),
          field.getLocalName(),
          field.getPropertyName());
    }
    EventDelegate<?> mapper;
    try {
      mapper = delegate(record);
    } catch (IllegalArgumentException failure) {
      row(out, "reject", scenario, record.getId(), failure.getClass().getSimpleName());
      return;
    }
    long[] seconds = {0, -1, Integer.MIN_VALUE, Integer.MAX_VALUE, 17};
    long[] askSeconds = {-1, 0, (long) Integer.MIN_VALUE + 1, (long) Integer.MAX_VALUE - 1, 18};
    long[] millis = {0, -1, Long.MIN_VALUE, Long.MAX_VALUE, -1001};
    long[] askMillis = {-1001, -2001, Long.MIN_VALUE + 1, Long.MAX_VALUE - 1, 9001};
    int[] sequence = {
      0, (999 << 22) | Quote.MAX_SEQUENCE, -1, Integer.MIN_VALUE, Integer.MAX_VALUE
    };
    int[] nano = {0, 999999, -1, Integer.MIN_VALUE, Integer.MAX_VALUE};
    long[] eventTime = {0, -1, Long.MIN_VALUE, Long.MAX_VALUE, 0x123456789abcdef0L};
    for (int sample = 0; sample < seconds.length; ++sample) {
      RecordBuffer buffer = new RecordBuffer(RecordMode.DATA.withEventTimeSequence());
      String symbol = sample == 4 ? "AAPL&N{a=b}" : "AAPL";
      RecordCursor cursor = buffer.add(record, 0, symbol);
      cursor.setEventTimeSequence(eventTime[sample]);
      for (DataField field : fields) {
        if (field instanceof DataObjField) {
          cursor.setObj(field.getIndex(), sample == 0 ? null : sample == 1 ? "" : "text");
          continue;
        }
        long raw = 0;
        if (field.getLocalName().equals("Sequence")) {
          raw = sequence[sample];
        } else if (field.getLocalName().equals("TimeNanoPart")) {
          raw = nano[sample];
        } else if (field.getSerialType().getId() == SerialFieldType.TIME_SECONDS.getId()) {
          raw = field.getLocalName().equals("Ask.Time") ? askSeconds[sample] : seconds[sample];
        } else if (field.getSerialType().getId() == SerialFieldType.TIME_MILLIS.getId()) {
          raw = field.getLocalName().equals("Ask.Time") ? askMillis[sample] : millis[sample];
        } else if (field.getSerialType().getId() == SerialFieldType.UTF_CHAR.getId()) {
          raw = sample == 0 ? 0 : 'N';
        }
        if (field.getSerialType().isLong()) {
          cursor.setLong(field.getIndex(), raw);
        } else {
          cursor.setInt(field.getIndex(), (int) raw);
        }
      }
      row(out, "input", scenario, record.getId(), sample, units(symbol), eventTime[sample]);
      for (int i = 0; i < fields.size(); ++i) {
        DataField field = fields.get(i);
        boolean object = field instanceof DataObjField;
        long raw =
            object
                ? 0
                : field.getSerialType().isLong()
                    ? cursor.getLong(field.getIndex())
                    : cursor.getInt(field.getIndex());
        row(
            out,
            "value",
            scenario,
            record.getId(),
            sample,
            i,
            raw,
            object ? units((String) cursor.getObj(field.getIndex())) : "null");
      }
      MarketEvent event = (MarketEvent) mapper.createEvent(cursor);
      if (event instanceof Quote) {
        Quote quote = (Quote) event;
        row(
            out,
            "quote",
            scenario,
            record.getId(),
            sample,
            units(quote.getEventSymbol()),
            quote.getBidTime(),
            (int) quote.getBidExchangeCode(),
            quote.getAskTime(),
            (int) quote.getAskExchangeCode(),
            quote.getTime(),
            quote.getSequence(),
            quote.getTimeNanoPart(),
            quote.getEventTime(),
            quote.getTimeNanos());
      } else {
        Profile profile = (Profile) event;
        row(
            out,
            "profile",
            scenario,
            record.getId(),
            sample,
            units(profile.getEventSymbol()),
            profile.getHaltStartTime(),
            profile.getHaltEndTime(),
            profile.getEventTime(),
            units(profile.getDescription()),
            units(profile.getStatusReason()));
      }
    }
  }

  /**
   * Takes output TSV and scenario; starts no network or endpoint and propagates errors to process
   * exit.
   */
  public static void main(String[] args) throws Exception {
    if (args.length != 2) {
      throw new IllegalArgumentException("Expected output TSV filename and scenario");
    }
    String scenario = args[1];
    DataScheme source = DXFeedScheme.getInstance();
    DataRecord[] records = new DataRecord[4];
    String[] names = {"Quote", "Quote&Q", "Quote&z", "Profile"};
    for (int i = 0; i < records.length; ++i) {
      DataRecord original = source.findRecordByName(names[i]);
      if (original == null) {
        throw new IllegalStateException("Missing production record: " + names[i]);
      }
      records[i] = variant(original, i, scenario);
    }
    DataScheme selected = new DefaultScheme(PentaCodec.INSTANCE, records);
    try (PrintWriter out =
        new PrintWriter(Files.newBufferedWriter(Paths.get(args[0]), StandardCharsets.UTF_8))) {
      for (int i = 0; i < selected.getRecordCount(); ++i) {
        observe(out, selected.getRecord(i), scenario);
      }
      if (scenario.equals("default")) {
        routes(out, selected);
        Path output = Paths.get(args[0]);
        representability(
            output.resolveSibling(
                output.getFileName().toString().replaceFirst("\\.tsv$", "-representability.tsv")),
            selected);
      }
      if (out.checkError()) {
        throw new IllegalStateException("Cannot write observations");
      }
    }
  }
}
