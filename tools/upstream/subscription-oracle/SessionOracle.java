package qdcxx.oracle;

import com.devexperts.auth.AuthSession;
import com.devexperts.auth.AuthToken;
import com.devexperts.connector.proto.ApplicationConnection;
import com.devexperts.connector.proto.TransportConnection;
import com.devexperts.io.ByteArrayInput;
import com.devexperts.io.ByteArrayOutput;
import com.devexperts.io.Chunk;
import com.devexperts.io.ChunkList;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.QDAgent;
import com.devexperts.qd.QDFactory;
import com.devexperts.qd.QDTicker;
import com.devexperts.qd.SubscriptionIterator;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.ng.RecordBuffer;
import com.devexperts.qd.ng.RecordMode;
import com.devexperts.qd.qtp.AgentAdapter;
import com.devexperts.qd.qtp.BinaryQTPComposer;
import com.devexperts.qd.qtp.BinaryQTPParser;
import com.devexperts.qd.qtp.DistributorAdapter;
import com.devexperts.qd.qtp.HeartbeatPayload;
import com.devexperts.qd.qtp.MessageAdapter;
import com.devexperts.qd.qtp.MessageAdapterConnectionFactory;
import com.devexperts.qd.qtp.MessageConnectors;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageDescriptor;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.ProtocolDescriptor;
import com.devexperts.qd.qtp.auth.BasicChannelShaperFactory;
import com.devexperts.qd.qtp.auth.QDAuthRealm;
import com.devexperts.qd.qtp.auth.QDLoginHandler;
import com.devexperts.qd.stats.QDStats;
import com.devexperts.util.TimePeriod;
import com.devexperts.util.TypedMap;
import com.dxfeed.promise.Promise;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/**
 * Offline production session observations. Invoke with --out NEW_DIRECTORY on the locked QD
 * classpath; no arguments or --help print help without initializing QD. Input is fixed synthetic
 * metadata and in-memory chunks. Output is UTF-8/LF TSV and exact binary captures. Usage errors
 * exit 3; observation/I/O errors exit 1. No sockets, external credentials, or background workers
 * are used.
 */
public final class SessionOracle {
  private static final String HELP =
      "SessionOracle --out NEW_DIRECTORY\n"
          + "Capture offline locked-QD session observations using synthetic in-memory input.\n"
          + "No defaults or network access. The output directory must not exist.\n"
          + "Writes observations.tsv, cases.tsv, deterministic wire/*.bin and raw/*.bin.\n"
          + "raw/ preserves clock-dependent connection bytes; never compare it as a golden.\n"
          + "Uses a bounded 200ms synthetic authentication timeout in this process.\n"
          + "Exit codes: 0 success/help, 3 usage, 1 observation or I/O failure.\n";
  private static final long WAIT_BUDGET_MILLIS = 3000;
  private final Path out;
  private final StringBuilder observations = new StringBuilder("case\tphase\tindex\tkey\tvalue\n");
  private final StringBuilder cases =
      new StringBuilder("case\tevidence_kind\tproduction_path\toutcome\n");
  private final Set<String> keys = new HashSet<>();
  private final DataScheme scheme;

  private SessionOracle(Path out) {
    this.out = out;
    DataRecord record =
        new DefaultRecord(0, "Session", false, new DataIntField[0], new DataObjField[0]);
    scheme = new DefaultScheme(PentaCodec.INSTANCE, new DataRecord[] {record});
  }

  public static void main(String[] args) {
    if (args.length == 0 || (args.length == 1 && args[0].equals("--help"))) {
      System.out.print(HELP);
      if (System.out.checkError()) {
        System.exit(1);
      }
      return;
    }
    if (args.length != 2 || !args[0].equals("--out") || args[1].isEmpty()) {
      System.err.println("Usage: SessionOracle --out NEW_DIRECTORY");
      System.exit(3);
    }
    try {
      Path path = Paths.get(args[1]);
      Files.createDirectory(path);
      Files.createDirectory(path.resolve("wire"));
      Files.createDirectory(path.resolve("raw"));
      // AuthManager reads this documented property once; set it before constructing any adapter.
      System.setProperty("com.devexperts.qd.qtp.AuthManager.authenticateTimeout", ".2s");
      new SessionOracle(path).run();
    } catch (Exception | AssertionError e) {
      System.err.println(
          "Session observation failed: " + e.getClass().getSimpleName() + ": " + e.getMessage());
      e.printStackTrace(System.err);
      System.exit(1);
    }
  }

  private void run() throws Exception {
    protocol();
    heartbeats();
    adapterDeltas();
    capability();
    connection();
    inactivity();
    authentication(false);
    authentication(true);
    authTimeout();
    subscriptionReadiness();
    clientLogin("success");
    clientLogin("refused");
    clientLogin("cancelled");
    write("observations.tsv", observations.toString());
    write("cases.tsv", cases.toString());
  }

