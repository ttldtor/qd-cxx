package com.devexperts.qd.qtp;

import com.devexperts.io.BufferedInput;
import com.devexperts.io.ChunkPool;
import com.devexperts.io.ChunkedInput;
import java.util.Arrays;

/**
 * Small P02 inputs; hooks observe boundaries without changing parsing or synthesizing callbacks.
 */
public final class FramingOracle {
  private static final class Parser extends BinaryQTPParser {
    Parser() {
      super(ProtocolOracle.scheme());
    }

    protected void doBeforeMessageLength(BufferedInput in) {
      ProtocolOracle.row("hook.before", in.totalPosition());
    }

    protected void doAfterMessageType(BufferedInput in) {
      ProtocolOracle.row("hook.type", in.totalPosition());
    }

    protected void doAfterMessageBody(BufferedInput in, int type) {
      ProtocolOracle.row("hook.body", in.totalPosition(), type);
    }
  }

  private static byte[] bytes(String hex) {
    byte[] result = new byte[hex.length() / 2];
    for (int i = 0; i < result.length; i++) {
      result[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
    }
    return result;
  }

  private static void replay(String id, byte[] data, String schedule, int[] chunks)
      throws Exception {
    ProtocolOracle.row("p02.replay", id, schedule, Arrays.toString(chunks));
    ChunkedInput in = new ChunkedInput();
    Parser parser = new Parser();
    parser.setInput(in);
    ProtocolOracle.Observer observer =
        new ProtocolOracle.Observer() {
          public void processHeartbeat(HeartbeatPayload payload) {
            ProtocolOracle.row("callback.heartbeat", callbacks++);
          }
        };
    int offset = 0;
    for (int count : chunks) {
      in.addAllToInput(ChunkPool.DEFAULT.copyToChunkList(data, offset, count, observer), observer);
      offset += count;
      ProtocolOracle.row("p02.feed", count, offset);
      parser.parse(observer);
      ProtocolOracle.row(
          "p02.state", in.totalPosition(), in.available(), observer.callbacks, observer.errors);
    }
    in.clear();
  }

  static void run() throws Exception {
    System.setProperty("com.devexperts.qd.qtp.maxMessageSize", "8");
    ProtocolOracle.row(
        "p02.profile", "maxMessageSize=8; returning consumer; hooks observational; no EOF API");
    String[][] cases = {
      {"empty", "00"},
      {"explicit-zero", "0100"},
      {"tiny-heartbeat", "020000"},
      {"concatenated", "000100020000"},
      {"negative-length", "7f020000"},
      {"limit-minus", "0703000000000000020000"},
      {"limit-at", "080300000000000000020000"},
      {"limit-plus", "09030000000000000000020000"},
      {"partial-length", "80"},
      {"partial-body", "0300"},
      {"truncated-type", "0180020000"},
      {"negative-type", "017f020000"},
      {"large-type", "05f080000000020000"},
      {"overlong", "80028000020000"},
      {"unknown", "013e020000"},
      {"bad-payload", "020001020000"},
      {"good-before-error", "0200000180020000"}
    };
    for (String[] entry : cases) {
      byte[] data = bytes(entry[1]);
      ProtocolOracle.row("p02.input", entry[0], entry[1]);
      replay(entry[0], data, "whole", new int[] {data.length});
      for (int split = 1; split < data.length; split++) {
        replay(entry[0], data, "split-" + split, new int[] {split, data.length - split});
      }
      int[] ones = new int[data.length * 2 + 1];
      for (int i = 1; i < ones.length; i += 2) {
        ones[i] = 1;
      }
      replay(entry[0], data, "bytewise-empty", ones);
    }
    ProtocolOracle.Adapter adapter = new ProtocolOracle.Adapter(ProtocolOracle.scheme());
    adapter.start();
    ProtocolOracle.row("adapter.before", adapter.isAlive(), adapter.isClosed());
    BinaryQTPParser parser = new BinaryQTPParser(ProtocolOracle.scheme());
    parser.setInput(new com.devexperts.io.ByteArrayInput(bytes("013e")));
    parser.parse(adapter);
    ProtocolOracle.row(
        "adapter.after", adapter.isAlive(), adapter.isClosed(), adapter.observer.errors);
  }
}
