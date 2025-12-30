# Glyph

Single-header character-based VM in ~100 lines of C.

## Build

```bash
make        # builds example and test
./example   # prints "5 + 3 = 8"
./test      # runs tests
```

## Usage

```c
#define GLYPH_IMPL
#include "glyph.h"

uint8_t mem[256];
Glyph vm;

glyph_init(&vm, mem, sizeof(mem));
memcpy(mem, ":ad5 :bd3 +cab", 15);  // a=5, b=3, c=a+b
glyph_run(&vm);
// vm.reg['c'] == 8
```

## Instructions

| Op | Args | Description |
|----|------|-------------|
| `+` `-` `*` `/` `%` | abc | a = b op c |
| `&` `\|` `^` `<` `>` | abc | bitwise |
| `~` | ab | a = ~b |
| `:` | agX | load glyph X |
| `:` | axN | load hex N |
| `:` | a.b | copy reg |
| `@` | ab | a = mem[b] |
| `!` | ab | mem[a] = b |
| `(` | ab | a = port[b] |
| `)` | ab | port[a] = b |
| `.` | a | jump to a |
| `?` | =ab | skip if a==b |
| `?` | !ab | skip if a!=b |
| `;` | a | call |
| `,` | | return |
| `\0` | | halt |

Whitespace is ignored. 128 registers (indexed by char). 256 ports.
