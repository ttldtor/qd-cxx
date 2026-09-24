package qdcxx.schemaexport;

import static qdcxx.schemaexport.ResolvedSchemaExporter.quote;

import com.devexperts.qd.DataScheme;
import com.dxfeed.api.DXEndpoint;
import com.dxfeed.api.impl.DXEndpointImpl;
import com.dxfeed.api.impl.DXFeedScheme;
import com.dxfeed.scheme.DXScheme;
import com.dxfeed.scheme.SchemeLoadingOptions;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.TreeMap;
import java.util.logging.Level;
import java.util.logging.LogManager;
import java.util.logging.Logger;
import java.util.stream.Stream;
import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.helpers.DefaultHandler;

/** Controlled command-only source acquisition; the reusable exporter never calls this adapter. */
public final class SchemaSource implements AutoCloseable {
  private static final String NS = "https://www.dxfeed.com/datascheme";
  private static final String XSI = XMLConstants.W3C_XML_SCHEMA_INSTANCE_NS_URI;
  private static final Set<String> RESOURCES =
      new HashSet<>(Arrays.asList("dxlink.xml", "dxfeed.xml", "types.xml", "visibility.xml"));

  /**
   * Resolved upstream production scheme, valid independently of the snapshot after construction.
   */
  public final DataScheme scheme;

  /** Deterministic relevant input provenance, excluding absolute paths and ambient properties. */
  public final String provenanceJson;

  private final Path snapshot;
  private final DXEndpoint endpoint;

  private SchemaSource(
      DataScheme scheme, String provenanceJson, Path snapshot, DXEndpoint endpoint) {
    this.scheme = scheme;
    this.provenanceJson = provenanceJson;
    this.snapshot = snapshot;
    this.endpoint = endpoint;
  }

