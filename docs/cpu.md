# IRX - A fantasy computer toolchain

Inspired by [uxn](https://100r.co/site/uxn.html) and its goal of 
devising a clean-slate computing stack for salvaging arbitrary hardware,
I thought I'd dip my toe into the waters with my own design.

Enter: irx - instructions, registers, execution.

irx is an 8-bit register machine with a 16-bit data address bus.
This means that irx can only address a maximum of 64 kilobytes of memory.

## Registers 

irx has a selection of 8-bit registers.

 * A, B, C, D, G, H - General purpose registers. These are paired for certain 
16-bit operations. The A register is used for accumulator operations.
 * E, External address register
 * SP - Stack Pointer, 8-bit.
 * F - Flags (Zero, Carry, Error)
 * IP - Instruction Pointer, 16-bit

Flags in the F register are arranged like this:

  7|6|5|4|3|2|1|0
  ---------------
  U|U|B|I|O|N|Z|C

0) Carry
1) Zero
2) Negative
3) Overflow
4) Interrupt-Enable
5) Break (software-interrupt)
6) Unused
7) Unused

The irx instruction set supports a data stack of 256 bytes. Subroutine 
calls use two-bytes and an interrupt consumes three. The stack grows 
downwards from memory address 0xFFFF.

## Instruction Set

irx's instruction set is still in development.

### 0x00 NOOP

Mnemonic: NOP
Opcode: 0x00

Performs no action

### 0x01 HALT

Mnemonic: HALT
Opcode: 0x01

fffr0000

0x00: NOP
0x01: SYS (HALT, DATA_IN, DATA_OUT, CLEAR_INT, RET, RETI)
0x02: CLF (all flags)
0x03: SEF (all flags)

0x04: STK(Push/pop vars)
0x05: STK2(Push/pop vars)
0x06: COPY_IN(All registers)
0x07: COPY_OUT(All registers)

0x08: INC (All registers)
0x09: DEC (All registers)
0x0A: RTL (All registers)
0x0B: RTR (All registers)
0x0C: SHL (All registers)
0x0D: SHR (All registers)
0x0E: NOT (All registers)
0x0F: AND (All registers)
0x10: OR (All registers)
0x11: XOR (All registers)
0x12: ADD (All registers)
0x13: SUB (All registers)
0x14: MUL (All registers)
0x15: CMP (All registers)

0x16: ??
0x17: ?? (Reserve)

0x18: SET (All registers, immediate)
0x19: SWAP (All registers)
0x1A: LOAD_I (All registers, immediate location)
0x1B: STORE_I (All registers, immediate location)

0x1C: LOAD_R (field, extra byte for src)
0x1D: STORE_R (field, extra byte for dest?)
0x1E: JMP (immediate, pair-wise direct, call, pair-wise)
0x1F: BRCH (4 flags, positive and negative)







