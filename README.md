# PL/M-80 for cREXX

A PL/M-80 compiler written in cREXX Level B. It translates a PL/M module into
cREXX assembly (`.rxas`), which `rxas` assembles and `rxvm` runs, together with
a small runtime library (`plmrt`).

    make                  # build build/plm.rxbin and build/plmrt.rxbin
    ./plmc hello.plm      # compile and run
    ./plmc -c hello.plm   # compile only
    make test             # run the programs in tests/

`CREXX_BIN` names the directory with `rxc`, `rxas` and `rxvm` if they are not on `PATH`.

## The machine

The program runs on a simulated machine with 65536 bytes of memory. Variables are
static, as in PL/M-80, each at a fixed address starting at 256. `BYTE` is 8 bits,
`ADDRESS` is 16 bits (two bytes, low byte first), and arithmetic wraps at the width
of its operands: BYTE with BYTE gives a BYTE, anything with an ADDRESS gives an
ADDRESS. `.X` is the address of `X`, `BASED` variables work through a pointer, and
`AT`, `MEMORY`, `MOVE` and `LENGTH`/`LAST`/`SIZE` are there.

## Language

`DECLARE` with `BYTE`, `ADDRESS`, arrays, `STRUCTURE`, `BASED`, `AT`, `INITIAL`, `DATA`,
`LITERALLY`; procedures and functions (`REENTRANT` for recursion); `IF`, `DO`, `DO WHILE`,
`DO CASE`, iterative `DO`, `GO TO`, `CALL`, `RETURN`, `HALT`; the operators
`+ - * / MOD AND OR XOR NOT = <> < <= > >=` `PLUS` `MINUS`; the built-ins `SHL SHR ROL ROR
HIGH LOW DOUBLE INPUT OUTPUT`.

PL/M has no input or output of its own. The compiler predeclares a few console
procedures: `CO(char)`, `CI` (a function), `PRINT(.string)` (ends at `$`), `PRNUM(n)`,
`PRHEX(n)`, `CRLF`, `EXIT(code)`, and port 1 of `INPUT` and `OUTPUT` is the console.

Not supported: `CARRY ZERO SIGN PARITY SCL SCR DEC STACKPTR`, `EXTERNAL` procedures,
nested procedures, `INITIAL` for structures.

## Manual

See [docs/PLM-User-Guide.docx](docs/PLM-User-Guide.docx) (34 pages): language, built-in procedures,
examples, how the compiler works, messages, restrictions.

## Layout

    plm.crexx     the compiler            plmrt.crexx   the run time library
    plmc          compile and run         Makefile      build and test
    examples/     example programs        tests/        tests with expected output
    docs/         user manual