  private void protocol() throws IOException {
    String c = "S01-protocol-deltas";
    BinaryQTPParser parser = new BinaryQTPParser(scheme);
    Capture capture = new Capture();
    ProtocolDescriptor full = ProtocolDescriptor.newEmptyProtocolDescriptor();
    full.setProperty("type", "qtp");
    full.setProperty("opt", "hs,unknown");
    full.setProperty("retained", "first");
    full.setProperty("changed", "old");
    full.setProperty("reset", "nonempty");
    MessageDescriptor msg = full.newMessageDescriptor(MessageType.TICKER_DATA);
    msg.setProperty("retained", "message-first");
    full.addSend(msg);
    full.addReceive(full.newMessageDescriptor(MessageType.TICKER_ADD_SUBSCRIPTION));
    byte[] fullBytes = descriptorBytes(full.deltaFrom(null));
    save("wire/s01-full.bin", fullBytes);
    parse(parser, capture, fullBytes);
    descriptor(c, "full", 0, capture.descriptors.get(0));

    ProtocolDescriptor current = ProtocolDescriptor.newPeerProtocolDescriptor(full);
    current.setProperty("changed", "new");
    current.setProperty("reset", null);
    current.getSend(MessageType.TICKER_DATA).setProperty("added", "message-second");
    current.addSend(current.newMessageDescriptor(MessageType.STREAM_DATA));
    ProtocolDescriptor delta = current.deltaFrom(full);
    descriptor(c, "emitted-delta", 0, delta);
    byte[] deltaBytes = descriptorBytes(delta);
    save("wire/s01-delta.bin", deltaBytes);
    parse(parser, capture, deltaBytes);
    descriptor(c, "accumulated", 0, capture.descriptors.get(1));

    ProtocolDescriptor empty = current.deltaFrom(current);
    byte[] emptyBytes = descriptorBytes(empty);
    save("wire/s01-unchanged.bin", emptyBytes);
    parse(parser, capture, emptyBytes);
    descriptor(c, "unchanged-accumulated", 0, capture.descriptors.get(2));
    parser.resetSession();
    parse(parser, capture, deltaBytes);
    descriptor(c, "after-parser-reset", 0, capture.descriptors.get(3));
    Capture fresh = new Capture();
    parse(new BinaryQTPParser(scheme), fresh, deltaBytes);
    descriptor(c, "fresh-parser", 0, fresh.descriptors.get(0));
    check(
        capture.descriptors.get(1).getProperty("retained").equals("first"), "merge lost property");
    check(capture.descriptors.get(3).getProperty("retained") == null, "reset retained property");
    caseRow(
        c, "ProtocolDescriptor.deltaFrom/mergeFrom;BinaryQTPComposer;BinaryQTPParser.resetSession");
  }

  private void heartbeats() throws IOException {
    for (int mask = 0; mask < 16; mask++) {
      String c = String.format(java.util.Locale.ROOT, "S02-heartbeat-mask-%02d", mask);
      HeartbeatPayload payload = new HeartbeatPayload();
      if ((mask & 1) != 0) {
        payload.setTimeMillis(1700000000123L);
      }
      if ((mask & 2) != 0) {
        payload.setTimeMark(123456);
      }
      if ((mask & 4) != 0) {
        payload.setDeltaMark(-456);
      }
      if ((mask & 8) != 0) {
        payload.setLagMark(789);
      }
      ByteArrayOutput direct = new ByteArrayOutput();
      payload.composeTo(direct);
      value(c, "payload", 0, "bytes", "x:" + hex(direct.toByteArray()));
      HeartbeatPayload directParsed = new HeartbeatPayload();
      directParsed.parseFrom(new ByteArrayInput(direct.toByteArray()));
      heartbeat(c, "payload-parsed", 0, directParsed, true);
      BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
      ByteArrayOutput bytes = new ByteArrayOutput();
      composer.setOutput(bytes);
      composer.composeHeartbeatMessage(payload);
      byte[] wire = bytes.toByteArray();
      save("wire/" + c.toLowerCase(java.util.Locale.ROOT) + ".bin", wire);
      Capture parsed = new Capture();
      parse(new BinaryQTPParser(scheme), parsed, wire);
      number(c, "wire-parsed", 0, "callbacks", parsed.heartbeats.size());
      if (!parsed.heartbeats.isEmpty()) {
        heartbeat(c, "wire-parsed", 0, parsed.heartbeats.get(0), true);
      }
      caseRow(c, "HeartbeatPayload.composeTo/parseFrom;BinaryQTPComposer;BinaryQTPParser");
    }
    String c = "S03-heartbeat-reset";
    HeartbeatPayload all = new HeartbeatPayload();
    all.setTimeMillis(1700000000123L);
    all.setTimeMark(123456);
    all.setDeltaMark(-456);
    all.setLagMark(789);
    HeartbeatPayload partial = new HeartbeatPayload();
    partial.setLagMark(33);
    BinaryQTPParser parser = new BinaryQTPParser(scheme);
    Capture parsed = new Capture();
    parse(parser, parsed, heartbeatBytes(all));
    parse(parser, parsed, heartbeatBytes(partial));
    heartbeat(c, "partial-after-full", 0, parsed.heartbeats.get(1), true);
    parser.resetSession();
    parse(parser, parsed, heartbeatBytes(partial));
    heartbeat(c, "after-parser-reset", 0, parsed.heartbeats.get(2), true);
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.composeHeartbeatMessage(partial);
    byte[] before = output.toByteArray();
    output.clear();
    composer.resetSession();
    composer.composeHeartbeatMessage(partial);
    bool(c, "composer-reset", 0, "same-bytes", Arrays.equals(before, output.toByteArray()));
    save("wire/s03-after-composer-reset.bin", output.toByteArray());
    caseRow(c, "BinaryQTPParser.resetSession;BinaryQTPComposer.resetSession");
  }

