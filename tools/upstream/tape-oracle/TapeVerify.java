package qdcxx.oracle;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** Read-only, closed-profile artifact validator and exact raw-byte comparator. */
public final class TapeVerify {
  private static final List<String> PROFILES =
      Arrays.asList("default", "constrained", "text", "truncated");

  private static void require(boolean condition, String reason) {
    if (!condition) {
      throw new IllegalArgumentException(reason);
    }
  }

  private static long number(String value) {
    require(value.matches("0|-?[1-9][0-9]*"), "noncanonical integer: " + value);
    return Long.parseLong(value);
  }

  private static void units(String value) {
    require(value.matches("-|(?:[0-9a-f]{4})+"), "UTF-16 units");
  }

  private static void arity(String[] cells, int count) {
    require(cells.length == count, "arity: " + cells[0]);
    for (String cell : cells) {
      require(!cell.isEmpty(), "empty cell");
    }
  }

  private static List<String> lines(Path file) throws Exception {
    require(Files.size(file) > 0 && Files.size(file) <= 65536, "empty/oversized " + file);
    String text = new String(Files.readAllBytes(file), StandardCharsets.UTF_8);
    require(
        text.endsWith("\n") && !text.contains("\r") && !text.contains("\ufffd"), "text encoding");
    return Arrays.asList(text.substring(0, text.length() - 1).split("\n", -1));
  }

  private static Set<String> names() {
    Set<String> names = new TreeSet<>();
    for (String profile : PROFILES) {
      names.add(profile + (profile.equals("text") ? ".txt" : ".qtp"));
      names.add(profile + ".tsv");
      for (String suffix : Arrays.asList("stdout", "stderr", "exit")) {
        names.add(profile + "-inspect." + suffix);
      }
    }
    names.add("default-frames.tsv");
    names.add("constrained-frames.tsv");
    return names;
  }

  private static void observation(Path file, String profile) throws Exception {
    List<String> rows = lines(file);
    require(
        rows.get(0).equals("profile\t" + profile + "\tsingle-file\tMAX_SPEED\tauto"), "profile");
    require(rows.get(rows.size() - 1).equals("complete\t" + profile), "completion");
    int records = 0;
    int errors = 0;
    int heartbeats = 0;
    int descriptors = 0;
    int lastCallback = 0;
    Set<String> properties = new TreeSet<>();
    boolean ended = false;
    for (int index = 0; index < rows.size(); index++) {
      String[] c = rows.get(index).split("\t", -1);
      require(!ended || c[0].equals("complete"), "row after end");
      switch (c[0]) {
        case "profile":
          arity(c, 5);
          require(index == 0, "duplicate profile");
          break;
        case "property":
          arity(c, 3);
          units(c[1]);
          units(c[2]);
          require(records == 0 && properties.add(c[1]), "property order/duplicate");
          require(
              Arrays.asList(
                      "006f00700074",
                      "00740069006d0065",
                      "0074007900700065",
                      "00760065007200730069006f006e")
                  .contains(c[1]),
              "unknown property");
          break;
        case "descriptor":
          arity(c, 4);
          require(
              index == 1
                  && c[1].equals(
                      profile.equals("constrained") || profile.equals("truncated") ? "15" : "-")
                  && c[2].equals("-")
                  && c[3].equals("-"),
              "descriptor values");
          descriptors++;
          break;
        case "heartbeat":
          arity(c, 3);
          if (profile.equals("default")) {
            require(
                number(c[1]) == 1700000000123L + heartbeats * 1000L && c[2].equals("false"),
                "heartbeat value");
          } else {
            require(
                profile.equals("text") && c[1].equals("-") && c[2].equals("true"),
                "unexpected heartbeat");
          }
          heartbeats++;
          break;
        case "record":
          arity(c, 10);
          require(number(c[1]) == records && records < 7, "record index");
          int callback = (int) number(c[2]);
          require(callback >= lastCallback && callback <= lastCallback + 1, "callback sequence");
          require(number(c[3]) == 15 && number(c[8]) == 0, "type/flags");
          long id = number(c[4]);
          require(
              id >= 0 && id <= 2 && c[5].equals(new String[] {"0053", "0052", "0045"}[(int) id]),
              "local identity");
          units(c[6]);
          String[] values = c[7].equals("-") ? new String[0] : c[7].split(",", -1);
          require(values.length == new int[] {1, 3, 0}[(int) id], "local field arity");
          for (String value : values) {
            require(number(value) == Integer.parseInt(value), "int32 value");
          }
          require(number(c[9]) >= 0, "time sequence");
          lastCallback = callback;
          records++;
          break;
        case "error":
          arity(c, 3);
          require(
              profile.equals("truncated") && c[1].equals("stream") && c[2].equals("-"),
              "unexpected error");
          errors++;
          break;
        case "end":
          arity(c, 5);
          require(
              index == rows.size() - 2
                  && number(c[1]) == records
                  && number(c[2]) == lastCallback + 1
                  && number(c[3]) == errors
                  && c[4].equals("returned"),
              "end counts");
          ended = true;
          break;
        case "complete":
          arity(c, 2);
          require(ended && index == rows.size() - 1 && c[1].equals(profile), "complete order");
          break;
        default:
          throw new IllegalArgumentException("unknown observation row " + c[0]);
      }
    }
    require(
        descriptors == 1 && records == (profile.equals("truncated") ? 4 : 7),
        "missing deliveries/descriptor");
    require(errors == (profile.equals("truncated") ? 1 : 0), "error count");
    require(
        heartbeats == (profile.equals("default") ? 3 : profile.equals("text") ? 1 : 0),
        "heartbeat count");
    require(
        properties.size() == (profile.equals("default") || profile.equals("text") ? 4 : 3),
        "missing properties");
  }

