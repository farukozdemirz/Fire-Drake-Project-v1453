#!/usr/bin/env python3
"""Read-only decoder for Knight Online client tables (Data/*.tbl).

File format (the same algorithm as tools/client-tbl-quests.py):
  The whole file is encrypted with a rolling XOR (key 0x0816, c1 0x6081, c2 0x1608):
      plain = enc ^ (key >> 8);  key = ((enc + key) * c1 + c2) & 0xFFFF
  The plain text is: int32 column count, int32 type per column, int32 row count,
  then the rows, one value per column, all little-endian:
      1 int8, 2 uint8, 3 int16, 4 uint16, 5 int32, 6 uint32,
      7 string (int32 byte length + CP949 bytes), 8 float32, 9 float64
  An unknown column type is an error. Bytes left after the last row are not an
  error: their count is reported in Table.trailing_bytes (the 1534 client's
  Quest_Menu_us.tbl has 56 such bytes).

Module use:
    kotbl.load(path) -> Table          kotbl.decode(raw_bytes) -> Table
    Table.types, Table.rows, Table.trailing_bytes, Table.bad_strings
    kotbl.encode(types, rows) -> bytes (encrypted; used by the self test)

CLI:
    python3 tools/kotbl.py info <tbl>
    python3 tools/kotbl.py dump <tbl> [--tsv]
    python3 tools/kotbl.py --selftest

The tool only reads the file it is given; it never writes to or runs anything
from a client folder. Standard library only; works under python3 -I.
"""

import argparse
import os
import signal
import struct
import sys

KEY0 = 0x0816
C1 = 0x6081
C2 = 0x1608

TYPE_STRING = 7
FIXED = {1: ("<b", 1), 2: ("<B", 1), 3: ("<h", 2), 4: ("<H", 2),
         5: ("<i", 4), 6: ("<I", 4), 8: ("<f", 4), 9: ("<d", 8)}
TYPE_NAMES = {1: "int8", 2: "uint8", 3: "int16", 4: "uint16", 5: "int32",
              6: "uint32", 7: "string", 8: "float32", 9: "float64"}
INTEGER_TYPES = (1, 2, 3, 4, 5, 6)
MAX_COLUMNS = 1024
STRING_ENCODING = "cp949"


class TableError(ValueError):
    """The data is not a valid client table."""


class Table(object):
    """A decoded client table.

    types          column type codes (see TYPE_NAMES)
    rows           list of rows; each row is a list with one value per column
    size           size of the decrypted data in bytes
    end_offset     offset just after the last row
    trailing_bytes size - end_offset (0 for a clean table)
    bad_strings    strings that were not valid CP949 (decoded with U+FFFD)
    """

    def __init__(self, types, rows, size, end_offset, bad_strings=0):
        self.types = types
        self.rows = rows
        self.size = size
        self.end_offset = end_offset
        self.trailing_bytes = size - end_offset
        self.bad_strings = bad_strings

    @property
    def columns(self):
        return len(self.types)

    def type_names(self):
        return [TYPE_NAMES[t] for t in self.types]


def decrypt(data):
    key = KEY0
    out = bytearray(len(data))
    for i, b in enumerate(data):
        out[i] = b ^ (key >> 8)
        key = ((b + key) * C1 + C2) & 0xFFFF
    return bytes(out)


def encrypt(plain):
    key = KEY0
    out = bytearray(len(plain))
    for i, p in enumerate(plain):
        e = p ^ (key >> 8)
        out[i] = e
        key = ((e + key) * C1 + C2) & 0xFFFF
    return bytes(out)


