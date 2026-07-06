# xlate09 — M6809 to M68000 Assembly Source Translator

Translates Motorola M6809 assembly source into M68000 assembly source.
Produces an output source file and an error list file.

Original Pascal implementation by Mike Catherwood, Motorola Ltd., East Kilbride.
Copyright Motorola Inc. 1986. C11 port 2026.

---

# Version

1.2.2

---

## Files

| File | Description |
|---|---|
| `xlate09.c` | Translator source (C11) |
| `codes.dbb` | Opcode translation database 1 (grouped opcodes, operand modified) |
| `codes2.dbb` | Opcode translation database 2 (opcodes whose operand is passed through unchanged) |
| `stubxref.dbb` | Stub external reference declarations merged into output |
| `stub09.txt` | Stub subroutine source (must be linked with translated output) |
| `error.txt` | Error list generated on each run |
| `testfile.asc` | Example M6809 source input |
| `output.asc` | Reference M68000 output for `testfile.asc` |
| `manual.txt` | Original users guide |
| `DOS` | Original xlate09 files and DOS com file |

---

## Build

Requires a C11-capable compiler and GNU make.

```sh
make
```

This produces the `xlate09` binary.

---

## Usage

```
./xlate09 <infile> <outfile> [I]
```

| Argument | Description |
|---|---|
| `infile` | M6809 assembly source file |
| `outfile` | M68000 assembly output file (overwritten if it exists) |
| `I` | Optional. Interleave original M6809 source lines into output as comments |

The three database files (`codes.dbb`, `codes2.dbb`, `stubxref.dbb`) must be
present in the current working directory. The error list is always written to
`error.txt` in the current directory.

**Example:**

```sh
./xlate09 testfile.asc testfile-68k.asm
./xlate09 testfile.asc testfile-68k.asm I   # with interleave
```

---

## Test

```sh
make test
```

Runs the translator against `testfile.asc` and checks the output for
`** ERROR **` lines. The test input contains two deliberate invalid
instructions; the run is considered passing when exactly those two errors appear.

---

## Output format

Each translated source line is formatted as:

```
<label>    <instruction>          <comment>
```

Label field is 10 characters wide. If the instruction is under 20 characters
the comment is padded to column 33. Lines that overflow column 80 have their
comment continued on a following `*` line.

**Error output** (also in `error.txt`):

```
** ERROR ** <message>
 * <original source line>
```

**Warning output** (output file only):

```
** WARNING **                      <message>
```

---

## Register mapping

| M6809 | M68000 | Notes |
|---|---|---|
| A | D0 | |
| B | D1 | |
| D (A:B) | D0/D1 | split/merge via `..DIN`/`..DOUT` stubs |
| X | A0 | |
| Y | A1 | |
| PC (indirect) | A2 | temporary |
| — | A3 | stub temporary |
| DP | A4 | |
| U | A5 | |
| S | A6 | |

The stub routines referenced in the output (`..DIN`, `..DOUT`, `..DPR`,
`..DPW`, `..RTS`, `..JSR`, `..CTOX`) are defined in `stub09.txt` and must
be assembled and linked with the translated code.

---

## Translator directives

These lines may appear in the M6809 source to control translation:

| Directive | Effect |
|---|---|
| `**PASS` | Pass all subsequent lines to output unchanged |
| `**PASSOFF` | Resume normal translation |

---

## Database format

Both `codes.dbb` and `codes2.dbb` are plain text files with alternating lines:
odd lines are opcodes, even lines are translation templates.

A `*` prefix on an opcode (e.g. `*AND`) denotes a group entry that matches any
opcode whose stem equals the entry (e.g. `ANDA`, `ANDB`). The last character of
the matched opcode selects the register and size suffix.

Template tokens:

| Token | Substitution |
|---|---|
| `o` | Translated operand |
| `\` | Implied register number (`0`–`6`) |
| `.` | Size suffix (`.B` or `.W`) |
| `;` | Line split point — remainder becomes a new output line |
| `^` | Conditional split for register-group instructions |
| `p` | Flag: replace `(PC)` with `(A2)` and prepend `LEA.L 0(PC),A2` |
| `*…*` | Warning message passed to `** WARNING **` output |
