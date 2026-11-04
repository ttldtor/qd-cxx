// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package com.dxfeed.ipf.filter;

import com.devexperts.qd.util.QDConfig;
import com.devexperts.util.TimePeriod;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;

/** Production QD TimePeriod and IPF property grammar, without files, sources or timers. */
public final class FeedFilterConfigOracle {
  public static void main(String[] args) throws Exception {
    if (args.length != 1) {
      throw new IllegalArgumentException("Expected output TSV path");
    }
    try (PrintStream output = new PrintStream(args[0], "UTF-8")) {
      System.setOut(output);
      for (String value :
          new String[] {
            "0",
            "0s",
            "1",
            "1s",
            "1d",
            "1h",
            "1m",
            "P1DT2H3M4.005S",
            "PT0.0004S",
            "PT0.0005S",
            "PT0.0015S",
            "1.9995",
            ".5",
            "1.",
            "inf",
            "INF",
            "\u0130nf",
            "\u0131nf",
            "1\u017f",
            "\u0661D",
            "\uff11H",
            "-1",
            "P",
            "PT",
            "1T",
            "1ms",
            "1w",
            "2147483647D",
            "2147483648D",
            "9223372036854775.807s",
            "9223372036854775807",
            " 1s",
            "1s ",
            "NaN"
          }) {
        String label =
            value
                .replace("\u0130", "U0130")
                .replace("\u0131", "U0131")
                .replace("\u017f", "U017f")
                .replace("\u0661", "U0661")
                .replace("\uff11", "Uff11");
        try {
          System.out.println("period\t" + label + "\t" + TimePeriod.valueOf(value).getTime());
        } catch (RuntimeException failure) {
          System.out.println("period-invalid\t" + label + "\t" + failure.getClass().getName());
        }
      }
      String[] expressions = {
        "ipf[fixture]",
        "ipf[fixture,update=1d]",
        "ipf[fixture][update=PT0.001S]",
        "ipf(fixture)(update=1s)",
        "ipf[fi\\,xture,update=1s]",
        "ipf[fixture,update=1s,schedule=]",
        "ipf[fixture,schedule=]",
        "ipf[fixture,update=0s,schedule=]",
        "ipf[fixture,update=1s,update=2s]",
        "ipf[fixture,unknown=x]",
        "ipf[fixture,update=-1]",
        "ipf[fixture,update=NaN]",
        " ipf [ fixture , update = 1s ] ",
        "ipf[fixture,Update=1s]",
        "ipf[fixture,update]",
        "ipf[fixture,update=1s,schedule=X(a=b)]",
        "ipf[fixture,update=1s,schedule=[x,y]]",
        "ipf[fixture,update=1s,schedule]",
        "ipf[fixture,update=1s,schedule=X(a=\\\\,b)]",
        "ipf[fixture,update=1s,u\\u017fer=,password=]"
      };
      for (int i = 0; i < expressions.length; i++) {
        try {
          List<String> properties = new ArrayList<String>();
          String prefix = QDConfig.parseProperties(expressions[i], properties);
          IPFSymbolFilter.Config config = new IPFSymbolFilter.Config();
          QDConfig.setProperties(config, properties.subList(1, properties.size()));
          config.validate();
          System.out.println(
              "config\t"
                  + i
                  + "\t"
                  + prefix
                  + "\t"
                  + QDConfig.unescape(properties.get(0))
                  + "\t"
                  + (config.getUpdate() == null ? 0 : config.getUpdate().getTime())
                  + "\t"
                  + (config.getSchedule() == null ? "<null>" : config.getSchedule()));
        } catch (RuntimeException failure) {
          System.out.println("config-invalid\t" + i + "\t" + failure.getClass().getName());
        }
      }
      output.flush();
      output.close();
      if (output.checkError()) {
        throw new IOException("Failed writing oracle output");
      }
    }
  }
}