  /**
   * Validates explicit inputs before constructing a production scheme in a fresh command JVM.
   * Enabled selections admit at most 128 ASCII letters/digits and '*' glob characters, starting
   * with a letter or '*'; no list, regex or whitespace grammar is accepted. Limits count
   * properties, XML and the pinned XSD; root import depth is one. Global properties are replaced
   * deliberately.
   */
  public static SchemaSource resolve(
      String mode,
      Map<String, String> startup,
      Path propertiesFile,
      Map<String, String> overrides,
      Path inputRoot,
      List<String> xmlFiles,
      boolean dxlink,
      boolean dxprops,
      boolean sysprops,
      int maxFiles,
      int maxBytes,
      int maxDepth)
      throws Exception {
    if (!Arrays.asList("default", "endpoint", "xml").contains(mode)) {
      throw failure("INVALID_INPUT");
    }
    if (maxFiles < 0
        || maxFiles > 64
        || maxBytes < 0
        || maxBytes > 1048576
        || maxDepth < 0
        || maxDepth > 16) {
      throw failure("RESOURCE_LIMIT");
    }
    validateSettings(startup, mode);
    validateSettings(overrides, mode);
    Budget budget = new Budget(maxFiles, maxBytes, maxDepth);
    byte[] propertyBytes = null;
    Properties fileProperties = new UniqueProperties();
    if (propertiesFile != null) {
      propertyBytes = budget.read(Files.newInputStream(propertiesFile));
      try {
        fileProperties.load(new ByteArrayInputStream(propertyBytes));
      } catch (IllegalArgumentException invalid) {
        throw failure("INVALID_CONFIGURATION");
      }
      Map<String, String> values = new TreeMap<>();
      for (String key : fileProperties.stringPropertyNames()) {
        values.put(key, fileProperties.getProperty(key));
      }
      validateSettings(values, mode);
    }
    boolean xml = mode.equals("xml");
    if ((!xml && (inputRoot != null || !xmlFiles.isEmpty() || dxlink || dxprops || sysprops))
        || (mode.equals("default") && (propertiesFile != null || !overrides.isEmpty()))
        || (xml
            && ((!startup.isEmpty() && !sysprops)
                || ((propertiesFile != null || !overrides.isEmpty()) && !dxprops)))
        || (xml && (xmlFiles.isEmpty() && !dxlink))
        || (xml && (!xmlFiles.isEmpty() && inputRoot == null))) {
      throw failure("INVALID_CONFIGURATION");
    }
    controlledProperties(startup);
    // The fixed upstream classpath uses JUL. No handlers prevents its console setup; keep
    // source paths and parser details out of the command's single-row diagnostic channel.
    LogManager.getLogManager().reset();
    Logger.getLogger("").setLevel(Level.OFF);
    DocumentBuilder parser = hardenedParser();
    Path snapshot = Files.createTempDirectory("qdcxx-schema-source-");
    DXEndpoint endpoint = null;
    boolean success = false;
    try {
      TreeMap<String, byte[]> inputs = new TreeMap<>();
      Map<String, Integer> heights = new TreeMap<>();
      List<String> roots = new ArrayList<>();
      Path realRoot = inputRoot == null ? null : inputRoot.toRealPath();
      if (xml) {
        byte[] xsd = budget.read(resource("dxfeed-schema.xsd"));
        Document schemaDoc = parse(parser, xsd);
        NodeList schemaNodes =
            schemaDoc.getElementsByTagNameNS(XMLConstants.W3C_XML_SCHEMA_NS_URI, "*");
        for (int i = 0; i < schemaNodes.getLength(); i++) {
          String tag = schemaNodes.item(i).getLocalName();
          if (tag.equals("import") || tag.equals("include") || tag.equals("redefine")) {
            throw failure("UNSUPPORTED_SOURCE");
          }
        }
        inputs.put("resource/dxfeed-schema.xsd", xsd);
        if (dxlink) {
          collect("dxlink.xml", true, null, budget, parser, inputs, heights, new HashSet<>(), 1);
          roots.add("resource/dxlink.xml");
        }
        Set<String> uniqueRoots = new HashSet<>();
        for (String name : xmlFiles) {
          String relative = relative(name);
          if (!uniqueRoots.add(relative)) {
            throw failure("INVALID_INPUT");
          }
          collect(relative, false, realRoot, budget, parser, inputs, heights, new HashSet<>(), 1);
          roots.add("local/" + relative);
        }
        for (Map.Entry<String, byte[]> input : inputs.entrySet()) {
          Path path = snapshot.resolve(input.getKey());
          Files.createDirectories(path.getParent());
          Files.write(path, input.getValue());
        }
        for (String name : inputs.keySet()) {
          if (name.equals("resource/dxfeed-schema.xsd")) {
            continue;
          }
          Path schemaPath = snapshot.resolve(name).getParent().resolve("dxfeed-schema.xsd");
          String schemaKey = snapshot.relativize(schemaPath).toString().replace('\\', '/');
          if (inputs.containsKey(schemaKey) && !schemaKey.equals("resource/dxfeed-schema.xsd")) {
            throw failure("INVALID_INPUT");
          }
          Files.write(schemaPath, xsd);
        }
      }
      DataScheme scheme;
      if (mode.equals("default")) {
        scheme = DXFeedScheme.getInstance();
      } else if (mode.equals("endpoint")) {
        Path explicitFile = snapshot.resolve("endpoint.properties");
        Files.write(explicitFile, propertyBytes == null ? new byte[0] : propertyBytes);
        DXEndpoint.Builder builder =
            DXEndpoint.newBuilder()
                .withRole(DXEndpoint.Role.FEED)
                .withProperty("dxfeed.properties", explicitFile.toString());
        for (Map.Entry<String, String> setting : new TreeMap<>(overrides).entrySet()) {
          builder.withProperty(setting.getKey(), setting.getValue());
        }
        endpoint = builder.build();
        scheme = ((DXEndpointImpl) endpoint).getQDEndpoint().getScheme();
      } else {
        SchemeLoadingOptions options = new SchemeLoadingOptions();
        options.useDXFeedProperties(dxprops);
        options.useSystemProperties(sysprops);
        DXScheme.Loader loader = DXScheme.newLoader().withOptions(options);
        loader.withProperties(fileProperties);
        for (Map.Entry<String, String> setting : new TreeMap<>(overrides).entrySet()) {
          loader.withProperty(setting.getKey(), setting.getValue());
        }
        for (String root : roots) {
          loader.fromURL(snapshot.resolve(root).toUri().toURL());
        }
        scheme = loader.load();
      }
      StringBuilder provenance = new StringBuilder("{\"mode\":").append(quote(mode));
      provenance.append(",\"startup\":").append(settingsJson(startup));
      provenance.append(",\"properties\":").append(settingsJson(propertiesMap(fileProperties)));
      provenance
          .append(",\"propertiesSha256\":")
          .append(propertyBytes == null ? "null" : quote(hash(propertyBytes)));
      provenance.append(",\"overrides\":").append(settingsJson(overrides));
      provenance.append(",\"dxprops\":").append(dxprops).append(",\"sysprops\":").append(sysprops);
      provenance.append(",\"roots\":[");
      for (int i = 0; i < roots.size(); i++) {
        if (i != 0) {
          provenance.append(',');
        }
        provenance.append(quote(roots.get(i)));
      }
      provenance.append("],\"inputs\":[");
      boolean first = true;
      for (Map.Entry<String, byte[]> input : inputs.entrySet()) {
        if (!first) {
          provenance.append(',');
        }
        first = false;
        provenance
            .append("{\"path\":")
            .append(quote(input.getKey()))
            .append(",\"sha256\":")
            .append(quote(hash(input.getValue())))
            .append('}');
      }
      provenance.append("]}");
      SchemaSource result = new SchemaSource(scheme, provenance.toString(), snapshot, endpoint);
      success = true;
      return result;
    } finally {
      if (!success) {
        if (endpoint != null) {
          endpoint.close();
        }
        deleteSnapshot(snapshot);
      }
    }
  }

