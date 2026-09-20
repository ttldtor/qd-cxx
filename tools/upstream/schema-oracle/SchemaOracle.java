package qdcxx.oracle;

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataRecord;
import com.dxfeed.scheme.DXScheme;
import com.dxfeed.scheme.SchemeLoadingOptions;
import com.dxfeed.scheme.model.SchemeModel;
import com.dxfeed.scheme.model.SchemeRecord;
import java.net.URL;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.StringJoiner;
import java.util.TreeSet;
import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.validation.SchemaFactory;

/** Observation only: all loading, resolution and construction is upstream code. */
public final class SchemaOracle {
  // TSV with escaped cells; null is distinct from an empty string.
  static void row(Object... values) {
    StringJoiner line = new StringJoiner("\t");
    for (Object v : values) {
      line.add(
          v == null
              ? "\\N"
              : v.toString()
                  .replace("\\", "\\\\")
                  .replace("\t", "\\t")
                  .replace("\r", "\\r")
                  .replace("\n", "\\n"));
    }
    System.out.println(line);
  }

  static void error(String stage, Exception e) {
    row("error", stage);
    for (Throwable t = e; t != null; t = t.getCause()) {
      row("exception", t.getClass().getName(), t.getMessage());
      for (StackTraceElement frame : t.getStackTrace()) {
        if (frame.getClassName().startsWith("com.dxfeed.")
            || frame.getClassName().startsWith("com.devexperts.")) {
          row("origin", frame.getClassName(), frame.getMethodName(), frame.getLineNumber());
          break;
        }
      }
    }
  }

  static String target(DataField f) {
    return f == null ? null : f.getName();
  }

  static void field(String category, DataField f) {
    row(
        "slot",
        category,
        f.getIndex(),
        f.getName(),
        f.getLocalName(),
        f.getPropertyName(),
        f.getSerialType().getId(),
        f.getSerialType().getName(),
        f.getClass().getName());
  }

