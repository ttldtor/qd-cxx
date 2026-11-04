// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package com.dxfeed.api.impl;

import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.DXFeedFilter;
import com.dxfeed.api.DXFeedFilterListener;
import com.dxfeed.api.DXFeedFilterTracker;
import com.dxfeed.event.market.Quote;
import com.dxfeed.ipf.filter.IPFSymbolFilter;
import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.concurrent.atomic.AtomicInteger;

/** Finite local-file witnesses of the production IPF filter and lazy tracker. */
public final class FeedFilterIpfOracle {
  private static void write(File path, String contents) throws Exception {
    Files.write(path.toPath(), contents.getBytes(StandardCharsets.UTF_8));
  }

  private static String ipf(String symbol) {
    return "#STOCK::=TYPE,SYMBOL\nSTOCK," + symbol + "\n##COMPLETE\n";
  }

  private static IPFSymbolFilter leaf(DXFeedFilter filter) {
    return (IPFSymbolFilter) ((DXFeedFilterImpl) filter).getCategoryFilters()[0];
  }

  private static void accept(String label, DXFeedFilter filter) {
    System.out.println(
        label
            + "\t"
            + filter.accept(Quote.class, "AAPL")
            + "\t"
            + filter.accept(Quote.class, "AAPL{x=y}")
            + "\t"
            + filter.accept(Quote.class, "MSFT")
            + "\t"
            + filter.isDynamic());
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 2) {
      throw new IllegalArgumentException("Expected TSV and owned temporary-file paths");
    }
    File path = new File(args[1]);
    File schedulePath = new File(args[1] + ".scheduled");
    DXEndpoint endpoint = DXEndpoint.create();
    try (PrintStream output = new PrintStream(args[0], "UTF-8")) {
      System.setOut(output);
      write(path, ipf("AAPL"));
      String spec = "ipf[" + path.toURI().toString() + ",update=1d]";
      DXFeedFilter initial = DXFeedFilter.newBuilder(endpoint).withCategory("ipf", spec).build();
      DXFeedFilterTracker tracker = initial.getTracker();
      accept("initial", initial);
      System.out.println("initial-current\t" + (tracker.getCurrentFilter() == initial));
      write(path, ipf("MSFT"));
      leaf(initial).forceUpdate();
      System.out.println("no-listener-stale\t" + (tracker.getCurrentFilter() == initial));
      AtomicInteger changes = new AtomicInteger();
      DXFeedFilterListener counter = value -> changes.incrementAndGet();
      tracker.addListener(counter);
      DXFeedFilter current = tracker.getCurrentFilter();
      accept("catchup", current);
      accept("old-stable", initial);
      System.out.println("catchup-notification\t" + changes.get());
      write(path, ipf("AAPL"));
      leaf(current).forceUpdate();
      current = tracker.getCurrentFilter();
      accept("updated", current);
      System.out.println("change-notification\t" + changes.get());
      leaf(current).forceUpdate();
      System.out.println(
          "same-content\t" + (tracker.getCurrentFilter() == current) + "\t" + changes.get());
      write(path, "#STOCK::=TYPE,SYMBOL\nSTOCK,\"unterminated\n");
      leaf(current).forceUpdate();
      System.out.println(
          "failed-update-keeps\t" + (tracker.getCurrentFilter() == current) + "\t" + changes.get());
      DXFeedFilterListener throwing =
          value -> {
            throw new IllegalStateException("deliberate finite listener witness");
          };
      tracker.addListener(throwing);
      AtomicInteger afterThrow = new AtomicInteger();
      DXFeedFilterListener tail = value -> afterThrow.incrementAndGet();
      tracker.addListener(tail);
      write(path, ipf("MSFT"));
      leaf(current).forceUpdate();
      current = tracker.getCurrentFilter();
      System.out.println("throw-contained\t" + changes.get() + "\t" + afterThrow.get());
      tracker.removeListener(counter);
      tracker.removeListener(tail);
      tracker.removeListener(throwing);
      DXFeedFilter stopped = tracker.getCurrentFilter();
      write(path, ipf("AAPL"));
      leaf(stopped).forceUpdate();
      System.out.println("last-listener-stops-current\t" + (tracker.getCurrentFilter() == stopped));
      tracker.addListener(counter);
      current = tracker.getCurrentFilter();
      System.out.println(
          "restart-catchup\t" + current.accept(Quote.class, "AAPL") + "\t" + changes.get());
      tracker.removeListener(counter);
      AtomicInteger trailing = new AtomicInteger();
      DXFeedFilterListener tailSelected = value -> trailing.incrementAndGet();
      DXFeedFilterListener[] self = new DXFeedFilterListener[1];
      self[0] =
          value -> {
            value.removeListener(self[0]);
            value.removeListener(tailSelected);
          };
      tracker.addListener(self[0]);
      tracker.addListener(tailSelected);
      write(path, ipf("MSFT"));
      leaf(current).forceUpdate();
      System.out.println("selected-removal-trailing\t" + trailing.get());
      write(
          schedulePath, "#STOCK::=TYPE,SYMBOL,TRADING_HOURS\nSTOCK,AAPL,(tz=UTC;0=)\n##COMPLETE\n");
      DXFeedFilter scheduled =
          DXFeedFilter.newBuilder(endpoint)
              .withCategory(
                  "scheduled", "ipf[" + schedulePath.toURI().toString() + ",update=1d,schedule]")
              .build();
      DXFeedFilterTracker scheduledTracker = scheduled.getTracker();
      AtomicInteger scheduleChanges = new AtomicInteger();
      DXFeedFilterListener scheduleListener = value -> scheduleChanges.incrementAndGet();
      scheduledTracker.addListener(scheduleListener);
      write(
          schedulePath, "#STOCK::=TYPE,SYMBOL,TRADING_HOURS\nSTOCK,AAPL,(tz=GMT;0=)\n##COMPLETE\n");
      leaf(scheduled).forceUpdate();
      System.out.println(
          "trading-hours-only-update\t"
              + scheduleChanges.get()
              + "\t"
              + (scheduledTracker.getCurrentFilter() != scheduled)
              + "\t"
              + scheduledTracker.getCurrentFilter().accept(Quote.class, "AAPL"));
      String header = "#STOCK::=TYPE,SYMBOL,TRADING_HOURS\n";
      write(schedulePath, header + "STOCK,AAPL,(tz=UTC;0=)\nSTOCK,MSFT,(tz=GMT;0=)\n##COMPLETE\n");
      leaf(scheduledTracker.getCurrentFilter()).forceUpdate();
      DXFeedFilter hoursBase = scheduledTracker.getCurrentFilter();
      scheduleChanges.set(0);
      write(schedulePath, header + "STOCK,MSFT,(tz=GMT;0=)\nSTOCK,AAPL,(tz=UTC;0=)\n##COMPLETE\n");
      leaf(hoursBase).forceUpdate();
      System.out.println(
          "trading-hours-order\t"
              + scheduleChanges.get()
              + "\t"
              + (scheduledTracker.getCurrentFilter() == hoursBase));
      write(schedulePath, header + "STOCK,AAPL,(tz=GMT;0=)\nSTOCK,MSFT,(tz=UTC;0=)\n##COMPLETE\n");
      leaf(hoursBase).forceUpdate();
      System.out.println(
          "trading-hours-association\t"
              + scheduleChanges.get()
              + "\t"
              + (scheduledTracker.getCurrentFilter() == hoursBase));
      write(
          path,
          header
              + "STOCK,AAPL,(tz=UTC;0=)\n"
              + "STOCK,MSFT,(tz=GMT;0=)\n"
              + "STOCK,AAPL,(tz=UTC;0=)\n"
              + "##COMPLETE\n");
      leaf(hoursBase).forceUpdate();
      System.out.println(
          "trading-hours-duplicates\t"
              + scheduleChanges.get()
              + "\t"
              + (scheduledTracker.getCurrentFilter() == hoursBase));
      write(schedulePath, header + "STOCK,AAPL,(tz=UTC;0=)\nSTOCK,MSFT,(tz=UTC;0=)\n##COMPLETE\n");
      leaf(hoursBase).forceUpdate();
      System.out.println(
          "trading-hours-set-change\t"
              + scheduleChanges.get()
              + "\t"
              + (scheduledTracker.getCurrentFilter() != hoursBase));
      scheduledTracker.removeListener(scheduleListener);
      endpoint.close();
      System.out.println("snapshot-after-endpoint-close\t" + initial.accept(Quote.class, "AAPL"));
      write(path, "#STOCK::=TYPE,SYMBOL\nSTOCK,AAPL\n");
      try {
        IPFSymbolFilter plain =
            IPFSymbolFilter.create(
                DXFeedScheme.getInstance(), "ipf[" + path.toURI().toString() + "]");
        System.out.println(
            "incomplete-initial\tACCEPTED\t"
                + plain.getNumberOfSymbols()
                + "\t"
                + plain.isDynamic());
      } catch (RuntimeException failure) {
        System.out.println("incomplete-initial\t" + failure.getClass().getName());
      }
      write(path, "#STOCK::=TYPE,SYMBOL\nSTOCK,AAPL\n##COMPLETE\nSTOCK,\"unterminated\n");
      try {
        IPFSymbolFilter.create(DXFeedScheme.getInstance(), "ipf[" + path.toURI().toString() + "]");
        System.out.println("malformed-after-complete\tACCEPTED");
      } catch (RuntimeException failure) {
        System.out.println("malformed-after-complete\t" + failure.getClass().getName());
      }
      output.flush();
      output.close();
      if (output.checkError()) {
        throw new IOException("Failed writing oracle output");
      }
    } finally {
      endpoint.close();
      Files.deleteIfExists(path.toPath());
      Files.deleteIfExists(schedulePath.toPath());
    }
  }
}