  private static void validateSettings(Map<String, String> settings, String mode) {
    for (Map.Entry<String, String> item : settings.entrySet()) {
      String key = item.getKey();
      String value = item.getValue();
      boolean valid = false;
      if (key.equals("dxscheme.nanoTime") || key.equals("dxscheme.wide")) {
        valid = value.equals("true") || value.equals("false");
      } else if (key.equals("dxscheme.enabled.Sequence")
          || key.equals("dxscheme.enabled.TimeNanoPart")) {
        valid = value.length() <= 128 && value.matches("[A-Za-z*][A-Za-z0-9*]*");
      } else if (key.equals("dxscheme.price")
          || key.equals("dxscheme.size")
          || key.equals("dxscheme.shares")) {
        valid =
            value.matches("(?i:int|wide|tiny|decimal)")
                && !(mode.equals("xml") && key.equals("dxscheme.shares"));
      }
      if (!valid) {
        throw failure("INVALID_CONFIGURATION");
      }
    }
  }

  private static void controlledProperties(Map<String, String> startup) {
    Properties controlled = new Properties();
    for (String key :
        Arrays.asList(
            "java.home",
            "java.version",
            "java.vendor",
            "java.vendor.url",
            "java.class.version",
            "java.class.path",
            "java.library.path",
            "java.io.tmpdir",
            "java.vm.name",
            "java.vm.vendor",
            "java.vm.version",
            "java.vm.specification.name",
            "java.vm.specification.vendor",
            "java.vm.specification.version",
            "java.specification.name",
            "java.specification.vendor",
            "java.specification.version",
            "os.name",
            "os.arch",
            "os.version",
            "file.separator",
            "path.separator",
            "line.separator",
            "user.dir",
            "user.home")) {
      String value = System.getProperty(key);
      if (value != null) {
        controlled.setProperty(key, value);
      }
    }
    controlled.setProperty("file.encoding", "UTF-8");
    controlled.setProperty("user.language", "en");
    controlled.setProperty("user.country", "US");
    controlled.setProperty("user.timezone", "UTC");
    controlled.setProperty("javax.xml.accessExternalDTD", "");
    // QD's pinned explicit classpath XSD uses file URLs. XML preflight permits only its fixed local
    // declaration, and the private snapshot supplies that exact XSD beside every accepted input.
    controlled.setProperty("javax.xml.accessExternalSchema", "file");
    controlled.setProperty("javax.xml.accessExternalStylesheet", "");
    controlled.putAll(startup);
    System.setProperties(controlled);
  }