  private void adapterDeltas() throws IOException {
    String c = "S04-adapter-deltas";
    ProbeAdapter adapter = new ProbeAdapter(scheme);
    adapter.useDescribeProtocol();
    adapter.start();
    try {
      BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
      ByteArrayOutput output = new ByteArrayOutput();
      composer.setOutput(output);
      composer.compose(adapter);
      byte[] full = output.toByteArray();
      save("wire/s04-initial.bin", full);
      Capture parsed = new Capture();
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parse(parser, parsed, full);
      descriptor(c, "initial", 0, parsed.descriptors.get(0));
      output.clear();
      adapter.revision = "second";
      adapter.requestDescriptor();
      composer.compose(adapter);
      byte[] delta = output.toByteArray();
      save("wire/s04-delta.bin", delta);
      Capture deltaOnly = new Capture();
      parse(new BinaryQTPParser(scheme), deltaOnly, delta);
      descriptor(c, "delta-only", 0, deltaOnly.descriptors.get(0));
      parse(parser, parsed, delta);
      descriptor(c, "accumulated", 0, parsed.descriptors.get(1));
      output.clear();
      adapter.requestDescriptor();
      composer.compose(adapter);
      save("wire/s04-unchanged.bin", output.toByteArray());
      parse(new BinaryQTPParser(scheme), deltaOnly, output.toByteArray());
      descriptor(c, "unchanged-delta", 0, deltaOnly.descriptors.get(1));
    } finally {
      adapter.close();
    }
    caseRow(c, "MessageAdapter.retrieveDescribeProtocolMessage;BinaryQTPComposer;BinaryQTPParser");
  }

  private void capability() {
    String c = "S05-capability-intersection";
    QDTicker ticker = QDFactory.getDefaultFactory().createTicker(scheme);
    AgentAdapter adapter = new AgentAdapter(ticker, null, null, null, QDStats.VOID);
    try {
      ProtocolDescriptor local = ProtocolDescriptor.newEmptyProtocolDescriptor();
      adapter.prepareProtocolDescriptor(local);
      descriptor(c, "local", 0, local);
      for (int mask = 0; mask < 8; mask++) {
        ProtocolDescriptor peer = ProtocolDescriptor.newEmptyProtocolDescriptor();
        if ((mask & 1) != 0) {
          peer.addSend(peer.newMessageDescriptor(MessageType.TICKER_ADD_SUBSCRIPTION));
        }
        if ((mask & 2) != 0) {
          peer.addReceive(peer.newMessageDescriptor(MessageType.TICKER_DATA));
        }
        if ((mask & 4) != 0) {
          peer.addSend(peer.newMessageDescriptor(MessageType.STREAM_ADD_SUBSCRIPTION));
          peer.addReceive(peer.newMessageDescriptor(MessageType.STREAM_DATA));
        }
        bool(c, "peer", mask, "compatible", adapter.isProtocolDescriptorCompatible(peer));
      }
    } finally {
      adapter.close();
      ticker.close();
    }
    caseRow(c, "AgentAdapter.prepareProtocolDescriptor/isProtocolDescriptorCompatible");
  }

  private void connection() throws Exception {
    String c = "S06-connection-heartbeat";
    try (Connection run = new Connection(40, 20000)) {
      byte[] initial = run.retrieve();
      save("raw/s06-initial.bin", initial);
      Capture capture = new Capture();
      BinaryQTPParser parser = new BinaryQTPParser(scheme);
      parse(parser, capture, initial);
      number(c, "initial", 0, "descriptor-count", capture.descriptors.size());
      number(c, "initial", 0, "heartbeat-count", capture.heartbeats.size());
      text(c, "initial", 0, "callback-order", String.join(",", capture.events));
      heartbeat(c, "initial-projection", 0, capture.heartbeats.get(0), false);
      descriptor(c, "initial-projection", 0, capture.descriptors.get(0));
      long next = run.connection.examine(Long.MIN_VALUE);
      number(
          c, "timer", 0, "first-period-millis", next - capture.heartbeats.get(0).getTimeMillis());
      int notifications = run.transport.available;
      run.connection.examine(next - 1);
      number(
          c, "timer", 0, "before-deadline-notifications", run.transport.available - notifications);
      long reschedule = run.connection.examine(next);
      number(c, "timer", 0, "at-deadline-notifications", run.transport.available - notifications);
      number(c, "timer", 0, "examine-reschedule-millis", reschedule - next);
      waitUntil(next);
      byte[] timed = run.retrieve();
      save("raw/s06-timed.bin", timed);
      parse(parser, capture, timed);
      heartbeat(c, "timer-projection", 0, capture.heartbeats.get(1), false);
      long doubled = run.connection.examine(Long.MIN_VALUE);
      number(
          c,
          "timer",
          0,
          "second-period-millis",
          doubled - capture.heartbeats.get(1).getTimeMillis());
      HeartbeatPayload input = new HeartbeatPayload();
      input.setTimeMark(123456);
      input.setDeltaMark(-22);
      input.setLagMark(33);
      byte[] inputBytes = heartbeatBytes(input);
      save("wire/s06-input.bin", inputBytes);
      bool(
          c,
          "incoming",
          0,
          "ready-for-more",
          run.connection.processChunks(ChunkList.wrap(inputBytes, this), this));
      waitUntil(doubled);
      byte[] afterInput = run.retrieve();
      save("raw/s06-after-input.bin", afterInput);
      parse(parser, capture, afterInput);
      heartbeat(c, "after-input-projection", 0, capture.heartbeats.get(2), false);
      bool(c, "incoming", 0, "connection-closed", run.connection.isClosed());
    }
    try (Connection fresh = new Connection(40, 20000)) {
      byte[] bytes = fresh.retrieve();
      save("raw/s06-fresh-connection.bin", bytes);
      Capture parsed = new Capture();
      parse(new BinaryQTPParser(scheme), parsed, bytes);
      number(c, "fresh", 0, "descriptor-count", parsed.descriptors.size());
      heartbeat(c, "fresh-projection", 0, parsed.heartbeats.get(0), false);
    }
    caseRow(
        c,
        "MessageAdapterConnection.retrieveChunks/processChunks/examine;ConnectionQTPParser/Composer");
  }

