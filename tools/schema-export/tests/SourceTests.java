package qdcxx.schemaexport.tests;

import com.devexperts.qd.DataRecord;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Collections;
import java.util.Map;
import java.util.TreeMap;
import qdcxx.schemaexport.SchemaSource;

/** Focused command-source boundary checks; use a fresh JVM for each named scenario. */
public final class SourceTests {
  private static final Map<String, String> NONE = Collections.emptyMap();
  private static final String OPEN = "<dxfeed xmlns=\"https://www.dxfeed.com/datascheme\">";

  private SourceTests() {}

  /** Runs source validation or one production source construction scenario. */
  public static void main(String[] args) throws Exception {
    Path root = Paths.get(args[0]);
    Files.createDirectories(root);
    switch (args[1]) {
      case "boundary":
        boundary(root);
        break;
      case "default":
        try (SchemaSource source =
            resolve("default", NONE, null, NONE, null, false, false, false)) {
          check(source.scheme.findRecordByName("Quote") != null);
        }
        break;
      case "endpoint":
        Path props =
            write(
                root,
                "settings.properties",
                "dxscheme.nanoTime=false\ndxscheme.enabled.Sequence=Quote\n");
        try (SchemaSource source =
            resolve(
                "endpoint",
                Collections.singletonMap("dxscheme.nanoTime", "true"),
                props,
                NONE,
                null,
                false,
                false,
                false)) {
          DataRecord quote = source.scheme.findRecordByName("Quote");
          check(quote.findFieldByName("Sequence") != null);
          check(quote.findFieldByName("TimeNanoPart") == null);
        }
        break;
      case "endpoint-api":
        Path file =
            write(
                root,
                "settings.properties",
                "dxscheme.nanoTime=false\ndxscheme.enabled.Sequence=Quote\n");
        Map<String, String> settings = new TreeMap<>();
        settings.put("dxscheme.nanoTime", "true");
        settings.put("dxscheme.enabled.Sequence", "Profile");
        settings.put("dxscheme.price", "INT");
        try (SchemaSource source =
            resolve("endpoint", NONE, file, settings, null, false, false, false)) {
          DataRecord quote = source.scheme.findRecordByName("Quote");
          check(quote.findFieldByName("Sequence") == null);
          check(quote.findFieldByName("TimeNanoPart") != null);
          check(quote.findFieldByName("BidPrice").getSerialType().getId() == 8);
        }
        break;
      case "xml":
        write(
            root,
            "base.xml",
            OPEN
                + "<records><record name=\"Custom\"><field name=\"Value\""
                + " type=\"compact_int\"/></record></records></dxfeed>");
        write(
            root,
            "root.xml",
            OPEN
                + "<import>base.xml</import><records><record name=\"Custom\" mode=\"update\"><field"
                + " name=\"Text\" type=\"utf_char_array\"/></record></records></dxfeed>");
        try (SchemaSource source = resolve("xml", NONE, null, NONE, root, false, false, false)) {
          DataRecord custom = source.scheme.findRecordByName("Custom");
          check(custom.getIntFieldCount() == 1 && custom.getObjFieldCount() == 1);
          check(
              !source.provenanceJson.contains(
                  root.toAbsolutePath().toString().replace("\\", "\\\\")));
        }
        break;
      case "dxlink":
        try (SchemaSource source =
            resolve(
                "xml",
                Collections.singletonMap("dxscheme.nanoTime", "true"),
                null,
                NONE,
                null,
                true,
                false,
                true)) {
          check(source.scheme.findRecordByName("Quote").findFieldByName("TimeNanoPart") != null);
        }
        break;
      case "xml-properties":
        Map<String, String> xmlSettings = new TreeMap<>();
        xmlSettings.put("dxscheme.nanoTime", "false");
        xmlSettings.put("dxscheme.enabled.Sequence", "Qu*");
        xmlSettings.put("dxscheme.price", "DeCiMaL");
        try (SchemaSource source =
            resolve(
                "xml",
                Collections.singletonMap("dxscheme.nanoTime", "true"),
                null,
                xmlSettings,
                null,
                true,
                true,
                true)) {
          DataRecord quote = source.scheme.findRecordByName("Quote");
          check(quote.findFieldByName("Sequence") != null);
          check(quote.findFieldByName("TimeNanoPart") == null);
          check(quote.findFieldByName("BidPrice").getSerialType().getId() == 24);
        }
        break;
      case "limits":
        limits(root);
        break;
      case "symlinks":
        symlinks(root);
        break;
      default:
        throw new AssertionError("Unknown scenario");
    }
    System.out.println("source " + args[1] + " passed");
  }

