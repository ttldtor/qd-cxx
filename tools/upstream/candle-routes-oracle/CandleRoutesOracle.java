// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.api.impl.EventDelegateSet;
import com.dxfeed.event.candle.CandleByTradeHistoryDelegate;
import com.dxfeed.event.candle.CandleDelegate;
import com.dxfeed.event.candle.CandleFactoryImpl;
import com.dxfeed.event.candle.CandleSymbol;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Captures production Candle delegate selection and source symbol conversion. */
public final class CandleRoutesOracle {
  private static final class NoCandleDelegates extends IllegalStateException {
    NoCandleDelegates() {
      super("No Candle delegates");
    }
  }

  private static final String[] ALL = {
    "Candle",
    "TradeHistory",
    "Trade.1min",
    "Candle{=m,price=bid}",
    "Trade.5min",
    "Candle{=m,price=ask}"
  };
  private static final String[] SYMBOLS = {
    "IBM",
    "IBM{price=bid}",
    "IBM&Q{=1m}",
    "IBM&Q{=1m,price=bid}",
    "IBM{=1m,price=ask}",
    "IBM{=1m,price=mark}",
    "IBM{=5m,price=bid}",
    "IBM{=7m}",
    "IBM{=m,price=bid,tho=true}",
    "IBM{=m,price=s}"
  };

  private static void row(PrintWriter out, Object... values) {
    for (int index = 0; index < values.length; ++index) {
      if (index != 0) {
        out.print('\t');
      }
      out.print(values[index]);
    }
    out.print('\n');
  }

  private static List<DataIntField> logicalFields(DataRecord record) {
    List<DataIntField> fields = new ArrayList<>();
    for (int index = 0; index < record.getIntFieldCount(); ++index) {
      DataIntField field = record.getIntField(index);
      if (field.getSerialType().getId() != 0) {
        fields.add(field);
      }
    }
    return fields;
  }

  private static DefaultRecord copyRecord(int id, String name, DataRecord source) {
    List<DataIntField> fields = new ArrayList<>();
    for (DataIntField original : logicalFields(source)) {
      String fieldName = name + "." + original.getLocalName();
      fields.add(original.getSerialType().createDefaultIntInstance(fields.size(), fieldName));
      if (original.getSerialType().isLong()) {
        fields.add(new VoidIntField(fields.size(), fieldName + "$VoidTail"));
      }
    }
    return new DefaultRecord(
        id, name, true, fields.toArray(new DataIntField[0]), new DataObjField[0]);
  }

  private static DataScheme scheme(String[] names) {
    DataScheme standard = DXFeedScheme.getInstance();
    DataRecord candle = standard.findRecordByName("Candle");
    DataRecord trade = standard.findRecordByName("TradeHistory");
    if (candle == null || trade == null) {
      throw new IllegalStateException("Pinned standard Candle records are unavailable");
    }
    DataRecord[] records = new DataRecord[names.length];
    for (int index = 0; index < names.length; ++index) {
      records[index] =
          copyRecord(index, names[index], names[index].equals("TradeHistory") ? trade : candle);
    }
    return new DefaultScheme(PentaCodec.INSTANCE, records);
  }

  @SuppressWarnings({"rawtypes", "unchecked"})
  private static EventDelegateSet delegateSet(DataScheme scheme, QDContract contract) {
    EventDelegateSet set = null;
    CandleFactoryImpl factory = new CandleFactoryImpl();
    for (int index = 0; index < scheme.getRecordCount(); ++index) {
      DataRecord record = scheme.getRecord(index);
      for (EventDelegate<?> delegate : factory.createDelegates(record)) {
        if (!(delegate instanceof CandleDelegate
                || delegate instanceof CandleByTradeHistoryDelegate)
            || delegate.getContract() != contract) {
          continue;
        }
        if (set == null) {
          set = delegate.createDelegateSet();
        }
        set.add(delegate);
      }
    }
    if (set == null) {
      throw new NoCandleDelegates();
    }
    set.completeConstruction();
    return set;
  }

