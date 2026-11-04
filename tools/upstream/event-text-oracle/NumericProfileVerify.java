// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
package qdcxx.oracle;

import java.io.BufferedWriter;
import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

/**
 * Internal caller: Invoke-Oracle.ps1 with Java25 and three filenames. Inputs: complete legacy and
 * modern UTC captures. Output: exact per-case version-difference proof. Unexpected row ordering,
 * non-double differences, round-trip failure or decimal-rule violations fail.
 */
public final class NumericProfileVerify {
  public static void main(String[] args) throws Exception {
    if (args.length != 3) {
      throw new IllegalArgumentException(
          "Legacy capture, modern capture and proof output required");
    }
    List<String> legacy = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
    List<String> modern = Files.readAllLines(Paths.get(args[1]), StandardCharsets.UTF_8);
    if (legacy.size() != modern.size()) {
      throw new AssertionError("Row counts differ");
    }
    int differences = 0;
    try (BufferedWriter writer =
        Files.newBufferedWriter(Paths.get(args[2]), StandardCharsets.UTF_8)) {
      for (int i = 0; i < legacy.size(); ++i) {
        if (legacy.get(i).equals(modern.get(i))) {
          continue;
        }
        String[] oldRow = legacy.get(i).split("\t", -1);
        String[] newRow = modern.get(i).split("\t", -1);
        if (!oldRow[0].equals("double")
            || !newRow[0].equals("double")
            || !oldRow[1].equals(newRow[1])) {
          throw new AssertionError("Unexpected non-double or input difference at " + i);
        }
        long bits = Long.parseUnsignedLong(oldRow[1], 16);
        double value = Double.longBitsToDouble(bits);
        if (Double.doubleToRawLongBits(Double.parseDouble(oldRow[2])) != bits
            || Double.doubleToRawLongBits(Double.parseDouble(newRow[2])) != bits
            || !Double.toString(value).equals(newRow[2])) {
          throw new AssertionError("Exact binary64/current-spec oracle mismatch at " + i);
        }
        BigDecimal exact = new BigDecimal(value);
        BigDecimal oldValue = new BigDecimal(oldRow[2]);
        BigDecimal newValue = new BigDecimal(newRow[2]);
        int oldLength = oldValue.stripTrailingZeros().precision();
        int newLength = newValue.stripTrailingZeros().precision();
        BigDecimal oldDistance = exact.subtract(oldValue).abs();
        BigDecimal newDistance = exact.subtract(newValue).abs();
        String classification;
        if (newLength < oldLength) {
          classification = "shorter-roundtrip-decimal";
        } else if ((oldLength == 1 && newLength <= 2) || newLength == oldLength) {
          if (newDistance.compareTo(oldDistance) >= 0) {
            throw new AssertionError("Modern decimal is not strictly closer at " + i);
          }
          classification = oldLength == 1 ? "closest-length-one-or-two" : "closer-same-length";
        } else {
          throw new AssertionError("Unexpected decimal selection at " + i);
        }
        writer.write(
            oldRow[1]
                + "\t"
                + oldRow[2]
                + "\t"
                + newRow[2]
                + "\t"
                + oldLength
                + "\t"
                + newLength
                + "\t"
                + classification
                + "\t"
                + oldDistance.toString()
                + "\t"
                + newDistance.toString()
                + "\n");
        ++differences;
      }
    }
    if (differences < 13) {
      throw new AssertionError("Initial thirteen version-change regressions are missing");
    }
    System.out.println(
        "Classified " + differences + " exact JDK8/JDK25 numeric version differences");
  }
}