  public static void main(String[] args) throws Exception {
    if (args[0].equals("environment")) {
      for (String key :
          new String[] {
            "java.version",
            "java.vendor",
            "java.home",
            "java.class.path",
            "os.name",
            "os.version",
            "os.arch",
            "user.language",
            "user.country",
            "user.timezone",
            "file.encoding"
          }) {
        row("property", key, System.getProperty(key));
      }
      row("jaxp.dom", DocumentBuilderFactory.newInstance().getClass().getName());
      row(
          "jaxp.xsd",
          SchemaFactory.newInstance(XMLConstants.W3C_XML_SCHEMA_NS_URI).getClass().getName());
      // Environment keys only: never dump unrelated system properties or secrets.
      for (String key : new TreeSet<>(System.getProperties().stringPropertyNames())) {
        if (key.equals("scheme")
            || key.startsWith("dxscheme.")
            || key.startsWith("dxfeed.")
            || key.startsWith("com.devexperts.")
            || key.startsWith("javax.xml.")) {
          row("relevant.property", key, System.getProperty(key));
        }
      }
      for (String service :
          new String[] {
            "com.devexperts.qd.ng.RecordMappingFactory",
            "com.devexperts.qd.DataScheme",
            "com.devexperts.qd.DataSchemeFactory",
            "com.dxfeed.api.impl.EventDelegateFactory"
          }) {
        Enumeration<URL> resources =
            SchemaOracle.class.getClassLoader().getResources("META-INF/services/" + service);
        while (resources.hasMoreElements()) {
          URL u = resources.nextElement();
          row("service.resource", service, u);
          try (java.io.BufferedReader r =
              new java.io.BufferedReader(new java.io.InputStreamReader(u.openStream(), "UTF-8"))) {
            String s;
            while ((s = r.readLine()) != null) {
              if (!s.trim().isEmpty() && !s.trim().startsWith("#")) {
                row("service.entry", service, s);
              }
            }
          }
        }
      }
      return;
    }
    row("case", args[0]);
    List<URL> urls = new ArrayList<>();
    for (int i = 1; i < args.length; i++) {
      urls.add(Paths.get(args[i]).toUri().toURL());
    }
    for (URL url : urls) {
      row("input", url);
    }
    SchemeLoadingOptions options = new SchemeLoadingOptions();
    options.useSystemProperties(false);
    options.useDXFeedProperties(false);
    row(
        "options",
        "sysprops",
        options.shouldUseSystemProperties(),
        "dxprops",
        options.shouldUseDXFeedProperties());
    SchemeModel model = null;
    try {
      model = SchemeModel.newLoader().withOptions(options).fromURLs(urls).load();
    } catch (Exception e) {
      error("model.load", e);
    }
    // Serialization is outside the catch: harness defects must fail the process.
    if (model != null) {
      row("model", "merged", model.mergedInCount(), "empty", model.isEmpty());
      for (String source : model.getSources()) {
        row("model.source", source);
      }
      for (SchemeModel.RecordSource source : model.getRecordGenerationSources()) {
        row("generation", source.getSource(), source.getName());
      }
      for (SchemeRecord r : model.getRecords().values()) {
        row(
            "model.record",
            r.getName(),
            r.isDisabled(),
            model.isRecordEnabled(r, r.getName()),
            r.getIndex1(),
            r.getIndex2(),
            r.getBase(),
            r.hasRegionals(),
            r.getEventName());
        row("model.record.sources", r.getFilesList());
        for (SchemeRecord.Field f : r.getFields()) {
          row(
              "model.field",
              f.getName(),
              f.getType(),
              model.resolveType(f.getType()).getId(),
              model.resolveType(f.getType()).getName(),
              f.isDisabled(),
              model.isRecordFieldEnabled(r.getName(), f),
              f.getMainAlias());
          row("model.field.sources", f.getFilesList());
          for (SchemeRecord.Field.Alias alias : f.getAliases()) {
            row("model.alias", alias.getValue(), alias.isMain(), alias.getMode());
          }
        }
      }
    }
    DXScheme scheme = null;
    try {
      scheme = DXScheme.newLoader().withOptions(options).fromURLs(urls).load();
    } catch (Exception e) {
      error("runtime.load", e);
    }
    if (scheme != null) {
      row("runtime", scheme.getRecordCount(), scheme.getDigest());
      if (model != null) {
        for (String name : model.getRecords().keySet()) {
          row(
              "record.lookup",
              name,
              scheme.findRecordByName(name) == null ? null : scheme.findRecordByName(name).getId());
        }
      }
      row("record.lookup", "MissingRecord", scheme.findRecordByName("MissingRecord"));
      for (int i = 0; i < scheme.getRecordCount(); i++) {
        DataRecord r = scheme.getRecord(i);
        row(
            "record",
            r.getId(),
            r.getName(),
            r.getClass().getName(),
            r.hasTime(),
            r.getIntFieldCount(),
            r.getObjFieldCount());
        row("mapping.default", r.getMapping() == null ? null : r.getMapping().getClass().getName());
        for (Class<?> type : r.getMappings().keySet()) {
          row("mapping", type.getName());
        }
        LinkedHashSet<String> probes = new LinkedHashSet<>();
        for (int j = 0; j < r.getIntFieldCount(); j++) {
          DataField f = r.getIntField(j);
          field("int", f);
          probes.add(f.getName());
          probes.add(f.getLocalName());
          probes.add(f.getPropertyName());
          probes.add(f.getLocalName().replace(".", ""));
        }
        for (int j = 0; j < r.getObjFieldCount(); j++) {
          DataField f = r.getObjField(j);
          field("obj", f);
          probes.add(f.getName());
          probes.add(f.getLocalName());
          probes.add(f.getPropertyName());
          probes.add(f.getLocalName().replace(".", ""));
        }
        if (model != null && model.getRecords().containsKey(r.getName())) {
          for (SchemeRecord.Field f : model.getRecords().get(r.getName()).getFields()) {
            probes.add(f.getName());
            probes.add(r.getName() + "." + f.getName());
            for (SchemeRecord.Field.Alias a : f.getAliases()) {
              probes.add(a.getValue());
              probes.add(r.getName() + "." + a.getValue());
            }
          }
        }
        probes.add("Missing");
        for (String probe : probes) {
          row(
              "lookup",
              probe,
              target(r.findFieldByName(probe)),
              target(scheme.findIntFieldByName(probe)),
              target(scheme.findObjFieldByName(probe)));
        }
      }
    }
    row("complete", args[0]);
  }
}
