# Glyph Jump & Call Reference

## Control Flow Instructions

| Op | Syntax | Action |
|----|--------|--------|
| `.` | `.a` | Jump: `PC = R(a)` |
| `;` | `;a` | Call: push PC, `PC = R(a)` |
| `,` | `,` | Return: `PC = pop()` |
| `?` | `?=bct` | Conditional: if `R(b)==R(c)` then `PC = R(t)` |

## The Challenge

All jumps require the target address **in a register**. You must:
1. Calculate the target byte offset
2. Load it into a register
3. Then jump/call

## Manual Relative Jump Technique

Since register `.` (dot) **is** the PC, you can compute relative jumps:

### Forward Jump Pattern (14 bytes)

```
:t..:oXX+tto.t
```

| Bytes | Instruction | Effect |
|-------|-------------|--------|
| `:t..` | Load t from PC | `t = current_address` |
| `:oXX` | Load offset | `o = offset_value` |
| `+tto` | Add | `t = t + o` |
| `.t` | Jump | `PC = t` |

**Offset calculation:** When `:t..` executes, PC points to byte +4 from start of pattern.
The `.t` at the end is at byte +12. To land N bytes after the pattern: `offset = N + 10`

### Backward Jump Pattern (14 bytes)

```
:t..:oXX-tto.t
```

Same structure but subtract. To jump back to address A when pattern starts at P:
`offset = (P + 4) - A`

## Offset Quick Reference

Using `:oxN` (hex digit) for small offsets:

| Hex | Value | Skip bytes after pattern |
|-----|-------|--------------------------|
| `:ox0` | 0 | -10 (backward) |
| `:oxa` | 10 | 0 (jump to end) |
| `:oxb` | 11 | 1 |
| `:oxc` | 12 | 2 |
| `:oxd` | 13 | 3 |
| `:oxe` | 14 | 4 |
| `:oxf` | 15 | 5 |

For larger offsets, use `:ogX` (literal byte) or two hex digits:

```
:hx1:lx0<hh4|ohl    ; h=0x10, l=0x00, shift h left 4, or with l → 0x100
```

## Conditional Jump Pattern

```
?=abT:t..:oXX+tto.t
```

If condition fails, PC advances past the `T` and continues.
If condition succeeds, PC is set to `R(T)` — so preload T with target.

### Alternative: Conditional Skip

To skip N bytes when condition is true:
```
:T..:sXX+TTs?=abT
```
Precompute target, then conditional jump skips forward.

## Absolute Jump (when you know the address)

For known addresses like `0x0100`:

```
:hx1:lx0<hh4|ohl.l   ; jump to 0x0100
```

Or for addresses ≤ 255:
```
:agX.a               ; where X is the literal byte value
```

## Subroutine Call Example

```
; At 0x0100: main code
:fx1:gx2:hx0<ff4|gfg;g    ; call subroutine at 0x0120
... continue ...

; At 0x0120: subroutine
... do work ...
,                          ; return
```

## Tips

1. **Use the address tool**: `glyph-addr program.glyph` shows all byte positions
2. **Reserve registers**: Pick dedicated registers for jump targets (e.g., `T`, `L`)
3. **Plan subroutine locations**: Put them at memorable addresses (0x0200, 0x0300)
4. **Comment your math**: Keep notes of offset calculations