def parse(plain):
    """Parses decrypted table data. Raises TableError on a malformed table."""
    size = len(plain)
    if size < 12:
        raise TableError("data too short for a table header (%d bytes)" % size)
    (ncol,) = struct.unpack_from("<i", plain, 0)
    if ncol <= 0 or ncol > MAX_COLUMNS:
        raise TableError("bad column count %d" % ncol)
    offset = 4
    if offset + 4 * ncol + 4 > size:
        raise TableError("truncated header: %d columns need %d bytes, %d left"
                         % (ncol, 4 * ncol + 4, size - offset))
    types = list(struct.unpack_from("<%di" % ncol, plain, offset))
    offset += 4 * ncol
    for col, t in enumerate(types):
        if t != TYPE_STRING and t not in FIXED:
            raise TableError("unknown column type %d at column %d" % (t, col))
    (nrow,) = struct.unpack_from("<i", plain, offset)
    offset += 4
    min_row = sum(4 if t == TYPE_STRING else FIXED[t][1] for t in types)
    if nrow < 0 or nrow * min_row > size - offset:
        raise TableError("bad row count %d (%d bytes left, a row needs at least %d)"
                         % (nrow, size - offset, min_row))
    rows = []
    bad_strings = 0
    row_index = col = 0
    try:
        for row_index in range(nrow):
            row = []
            for col, t in enumerate(types):
                if t == TYPE_STRING:
                    (length,) = struct.unpack_from("<i", plain, offset)
                    offset += 4
                    if length < 0 or offset + length > size:
                        raise TableError("bad string length %d at row %d column %d"
                                         % (length, row_index, col))
                    raw = plain[offset:offset + length]
                    offset += length
                    try:
                        text = raw.decode(STRING_ENCODING)
                    except UnicodeDecodeError:
                        text = raw.decode(STRING_ENCODING, errors="replace")
                        bad_strings += 1
                    row.append(text)
                else:
                    fmt, width = FIXED[t]
                    row.append(struct.unpack_from(fmt, plain, offset)[0])
                    offset += width
            rows.append(row)
    except struct.error:
        raise TableError("truncated table at row %d column %d (offset %d of %d)"
                         % (row_index, col, offset, size))
    return Table(types, rows, size, offset, bad_strings)


def decode(data):
    """Decrypts and parses the raw bytes of a client .tbl file."""
    return parse(decrypt(data))


def load(path):
    with open(path, "rb") as f:
        return decode(f.read())


def build_plain(types, rows, trailing=b""):
    """Serializes a table without encryption (inverse of parse)."""
    out = bytearray(struct.pack("<i", len(types)))
    for t in types:
        if t != TYPE_STRING and t not in FIXED:
            raise TableError("unknown column type %d" % t)
        out += struct.pack("<i", t)
    out += struct.pack("<i", len(rows))
    for row in rows:
        if len(row) != len(types):
            raise TableError("row has %d values, table has %d columns" % (len(row), len(types)))
        for t, value in zip(types, row):
            if t == TYPE_STRING:
                raw = value.encode(STRING_ENCODING)
                out += struct.pack("<i", len(raw)) + raw
            else:
                out += struct.pack(FIXED[t][0], value)
    out += trailing
    return bytes(out)


def encode(types, rows, trailing=b""):
    """Builds an encrypted client table (the format decode() reads)."""
    return encrypt(build_plain(types, rows, trailing))


def format_value(t, value):
    if t == TYPE_STRING:
        return (value.replace("\\", "\\\\").replace("\t", "\\t")
                .replace("\r", "\\r").replace("\n", "\\n"))
    if t == 8:
        return "%.9g" % value
    if t == 9:
        return repr(value)
    return str(value)


def info_lines(table, name):
    lines = [
        "file: %s" % name,
        "size: %d" % table.size,
        "columns: %d" % table.columns,
        "types: %s" % " ".join(str(t) for t in table.types),
        "type_names: %s" % " ".join(table.type_names()),
        "rows: %d" % len(table.rows),
        "trailing_bytes: %d" % table.trailing_bytes,
        "bad_strings: %d" % table.bad_strings,
    ]
    return lines


def dump_lines(table, tsv):
    if tsv:
        yield "\t".join("c%d:%s" % (i, TYPE_NAMES[t]) for i, t in enumerate(table.types))
        for row in table.rows:
            yield "\t".join(format_value(t, v) for t, v in zip(table.types, row))
    else:
        for i, row in enumerate(table.rows):
            yield "[%d] %s" % (i, " | ".join(format_value(t, v) for t, v in zip(table.types, row)))


class SelftestError(Exception):
    pass


def _check(condition, message):
    if not condition:
        raise SelftestError(message)


def _expect_error(data, needle):
    try:
        decode(data)
    except TableError as e:
        _check(needle in str(e), "error text %r does not contain %r" % (str(e), needle))
        return
    raise SelftestError("no error for %s" % needle)


