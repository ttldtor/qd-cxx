// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.QDAgent;
import com.devexperts.qd.QDCollector;
import com.devexperts.qd.QDContract;
import com.devexperts.qd.QDDistributor;
import com.devexperts.qd.QDFactory;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.MarshalledObjField;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.stats.QDStats;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.api.impl.EventDelegateFlags;
import com.dxfeed.api.impl.VersionedRecord;
import com.dxfeed.event.misc.Configuration;
import com.dxfeed.event.misc.ConfigurationDelegate;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;

/**
 * Internal entry point called by Invoke-Oracle.ps1 with one fresh output-directory path. Writes
 * exact oracle.tsv observations through production collectors/delegates, without network or
 * endpoint threads. Invalid inputs, failed assertions and output errors fail the process; the
 * launcher owns help, timeouts, complete streams and provenance.
 */
public final class ConfigurationVersionOracle {
  private static void require(boolean ok, String message) {
    if (!ok) {
      throw new IllegalStateException(message);
    }
  }

  private static DataRecord record(String variant) {
    if (variant.equals("standard")) {
      return DXFeedScheme.getInstance().findRecordByName("Configuration");
    }
    DataIntField[] fields =
        variant.equals("absent")
            ? new DataIntField[0]
            : new DataIntField[] {
              SerialFieldType.LONG.createDefaultIntInstance(0, "Configuration.Version"),
              new VoidIntField(1, "Configuration.Version$VoidTail")
            };
    DataRecord result =
        new VersionedRecord(
            0,
            "Configuration",
            false,
            fields,
            new DataObjField[] {new MarshalledObjField(0, "Configuration.Configuration")},
            "Version");
    new DefaultScheme(PentaCodec.INSTANCE, result);
    return result;
  }

  private static void send(
      PrintWriter out,
      String variant,
      QDContract contract,
      QDAgent agent,
      QDDistributor distributor,
      ConfigurationDelegate delegate,
      String phase,
      int[] versions,
      String[] payloads) {
    RecordBuffer input = new RecordBuffer(RecordMode.DATA.withEventTimeSequence());
    for (int i = 0; i < versions.length; ++i) {
      Configuration event = new Configuration("CONFIG");
      event.setVersion(versions[i]);
      event.setEventTime(1600000000123L + i * 1000L);
      event.setAttachment(payloads[i]);
      delegate.putEvent(event, input);
      out.print(
          "input\t"
              + variant
              + "\t"
              + contract
              + "\t"
              + phase
              + "\t"
              + i
              + "\t"
              + versions[i]
              + "\t"
              + payloads[i]
              + "\n");
    }
    distributor.process(input);
    RecordBuffer output = new RecordBuffer(RecordMode.DATA.withEventTimeSequence());
    require(!agent.retrieve(output), "bounded drain must complete");
    List<Integer> deliveredVersions = new ArrayList<>();
    List<String> deliveredBodies = new ArrayList<>();
    RecordCursor cursor;
    while ((cursor = output.next()) != null) {
      Configuration event = delegate.createEvent(cursor);
      boolean versionedTicker = contract == QDContract.TICKER && !variant.startsWith("absent/");
      boolean freshDefault = versionedTicker && phase.equals("seed") && versions[0] < 0;
      int sourceIndex =
          contract == QDContract.STREAM
              ? deliveredVersions.size()
              : phase.equals("batch") && versionedTicker ? 2 : versions.length - 1;
      long expectedTime = freshDefault ? 0 : 1600000000123L + sourceIndex * 1000L;
      require(
          event.getEventSymbol().equals("CONFIG") && event.getEventTime() == expectedTime,
          "complete symbol/event-time projection");
      deliveredVersions.add(event.getVersion());
      deliveredBodies.add((String) event.getAttachment());
      out.print(
          "output\t"
              + variant
              + "\t"
              + contract
              + "\t"
              + phase
              + "\t"
              + (deliveredVersions.size() - 1)
              + "\t"
              + event.getVersion()
              + "\t"
              + event.getAttachment()
              + "\t"
              + event.getEventSymbol()
              + "\t"
              + event.getEventTime()
              + "\t"
              + cursor.getEventTimeSequence()
              + "\n");
    }
    boolean filtered = contract == QDContract.TICKER && !variant.startsWith("absent/");
    int count =
        contract == QDContract.STREAM
            ? versions.length
            : filtered && (phase.equals("lower") || phase.equals("wrap")) ? 0 : 1;
    require(
        deliveredVersions.size() == count,
        "actual production delivery count " + variant + "/" + contract + "/" + phase);
    if (count != 0) {
      int expected =
          variant.startsWith("absent/")
              ? 0
              : phase.equals("batch") && filtered ? 7 : versions[versions.length - 1];
      String body = phase.equals("batch") && filtered ? "v7b" : payloads[payloads.length - 1];
      if (filtered && phase.equals("seed") && versions[0] < 0) {
        expected = 0;
        body = null;
      }
      require(
          deliveredVersions.get(count - 1) == expected
              && java.util.Objects.equals(deliveredBodies.get(count - 1), body),
          "version/body state");
    }
    out.print("count\t" + variant + "\t" + contract + "\t" + phase + "\t" + count + "\n");
  }