  private static void frames(Path directory, String profile) throws Exception {
    List<String> rows = lines(directory.resolve(profile + "-frames.tsv"));
    int[] types =
        profile.equals("default")
            ? new int[] {1, 0, 2, 15, 0, 2, 15, 0, 2, 15, -1}
            : new int[] {1, 2, 15, 2, 15, 2, 15, -1};
    require(rows.size() == types.length + 1, "frame count");
    long end = 0;
    for (int i = 0; i < types.length; i++) {
      String[] c = rows.get(i).split("\t", -1);
      arity(c, 6);
      require(c[0].equals("frame") && number(c[1]) == i && number(c[2]) == end, "frame order");
      long next = number(c[3]);
      require(
          next > end && number(c[4]) == next - end - 1 && number(c[5]) == types[i], "frame values");
      end = next;
    }
    require(end == Files.size(directory.resolve(profile + ".qtp")), "envelope coverage");
    require(
        rows.get(types.length).equals("complete\tenvelopes\t" + types.length + "\t" + end),
        "envelope completion");
  }

  private static void inspector(Path directory, String profile) throws Exception {
    require(
        lines(directory.resolve(profile + "-inspect.exit")).equals(Arrays.asList("4")),
        "inspector exit");
    List<String> stdout = lines(directory.resolve(profile + "-inspect.stdout"));
    require(
        stdout.size() == 2 && stdout.get(0).equals("format\tqd-qtp-inspect\t1"),
        "inspector stdout");
    String[] frame = stdout.get(1).split("\t", -1);
    arity(frame, 7);
    require(
        frame[0].equals("frame") && frame[1].equals("0") && frame[2].equals("0"),
        "inspector frame");
    long end = number(frame[3]);
    require(number(frame[5]) == 2 && number(frame[6]) == end - 2, "inspector frame bounds");
    require(number(frame[4]) == (profile.equals("text") ? 61 : 1), "inspector type");
    List<String> stderr = lines(directory.resolve(profile + "-inspect.stderr"));
    require(stderr.size() == 1, "inspector stderr count");
    require(
        stderr.get(0).equals("incomplete\t4\ttype\t0\t" + frame[4] + "\t0\t2\t-\t-\t-\t-"),
        "inspector diagnostic");
  }

  private static void validate(Path directory) throws Exception {
    Set<String> found;
    try (Stream<Path> paths = Files.list(directory)) {
      found =
          paths.map(p -> p.getFileName().toString()).collect(Collectors.toCollection(TreeSet::new));
    }
    require(found.equals(names()), "artifact file set");
    for (String name : found) {
      Path file = directory.resolve(name);
      require(
          Files.isRegularFile(file)
              && !Files.isSymbolicLink(file)
              && Files.size(file) > 0
              && Files.size(file) <= 65536,
          "artifact size/type " + name);
    }
    for (String profile : PROFILES) {
      observation(directory.resolve(profile + ".tsv"), profile);
      inspector(directory, profile);
    }
    frames(directory, "default");
    frames(directory, "constrained");
    byte[] complete = Files.readAllBytes(directory.resolve("constrained.qtp"));
    require(
        Arrays.equals(
            Arrays.copyOf(complete, complete.length - 2),
            Files.readAllBytes(directory.resolve("truncated.qtp"))),
        "truncation derivation");
  }

