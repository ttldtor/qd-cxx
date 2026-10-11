package qdcxx.oracle;

import com.devexperts.qd.DataField;
import com.devexperts.qd.DataIntField;
import com.devexperts.qd.DataIterator;
import com.devexperts.qd.DataObjField;
import com.devexperts.qd.DataRecord;
import com.devexperts.qd.DataScheme;
import com.devexperts.qd.SerialFieldType;
import com.devexperts.qd.kit.DefaultRecord;
import com.devexperts.qd.kit.DefaultScheme;
import com.devexperts.qd.kit.PentaCodec;
import com.devexperts.qd.kit.VoidIntField;
import com.devexperts.qd.ng.RecordCursor;
import com.devexperts.qd.ng.RecordSource;
import com.devexperts.qd.qtp.MessageConsumerAdapter;
import com.devexperts.qd.qtp.MessageType;
import com.devexperts.qd.qtp.file.FileReader;
import com.devexperts.qd.qtp.file.FileReaderParams;
import com.devexperts.transport.stats.ConnectionStats;
import com.dxfeed.api.impl.DXFeedScheme;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Verify emitted tapes through the locked production FileReader STREAM projection. */
public final class TapeWriterInteropOracle {
  private static final int FILE_BYTES = 16 * 1024 * 1024;
  private static final int CALLBACK_RECORDS = 8192;
  private static final int RECORDS = 256;
  private static final int FIELDS = 2048;
  private static final int TEXT_UNITS = 1024 * 1024;
  private static final int OUTPUT_ROWS = CALLBACK_RECORDS * 16;
  private static final String HEX_DIGITS = "0123456789abcdef";
  private static final int HEX_BITS = 4;
  private static final int HEX_MASK = 0xf;

  private static final class Field {
    final int logicalId;
    final String name;
    final String wireName;
    final SerialFieldType type;
    DataField bound;

    Field(String[] cells) {
      logicalId = Integer.parseInt(cells[2]);
      name = decode(cells[3]);
      wireName = decode(cells[4]);
      type = serialType(Integer.parseInt(cells[5]));
    }
  }

  private static void require(boolean condition, String reason) {
    if (!condition) {
      throw new IllegalStateException(reason);
    }
  }

  private static SerialFieldType serialType(int id) {
    SerialFieldType result;
    switch (id) {
      case 2:
        result = SerialFieldType.UTF_CHAR;
        break;
      case 8:
        result = SerialFieldType.COMPACT_INT;
        break;
      case 10:
        result = SerialFieldType.UTF_CHAR_ARRAY;
        break;
      case 40:
        result = SerialFieldType.SHORT_STRING;
        break;
      case 56:
        result = SerialFieldType.TIME_SECONDS;
        break;
      case 72:
        result = SerialFieldType.SEQUENCE;
        break;
      case 88:
        result = SerialFieldType.DATE;
        break;
      case 120:
        result = SerialFieldType.WIDE_DECIMAL;
        break;
      default:
        throw new IllegalArgumentException("unsupported fixture type " + id);
    }
    require(result.getId() == id, "locked full type identity");
    return result;
  }

  private static String decode(String hex) {
    if (hex.equals("-")) {
      return "";
    }
    require(hex.length() % 4 == 0 && hex.length() <= 4 * TEXT_UNITS, "bounded units grammar");
    StringBuilder text = new StringBuilder(hex.length() / 4);
    for (int index = 0; index < hex.length(); index += 4) {
      text.append((char) Integer.parseInt(hex.substring(index, index + 4), 16));
    }
    return text.toString();
  }

  private static String units(Object value) {
    if (value == null) {
      return "null";
    }
    require(value instanceof String || value instanceof char[], "exact string representation");
    String text = value instanceof String ? (String) value : null;
    char[] characters = text == null ? (char[]) value : null;
    int length = text == null ? characters.length : text.length();
    require(length <= TEXT_UNITS, "finite output units");
    StringBuilder hex = new StringBuilder((Character.SIZE / HEX_BITS) * length);
    for (int index = 0; index < length; index++) {
      char character = text == null ? characters[index] : text.charAt(index);
      for (int shift = Character.SIZE - HEX_BITS; shift >= 0; shift -= HEX_BITS) {
        hex.append(HEX_DIGITS.charAt((character >>> shift) & HEX_MASK));
      }
    }
    return hex.length() == 0 ? "-" : hex.toString();
  }

  private static List<String> readLines(Path path) throws Exception {
    require(Files.size(path) <= FILE_BYTES, "finite fixture bytes: " + path.getFileName());
    List<String> lines = Files.readAllLines(path, StandardCharsets.UTF_8);
    require(lines.size() <= OUTPUT_ROWS, "finite fixture rows");
    return lines;
  }

  private static void append(List<String> actual, String row, int[] counts) {
    require(
        actual.size() < OUTPUT_ROWS && row.length() < FILE_BYTES - counts[3],
        "finite output rows and characters");
    counts[3] += row.length() + 1;
    actual.add(row);
  }

  private static Map<String, List<Field>> readSchema(Path path) throws Exception {
    Map<String, List<Field>> records = new LinkedHashMap<>();
    int fields = 0;
    for (String line : readLines(path)) {
      String[] cells = line.split("\t", -1);
      require(cells.length == 6, "schema row width");
      String name = decode(cells[1]);
      if (cells[0].equals("record")) {
        require(records.size() < RECORDS && !records.containsKey(name), "unique bounded records");
        records.put(name, new ArrayList<Field>());
      } else {
        require(cells[0].equals("field") && records.containsKey(name), "field record identity");
        require(fields++ < FIELDS, "bounded fields");
        Field field = new Field(cells);
        for (Field previous : records.get(name)) {
          require(previous.logicalId != field.logicalId, "unique logical field identity");
        }
        records.get(name).add(field);
      }
    }
    return records;
  }

