# Instrument-profile value contract {#instrument-profile-value}

[Русская версия](PROFILE.ru.md). English is authoritative.

This independent value model describes instrument-profile fields, not the separate market-data
Profile event. The reference is Java QD 3.355 revision
`0ddc0a09779fb61d74cc0b37c6449ff47dcb715f`, specifically
[InstrumentProfile.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfile.java),
[InstrumentProfileField.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileField.java) and
[InstrumentProfileType.java](https://github.com/devexperts/QD/blob/0ddc0a09779fb61d74cc0b37c6449ff47dcb715f/dxfeed-api/src/main/java/com/dxfeed/ipf/InstrumentProfileType.java).
[Captured observations](../../tests/fixtures/ipf/README.md) bind the conversion cases to these sources.

## Fields and identity

`standardFields()` enumerates all 31 exact names and their text/integer/number/date categories.
ICB/SIC are signed32 integers; MULTIPLIER/SPC/STRIKE are binary64; EXPIRATION/LAST_TRADE
are signed32 day IDs. Other standard fields are strings. All have named typed accessors,
including `getSymbol()/setSymbol()`, `getIcb()/setIcb()` and
`getExpiration()/setExpiration()`. Generic operations accept standard or custom names.
`standardTypes()` lists 17 known types for convenience; arbitrary TYPE strings remain supported.

Names and values are owned exact UTF-16 code units, including embedded zero and unpaired
surrogates. Names are case-sensitive: `SYMBOL` is standard and `symbol` is custom. Empty custom
names are supported. Missing/custom-empty/null text reads as empty; null setters normalize to
empty. Empty custom edits erase storage, and enumeration contains only nonempty custom values.
Raw parser presence is not part of the public value.

`sameSymbol()` compares exact symbol units, including empty symbols. Full equality includes all
standard and nonempty custom data, ignores configured limits, treats NaNs as equal and signed
zeros as distinct. Presentation order compares type, product, underlying, last-trade day, strike,
then symbol. Known types precede unknown types, ordered lexically. NaN strike compares neither
less nor greater. Presentation equivalence therefore does not establish full value equality or
business identity.

## Numeric conversions

Empty/null standard numeric values become zero. Standard textual fields reject numeric/date
access with `TEXTUAL_FIELD`. Custom missing numeric/date data returns zero. Custom numeric reads
parse a date only for exactly 10 units with hyphens at positions 4 and 7; other values parse as
numbers. Custom date reads always parse dates. Custom text is preserved by ordinary text edits.

Number parsing accepts ASCII control/space trimming, signed decimal/exponent forms, Java numeric
suffixes, hexadecimal floating-point forms with a binary exponent, `NaN` and `Infinity`.
Invalid forms return `INVALID_NUMBER`; overflow becomes infinity and underflow signed zero.
ICB/SIC and numeric/date-to-int conversions truncate toward zero, saturate at signed32 bounds,
and map NaN to zero. Typed numeric fields preserve binary64, including signed zero and nonfinite data.

Canonical number text maps both zeros to empty, emits Java saturated integral representations
when the binary64 value compares equal to that integer, and uses ungrouped US decimal text for
absolute values strictly between 1e-9 and 1e12. Fractional decimal output has at most 20 digits,
with half-even rounding of the actual binary64 value when its already rounded shortest text lands
on a decimal tie. Other values use uppercase scientific notation with a fractional digit;
nonfinite values use `NaN`, `Infinity`, `-Infinity`. Compatibility includes old-JDK boundaries such
as 1e23 becoming `9.999999999999999E22`, and the smallest subnormal becoming `4.9E-324`.

The independent implementation uses fixed 1536-bit integer arithmetic for exact rational digits
and binary rounding intervals. It does not invoke Java, use ambient locales, cache global data,
or include OpenJDK implementation code. The initial decimal exponent uses the historical first-order log10 estimate around normalized
mantissa1.5; compatibility also retains defined signed32/64 wrap effects in small-operand
rounding comparisons. C++ uses unsigned modulo arithmetic for these effects. Scaling is bounded by binary64 exponent/significand
ranges, at most 20 generated digits and fixed-size arithmetic. This is a value-conversion contract,
not a claim about other Java decimal-format patterns.

## Calendar conversions

Day IDs count days from 1970-01-01 in GMT, independently of host timezone. Zero formats as empty.
Text parsing follows lenient `yyyy-MM-dd` prefix semantics: variable-width components, negative
components, leading space/tab, Unicode 6.2 BMP decimal digits and ignored trailing text.
`2024-02-30` becomes day 19783 (`2024-03-01`); `2024-01-02T23:00:00-09:00` remains day 19724.
A missing year/month/day component or wrong delimiter returns `INVALID_DATE`.

The calendar is Julian before 1582-10-15 and Gregorian afterward: 1582-10-04 is day-141428,
1582-10-15 is day-141427. An input year before 1582 keeps Julian arithmetic even when
overflowing day/month components carry the result after the cutover; 1581-12-400 is day-141336. Year zero normalizes to year 1 before the common era; formatted text
contains the positive year number without an era marker, as in the reference. Numeric date parts
retain Java NumberFormat signed64 parsing/int narrowing, month-minus-one signed32 wrap and Date
signed64 millisecond wrap before division/truncation and signed32 day narrowing. Extreme day IDs
remain representable: signed32 minimum/maximum format as `5877521-03-03`/`5881580-07-11`.

## Ownership, errors and limits

Copies own independent storage and limits; moves leave the source empty with its original limits.
Move is conditionally noexcept: actual empty backing construction, limits copy and every body swap
must be nonthrowing. An allocating map sentinel can make move preparation throw std::bad_alloc
before changing the source. Assignment constructs its by-value replacement before its nonthrowing
swap body, so failed preparation preserves both source and destination. Do not assume the same
nothrow trait across standard-library implementations; value layout and logical limits are unchanged.
Vectors may copy the old prefix when move can throw. Reader growth can temporarily retain both
old/candidate text; this remains bounded by twice the admitted logical output-unit envelope,
without turning logical limits into an allocator/RSS bound.
Getter text views borrow this profile and are invalidated by mutation, assignment or destruction.
Custom-name enumeration returns independent owned strings. Callers serialize mutation and
observation; simultaneous read-only use is supported. No callbacks or deferred work exist.

`std::expected` reports `INVALID_NUMBER`, `INVALID_DATE`, `TEXTUAL_FIELD` and `LIMIT_EXCEEDED`.
Failed edits preserve all values and counters. Allocation failures propagate `std::bad_alloc`,
also preserving an edited destination. Copy assignment constructs its replacement before swapping.

`ProfileLimits` defaults to 256 nonempty custom fields, 256 name units, 65536 units per retained
value, 1048576 aggregate units and 65536 input units per operation. These are configurable engineering
defaults, not feed quotas. Aggregate units count standard text plus custom names/values, including
canonical numeric/date text. Input limits are checked before copying/scanning caller value data;
output limits also apply to typed setters. Zero and size_t-maximum limits use checked accounting.
No process properties are read. Public custom names/values are not rejected merely for being unknown.

## Component boundary

The optional default-OFF `qdcxx_ipf` target has only the C++23 standard-library dependency and no
feed/core/transport dependency. It starts no I/O or background work. IPF CSV readers/writers,
HTTP, live collectors, option-chain construction and schedule interpretation are separate features.
See the [application guide](../../docs/INSTRUMENT-PROFILES.md).


## Nonallocating retained metadata

`customFieldCount()` reports actual nonempty custom entries, including a possible empty name.
`retainedUnits()` reports standard text plus custom names/values in UTF-16 code units, excluding
configuration, container capacity and allocator overhead. Both getters allocate nothing.
Copy/move/assignment/edit guarantees apply to these observations; a moved-from profile reports
empty storage, and a failed edit leaves counts unchanged. The [local writer](WRITER.md) uses this
metadata to check actual input/count bounds before allocating an independent custom-name copy.