  private static void metadata(PrintWriter out, String catalog, DataScheme scheme) {
    for (int id = 0; id < scheme.getRecordCount(); ++id) {
      DataRecord record = scheme.getRecord(id);
      row(out, "record", catalog, id, record.getName(), record.hasTime());
      for (DataIntField field : logicalFields(record)) {
        row(
            out,
            "field",
            catalog,
            id,
            field.getSerialType().getId(),
            field.getLocalName(),
            field.getPropertyName());
      }
    }
  }

  @SuppressWarnings({"rawtypes", "unchecked"})
  private static void observations(
      PrintWriter out, String catalog, DataScheme scheme, QDContract contract) {
    EventDelegateSet set = delegateSet(scheme, contract);
    for (String text : SYMBOLS) {
      CandleSymbol symbol = CandleSymbol.valueOf(text);
      List<EventDelegate<?>> delegates =
          contract == QDContract.HISTORY
              ? set.getTimeSeriesDelegatesByEventSymbol(symbol)
              : set.getSubDelegatesBySubscriptionSymbol(symbol, -1);
      if (delegates.isEmpty()) {
        row(out, "route", catalog, contract, symbol, 0, "-", "-", "-", "-");
      } else {
        for (EventDelegate<?> delegate : delegates) {
          String physical = delegate.getQDSymbolByEventSymbol(symbol);
          Object restored = delegate.getEventSymbolByQDSymbol(physical);
          if (!symbol.equals(restored)) {
            throw new IllegalStateException(
                "Candle symbol did not round-trip for " + delegate.getRecord().getName());
          }
          row(
              out,
              "route",
              catalog,
              contract,
              symbol,
              delegates.size(),
              delegate.getRecord().getName(),
              physical,
              delegate instanceof CandleByTradeHistoryDelegate ? "TRADE_HISTORY" : "CANDLE",
              restored);
        }
      }
    }
  }

  private static void expectRefusal(
      PrintWriter out, String name, String[] records, QDContract contract) {
    try {
      delegateSet(scheme(records), contract);
      throw new IllegalStateException("Unexpected accepted record catalog " + name);
    } catch (IllegalArgumentException expected) {
      row(out, "reject", name, contract, expected.getClass().getSimpleName());
    } catch (NoCandleDelegates expected) {
      row(out, "reject", name, contract, "NO_DELEGATE");
    }
  }

  /** One fresh output directory; no arguments or --help print help without a capture. */
  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "CandleRoutesOracle <новый-каталог>: офлайн-захват маршрутов Candle в TSV."
                  + " Успех: 0; ошибка: ненулевой код."
              : "CandleRoutesOracle <new-directory>: offline Java QD candle routing TSV. "
                  + "Exit 0 on success, nonzero on failure.");
      return;
    }
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected exactly one new output directory");
    }
    Path directory = Files.createDirectory(Paths.get(args[0]));
    try (PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(directory.resolve("routes.tsv"), StandardCharsets.UTF_8))) {
      String[][] catalogs = {
        ALL,
        {"Candle", "TradeHistory"},
        {"TradeHistory"},
        {"Candle", "Candle{=m,price=bid}"},
        {"Candle{=m,price=bid}"}
      };
      String[] names = {"all", "generic", "tick-only", "bid-generic", "bid-only"};
      for (int index = 0; index < catalogs.length; ++index) {
        DataScheme candidate = scheme(catalogs[index]);
        metadata(out, names[index], candidate);
        observations(out, names[index], candidate, QDContract.TICKER);
        observations(out, names[index], candidate, QDContract.HISTORY);
      }
      for (QDContract contract : Arrays.asList(QDContract.TICKER, QDContract.HISTORY)) {
        expectRefusal(out, "duplicate-period", new String[] {"Trade.1min", "Candle{=m}"}, contract);
        expectRefusal(
            out, "invalid-price-period", new String[] {"Candle{=2m,price=bid}"}, contract);
        expectRefusal(out, "invalid-price", new String[] {"Candle{=m,price=wrong}"}, contract);
        expectRefusal(
            out, "unsupported-attributes", new String[] {"Candle{=m,tho=true}"}, contract);
      }
      if (out.checkError()) {
        throw new IllegalStateException("Incomplete route observations");
      }
    }
  }
}