  private static DataScheme scheme(Map<String, List<Field>> records, boolean standard) {
    if (standard) {
      DataScheme scheme = DXFeedScheme.getInstance();
      for (Map.Entry<String, List<Field>> entry : records.entrySet()) {
        DataRecord record = scheme.findRecordByName(entry.getKey());
        require(record != null, "unchanged standard record exists");
        HashSet<DataField> covered = new HashSet<>();
        for (Field field : entry.getValue()) {
          field.bound = record.findFieldByName(field.wireName);
          require(field.bound != null, "unchanged standard lookup exists");
          require(field.bound.getSerialType().getId() == field.type.getId(), "exact standard type");
          require(covered.add(field.bound), "unique standard field coverage");
        }
        for (int index = 0; index < record.getIntFieldCount(); index++) {
          DataField field = record.getIntField(index);
          if (field.getSerialType().getId() != 0) {
            require(covered.remove(field), "complete unchanged standard integer fields");
          }
        }
        for (int index = 0; index < record.getObjFieldCount(); index++) {
          require(
              covered.remove(record.getObjField(index)),
              "complete unchanged standard object fields");
        }
        require(covered.isEmpty(), "no extra standard fields");
      }
      return scheme;
    }
    List<DataRecord> result = new ArrayList<>();
    for (Map.Entry<String, List<Field>> entry : records.entrySet()) {
      List<DataIntField> ints = new ArrayList<>();
      List<DataObjField> objects = new ArrayList<>();
      for (Field field : entry.getValue()) {
        String name = entry.getKey() + "." + field.name;
        if (field.type.getId() == 10) {
          field.bound = field.type.createDefaultObjInstance(objects.size(), name);
          objects.add((DataObjField) field.bound);
        } else {
          field.bound = field.type.createDefaultIntInstance(ints.size(), name);
          ints.add((DataIntField) field.bound);
          if (field.type.isLong()) {
            ints.add(new VoidIntField(ints.size(), name + "$VoidTail"));
          }
        }
      }
      DefaultRecord record =
          new DefaultRecord(
              result.size(),
              entry.getKey(),
              false,
              ints.toArray(new DataIntField[0]),
              objects.toArray(new DataObjField[0]));
      for (Field field : entry.getValue()) {
        record.addFieldAlias(field.name, field.wireName);
      }
      result.add(record);
    }
    return new DefaultScheme(PentaCodec.INSTANCE, result.toArray(new DataRecord[0]));
  }

  public static void main(String[] args) throws Exception {
    require(args.length == 2 || args.length == 3, "case-directory output-file [standard]");
    require(args.length != 3 || args[2].equals("standard"), "explicit standard mode");
    Path root = Paths.get(args[0]);
    Map<String, List<Field>> records = readSchema(root.resolve("schema.tsv"));
    DataScheme scheme = scheme(records, args.length == 3);
    Path tape = root.resolve("tape.qtp");
    require(Files.size(tape) <= FILE_BYTES, "finite tape bytes");
    final List<String> actual = new ArrayList<>();
    final int[] counts = new int[4];
    FileReaderParams.Default params = new FileReaderParams.Default();
    params.setSpeed(FileReaderParams.MAX_SPEED);
    params.setCycle(false);
    FileReader reader = new FileReader(tape.toString(), new ConnectionStats(), params);
    reader.setScheme(scheme);
    try {
      reader.readInto(
          new MessageConsumerAdapter() {
            @Override
            protected void processData(DataIterator iterator, MessageType type) {
              RecordCursor cursor;
              while ((cursor = ((RecordSource) iterator).next()) != null) {
                require(counts[2]++ < CALLBACK_RECORDS, "finite callback records");
                if (type != MessageType.STREAM_DATA) {
                  continue;
                }
                int index = counts[0]++;
                DataRecord record = cursor.getRecord();
                require(records.containsKey(record.getName()), "no unrepresented record");
                String symbol = scheme.getCodec().decode(cursor.getCipher(), cursor.getSymbol());
                append(
                    actual,
                    "record\t"
                        + index
                        + "\t"
                        + units(record.getName())
                        + "\t"
                        + units(symbol)
                        + "\t"
                        + cursor.getEventFlags()
                        + "\t"
                        + cursor.getEventTimeSequence(),
                    counts);
                for (Field field : records.get(record.getName())) {
                  int slot = field.bound.getIndex();
                  long raw =
                      field.type.getId() == 10
                          ? 0
                          : field.type.isLong() ? cursor.getLong(slot) : cursor.getInt(slot);
                  String text = field.type.getId() == 10 ? units(cursor.getObj(slot)) : "null";
                  append(
                      actual,
                      "value\t"
                          + index
                          + "\t"
                          + field.logicalId
                          + "\t"
                          + field.type.getId()
                          + "\t"
                          + raw
                          + "\t"
                          + text,
                      counts);
                }
              }
            }

            @Override
            public void handleCorruptedStream() {
              counts[1]++;
            }

            @Override
            public void handleCorruptedMessage(int type) {
              counts[1]++;
            }
          });
    } finally {
      reader.close();
    }
    Files.write(Paths.get(args[1]), actual, StandardCharsets.UTF_8);
    require(counts[1] == 0, "production parser reported corruption");
    List<String> expected = readLines(root.resolve("expected.tsv"));
    require(expected.equals(actual), "production STREAM projection differs from C++ expectation");
    System.out.println(
        "VERIFY COMPLETE: records="
            + counts[0]
            + " errors="
            + counts[1]
            + " callbacks="
            + counts[2]
            + " rows="
            + actual.size());
  }

  private TapeWriterInteropOracle() {}
}