  private void inactivity() throws Exception {
    String c = "S07-inactivity-deadline";
    try (Connection run = new Connection(60000, 10000)) {
      save("raw/s07-initial.bin", run.retrieve());
      long deadline = run.connection.examine(Long.MIN_VALUE);
      run.connection.examine(deadline - 1);
      bool(c, "initial-deadline", 0, "closed-before", run.connection.isClosed());
      run.connection.examine(deadline);
      bool(c, "initial-deadline", 0, "closed-at", run.connection.isClosed());
      number(c, "initial-deadline", 0, "close-callbacks", run.transport.closed);
    }
    try (Connection run = new Connection(60000, 10000)) {
      save("raw/s07-refreshed-initial.bin", run.retrieve());
      long old = run.connection.examine(Long.MIN_VALUE);
      waitUntil(System.currentTimeMillis() + 5);
      // Even an empty heartbeat is incoming traffic, although the parser emits no callback.
      byte[] input = new byte[] {1, 0};
      save("wire/s07-empty-heartbeat.bin", input);
      run.connection.processChunks(ChunkList.wrap(input, this), this);
      long refreshed = run.connection.examine(Long.MIN_VALUE);
      bool(c, "refreshed-deadline", 0, "deadline-extended", refreshed > old);
      run.connection.examine(old);
      bool(c, "refreshed-deadline", 0, "closed-at-old", run.connection.isClosed());
      run.connection.examine(refreshed - 1);
      bool(c, "refreshed-deadline", 0, "closed-before-new", run.connection.isClosed());
      run.connection.examine(refreshed);
      bool(c, "refreshed-deadline", 0, "closed-at-new", run.connection.isClosed());
    }
    caseRow(c, "MessageAdapterConnection.examine/processChunks");
  }

  private void authentication(boolean refused) throws Exception {
    String c = refused ? "S09-auth-refused" : "S08-auth-complete";
    QDTicker ticker = QDFactory.getDefaultFactory().createTicker(scheme);
    AgentAdapter.Factory factory = new AgentAdapter.Factory(ticker);
    MessageAdapter adapter = factory.createAdapter(QDStats.VOID);
    TypedMap variables = new TypedMap();
    SyntheticRealm realm = new SyntheticRealm();
    adapter.setConnectionVariables(variables);
    adapter.setAuthRealm(realm);
    adapter.useDescribeProtocol();
    adapter.start();
    BinaryQTPParser parser = new BinaryQTPParser(scheme);
    Capture parsed = new Capture();
    try {
      byte[] initial = retrieve(adapter);
      save("wire/" + c.toLowerCase(java.util.Locale.ROOT) + "-required.bin", initial);
      parse(parser, parsed, initial);
      descriptor(c, "required", 0, parsed.descriptors.get(0));
      bool(c, "required", 0, "adapter-alive", adapter.isAlive());
      bool(c, "required", 0, "finite-auth-deadline", adapter.nextRetrieveTime(0) != Long.MAX_VALUE);
      ProtocolDescriptor token = ProtocolDescriptor.newEmptyProtocolDescriptor();
      token.setProperty(ProtocolDescriptor.AUTHORIZATION_PROPERTY, "Basic c3ludGhldGljOnRva2Vu");
      byte[] input = descriptorBytes(token);
      save("wire/" + c.toLowerCase(java.util.Locale.ROOT) + "-authorization.bin", input);
      parse(new BinaryQTPParser(scheme), adapter, input);
      number(c, "pending", 0, "realm-calls", realm.calls);
      bool(
          c,
          "pending",
          0,
          "subject-present",
          variables.get(TransportConnection.SUBJECT_KEY) != null);
      if (refused) {
        realm.pending.completeExceptionally(new SecurityException("synthetic-refused"));
      } else {
        realm.session = new AuthSession("synthetic-subject");
        realm
            .session
            .variables()
            .set(
                BasicChannelShaperFactory.CHANNEL_CONFIGURATION_KEY,
                BasicChannelShaperFactory.ALL_DATA);
        realm.session.variables().set(BasicChannelShaperFactory.FACTORY_KEY, factory);
        realm.pending.complete(realm.session);
      }
      byte[] response = retrieve(adapter);
      save("wire/" + c.toLowerCase(java.util.Locale.ROOT) + "-response.bin", response);
      Capture delta = new Capture();
      parse(new BinaryQTPParser(scheme), delta, response);
      descriptor(c, "response-delta", 0, delta.descriptors.get(0));
      parse(parser, parsed, response);
      descriptor(c, "response-accumulated", 0, parsed.descriptors.get(1));
      bool(c, "response", 0, "adapter-alive", adapter.isAlive());
      text(c, "response", 0, "subject", (String) variables.get(TransportConnection.SUBJECT_KEY));
      bool(c, "response", 0, "finite-auth-deadline", adapter.nextRetrieveTime(0) != Long.MAX_VALUE);
      if (!refused) {
        realm.session.close("synthetic-session-close");
        bool(c, "session-close", 0, "adapter-closed", adapter.isClosed());
      }
    } finally {
      adapter.close();
      ticker.close();
    }
    caseRow(
        c,
        "MessageAdapter;AgentAdapter.Factory/reinitConfiguration;AuthManager;AuthSession;BinaryQTPParser");
  }

