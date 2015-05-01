# Fermat
A compiler for CS445: Compiler Construction at Clarkson University.

## Dependencies
- a C compiler
- a lex equivalent (flex)
- a yacc equivalent (bison)
- tolerance for terrible error messages

Be sure to set CC, LEX, and YACC in the Makefile to match your appropriate utilities.

## Usage
Source text is inputted on `stdin` (likely to change in the future).
The optional `-v` flag can be used to display the scanning/parsing magic.
Provided the file is compileable, the output assembly will be in `loloutput.s` (compile with gcc).

## Design
Specifications for the assignment can be found [here](http://web2.clarkson.edu/class/cs445/445-s15.html).

Highlights:
- Symbol table consisting of a linked list of scopes, each containing a hash table
- Single-pass design, generates code from smaller trees rather than one large one
- Entry point functions that switch to the relevant ones based on type (so `gencode()` can be called everywhere rather than a specific one)
- `ID` now contains information about return type and its parameter list if it is a function/procedure
- Offset from the frame pointer is stored alongside the `ID` to simplify lookup and reference

## Known Bugs
This is a non-exhaustive list of the known problems:
- `IF THEN..ELSE` cannot be nested, as the labels will not be generated in the correct order
- Return values from functions are not always set properly
- Parameter lists into procedure/function calls are occaisionally parsed incorrectly, resulted in an erroneous syntax error
- Writing to global scope is broken, but reading works
- While loops apparently don't work in procedures?

Other limitations:
- Case 4 of `gencode()` is not implemented (limited to 6 registers)
- Compiler aborts at the first sign of struggle

Cool things:
- Spits out way more debug information than you possibly could want (some of it is even toggleable with `-v`)
- Can also generate almost working assembly for armv6 (Raspberry Pi). See `http://github.com/richteer/wiles` (free will)

## Haiku

```
I've slain the dragon
Now let us celebrate this
with Raspberry Pi
```

`http://www.github.com/richteer/fermat`

