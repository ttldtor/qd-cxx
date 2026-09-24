package qdcxx.schemaexport;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Stream;

/** Fresh-JVM offline command. The portable launcher owns classpath and process qualification. */
public final class SchemaExport {
  private SchemaExport() {}

  private static final String HELP =
      "qdcxx Java resolved schema export\n"
          + "Use tools/schema-export/Export.cmake with explicit JDK8 and classpath receipt.\n"
          + "--source default|endpoint|xml --output NEW_DIRECTORY [--record EXACT_NAME ...]\n"
          + "--startup KEY=VALUE ...  --properties FILE  --set KEY=VALUE ...\n"
          + "XML: --input-root DIR --xml RELATIVE_FILE ... --resource dxlink\n"
          + "     --xml-dxprops true|false --xml-sysprops true|false\n"
          + "Limits: --max-schema-bytes|records|fields|lookups|name-units|total-name-units N\n"
          + "        --max-input-files N --max-input-bytes N --max-import-depth N\n"
          + "        --max-sidecar-bytes N\n";

  /** Prints stable diagnostics without echoing configuration, paths or upstream exception text. */
  public static void main(String[] args) {
    int status;
    try {
      status = run(args);
    } catch (OutOfMemoryError failure) {
      status = error("RESOURCE_LIMIT");
    } catch (ReflectiveOperationException | SecurityException failure) {
      status = error("UNSUPPORTED_LOOKUPS");
    } catch (IOException failure) {
      status = error("IO_ERROR");
    } catch (Exception failure) {
      String code = failure.getMessage();
      Set<String> known =
          new HashSet<>(
              Arrays.asList(
                  "USAGE",
                  "INVALID_INPUT",
                  "INVALID_SCHEMA",
                  "INVALID_CONFIGURATION",
                  "RESOURCE_LIMIT",
                  "UNSUPPORTED_RECORD",
                  "UNSUPPORTED_TYPE",
                  "UNSUPPORTED_INDEXED",
                  "UNSUPPORTED_LOOKUPS",
                  "UNSUPPORTED_SOURCE",
                  "PUBLICATION_FAILED"));
      status = error(known.contains(code) ? code : "INVALID_INPUT");
    }
    if (status != 0) {
      System.exit(status);
    }
  }

  private static int error(String code) {
    System.err.print("schema-export-error\t1\t" + code + "\n");
    if (code.equals("USAGE")) {
      return 2;
    }
    if (code.equals("IO_ERROR") || code.equals("PUBLICATION_FAILED")) {
      return 3;
    }
    if (code.startsWith("UNSUPPORTED_")) {
      return 4;
    }
    return code.equals("RESOURCE_LIMIT") ? 6 : 5;
  }

