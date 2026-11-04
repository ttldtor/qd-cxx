// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
import com.devexperts.connector.proto.ApplicationConnection;
import com.devexperts.connector.proto.ApplicationConnectionFactory;
import com.devexperts.connector.proto.TransportConnection;
import com.devexperts.qd.qtp.MessageConnector;
import com.devexperts.qd.qtp.MessageConnectors;
import com.devexperts.qd.qtp.socket.SocketAddress;
import com.devexperts.qd.qtp.socket.SocketUtil;
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.impl.DXEndpointImpl;
import java.io.IOException;
import java.io.PrintStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.util.List;
import java.util.Properties;

/**
 * Actual QD parser/setter/builder observations; only the explicit auto-connect case owns loopback
 * I/O.
 */
public final class EndpointConfigurationOracle {
  /**
   * Parser-only application factory: accidental activation fails before creating an application
   * session.
   */
  private static final class NeverActivatedFactory extends ApplicationConnectionFactory {
    @Override
    public ApplicationConnection<?> createConnection(TransportConnection transport)
        throws IOException {
      throw new IOException("Parser-only factory must not be activated");
    }

    @Override
    public String toString() {
      return "configuration-oracle";
    }
  }

  private static void addresses() {
    String[] inputs = {
      "localhost:1234",
      "127.0.0.1:1234",
      "[::1]:1234",
      "[2001:db8::1]:65535",
      "host:0",
      "host:-1",
      "host:+1",
      "host:65536",
      "host:1x",
      "host",
      "host1,host2:1234",
      "host1:11,host2:22",
      "::1:1234",
      "[::1:1234",
      "bad_host:1234",
      "host name:1234"
    };
    for (int i = 0; i < inputs.length; i++) {
      try {
        List<SocketAddress> values = SocketUtil.parseAddressList(inputs[i]);
        StringBuilder result = new StringBuilder();
        for (SocketAddress value : values) {
          if (result.length() != 0) result.append('|');
          result.append(value.host).append(':').append(value.port);
        }
        System.out.println("address\t" + i + "\tok\t" + values.size() + "\t" + result);
      } catch (RuntimeException failure) {
        System.out.println("address\t" + i + "\terror\t" + failure.getClass().getSimpleName());
      }
    }
  }

  private static void connectors() throws Exception {
    String[] inputs = {
      "localhost:1234",
      "localhost:1234[reconnectDelay=17]",
      "localhost:1234[reconnectDelay=17,reconnectDelay=29]",
      "localhost:1234[reconnectDelay=17][RECONNECTDELAY=31]",
      "localhost:1234(reconnectDelay=37)",
      "localhost:1234[reconnectDelay=0]",
      "localhost:1234[reconnectDelay=-1]",
      "localhost:1234[reconnectDelay=1s]",
      "localhost:1234[unknown=1]",
      "localhost:1234[name=one,name=two]",
      "localhost:1234[user=sample,password=sample]",
      "host1,host2:1234"
    };
    for (int i = 0; i < inputs.length; i++) {
      List<MessageConnector> values = null;
      try {
        try {
          values =
              MessageConnectors.createMessageConnectors(new NeverActivatedFactory(), inputs[i]);
        } catch (RuntimeException failure) {
          System.out.println("connector\t" + i + "\terror\t" + failure.getClass().getSimpleName());
          continue;
        }
        MessageConnector first = values.get(0);
        System.out.println(
            "connector\t"
                + i
                + "\tok\t"
                + values.size()
                + "\t"
                + first.getReconnectDelay()
                + "\t"
                + first.isActive()
                + "\t"
                + ("one".equals(first.getName()))
                + "\t"
                + (first.getPassword() != null && !first.getPassword().isEmpty()));
      } finally {
        if (values != null)
          for (MessageConnector value : values) {
            value.stopAndWait();
            value.close();
          }
      }
    }
  }

