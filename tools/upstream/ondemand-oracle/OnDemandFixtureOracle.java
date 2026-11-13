// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.util.TimeSequenceUtil;
import com.dxfeed.event.market.impl.QuoteMapping;
import com.dxfeed.ondemand.impl.Block;
import com.dxfeed.ondemand.impl.Key;
import com.dxfeed.ondemand.impl.MarketDataToken;
import com.dxfeed.ondemand.impl.ReplayRequest;
import com.dxfeed.ondemand.impl.ReplayResponse;
import com.dxfeed.ondemand.impl.event.MDREventUtil;
import com.dxfeed.ondemand.impl.event.MDRQuote;
import java.io.IOException;
import java.time.Instant;
import java.util.Collections;

public final class OnDemandFixtureOracle {
  private static final String SYMBOL = "MOCK";
  private static final char QUOTE_TYPE = 'Q';
  private static final char COMPOSITE = 0;
  private static final long CUT = Instant.parse("2024-02-12T14:30:05Z").toEpochMilli();
  private static final long COVERAGE_START = CUT - 5000;
  private static final long COVERAGE_END = CUT + 60000;

  private OnDemandFixtureOracle() {}

  public static void main(String[] args) throws IOException {
    if (args.length == 0 || (args.length == 1 && "--help".equals(args[0]))) {
      System.out.println("OnDemandFixtureOracle --generate | --help | --help-ru");
      System.out.println(
          "No arguments display help. --generate writes three synthetic QD fixture rows to"
              + " stdout.");
      System.out.println("Uses pinned QD 3.355 classes. Opens no sockets and writes no files.");
      System.out.println("Exit 0 for success, 1 for failed generation, 2 for invalid arguments.");
      return;
    }
    if (args.length == 1 && "--help-ru".equals(args[0])) {
      System.out.println("OnDemandFixtureOracle --generate | --help | --help-ru");
      System.out.println(
          "Без аргументов выводит справку. --generate печатает три строки синтетических фикстур"
              + " QD.");
      System.out.println(
          "Использует закреплённые классы QD 3.355. Не открывает сокеты и не пишет файлы.");
      System.out.println("Код 0 при успехе, 1 при ошибке генерации, 2 при неверных аргументах.");
      return;
    }
    if (args.length != 1 || !"--generate".equals(args[0])) {
      System.err.println("Expected --generate, --help or --help-ru.");
      System.exit(2);
      return;
    }

    Generator.run();
  }

  private static final class Generator {
    private static final char[] HEX = "0123456789abcdef".toCharArray();

    static void run() throws IOException {
      final String request = hex(request());
      final String normalResponse = hex(response(750));
      final String controlResponse = hex(response(2000));
      System.out.println("request\t" + request);
      System.out.println("response-750ms\t" + normalResponse);
      System.out.println("response-2000ms\t" + controlResponse);
    }

    private static ByteArrayOutput request() throws IOException {
      ReplayRequest request = new ReplayRequest();
      request.setToken(new MarketDataToken("", "", Long.MAX_VALUE, new byte[32]));
      request.setRequestTime(CUT);
      request.addRequestKeys(Collections.singletonList(new Key(SYMBOL, COMPOSITE, QUOTE_TYPE)));
      return request.write();
    }

    private static ByteArrayOutput response(long secondOffset) throws IOException {
      DataRecord record = MDREventUtil.getRecords(QUOTE_TYPE, COMPOSITE)[0];
      QuoteMapping mapping = record.getMapping(QuoteMapping.class);
      RecordBuffer buffer = RecordBuffer.getInstance(RecordMode.TIMESTAMPED_DATA);
      try {
        RecordCursor cursor = buffer.add(record, MDREventUtil.CODEC.encode(SYMBOL), SYMBOL);
        cursor.setEventTimeSequence(TimeSequenceUtil.getTimeSequenceFromTimeMillis(CUT));
        mapping.setBidTimeSeconds(cursor, (int) (CUT / 1000));
        mapping.setAskTimeSeconds(cursor, (int) (CUT / 1000));
        mapping.setBidExchangeCode(cursor, 'Q');
        mapping.setAskExchangeCode(cursor, 'Q');
        mapping.setBidPrice(cursor, 12.25);
        mapping.setAskPrice(cursor, 12.50);
        mapping.setBidSize(cursor, 10);
        mapping.setAskSize(cursor, 20);
        MDRQuote baseline = new MDRQuote();
        baseline.init(COVERAGE_START);
        MDRQuote event = new MDRQuote();
        event.setFrom(cursor);
        ByteArrayOutput payload = new ByteArrayOutput();
        baseline.write(payload, event);

        cursor.setEventTimeSequence(
            TimeSequenceUtil.getTimeSequenceFromTimeMillis(CUT + secondOffset));
        mapping.setBidPrice(cursor, 13.25);
        mapping.setAskPrice(cursor, 13.50);
        event.setFrom(cursor);
        baseline.write(payload, event);

        Block block = new Block();
        block.setSymbol(SYMBOL);
        block.setType(QUOTE_TYPE);
        block.setExchange(COMPOSITE);
        block.setStartTime(COVERAGE_START);
        block.setEndTime(COVERAGE_END);
        block.setData(payload.getBuffer(), 0, payload.getPosition());
        ReplayResponse response = new ReplayResponse();
        block.writeBlock(response.getResponseBlocksOutput());
        return response.write();
      } finally {
        buffer.release();
      }
    }

    private static String hex(ByteArrayOutput output) {
      StringBuilder text = new StringBuilder(output.getPosition() * 2);
      byte[] bytes = output.getBuffer();
      for (int i = 0; i < output.getPosition(); i++) {
        int value = bytes[i] & 0xff;
        text.append(HEX[value >>> 4]).append(HEX[value & 0xf]);
      }
      return text.toString();
    }
  }
}