  private static void boundary(Path root) throws Exception {
    reject(
        () ->
            resolve(
                "default",
                Collections.singletonMap("dxfeed.address", "secret"),
                null,
                NONE,
                null,
                false,
                false,
                false),
        "INVALID_CONFIGURATION");
    reject(
        () ->
            resolve(
                "default",
                NONE,
                null,
                Collections.singletonMap("dxscheme.nanoTime", "true"),
                null,
                false,
                false,
                false),
        "INVALID_CONFIGURATION");
    reject(
        () ->
            resolve(
                "default",
                Collections.singletonMap("dxscheme.nanoTime", "yes"),
                null,
                NONE,
                null,
                false,
                false,
                false),
        "INVALID_CONFIGURATION");
    reject(
        () ->
            resolve(
                "xml",
                Collections.singletonMap("dxscheme.nanoTime", "true"),
                null,
                NONE,
                root,
                false,
                false,
                false),
        "INVALID_CONFIGURATION");
    reject(
        () ->
            resolve(
                "xml",
                NONE,
                null,
                Collections.singletonMap("dxscheme.shares", "int"),
                null,
                true,
                true,
                false),
        "INVALID_CONFIGURATION");
    reject(
        () ->
            resolve(
                "default",
                Collections.singletonMap("dxscheme.enabled.Sequence", "Quote,Profile"),
                null,
                NONE,
                null,
                false,
                false,
                false),
        "INVALID_CONFIGURATION");
    reject(
        () ->
            resolve(
                "default",
                Collections.singletonMap("dxscheme.enabled.Sequence", "Qu?*"),
                null,
                NONE,
                null,
                false,
                false,
                false),
        "INVALID_CONFIGURATION");
    reject(
        () ->
            SchemaSource.resolve(
                "xml",
                NONE,
                null,
                NONE,
                root,
                Arrays.asList("https://invalid.example/schema.xml"),
                false,
                false,
                false,
                64,
                1048576,
                16),
        "INVALID_INPUT");
    Path duplicate =
        write(root, "duplicate.properties", "dxscheme.nanoTime=true\ndxscheme.nanoTime=false\n");
    reject(
        () -> resolve("endpoint", NONE, duplicate, NONE, null, false, false, false),
        "INVALID_CONFIGURATION");
    for (String body :
        Arrays.asList(
            "<import>https://invalid.example/schema.xml</import>",
            "<import>/absolute.xml</import>",
            "<import>../escape.xml</import>",
            "<import>root.xml</import>",
            "<xi:include xmlns:xi=\"http://www.w3.org/2001/XInclude\" href=\"other.xml\"/>",
            "<records xml:base=\"https://invalid.example/\"/>")) {
      write(root, "root.xml", OPEN + body + "</dxfeed>");
      reject(() -> resolve("xml", NONE, null, NONE, root, false, false, false), "INVALID_INPUT");
    }
    write(
        root,
        "root.xml",
        "<!DOCTYPE dxfeed [<!ENTITY x SYSTEM 'file:///missing'>]>" + OPEN + "&x;</dxfeed>");
    reject(() -> resolve("xml", NONE, null, NONE, root, false, false, false), "INVALID_INPUT");
    for (String attribute :
        Arrays.asList(
            "xsi:schemaLocation=\"https://www.dxfeed.com/datascheme"
                + " https://invalid.example/schema.xsd\"",
            "xsi:noNamespaceSchemaLocation=\"dxfeed-schema.xsd\"",
            "xsi:schemaLocation=\"https://www.dxfeed.com/datascheme other.xsd\"")) {
      write(
          root,
          "root.xml",
          "<dxfeed xmlns=\"https://www.dxfeed.com/datascheme\""
              + " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
              + attribute
              + "/>");
      reject(() -> resolve("xml", NONE, null, NONE, root, false, false, false), "INVALID_INPUT");
    }
    write(root, "root.xml", OPEN + "</dxfeed>");
    reject(
        () ->
            SchemaSource.resolve(
                "xml",
                NONE,
                null,
                NONE,
                root,
                Arrays.asList("root.xml"),
                false,
                false,
                false,
                0,
                1048576,
                16),
        "RESOURCE_LIMIT");
    reject(
        () ->
            SchemaSource.resolve(
                "xml",
                NONE,
                null,
                NONE,
                root,
                Arrays.asList("root.xml"),
                false,
                false,
                false,
                64,
                1,
                16),
        "RESOURCE_LIMIT");
    reject(
        () ->
            SchemaSource.resolve(
                "xml",
                NONE,
                null,
                NONE,
                root,
                Arrays.asList("root.xml"),
                false,
                false,
                false,
                64,
                1048576,
                0),
        "RESOURCE_LIMIT");
    jaxpRestrictions();
  }