  static int run(String[] args) throws Exception {
    // Launcher metadata is not opened on the help path.
    int start = args.length >= 2 && args[0].equals("--provenance") ? 2 : 0;
    if (args.length == start || (args.length == start + 1 && args[start].equals("--help"))) {
      System.out.print(HELP);
      return System.out.checkError() ? 3 : 0;
    }
    ResolvedSchemaExporter.require(args.length <= 4096, "RESOURCE_LIMIT");
    Map<String, String> options = new HashMap<>();
    Map<String, String> startup = new LinkedHashMap<>();
    Map<String, String> overrides = new LinkedHashMap<>();
    List<String> records = new ArrayList<>();
    List<String> xml = new ArrayList<>();
    Set<String> seenRecords = new HashSet<>();
    Set<String> seenXml = new HashSet<>();
    Set<String> singletons =
        new HashSet<>(
            Arrays.asList(
                "--source",
                "--output",
                "--properties",
                "--input-root",
                "--resource",
                "--xml-dxprops",
                "--xml-sysprops",
                "--max-schema-bytes",
                "--max-schema-records",
                "--max-schema-fields",
                "--max-schema-lookups",
                "--max-schema-name-units",
                "--max-schema-total-name-units",
                "--max-input-files",
                "--max-input-bytes",
                "--max-import-depth",
                "--max-sidecar-bytes"));
    for (int i = start; i < args.length; i += 2) {
      ResolvedSchemaExporter.require(i + 1 < args.length, "USAGE");
      String key = args[i];
      String value = args[i + 1];
      ResolvedSchemaExporter.require(value.length() <= 32768, "RESOURCE_LIMIT");
      if (key.equals("--record")) {
        ResolvedSchemaExporter.require(seenRecords.add(value), "USAGE");
        records.add(value);
      } else if (key.equals("--xml")) {
        ResolvedSchemaExporter.require(seenXml.add(value), "USAGE");
        xml.add(value);
      } else if (key.equals("--startup") || key.equals("--set")) {
        Map<String, String> settings = key.equals("--startup") ? startup : overrides;
        int equals = value.indexOf('=');
        ResolvedSchemaExporter.require(equals > 0, "USAGE");
        ResolvedSchemaExporter.require(
            settings.put(value.substring(0, equals), value.substring(equals + 1)) == null, "USAGE");
      } else {
        ResolvedSchemaExporter.require(
            singletons.contains(key) && options.put(key, value) == null, "USAGE");
      }
    }
    ResolvedSchemaExporter.require(
        options.containsKey("--source") && options.containsKey("--output") && start == 2, "USAGE");
    ResolvedSchemaExporter.Limits limits = new ResolvedSchemaExporter.Limits();
    limits.bytes = number(options, "--max-schema-bytes", limits.bytes);
    limits.records = number(options, "--max-schema-records", limits.records);
    limits.fields = number(options, "--max-schema-fields", limits.fields);
    limits.lookups = number(options, "--max-schema-lookups", limits.lookups);
    limits.nameUnits = number(options, "--max-schema-name-units", limits.nameUnits);
    limits.totalNameUnits = number(options, "--max-schema-total-name-units", limits.totalNameUnits);
    limits.sidecarBytes = number(options, "--max-sidecar-bytes", limits.sidecarBytes);
    int files = number(options, "--max-input-files", 64);
    int bytes = number(options, "--max-input-bytes", 1048576);
    int depth = number(options, "--max-import-depth", 16);
    Path destination = Paths.get(options.get("--output")).toAbsolutePath().normalize();
    ResolvedSchemaExporter.require(
        !Files.exists(destination, LinkOption.NOFOLLOW_LINKS)
            && destination.getParent() != null
            && Files.isDirectory(destination.getParent()),
        "PUBLICATION_FAILED");
    String provenance =
        new String(readBounded(Paths.get(args[1]), 65536), StandardCharsets.UTF_8).trim();
    ResolvedSchemaExporter.require(
        provenance.startsWith("{") && provenance.endsWith("}"), "INVALID_INPUT");
    ResolvedSchemaExporter.require(
        !options.containsKey("--resource") || options.get("--resource").equals("dxlink"), "USAGE");
    String mode = options.get("--source");
    ResolvedSchemaExporter.require(
        mode.equals("xml")
            || (!options.containsKey("--input-root")
                && !options.containsKey("--resource")
                && !options.containsKey("--xml-dxprops")
                && !options.containsKey("--xml-sysprops")
                && xml.isEmpty()),
        "USAGE");
    ResolvedSchemaExporter.Snapshot snapshot;
    String sidecarJson;
    try (SchemaSource source =
        SchemaSource.resolve(
            mode,
            startup,
            path(options, "--properties"),
            overrides,
            path(options, "--input-root"),
            xml,
            options.containsKey("--resource"),
            bool(options, "--xml-dxprops"),
            bool(options, "--xml-sysprops"),
            files,
            bytes,
            depth)) {
      snapshot = ResolvedSchemaExporter.export(source.scheme, records, limits);
      ResolvedSchemaExporter.Writer sidecar =
          new ResolvedSchemaExporter.Writer(limits.sidecarBytes);
      sidecar.add(
          "{\"format\":\"qdcxx-java-export-v1\",\"tool\":"
              + provenance
              + ",\"source\":"
              + source.provenanceJson
              + ",\"selection\":[");
      for (int i = 0; i < records.size(); i++) {
        if (i != 0) {
          sidecar.add(",");
        }
        sidecar.add(ResolvedSchemaExporter.quote(records.get(i)));
      }
      sidecar.add("],\"projection\":" + snapshot.projectionJson + "}\n");
      sidecarJson = sidecar.toString();
    }
    publish(destination, snapshot.schemaJson, sidecarJson);
    return 0;
  }

  private static Path path(Map<String, String> options, String key) {
    return options.containsKey(key) ? Paths.get(options.get(key)) : null;
  }

  private static boolean bool(Map<String, String> options, String key) {
    String value = options.getOrDefault(key, "false");
    ResolvedSchemaExporter.require(value.equals("true") || value.equals("false"), "USAGE");
    return value.equals("true");
  }

  private static int number(Map<String, String> options, String key, int fallback) {
    String value = options.get(key);
    if (value == null) {
      return fallback;
    }
    ResolvedSchemaExporter.require(value.matches("[0-9]{1,10}"), "USAGE");
    long parsed = Long.parseLong(value);
    ResolvedSchemaExporter.require(parsed <= Integer.MAX_VALUE, "USAGE");
    return (int) parsed;
  }

  static byte[] readBounded(Path file, int limit) throws IOException {
    try (java.io.InputStream input = Files.newInputStream(file);
        java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream()) {
      byte[] chunk = new byte[8192];
      int count;
      while ((count = input.read(chunk)) != -1) {
        ResolvedSchemaExporter.require(count <= limit - output.size(), "RESOURCE_LIMIT");
        output.write(chunk, 0, count);
      }
      return output.toByteArray();
    }
  }

  /**
   * Same-parent directory move without replacement; only complete bundles reach the destination.
   */
  static void publish(Path destination, String schema, String provenance) throws IOException {
    Path staging = Files.createTempDirectory(destination.getParent(), ".qdcxx-schema-");
    try {
      Files.write(staging.resolve("schema.json"), schema.getBytes(StandardCharsets.UTF_8));
      Files.write(staging.resolve("provenance.json"), provenance.getBytes(StandardCharsets.UTF_8));
      Files.move(staging, destination);
    } finally {
      if (Files.exists(staging, LinkOption.NOFOLLOW_LINKS)) {
        try (Stream<Path> paths = Files.walk(staging)) {
          for (Path path : (Iterable<Path>) paths.sorted(Comparator.reverseOrder())::iterator) {
            Files.delete(path);
          }
        }
      }
    }
  }
}
