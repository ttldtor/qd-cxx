// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegate;
import com.dxfeed.event.market.MarketFactoryImpl;
import com.dxfeed.event.market.Order;
import com.dxfeed.event.market.OrderBase;
import com.dxfeed.event.market.OrderDelegate;
import com.dxfeed.event.market.OrderSource;
import com.dxfeed.event.market.impl.OrderMapping;
import java.io.File;
import java.io.PrintWriter;
import java.lang.reflect.Field;

/**
 * Observes production mapping, delegate and order-index methods without implementing their
 * algorithms.
 */
public final class OrderIndexOracle {
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
    StringBuilder value = new StringBuilder();
    for (int i = 0; i < text.length(); ++i) {
      value.append(String.format("%04x", (int) text.charAt(i)));
    }
    return value.toString();
  }

  private static DataRecord variant(DataRecord source, String name, SerialFieldType indexType) {
    DataIntField[] ints = new DataIntField[source.getIntFieldCount()];
    for (int i = 0; i < ints.length; ++i) {
      DataIntField field = source.getIntField(i);
      SerialFieldType type =
          field.getLocalName().equals("Index") ? indexType : field.getSerialType();
      ints[i] = type.createDefaultIntInstance(i, name + "." + field.getLocalName());
    }
    DataObjField[] objects = new DataObjField[source.getObjFieldCount()];
    for (int i = 0; i < objects.length; ++i) {
      DataObjField field = source.getObjField(i);
      objects[i] =
          field.getSerialType().createDefaultObjInstance(i, name + "." + field.getLocalName());
    }
    return new DefaultRecord(0, name, source.hasTime(), ints, objects);
  }

  private static OrderDelegate delegate(DataRecord record) {
    for (EventDelegate<?> candidate : new MarketFactoryImpl().createStreamOnlyDelegates(record)) {
      if (candidate instanceof OrderDelegate) {
        return (OrderDelegate) candidate;
      }
    }
    throw new IllegalArgumentException("No production Order delegate");
  }

  private static void record(PrintWriter out, DataRecord original, String name) {
    DataRecord record;
    OrderMapping mapping;
    try {
      record = variant(original, name, SerialFieldType.COMPACT_INT);
      new DefaultScheme(PentaCodec.INSTANCE, new DataRecord[] {record});
      mapping = new OrderMapping(record);
    } catch (RuntimeException error) {
      row(out, "RECORD_INVALID", units(name), error.getClass().getSimpleName());
      return;
    }
    for (int field = 0; field < record.getIntFieldCount(); ++field) {
      DataIntField value = record.getIntField(field);
      if (value.getSerialType().getId() != 0) {
        row(out, "FIELD", units(name), units(value.getLocalName()), value.getSerialType().getId());
      }
    }
    for (int field = 0; field < record.getObjFieldCount(); ++field) {
      DataObjField value = record.getObjField(field);
      row(out, "FIELD", units(name), units(value.getLocalName()), value.getSerialType().getId());
    }
    row(out, "RECORD", units(name), mapping.getRecordSourceId(), (int) mapping.getRecordExchange());
    try {
      OrderDelegate delegate = delegate(record);
      for (int index : new int[] {Integer.MIN_VALUE, -1, 0, 1, Integer.MAX_VALUE}) {
        RecordBuffer buffer = new RecordBuffer(RecordMode.DATA);
        RecordCursor cursor = buffer.add(record, 0, "AAPL");
        mapping.setIndex(cursor, index);
        Order event = delegate.getEvent(new Order(), cursor);
        row(out, "DELEGATE", units(name), index, event.getIndex(), event.getSource().id());
      }
    } catch (RuntimeException error) {
      row(out, "DELEGATE_UNAVAILABLE", units(name), error.getClass().getSimpleName());
    }
  }

  /** Writes one requested TSV; all production calls and failures are retained as observations. */
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected one output filename");
    }
    Field indexField = OrderBase.class.getDeclaredField("index");
    indexField.setAccessible(true);
    try (PrintWriter out = new PrintWriter(new File(args[0]), "UTF-8")) {
      int[] sourceIds = {
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 31, 32, 65, 0x4e5456, 0x54455354, Integer.MAX_VALUE
      };
      for (int id : sourceIds) {
        row(out, "SPECIAL", id, OrderSource.isSpecialSourceId(id));
      }
      long[] indices = {
        Long.MIN_VALUE,
        -1,
        0,
        1,
        0xffffffffL,
        0x100000000L,
        0x0001000000000000L,
        0x0001005100000001L,
        0x0009007affffffffL,
        0x000a005100000001L,
        0x004e545680000000L,
        0x7fffffffffffffffL,
        0xffff0051ffffffffL
      };
      for (long index : indices) {
        Order event = new Order();
        // Deliberate raw-state probe: negative indices cannot enter through public setIndex.
        indexField.setLong(event, index);
        try {
          row(out, "GET", index, event.getSource().id());
        } catch (RuntimeException error) {
          row(out, "GET_INVALID", index, error.getClass().getSimpleName());
        }
        for (int id : sourceIds) {
          OrderSource source;
          try {
            source = OrderSource.valueOf(id);
          } catch (RuntimeException error) {
            continue;
          }
          indexField.setLong(event, index);
          event.setSource(source);
          row(out, "CHANGE", index, id, event.getIndex());
        }
        try {
          event.setIndex(index);
          row(out, "SET_INDEX", index, event.getIndex());
        } catch (RuntimeException error) {
          row(out, "SET_INDEX_INVALID", index, error.getClass().getSimpleName());
        }
      }
      DataRecord original = DXFeedScheme.getInstance().findRecordByName("Order#NTV");
      if (original == null) {
        throw new IllegalStateException("Standard Order#NTV not available");
      }
      for (String name :
          new String[] {
            "Order",
            "Order#NTV",
            "Order#ntv",
            "Order#TEST",
            "Order#A",
            "Order#1234",
            "Order#NTV#BZX",
            "Order#",
            "Order#DEFAULT",
            "Order#COMPOSITE_BID",
            "Order#A_B",
            "Order#é",
            "Order&Q#NTV",
            "Order#NTV&Q"
          }) {
        record(out, original, name);
      }
      for (SerialFieldType type :
          new SerialFieldType[] {
            SerialFieldType.BYTE,
            SerialFieldType.SHORT,
            SerialFieldType.INT,
            SerialFieldType.COMPACT_INT,
            SerialFieldType.DECIMAL,
            SerialFieldType.SEQUENCE
          }) {
        try {
          DataRecord record = variant(original, "Order#NTV", type);
          OrderMapping mapping = new OrderMapping(record);
          RecordCursor cursor = new RecordBuffer(RecordMode.DATA).add(record, 0, "AAPL");
          mapping.setIndex(cursor, -1);
          row(out, "PHYSICAL_INT", type.getId(), mapping.getIndex(cursor));
        } catch (RuntimeException error) {
          row(out, "PHYSICAL_INT_INVALID", type.getId(), error.getClass().getSimpleName());
        }
      }
      if (out.checkError()) {
        throw new IllegalStateException("Cannot write observations");
      }
    }
  }
}