  private void authTimeout() throws Exception {
    String c = "S10-auth-timeout";
    ProbeAdapter adapter = new ProbeAdapter(scheme);
    SyntheticRealm realm = new SyntheticRealm();
    adapter.setConnectionVariables(new TypedMap());
    adapter.setAuthRealm(realm);
    adapter.useDescribeProtocol();
    adapter.start();
    try {
      save("wire/s10-required.bin", retrieve(adapter));
      long deadline = adapter.nextRetrieveTime(0);
      bool(c, "pending", 0, "finite-deadline", deadline != Long.MAX_VALUE);
      bool(c, "pending", 0, "closed", adapter.isClosed());
      ProtocolDescriptor token = ProtocolDescriptor.newEmptyProtocolDescriptor();
      token.setProperty(ProtocolDescriptor.AUTHORIZATION_PROPERTY, "Basic c3ludGhldGljOnRva2Vu");
      parse(new BinaryQTPParser(scheme), adapter, descriptorBytes(token));
      waitUntil(deadline + 1);
      retrieve(adapter);
      bool(c, "after-deadline", 0, "closed", adapter.isClosed());
      bool(c, "after-deadline", 0, "pending-promise-cancelled", realm.pending.isCancelled());
      number(c, "after-deadline", 0, "realm-calls", realm.calls);
    } finally {
      adapter.close();
    }
    caseRow(c, "MessageAdapter.nextRetrieveTime/retrieveMessages;AuthManager.close");
  }

  private void subscriptionReadiness() throws IOException {
    for (int mask = 0; mask < 4; mask++) {
      String c = "S11-subscription-readiness-" + mask;
      QDTicker ticker = QDFactory.getDefaultFactory().createTicker(scheme);
      QDAgent agent = ticker.agentBuilder().build();
      DistributorAdapter adapter = new DistributorAdapter(ticker, null, null, null, QDStats.VOID);
      adapter.useDescribeProtocol();
      adapter.start();
      RecordBuffer subscription = new RecordBuffer(RecordMode.SUBSCRIPTION);
      subscription.add(scheme.getRecord(0), scheme.getCodec().encode("SYNTH"), null);
      try {
        agent.addSubscription(subscription);
        Capture capture = new Capture();
        BinaryQTPParser parser = new BinaryQTPParser(scheme);
        byte[] initial = retrieve(adapter);
        save("wire/" + c.toLowerCase(java.util.Locale.ROOT) + "-initial.bin", initial);
        parse(parser, capture, initial);
        number(c, "before-peer", 0, "subscription-records", capture.subscriptions.size());
        ProtocolDescriptor peer = ProtocolDescriptor.newEmptyProtocolDescriptor();
        if ((mask & 1) != 0) {
          peer.addReceive(peer.newMessageDescriptor(MessageType.TICKER_ADD_SUBSCRIPTION));
        }
        if ((mask & 2) != 0) {
          peer.addReceive(peer.newMessageDescriptor(MessageType.TICKER_REMOVE_SUBSCRIPTION));
        }
        peer.addSend(peer.newMessageDescriptor(MessageType.TICKER_DATA));
        bool(c, "peer", 0, "compatible", adapter.isProtocolDescriptorCompatible(peer));
        byte[] input = descriptorBytes(peer);
        save("wire/" + c.toLowerCase(java.util.Locale.ROOT) + "-peer.bin", input);
        parse(new BinaryQTPParser(scheme), adapter, input);
        byte[] afterPeer = retrieve(adapter);
        save("wire/" + c.toLowerCase(java.util.Locale.ROOT) + "-after-peer.bin", afterPeer);
        parse(parser, capture, afterPeer);
        number(c, "after-peer", 0, "subscription-records", capture.subscriptions.size());
        subscription.rewind();
        agent.removeSubscription(subscription);
        byte[] removed = retrieve(adapter);
        save("wire/" + c.toLowerCase(java.util.Locale.ROOT) + "-removed.bin", removed);
        parse(parser, capture, removed);
        number(c, "after-remove", 0, "subscription-records", capture.subscriptions.size());
        for (int i = 0; i < capture.subscriptions.size(); i++) {
          text(c, "subscription", i, "message-symbol", capture.subscriptions.get(i));
        }
      } finally {
        adapter.close();
        agent.close();
        subscription.release();
        ticker.close();
      }
      caseRow(
          c,
          "DistributorAdapter.startImpl/processDescribeProtocol/retrieveMessages;BinaryQTPParser");
    }
  }