  private static void jaxpRestrictions() throws Exception {
    javax.xml.parsers.DocumentBuilderFactory factory =
        javax.xml.parsers.DocumentBuilderFactory.newInstance();
    javax.xml.parsers.DocumentBuilder parser = factory.newDocumentBuilder();
    parser.setErrorHandler(
        new org.xml.sax.helpers.DefaultHandler() {
          @Override
          public void fatalError(org.xml.sax.SAXParseException error)
              throws org.xml.sax.SAXException {
            throw error;
          }
        });
    try {
      parser.parse(
          new java.io.ByteArrayInputStream(
              "<!DOCTYPE a SYSTEM 'http://127.0.0.1:1/forbidden'><a/>"
                  .getBytes(StandardCharsets.UTF_8)));
      throw new AssertionError("JAXP allowed external DTD");
    } catch (org.xml.sax.SAXException expected) {
      check(expected.getMessage().contains("accessExternalDTD"));
    }
    factory.setNamespaceAware(true);
    factory.setValidating(true);
    factory.setAttribute(
        "http://java.sun.com/xml/jaxp/properties/schemaLanguage",
        "http://www.w3.org/2001/XMLSchema");
    parser = factory.newDocumentBuilder();
    parser.setErrorHandler(
        new org.xml.sax.helpers.DefaultHandler() {
          @Override
          public void error(org.xml.sax.SAXParseException error) throws org.xml.sax.SAXException {
            throw error;
          }

          @Override
          public void fatalError(org.xml.sax.SAXParseException error)
              throws org.xml.sax.SAXException {
            throw error;
          }
        });
    try {
      parser.parse(
          new java.io.ByteArrayInputStream(
              "<a xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:noNamespaceSchemaLocation='http://127.0.0.1:1/forbidden'/>"
                  .getBytes(StandardCharsets.UTF_8)));
      throw new AssertionError("JAXP allowed remote schema");
    } catch (org.xml.sax.SAXException expected) {
      check(expected.getMessage().contains("accessExternalSchema"));
    }
  }