  private static DocumentBuilder hardenedParser() throws Exception {
    DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
    factory.setNamespaceAware(true);
    factory.setFeature(XMLConstants.FEATURE_SECURE_PROCESSING, true);
    factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl", true);
    factory.setFeature("http://xml.org/sax/features/external-general-entities", false);
    factory.setFeature("http://xml.org/sax/features/external-parameter-entities", false);
    factory.setFeature("http://apache.org/xml/features/nonvalidating/load-external-dtd", false);
    factory.setXIncludeAware(false);
    factory.setExpandEntityReferences(false);
    factory.setAttribute(XMLConstants.ACCESS_EXTERNAL_DTD, "");
    factory.setAttribute(XMLConstants.ACCESS_EXTERNAL_SCHEMA, "");
    if (!"".equals(factory.getAttribute(XMLConstants.ACCESS_EXTERNAL_DTD))
        || !"".equals(factory.getAttribute(XMLConstants.ACCESS_EXTERNAL_SCHEMA))) {
      throw failure("UNSUPPORTED_SOURCE");
    }
    DocumentBuilder parser = factory.newDocumentBuilder();
    parser.setEntityResolver(
        (publicId, systemId) -> {
          throw new SAXException("INVALID_INPUT");
        });
    parser.setErrorHandler(
        new DefaultHandler() {
          @Override
          public void error(SAXParseException error) throws SAXException {
            throw error;
          }

          @Override
          public void fatalError(SAXParseException error) throws SAXException {
            throw error;
          }
        });
    return parser;
  }

  private static Document parse(DocumentBuilder parser, byte[] bytes) throws Exception {
    try {
      return parser.parse(new ByteArrayInputStream(bytes));
    } catch (SAXException invalid) {
      throw failure("INVALID_INPUT");
    }
  }

  private static int collect(
      String name,
      boolean resource,
      Path root,
      Budget budget,
      DocumentBuilder parser,
      Map<String, byte[]> inputs,
      Map<String, Integer> heights,
      Set<String> active,
      int depth)
      throws Exception {
    if (depth > budget.maxDepth) {
      throw failure("RESOURCE_LIMIT");
    }
    String key = (resource ? "resource/" : "local/") + name;
    Path real = null;
    if (resource) {
      if (!RESOURCES.contains(name)) {
        throw failure("INVALID_INPUT");
      }
    } else {
      Path lexical = root.resolve(name).normalize();
      if (!lexical.startsWith(root)) {
        throw failure("INVALID_INPUT");
      }
      real = lexical.toRealPath();
      if (!real.startsWith(root) || !Files.isRegularFile(real)) {
        throw failure("INVALID_INPUT");
      }
    }
    String identity = resource ? key : real.toString();
    if (active.contains(identity)) {
      throw failure("INVALID_INPUT");
    }
    if (heights.containsKey(key)) {
      int height = heights.get(key);
      if (depth + height - 1 > budget.maxDepth) {
        throw failure("RESOURCE_LIMIT");
      }
      return height;
    }
    active.add(identity);
    byte[] bytes = inputs.get(key);
    if (bytes == null) {
      bytes = budget.read(resource ? resource(name) : Files.newInputStream(real));
    }
    Document document = parse(parser, bytes);
    Element documentRoot = document.getDocumentElement();
    if (!NS.equals(documentRoot.getNamespaceURI())
        || !"dxfeed".equals(documentRoot.getNodeName())) {
      throw failure("INVALID_INPUT");
    }
    NodeList nodes = document.getElementsByTagName("*");
    List<String> imports = new ArrayList<>();
    for (int i = 0; i < nodes.getLength(); i++) {
      Element node = (Element) nodes.item(i);
      if ("http://www.w3.org/2001/XInclude".equals(node.getNamespaceURI())
          || node.hasAttributeNS(XMLConstants.XML_NS_URI, "base")
          || node.hasAttributeNS(XSI, "noNamespaceSchemaLocation")) {
        throw failure("INVALID_INPUT");
      }
      if (node.hasAttributeNS(XSI, "schemaLocation")
          && !node.getAttributeNS(XSI, "schemaLocation").equals(NS + " dxfeed-schema.xsd")) {
        throw failure("INVALID_INPUT");
      }
      if ("import".equals(node.getLocalName())) {
        if (!NS.equals(node.getNamespaceURI()) || !"import".equals(node.getNodeName())) {
          throw failure("INVALID_INPUT");
        }
        String target = node.getTextContent();
        validateRelativeSpelling(target);
        Path parent = Paths.get(name).getParent();
        imports.add(
            relative(
                (parent == null ? Paths.get(target) : parent.resolve(target))
                    .normalize()
                    .toString()
                    .replace('\\', '/')));
      }
    }
    inputs.put(key, bytes);
    int height = 1;
    for (String imported : imports) {
      height =
          Math.max(
              height,
              1
                  + collect(
                      imported, resource, root, budget, parser, inputs, heights, active,
                      depth + 1));
    }
    active.remove(identity);
    heights.put(key, height);
    return height;
  }