def selftest():
    # Cipher: round trip over every byte value, and a fixed known-answer vector
    # (protects against a symmetric change of both directions).
    sample = bytes(range(256)) * 4
    _check(decrypt(encrypt(sample)) == sample, "cipher round trip")
    _check(encrypt(b"KOTBL\x00\x01\xff").hex() == SELFTEST_VECTOR, "cipher known-answer vector")

    # Every column type, with the limits of each integer type, an empty string,
    # ASCII text, Korean CP949 text and exactly representable float32 values.
    korean = "\uae30\ubcf8\ub9dd\ud1a0"  # "basic cape" in Korean
    types = [1, 2, 3, 4, 5, 6, 7, 8, 9]
    rows = [
        [-128, 0, -32768, 0, -2147483648, 0, "", -1.5, -2.25],
        [127, 255, 32767, 65535, 2147483647, 4294967295, korean, 0.15625, 1e300],
        [0, 7, 1, 2, 3, 4, "pattern01light green", 3.0, -0.1],
    ]
    raw = encode(types, rows)
    plain = build_plain(types, rows)
    _check(raw != plain and len(raw) == len(plain), "encode must encrypt")
    _check(korean.encode(STRING_ENCODING) in plain, "CP949 bytes in the plain table")
    table = decode(raw)
    _check(table.types == types, "types round trip")
    _check(table.rows == rows, "rows round trip")
    _check(table.trailing_bytes == 0 and table.bad_strings == 0, "clean table")
    _check(table.end_offset == table.size == len(raw), "end offset")

    # A table with several string columns and no rows.
    empty = decode(encode([7, 6, 7], []))
    _check(empty.types == [7, 6, 7] and empty.rows == [] and empty.trailing_bytes == 0, "empty table")

    # Trailing bytes are reported, not an error.
    table = decode(encode(types, rows, trailing=b"\x00" * 56))
    _check(table.rows == rows and table.trailing_bytes == 56, "trailing bytes")

    # Malformed tables are explicit errors.
    _expect_error(encrypt(struct.pack("<iii", 2, 6, 10) + struct.pack("<i", 0)), "unknown column type 10")
    _expect_error(encrypt(struct.pack("<iii", 0, 0, 0)), "bad column count")
    _expect_error(raw[:-3], "truncated table")
    _expect_error(encrypt(struct.pack("<iii", 1, 7, 1) + struct.pack("<i", 50) + b"abc"), "bad string length")
    _expect_error(encrypt(struct.pack("<iii", 1, 6, 5) + b"\x00" * 8), "bad row count")
    _expect_error(b"\x01\x02", "too short")

    # Text output escapes control characters and keeps float32 readable.
    _check(format_value(7, "a\tb\\c\n") == "a\\tb\\\\c\\n", "string escape")
    _check(format_value(8, struct.unpack("<f", struct.pack("<f", 0.1))[0]) == "0.100000001", "float32 text")
    lines = list(dump_lines(decode(encode([6, 7], [[1, korean]])), True))
    _check(lines == ["c0:uint32\tc1:string", "1\t" + korean], "tsv dump")
    return 0


SELFTEST_VECTOR = "43e5b0b8bc4846d2"


def _stdout_utf8():
    try:
        sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    except (AttributeError, ValueError):
        pass


def main(argv=None):
    if hasattr(signal, "SIGPIPE"):
        signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    ap = argparse.ArgumentParser(description="Read-only decoder for Knight Online client .tbl files.")
    ap.add_argument("--selftest", action="store_true", help="run the built-in self test (no files needed)")
    sub = ap.add_subparsers(dest="command")
    p_info = sub.add_parser("info", help="column count/types, row count, trailing bytes")
    p_info.add_argument("tbl")
    p_dump = sub.add_parser("dump", help="print every row as UTF-8 text")
    p_dump.add_argument("tbl")
    p_dump.add_argument("--tsv", action="store_true", help="tab separated, with a header line")
    args = ap.parse_args(argv)

    if args.selftest:
        try:
            selftest()
        except (SelftestError, TableError) as e:
            print("selftest FAILED: %s" % e)
            return 1
        print("selftest OK")
        return 0
    if args.command is None:
        ap.print_help()
        return 2

    _stdout_utf8()
    try:
        table = load(args.tbl)
    except (OSError, TableError) as e:
        sys.stderr.write("error: %s: %s\n" % (args.tbl, e))
        return 1
    if args.command == "info":
        for line in info_lines(table, os.path.basename(args.tbl)):
            print(line)
    else:
        for line in dump_lines(table, args.tsv):
            print(line)
    if table.trailing_bytes:
        sys.stderr.write("warning: %d trailing bytes after the last row\n" % table.trailing_bytes)
    if table.bad_strings:
        sys.stderr.write("warning: %d strings are not valid CP949\n" % table.bad_strings)
    return 0


if __name__ == "__main__":
    sys.exit(main())