  private void clientLogin(String scenario) throws Exception {
    String c = "S12-client-login-" + scenario;
    QDTicker ticker = QDFactory.getDefaultFactory().createTicker(scheme);
    QDAgent agent = ticker.agentBuilder().build();
    LoginClient adapter = new LoginClient(ticker);
    SyntheticLogin login = new SyntheticLogin();
    adapter.setConnectionVariables(new TypedMap());
    adapter.setLoginHandler(login);
    adapter.useDescribeProtocol();
    adapter.start();
    RecordBuffer subscription = new RecordBuffer(RecordMode.SUBSCRIPTION);
    subscription.add(scheme.getRecord(0), scheme.getCodec().encode("SYNTH"), null);
    BinaryQTPParser incoming = new BinaryQTPParser(scheme);
    BinaryQTPParser outgoing = new BinaryQTPParser(scheme);
    Capture capture = new Capture();
    try {
      agent.addSubscription(subscription);
      byte[] initial = retrieve(adapter);
      save("wire/" + c + "-initial.bin", initial);
      parse(outgoing, capture, initial);
      descriptor(c, "initial", 0, capture.descriptors.get(0));
      number(c, "initial", 0, "subscription-records", capture.subscriptions.size());
      number(c, "initial", 0, "login-calls", login.reasons.size());
      bool(c, "initial", 0, "finite-auth-deadline", adapter.nextRetrieveTime(0) != Long.MAX_VALUE);

      ProtocolDescriptor required = ProtocolDescriptor.newEmptyProtocolDescriptor();
      required.setProperty(ProtocolDescriptor.AUTHENTICATION_PROPERTY, "LOGIN synthetic-realm");
      // Base MessageAdapter advertises PART independently of collector traffic or auth readiness.
      required.addSend(required.newMessageDescriptor(MessageType.PART));
      required.addReceive(required.newMessageDescriptor(MessageType.PART));
      byte[] requiredBytes = descriptorBytes(required);
      save("wire/" + c + "-required.bin", requiredBytes);
      parse(incoming, adapter, requiredBytes);
      descriptor(c, "required-received", 0, adapter.received.get(0));
      number(c, "required", 0, "login-calls", login.reasons.size());
      text(c, "required", 0, "login-reason", login.reasons.get(0));
      bool(c, "required", 0, "promise-done", login.promises.get(0).isDone());
      bool(c, "required", 0, "adapter-alive", adapter.isAlive());
      bool(c, "required", 0, "finite-auth-deadline", adapter.nextRetrieveTime(0) != Long.MAX_VALUE);
      byte[] pending = retrieve(adapter);
      save("wire/" + c + "-pending.bin", pending);
      number(c, "required", 0, "outgoing-bytes", pending.length);
      parse(outgoing, capture, pending);
      number(c, "required", 0, "subscription-records", capture.subscriptions.size());

      if (scenario.equals("cancelled")) {
        // Only the server AuthManager uses the process's .2s timeout. Observe this client
        // still pending after that interval, without inventing a LoginManager timeout.
        waitUntil(System.currentTimeMillis() + 250);
        byte[] afterWait = retrieve(adapter);
        save("wire/" + c + "-after-wait.bin", afterWait);
        bool(c, "after-250ms", 0, "adapter-alive", adapter.isAlive());
        bool(c, "after-250ms", 0, "promise-done", login.promises.get(0).isDone());
        bool(
            c,
            "after-250ms",
            0,
            "finite-auth-deadline",
            adapter.nextRetrieveTime(0) != Long.MAX_VALUE);
        adapter.close();
        bool(c, "closed", 0, "adapter-closed", adapter.isClosed());
        bool(c, "closed", 0, "promise-cancelled", login.promises.get(0).isCancelled());
      } else {
        login.token = AuthToken.valueOf("Basic c3ludGhldGljOnRva2Vu");
        login.promises.get(0).complete(login.token);
        byte[] authorization = retrieve(adapter);
        save("wire/" + c + "-authorization.bin", authorization);
        Capture delta = new Capture();
        parse(new BinaryQTPParser(scheme), delta, authorization);
        descriptor(c, "authorization-delta", 0, delta.descriptors.get(0));
        parse(outgoing, capture, authorization);
        number(c, "authorization", 0, "subscription-records", capture.subscriptions.size());
        bool(c, "authorization", 0, "adapter-alive", adapter.isAlive());

        if (scenario.equals("refused")) {
          ProtocolDescriptor refused = ProtocolDescriptor.newEmptyProtocolDescriptor();
          refused.setProperty(ProtocolDescriptor.AUTHENTICATION_PROPERTY, "synthetic-refused");
          byte[] refusedBytes = descriptorBytes(refused);
          save("wire/" + c + "-refused.bin", refusedBytes);
          parse(incoming, adapter, refusedBytes);
          descriptor(c, "refused-received", 0, adapter.received.get(1));
          number(c, "refused", 0, "login-calls", login.reasons.size());
          text(c, "refused", 0, "login-reason", login.reasons.get(1));
          login
              .promises
              .get(1)
              .completeExceptionally(new SecurityException("synthetic-login-failed"));
          byte[] failed = retrieve(adapter);
          save("wire/" + c + "-handler-failed.bin", failed);
          parse(outgoing, capture, failed);
          number(c, "handler-failed", 0, "outgoing-bytes", failed.length);
          bool(
              c,
              "handler-failed",
              0,
              "promise-has-exception",
              login.promises.get(1).hasException());
          bool(c, "handler-failed", 0, "adapter-alive", adapter.isAlive());
          bool(
              c,
              "handler-failed",
              0,
              "finite-auth-deadline",
              adapter.nextRetrieveTime(0) != Long.MAX_VALUE);
          number(c, "handler-failed", 0, "subscription-records", capture.subscriptions.size());
        } else {
          ProtocolDescriptor ready = ProtocolDescriptor.newEmptyProtocolDescriptor();
          ready.setProperty(ProtocolDescriptor.AUTHENTICATION_PROPERTY, "");
          ready.addSend(ready.newMessageDescriptor(MessageType.TICKER_DATA));
          ready.addReceive(ready.newMessageDescriptor(MessageType.TICKER_ADD_SUBSCRIPTION));
          ready.addReceive(ready.newMessageDescriptor(MessageType.TICKER_REMOVE_SUBSCRIPTION));
          byte[] readyBytes = descriptorBytes(ready);
          save("wire/" + c + "-ready.bin", readyBytes);
          parse(incoming, adapter, readyBytes);
          descriptor(c, "ready-received", 0, adapter.received.get(1));
          number(c, "ready", 0, "login-calls", login.reasons.size());
          bool(c, "ready", 0, "adapter-alive", adapter.isAlive());
          byte[] afterReady = retrieve(adapter);
          save("wire/" + c + "-after-ready.bin", afterReady);
          parse(outgoing, capture, afterReady);
          number(c, "ready", 0, "subscription-records", capture.subscriptions.size());
          subscription.rewind();
          agent.removeSubscription(subscription);
          byte[] removed = retrieve(adapter);
          save("wire/" + c + "-removed.bin", removed);
          parse(outgoing, capture, removed);
          number(c, "after-remove", 0, "subscription-records", capture.subscriptions.size());
          for (int i = 0; i < capture.subscriptions.size(); i++) {
            text(c, "subscription", i, "message-symbol", capture.subscriptions.get(i));
          }
        }
      }
    } finally {
      adapter.close();
      agent.close();
      subscription.release();
      ticker.close();
    }
    caseRow(c, "DistributorAdapter;MessageAdapter;LoginManager;QDLoginHandler;BinaryQTPParser");
  }