  private static void properties() throws Exception {
    Properties values = new Properties();
    values.setProperty("name", "defaults");
    values.setProperty("dxfeed.wildcard.enable", "true");
    DXEndpoint.Builder builder =
        DXEndpoint.newBuilder()
            .withRole(DXEndpoint.Role.STREAM_FEED)
            .withProperties(values)
            .withProperty("name", "explicit")
            .withProperty("name", "last")
            .withProperty("dxfeed.wildcard.enable", "false")
            .withProperty("not-supported", "sample")
            .withProperty("dxfeed.address", "localhost:1234[reconnectDelay=47]");
    DXEndpointImpl first = (DXEndpointImpl) builder.build();
    try {
      System.out.println("property\tlast-duplicate\t" + first.getProperty("name"));
      System.out.println(
          "property\tfalse-override\t" + first.getProperty("dxfeed.wildcard.enable"));
      System.out.println("property\tunknown-ignored\t" + !first.hasProperty("not-supported"));
      values.setProperty("name", "mutated");
      System.out.println("property\tinput-owning\t" + first.getProperty("name"));
      builder.withProperty("name", "later");
      System.out.println("property\tbuilder-mutation-visible\t" + first.getProperty("name"));
      System.out.println(
          "property\tstream-address-not-auto\t" + first.getQDEndpoint().getConnectors().size());
      System.out.println(
          "property\tinline-before-connect\t"
              + first.getProperty("dxfeed.address").endsWith("[reconnectDelay=47]"));
      System.out.println("property\tsupported-name\t" + builder.supportsProperty("name"));
      System.out.println(
          "property\tunsupported-native-key\t"
              + builder.supportsProperty("qdcxx.connectTimeoutMillis"));
    } finally {
      first.closeAndAwaitTermination();
    }
    DXEndpointImpl malformed =
        (DXEndpointImpl)
            DXEndpoint.newBuilder()
                .withRole(DXEndpoint.Role.STREAM_FEED)
                .withProperty("dxfeed.wildcard.enable", "truthy")
                .build();
    try {
      System.out.println(
          "property\tmalformed-boolean-retained\t"
              + malformed.getProperty("dxfeed.wildcard.enable"));
    } finally {
      malformed.closeAndAwaitTermination();
    }
  }

  private static void defaults(java.io.File directory) throws Exception {
    java.io.File file = new java.io.File(directory, "explicit-defaults.properties");
    Properties defaults = new Properties();
    defaults.setProperty("name", "file");
    defaults.setProperty("dxfeed.wildcard.enable", "true");
    defaults.setProperty("dxfeed.address", "");
    try (java.io.OutputStream output = new java.io.FileOutputStream(file)) {
      defaults.store(output, "Owned oracle defaults; no credentials");
    }
    String oldWildcard = System.getProperty("dxfeed.wildcard.enable");
    String oldName = System.getProperty("name");
    System.setProperty("dxfeed.wildcard.enable", "false");
    System.setProperty("name", "system");
    try {
      DXEndpointImpl fromFile =
          (DXEndpointImpl)
              DXEndpoint.newBuilder()
                  .withRole(DXEndpoint.Role.FEED)
                  .withProperty("dxfeed.properties", file.getAbsolutePath())
                  .build();
      try {
        System.out.println(
            "defaults\tfile-before-system\t" + fromFile.getProperty("dxfeed.wildcard.enable"));
        System.out.println("defaults\tfile-name\t" + fromFile.getProperty("name"));
      } finally {
        fromFile.closeAndAwaitTermination();
      }
      DXEndpointImpl explicit =
          (DXEndpointImpl)
              DXEndpoint.newBuilder()
                  .withRole(DXEndpoint.Role.FEED)
                  .withProperty("dxfeed.properties", file.getAbsolutePath())
                  .withProperty("dxfeed.wildcard.enable", "false")
                  .withProperty("name", "explicit")
                  .build();
      try {
        System.out.println(
            "defaults\texplicit-before-file\t" + explicit.getProperty("dxfeed.wildcard.enable"));
        System.out.println("defaults\texplicit-name\t" + explicit.getProperty("name"));
      } finally {
        explicit.closeAndAwaitTermination();
      }
    } finally {
      if (oldWildcard == null) System.clearProperty("dxfeed.wildcard.enable");
      else System.setProperty("dxfeed.wildcard.enable", oldWildcard);
      if (oldName == null) System.clearProperty("name");
      else System.setProperty("name", oldName);
    }
  }

  private static void autoConnect() throws Exception {
    try (ServerSocket listener = new ServerSocket(0, 1, InetAddress.getByName("127.0.0.1"))) {
      String address = "127.0.0.1:" + listener.getLocalPort() + "[reconnectDelay=53]";
      DXEndpointImpl endpoint =
          (DXEndpointImpl)
              DXEndpoint.newBuilder()
                  .withRole(DXEndpoint.Role.FEED)
                  .withProperty("dxfeed.address", address)
                  .build();
      try {
        List<MessageConnector> connectors = endpoint.getQDEndpoint().getConnectors();
        System.out.println(
            "lifecycle\tfeed-build-autoconnect\t"
                + connectors.size()
                + "\t"
                + connectors.get(0).isActive());
        System.out.println(
            "lifecycle\tinline-at-activation\t" + connectors.get(0).getReconnectDelay());
      } finally {
        endpoint.closeAndAwaitTermination();
      }
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length != 1) throw new IllegalArgumentException("Expected output TSV path");
    try (PrintStream output = new PrintStream(args[0], "UTF-8")) {
      System.setOut(output);
      addresses();
      connectors();
      properties();
      defaults(new java.io.File(args[0]).getAbsoluteFile().getParentFile());
      autoConnect();
      output.flush();
      output.close();
      if (output.checkError()) throw new IOException("Failed writing oracle output");
    }
  }
}