  private static String relative(String name) {
    validateRelativeSpelling(name);
    Path path = Paths.get(name).normalize();
    if (path.isAbsolute() || path.startsWith("..") || path.toString().isEmpty()) {
      throw failure("INVALID_INPUT");
    }
    return path.toString().replace('\\', '/');
  }

  private static void validateRelativeSpelling(String name) {
    if (name.isEmpty()
        || name.length() > 240
        || !name.matches("[A-Za-z0-9_. /-]+")
        || name.startsWith("/")
        || !name.equals(name.trim())) {
      throw failure("INVALID_INPUT");
    }
  }

  private static InputStream resource(String name) {
    InputStream stream = SchemaSource.class.getClassLoader().getResourceAsStream(name);
    if (stream == null) {
      throw failure("UNSUPPORTED_SOURCE");
    }
    return stream;
  }

  private static final class Budget {
    private int files;
    private int bytes;
    private final int maxDepth;

    private Budget(int files, int bytes, int maxDepth) {
      this.files = files;
      this.bytes = bytes;
      this.maxDepth = maxDepth;
    }

    private byte[] read(InputStream input) throws IOException {
      try (InputStream owned = input) {
        if (files-- == 0) {
          throw failure("RESOURCE_LIMIT");
        }
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int count;
        while ((count = owned.read(buffer, 0, Math.min(buffer.length, bytes + 1))) != -1) {
          if (count > bytes) {
            throw failure("RESOURCE_LIMIT");
          }
          out.write(buffer, 0, count);
          bytes -= count;
        }
        return out.toByteArray();
      }
    }
  }

  private static final class UniqueProperties extends Properties {
    @Override
    public synchronized Object put(Object key, Object value) {
      if (containsKey(key)) {
        throw failure("INVALID_CONFIGURATION");
      }
      return super.put(key, value);
    }
  }

  private static Map<String, String> propertiesMap(Properties properties) {
    Map<String, String> result = new TreeMap<>();
    for (String key : properties.stringPropertyNames()) {
      result.put(key, properties.getProperty(key));
    }
    return result;
  }

  private static String settingsJson(Map<String, String> settings) {
    StringBuilder out = new StringBuilder("{");
    boolean first = true;
    for (Map.Entry<String, String> item : new TreeMap<>(settings).entrySet()) {
      if (!first) {
        out.append(',');
      }
      first = false;
      out.append(quote(item.getKey())).append(':').append(quote(item.getValue()));
    }
    return out.append('}').toString();
  }

  private static String hash(byte[] bytes) throws Exception {
    StringBuilder result = new StringBuilder();
    for (byte value : MessageDigest.getInstance("SHA-256").digest(bytes)) {
      result.append(String.format(java.util.Locale.ROOT, "%02x", value & 255));
    }
    return result.toString();
  }

  private static IllegalArgumentException failure(String code) {
    return new IllegalArgumentException(code);
  }

  private static void deleteSnapshot(Path snapshot) throws IOException {
    try (Stream<Path> entries = Files.walk(snapshot)) {
      Path[] ordered = entries.sorted(Comparator.reverseOrder()).toArray(Path[]::new);
      for (Path entry : ordered) {
        Files.delete(entry);
      }
    }
  }

  /** Closes the unconnected endpoint and removes only the owned private snapshot. */
  @Override
  public void close() throws IOException {
    try {
      if (endpoint != null) {
        endpoint.close();
      }
    } finally {
      deleteSnapshot(snapshot);
    }
  }
}