  private byte[] retrieve(MessageAdapter adapter) {
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.compose(adapter);
    return output.toByteArray();
  }

  private byte[] descriptorBytes(ProtocolDescriptor descriptor) {
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.composeDescribeProtocol(descriptor);
    return output.toByteArray();
  }

  private byte[] heartbeatBytes(HeartbeatPayload payload) {
    BinaryQTPComposer composer = new BinaryQTPComposer(scheme, true);
    ByteArrayOutput output = new ByteArrayOutput();
    composer.setOutput(output);
    composer.composeHeartbeatMessage(payload);
    return output.toByteArray();
  }

  private static void parse(BinaryQTPParser parser, MessageConsumerAdapter consumer, byte[] bytes) {
    parser.setInput(new ByteArrayInput(bytes));
    parser.parse(consumer);
  }

  private void descriptor(String c, String phase, int index, ProtocolDescriptor desc) {
    for (Map.Entry<String, String> entry : new TreeMap<>(desc.getProperties()).entrySet()) {
      text(c, phase, index, "property." + entry.getKey(), entry.getValue());
    }
    number(c, phase, index, "send-count", desc.getSendMessages().size());
    number(c, phase, index, "receive-count", desc.getReceiveMessages().size());
    for (MessageDescriptor md : desc.getSendMessages()) {
      message(c, phase, index, "send.", md);
    }
    for (MessageDescriptor md : desc.getReceiveMessages()) {
      message(c, phase, index, "receive.", md);
    }
  }

  private void message(String c, String phase, int index, String direction, MessageDescriptor md) {
    text(c, phase, index, direction + md.getId() + ".name", md.getName());
    for (Map.Entry<String, String> entry : new TreeMap<>(md.getProperties()).entrySet()) {
      text(c, phase, index, direction + md.getId() + "." + entry.getKey(), entry.getValue());
    }
  }

  private void heartbeat(String c, String phase, int index, HeartbeatPayload p, boolean values) {
    bool(c, phase, index, "empty", p.isEmpty());
    bool(c, phase, index, "has-time-millis", p.hasTimeMillis());
    bool(c, phase, index, "has-time-mark", p.hasTimeMark());
    bool(c, phase, index, "has-delta-mark", p.hasDeltaMark());
    bool(c, phase, index, "has-lag-mark", p.hasLagMark());
    if (values) {
      number(c, phase, index, "time-millis", p.getTimeMillis());
      number(c, phase, index, "time-mark", p.getTimeMark());
      number(c, phase, index, "delta-mark", p.getDeltaMark());
      number(c, phase, index, "lag-mark", p.getLagMark());
    }
  }

  private void number(String c, String phase, int index, String key, long n) {
    value(c, phase, index, key, "i:" + n);
  }

  private void bool(String c, String phase, int index, String key, boolean b) {
    value(c, phase, index, key, b ? "b:1" : "b:0");
  }

  private void text(String c, String phase, int index, String key, String s) {
    if (s == null) {
      value(c, phase, index, key, "n:null");
      return;
    }
    StringBuilder encoded = new StringBuilder("u:");
    for (int i = 0; i < s.length(); i++) {
      encoded.append(String.format(java.util.Locale.ROOT, "%04x", (int) s.charAt(i)));
    }
    value(c, phase, index, key, encoded.toString());
  }

  private void value(String c, String phase, int index, String key, String value) {
    String tuple = c + "\t" + phase + "\t" + index + "\t" + key;
    check(keys.add(tuple), "duplicate observation key");
    observations.append(tuple).append('\t').append(value).append('\n');
  }

  private void caseRow(String c, String path) {
    cases.append(c).append("\tproduction-observation\t").append(path).append("\tobserved\n");
  }

  private void save(String file, byte[] bytes) throws IOException {
    Files.write(out.resolve(file), bytes);
  }

  private void write(String file, String contents) throws IOException {
    save(file, contents.getBytes(StandardCharsets.UTF_8));
  }

  private static String hex(byte[] bytes) {
    StringBuilder result = new StringBuilder();
    for (byte b : bytes) {
      result.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
    }
    return result.toString();
  }

  private static void check(boolean condition, String message) {
    if (!condition) {
      throw new AssertionError(message);
    }
  }

  private static void waitUntil(long deadline) throws InterruptedException {
    long start = System.nanoTime();
    while (System.currentTimeMillis() < deadline) {
      check((System.nanoTime() - start) / 1000000 < WAIT_BUDGET_MILLIS, "wait budget exceeded");
      Thread.sleep(1);
    }
  }

  private static final class Capture extends MessageConsumerAdapter {
    final List<ProtocolDescriptor> descriptors = new ArrayList<>();
    final List<HeartbeatPayload> heartbeats = new ArrayList<>();
    final List<String> events = new ArrayList<>();
    final List<String> subscriptions = new ArrayList<>();

    @Override
    public void processDescribeProtocol(ProtocolDescriptor desc, boolean logging) {
      events.add("DESCRIBE_PROTOCOL");
      descriptors.add(ProtocolDescriptor.newPeerProtocolDescriptor(desc));
    }