  private static void compare(Path expected, Path actual) throws Exception {
    validate(expected);
    validate(actual);
    for (String name : names()) {
      require(
          Arrays.equals(
              Files.readAllBytes(expected.resolve(name)), Files.readAllBytes(actual.resolve(name))),
          "byte mismatch: " + name);
    }
  }

  private static void write(Path path, String text) throws Exception {
    Files.write(path, text.getBytes(StandardCharsets.UTF_8));
  }

  private static void negatives(Path expected, Path output) throws Exception {
    validate(expected);
    Files.createDirectory(output);
    List<String> cases =
        new ArrayList<>(
            Arrays.asList(
                "byte",
                "value",
                "empty",
                "missing",
                "extra",
                "unknown",
                "completion",
                "inspector-stdout",
                "inspector-stderr",
                "inspector-exit"));
    for (String row :
        Arrays.asList(
            "profile",
            "descriptor",
            "property",
            "heartbeat",
            "record",
            "end",
            "complete",
            "error",
            "frame")) {
      cases.add("arity-" + row);
    }
    for (String test : cases) {
      Path copy = output.resolve(test);
      Files.createDirectory(copy);
      for (String name : names()) {
        Files.copy(expected.resolve(name), copy.resolve(name));
      }
      Path observation = copy.resolve("default.tsv");
      String text = new String(Files.readAllBytes(observation), StandardCharsets.UTF_8);
      switch (test) {
        case "byte":
          byte[] bytes = Files.readAllBytes(copy.resolve("default.qtp"));
          bytes[bytes.length - 2] ^= 1;
          Files.write(copy.resolve("default.qtp"), bytes);
          break;
        case "value":
          require(text.contains("\t7\t0\t"), "mutation target");
          write(observation, text.replace("\t7\t0\t", "\t8\t0\t"));
          break;
        case "empty":
          write(observation, "");
          break;
        case "missing":
          Files.delete(observation);
          break;
        case "extra":
          write(copy.resolve("extra.tsv"), "extra\n");
          break;
        case "unknown":
          write(observation, text.replace("end\t", "unknown\t"));
          break;
        case "completion":
          write(observation, text.replace("complete\tdefault\n", ""));
          break;
        case "inspector-stdout":
        case "inspector-stderr":
        case "inspector-exit":
          write(copy.resolve("default-inspect." + test.substring(10)), "0\n");
          break;
        default:
          String row = test.substring(6);
          Path target =
              copy.resolve(
                  row.equals("error")
                      ? "truncated.tsv"
                      : row.equals("frame") ? "default-frames.tsv" : "default.tsv");
          String original = new String(Files.readAllBytes(target), StandardCharsets.UTF_8);
          require(original.contains(row + "\t"), "arity mutation target");
          write(target, original.replace(row + "\t", row + "\textra\t"));
      }
      boolean rejected = false;
      try {
        compare(expected, copy);
      } catch (Exception failure) {
        rejected = true;
        System.out.println("REJECT\t" + test + "\t" + failure.getMessage());
      }
      require(rejected, "mutation accepted: " + test);
    }
    compare(expected, expected);
    System.out.println(
        "PASS\t" + cases.size() + " mutations rejected; baseline passes; expectations unchanged");
  }

  public static void main(String[] args) throws Exception {
    require(args.length >= 2, "validate DIR | compare EXPECTED ACTUAL | negative EXPECTED NEW-DIR");
    if (args[0].equals("validate") && args.length == 2) {
      validate(Paths.get(args[1]));
    } else if (args[0].equals("compare") && args.length == 3) {
      compare(Paths.get(args[1]), Paths.get(args[2]));
    } else if (args[0].equals("negative") && args.length == 3) {
      negatives(Paths.get(args[1]), Paths.get(args[2]));
    } else {
      throw new IllegalArgumentException("arguments");
    }
    System.out.println("VERIFY PASS: " + args[0]);
  }
}
