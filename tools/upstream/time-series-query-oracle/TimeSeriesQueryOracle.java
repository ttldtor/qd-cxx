// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.util.TimeSequenceUtil;
import com.dxfeed.event.market.TimeAndSale;
import com.dxfeed.impl.AbstractIndexedList;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

/** Captures production Java time-sequence and indexed-list behavior offline. */
public final class TimeSeriesQueryOracle {
  private static final class IndexedList extends AbstractIndexedList<TimeAndSale> {
    @Override
    protected long getIndex(TimeAndSale event) {
      return event.getIndex();
    }

    void insert(TimeAndSale event) {
      updateImpl(event, false);
    }
  }

  private static void row(PrintWriter out, Object... cells) {
    for (int i = 0; i < cells.length; ++i) {
      if (i != 0) out.print('\t');
      out.print(cells[i]);
    }
    out.print('\n');
  }

  private static long raw(int seconds, int millis, int sequence) {
    return ((long) seconds << 32) | ((long) millis << 22) | sequence;
  }

  private static void capture(Path file) throws Exception {
    try (PrintWriter out = new PrintWriter(Files.newBufferedWriter(file, StandardCharsets.UTF_8))) {
      int[] seconds = {Integer.MIN_VALUE, -2, -1, 0, 1, 2, Integer.MAX_VALUE};
      int[] millis = {0, 1, 23, 24, 999, 1000, 1001, 1023};
      int[] sequences = {0, 1, (1 << 22) - 1};
      for (int second : seconds)
        for (int milli : millis)
          for (int sequence : sequences) {
            long index = raw(second, milli, sequence);
            TimeAndSale event = new TimeAndSale("TEST");
            event.setIndex(index);
            row(
                out,
                "event",
                index,
                event.getTime(),
                event.getSequence(),
                TimeSequenceUtil.getTimeMillisFromTimeSequence(index));
          }
      long upper = TimeSequenceUtil.getTimeSequenceFromTimeMillis(1000);
      long sameMillisNonzeroSequence = raw(1, 0, 3);
      row(
          out,
          "inclusive",
          1000,
          upper,
          sameMillisNonzeroSequence,
          TimeSequenceUtil.getTimeMillisFromTimeSequence(sameMillisNonzeroSequence),
          sameMillisNonzeroSequence <= upper);
      IndexedList list = new IndexedList();
      for (long index : new long[] {raw(1, 0, 3), raw(0, 1000, 7), raw(1, 0, 1)}) {
        TimeAndSale event = new TimeAndSale("TEST");
        event.setIndex(index);
        list.insert(event);
      }
      for (TimeAndSale event : list) row(out, "order", event.getIndex(), event.getTime());
      if (out.checkError()) throw new IllegalStateException("Cannot write observations");
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length == 0
        || args.length == 1 && ("--help".equals(args[0]) || "--help=ru".equals(args[0]))) {
      System.out.println(
          args.length == 1 && "--help=ru".equals(args[0])
              ? "TimeSeriesQueryOracle <новый-файл>: офлайн-наблюдения QD; успех 0, ошибка"
                  + " ненулевая."
              : "TimeSeriesQueryOracle <new-file>: offline QD observations; exit 0 on success,"
                  + " nonzero on error.");
      return;
    }
    if (args.length != 1 || Files.exists(Paths.get(args[0])))
      throw new IllegalArgumentException("Expected one new file");
    capture(Paths.get(args[0]));
  }
}