    @Override
    public void processHeartbeat(HeartbeatPayload p) {
      events.add("HEARTBEAT");
      HeartbeatPayload copy = new HeartbeatPayload();
      copy.updateFrom(p);
      heartbeats.add(copy);
    }

    @Override
    protected void processSubscription(SubscriptionIterator iterator, MessageType message) {
      for (DataRecord record; (record = iterator.nextRecord()) != null; ) {
        String symbol =
            record.getScheme().getCodec().decode(iterator.getCipher(), iterator.getSymbol());
        subscriptions.add(message.name() + ":" + symbol);
      }
    }

    @Override
    public void handleCorruptedStream() {
      throw new AssertionError("corrupted stream");
    }

    @Override
    public void handleCorruptedMessage(int type) {
      throw new AssertionError("corrupted message " + type);
    }

    @Override
    public void handleUnknownMessage(int type) {
      throw new AssertionError("unknown message " + type);
    }
  }

  /**
   * Synthetic application metadata provider; all session/auth/delta logic remains in
   * MessageAdapter.
   */
  private static final class ProbeAdapter extends MessageAdapter {
    private final DataScheme scheme;
    String revision = "first";

    ProbeAdapter(DataScheme scheme) {
      super(QDStats.VOID);
      this.scheme = scheme;
    }

    @Override
    public DataScheme getScheme() {
      return scheme;
    }

    @Override
    public void prepareProtocolDescriptor(ProtocolDescriptor descriptor) {
      super.prepareProtocolDescriptor(descriptor);
      descriptor.setProperty("synthetic-revision", revision);
      descriptor.addSend(descriptor.newMessageDescriptor(MessageType.TICKER_DATA));
      descriptor.addReceive(descriptor.newMessageDescriptor(MessageType.TICKER_ADD_SUBSCRIPTION));
    }

    void requestDescriptor() {
      addMask(getMessageMask(MessageType.DESCRIBE_PROTOCOL));
    }

    @Override
    public boolean retrieveMessages(com.devexperts.qd.qtp.MessageVisitor visitor) {
      super.retrieveMessages(visitor);
      long mask = retrieveDescribeProtocolMessage(visitor, retrieveMask());
      addMask(mask);
      return mask != 0;
    }
  }

  /** Explicitly completed synthetic realm; no thread, credential lookup, or external service. */
  private static final class SyntheticRealm implements QDAuthRealm {
    final Promise<AuthSession> pending = new Promise<>();
    int calls;
    AuthSession session;

    @Override
    public Promise<AuthSession> authenticate(AuthToken token, TypedMap variables) {
      calls++;
      return pending;
    }

    @Override
    public String getAuthenticationInfo() {
      return "synthetic-realm";
    }
  }

  /** Records the public callback after production client processing without changing its result. */
  private static final class LoginClient extends DistributorAdapter {
    final List<ProtocolDescriptor> received = new ArrayList<>();

    LoginClient(QDTicker ticker) {
      super(ticker, null, null, null, QDStats.VOID);
    }

    @Override
    public void processDescribeProtocol(ProtocolDescriptor descriptor, boolean logging) {
      super.processDescribeProtocol(descriptor, logging);
      received.add(ProtocolDescriptor.newPeerProtocolDescriptor(descriptor));
    }
  }

  /** Synthetic callback results only; the production LoginManager owns login state and masks. */
  private static final class SyntheticLogin implements QDLoginHandler {
    final List<String> reasons = new ArrayList<>();
    final List<Promise<AuthToken>> promises = new ArrayList<>();
    AuthToken token;

    @Override
    public Promise<AuthToken> login(String reason) {
      reasons.add(reason);
      Promise<AuthToken> promise = new Promise<>();
      promises.add(promise);
      return promise;
    }

    @Override
    public AuthToken getAuthToken() {
      return token;
    }
  }

  private final class Connection implements AutoCloseable {
    final MemoryTransport transport = new MemoryTransport();
    final ApplicationConnection<?> connection;

    Connection(int periodMillis, int timeoutMillis) throws IOException {
      MessageAdapter.ConfigurableFactory adapters =
          new MessageAdapter.ConfigurableFactory() {
            @Override
            public MessageAdapter createAdapter(QDStats stats) {
              return new ProbeAdapter(scheme);
            }
          };
      MessageAdapterConnectionFactory factory =
          (MessageAdapterConnectionFactory)
              MessageConnectors.applicationConnectionFactory(adapters);
      factory.setInitialHeartbeatPeriod(TimePeriod.valueOf(periodMillis));
      factory.setHeartbeatPeriod(TimePeriod.valueOf(periodMillis * 4L));
      factory.setHeartbeatTimeout(TimePeriod.valueOf(timeoutMillis));
      connection = factory.createConnection(transport);
      connection.start();
    }

    byte[] retrieve() throws IOException {
      ChunkList chunks = connection.retrieveChunks(this);
      if (chunks == null) {
        return new byte[0];
      }
      try {
        ByteArrayOutput output = new ByteArrayOutput();
        for (Chunk chunk : chunks) {
          output.write(chunk.getBytes(), chunk.getOffset(), chunk.getLength());
        }
        return output.toByteArray();
      } finally {
        chunks.recycle(this);
      }
    }

    @Override
    public void close() {
      connection.close();
    }
  }

  private static final class MemoryTransport implements TransportConnection {
    final TypedMap variables = new TypedMap();
    int available;
    int closed;

    @Override
    public TypedMap variables() {
      return variables;
    }

    @Override
    public void markForImmediateRestart() {}

    @Override
    public void connectionClosed() {
      closed++;
    }

    @Override
    public void chunksAvailable() {
      available++;
    }

    @Override
    public void readyToProcessChunks() {}
  }
}
