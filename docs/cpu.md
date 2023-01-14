# IRX - A fantasy computer toolchain

Inspired by [uxn](https://100r.co/site/uxn.html) and its goal of 
devising a clean-slate computing stack for salvaging arbitrary hardware,
I thought I'd dip my toe into the waters with my own design.

Enter: irx - instructions, registers, execution.

irx is an 8-bit register machine with a 16-bit data address bus.
This means that irx can only address a maximum of 64 kilobytes of memory.

## Registers 

irx has a selection of 8-bit registers.

 * A, B, C, D, G, H - General purpose registers. These are paired for certain 16-bit operations. The A register is used for accumulator operations.
 * E, F - Extra, Flags (Zero, Carry, Error)
 * IP - Instruction Pointer, 16-bit
 * SP - Stack Pointer, 8-bit.


The E register is currently unused. F is where flags are stored.
The stack grows downwards from the end of the memory space. It is used
for subroutine execution.

## Instruction Set

irx's instruction set is still in development.