  private static void limits(Path root) throws Exception {
    Files.createDirectories(root.resolve("nested"));
    write(
        root,
        "base.xml",
        OPEN
            + "<records><record name=\"Custom\"><field name=\"Value\""
            + " type=\"compact_int\"/></record></records></dxfeed>");
    write(root, "nested/root.xml", OPEN + "<import>../base.xml</import></dxfeed>");
    int xsdBytes = 0;
    try (java.io.InputStream input =
        SourceTests.class.getClassLoader().getResourceAsStream("dxfeed-schema.xsd")) {
      while (input.read() != -1) {
        xsdBytes++;
      }
    }
    final int bytes =
        xsdBytes
            + (int) Files.size(root.resolve("base.xml"))
            + (int) Files.size(root.resolve("nested/root.xml"));
    try (SchemaSource source =
        SchemaSource.resolve(
            "xml",
            NONE,
            null,
            NONE,
            root,
            Arrays.asList("nested/root.xml"),
            false,
            false,
            false,
            3,
            bytes,
            2)) {
      check(source.scheme.findRecordByName("Custom") != null);
    }
    reject(
        () ->
            SchemaSource.resolve(
                "xml",
                NONE,
                null,
                NONE,
                root,
                Arrays.asList("nested/root.xml"),
                false,
                false,
                false,
                2,
                bytes,
                2),
        "RESOURCE_LIMIT");
    reject(
        () ->
            SchemaSource.resolve(
                "xml",
                NONE,
                null,
                NONE,
                root,
                Arrays.asList("nested/root.xml"),
                false,
                false,
                false,
                3,
                bytes - 1,
                2),
        "RESOURCE_LIMIT");
    reject(
        () ->
            SchemaSource.resolve(
                "xml",
                NONE,
                null,
                NONE,
                root,
                Arrays.asList("base.xml", "nested/root.xml"),
                false,
                false,
                false,
                3,
                bytes,
                1),
        "RESOURCE_LIMIT");
  }

  private static void symlinks(Path root) throws Exception {
    Path absoluteRoot = root.toAbsolutePath();
    Files.createDirectories(absoluteRoot.resolve("real"));
    write(
        absoluteRoot,
        "real/base.xml",
        OPEN
            + "<records><record name=\"Custom\"><field name=\"Value\""
            + " type=\"compact_int\"/></record></records></dxfeed>");
    write(absoluteRoot, "real/root.xml", OPEN + "<import>base.xml</import></dxfeed>");
    Files.deleteIfExists(absoluteRoot.resolve("alias"));
    Files.createSymbolicLink(absoluteRoot.resolve("alias"), absoluteRoot.resolve("real"));
    try (SchemaSource source =
        SchemaSource.resolve(
            "xml",
            NONE,
            null,
            NONE,
            absoluteRoot,
            Arrays.asList("alias/root.xml"),
            false,
            false,
            false,
            64,
            1048576,
            16)) {
      check(source.scheme.findRecordByName("Custom") != null);
      check(source.provenanceJson.contains("local/alias/base.xml"));
    }
    Path outside = write(absoluteRoot.getParent(), "outside.xml", OPEN + "</dxfeed>");
    Files.deleteIfExists(absoluteRoot.resolve("escape.xml"));
    Files.createSymbolicLink(absoluteRoot.resolve("escape.xml"), outside);
    reject(
        () ->
            SchemaSource.resolve(
                "xml",
                NONE,
                null,
                NONE,
                absoluteRoot,
                Arrays.asList("escape.xml"),
                false,
                false,
                false,
                64,
                1048576,
                16),
        "INVALID_INPUT");
  }

  private static SchemaSource resolve(
      String mode,
      Map<String, String> startup,
      Path props,
      Map<String, String> overrides,
      Path root,
      boolean dxlink,
      boolean dxprops,
      boolean sysprops)
      throws Exception {
    return SchemaSource.resolve(
        mode,
        startup,
        props,
        overrides,
        root,
        root == null ? Collections.emptyList() : Arrays.asList("root.xml"),
        dxlink,
        dxprops,
        sysprops,
        64,
        1048576,
        16);
  }

  private static Path write(Path root, String name, String value) throws Exception {
    Path file = root.resolve(name);
    Files.write(file, value.getBytes(StandardCharsets.UTF_8));
    return file;
  }

  private interface Attempt {
    void run() throws Exception;
  }

  private static void reject(Attempt attempt, String code) throws Exception {
    try {
      attempt.run();
      throw new AssertionError("Expected " + code);
    } catch (IllegalArgumentException expected) {
      check(expected.getMessage().equals(code));
    }
  }

  private static void check(boolean value) {
    if (!value) {
      throw new AssertionError("Source boundary assertion failed");
    }
  }
}