  private static void run(
      PrintWriter out, String variant, DataRecord record, QDContract contract, int seed) {
    DataScheme scheme = record.getScheme();
    QDCollector collector =
        QDFactory.getDefaultFactory()
            .collectorBuilder(contract)
            .withScheme(scheme)
            .withStats(QDStats.VOID)
            .withEventTimeSequence(true)
            .build();
    QDAgent agent = collector.agentBuilder().build();
    QDDistributor distributor = collector.distributorBuilder().build();
    try {
      ConfigurationDelegate delegate =
          new ConfigurationDelegate(
              record, contract, EnumSet.of(EventDelegateFlags.PUB, EventDelegateFlags.SUB));
      RecordBuffer subscription = new RecordBuffer(RecordMode.SUBSCRIPTION);
      subscription.add(record, scheme.getCodec().encode("CONFIG"), null);
      agent.addSubscription(subscription);
      String id = variant + "/" + seed;
      send(
          out,
          id,
          contract,
          agent,
          distributor,
          delegate,
          "seed",
          new int[] {seed},
          new String[] {"seed"});
      if (seed != Integer.MAX_VALUE) {
        send(
            out,
            id,
            contract,
            agent,
            distributor,
            delegate,
            "lower",
            new int[] {-8},
            new String[] {"lower"});
        send(
            out,
            id,
            contract,
            agent,
            distributor,
            delegate,
            "equal",
            new int[] {0},
            new String[] {"equal"});
        send(
            out,
            id,
            contract,
            agent,
            distributor,
            delegate,
            "higher",
            new int[] {5},
            new String[] {"higher"});
        send(
            out,
            id,
            contract,
            agent,
            distributor,
            delegate,
            "batch",
            new int[] {7, 6, 7, Integer.MIN_VALUE},
            new String[] {"v7", "v6", "v7b", "min"});
        send(
            out,
            id,
            contract,
            agent,
            distributor,
            delegate,
            "maximum",
            new int[] {Integer.MAX_VALUE},
            new String[] {"max"});
      }
      send(
          out,
          id,
          contract,
          agent,
          distributor,
          delegate,
          "wrap",
          new int[] {Integer.MIN_VALUE},
          new String[] {"wrap"});
    } finally {
      agent.close();
      distributor.close();
      collector.close();
    }
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 1, "fresh output directory required; use launcher help");
    Path root = Paths.get(args[0]);
    Files.createDirectory(root);
    PrintWriter out =
        new PrintWriter(
            Files.newBufferedWriter(root.resolve("oracle.tsv"), StandardCharsets.UTF_8));
    try {
      for (String variant : new String[] {"standard", "absent", "long"}) {
        DataRecord record = record(variant);
        require(record instanceof VersionedRecord, "actual production versioned record");
        for (QDContract contract : new QDContract[] {QDContract.TICKER, QDContract.STREAM}) {
          for (int seed : new int[] {-7, 0, Integer.MIN_VALUE, Integer.MAX_VALUE}) {
            run(out, variant, record, contract, seed);
          }
        }
      }
      out.flush();
      require(!out.checkError(), "oracle output flush failure");
    } finally {
      out.close();
    }
    require(!out.checkError(), "oracle output close failure");
  }
}
