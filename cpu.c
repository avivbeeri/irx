#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
   irx cpu core
   (instructions, registers, execution)
   */

#define STACK_SIZE 256
#define MEMORY_SIZE (64 * 1024)

typedef struct CPU_t {
  bool running;

  // General purpose registers
  union {
    uint8_t registers[8];
    struct {
      uint8_t a;
      uint8_t b;
      uint8_t c;
      uint8_t d;
      uint8_t g;
      uint8_t h;

      // Special registers
      uint8_t e;
      uint8_t f;
    };
  };

  uint16_t ip;
  uint8_t sp; // stack pointer
  uint8_t wp; // write-protect register (can be used for locking memory)
  uint8_t memory[MEMORY_SIZE];

} CPU;

typedef enum {
  NOOP = 0,
  HALT,
  SET,
  ADD,
  SUB,
  MUL,
  IMUL,
  DIV,
  MOD,
  AND,
  OR,
  XOR,
  NOT,
  SWAP,
  LOAD_I,
  STORE_I,
  LOAD_IR,
  STORE_IR,

  // Copy takes up multiple opcodes
  COPY_IN = 0x10, // to A
  COPY_OUT = 0x11, // from A

  // Control flow
  JMP,
  BRCH,
  CMP, // compare?

  CLF, // clear flag
  SEF, // set flag
  INC,
  DEC,
  RTL, // rotate left
  RTR, // rotate right

  PUSH,
  POP,
  CALL,
  RET
} OP;

// Based on 6502 format
typedef enum {
  FLAG_C = 1, // Carry
  FLAG_Z = 2, // Zero
  FLAG_I = 4, // Interrupt
  FLAG_WP = 8, // Write-protect-enable
  FLAG_BRK = 16, // Break (software interrupt)
  FLAG_U = 32, // unused
  FLAG_N = 64, // Negative
  FLAG_O = 128, // Overflow
} FLAG;

#define OPZ(opcode) opcode
#define OP(opcode, flag) (flag << 5) | opcode

uint8_t CPU_fetch(CPU* cpu) {
  uint8_t data = cpu->memory[cpu->ip++];
  return data;
}

bool isBitSet(uint16_t value, uint8_t position) {
  return (value & (1 << position)) != 0;
}

void CPU_execute(CPU* cpu, uint8_t opcode, uint8_t field) {
  switch (opcode) {
    case COPY_IN:
        // A->A
        // B->A
        // C->A
        // D->A
        // G->A
        // H->A
      {
        cpu->registers[0] = cpu->registers[field];
      }
      break;
    case COPY_OUT:
      // register to register
      {
        // A->A
        // A->B
        // A->C
        // A->D
        // A->G
        // A->H
        cpu->registers[field] = cpu->registers[0];
      }
      break;

    case RTL:
      {
        uint8_t value = cpu->registers[field];
        uint8_t i = value << 1;
        uint8_t j = value >> 7;
        if (((value >> 8) & 0x01) == 1) {
          cpu->f |= FLAG_C;
        }
        cpu->registers[field] = i | j;
      }
      break;
    case RTR:
      {
        uint8_t value = cpu->registers[field];
        uint8_t i = value >> 1;
        uint8_t j = value << 7;
        if ((value & 0x01) == 1) {
          cpu->f |= FLAG_C;
        }
        cpu->registers[field] = i | j;
      }
      break;
    case CLF:
      {
        cpu->f ^= (1 << field);
      }
      break;
    case SEF:
      {
        cpu->f |= (1 << field);
      }
      break;
    case JMP:
      {
        // Immediate
        // relative
        // direct
        // conditionals?
        switch(field) {
          case 0:
            {
              uint8_t lo = CPU_fetch(cpu);
              uint8_t hi = CPU_fetch(cpu);
              uint16_t addr = (hi << 8) | lo;
              cpu->ip = addr;
            }
            break;
            // pair 16-bit mode
          case 1:
          case 2:
          case 3:
            {
              uint8_t pair = (field - 1)*2;
              uint8_t lo = cpu->registers[pair];
              uint8_t hi = cpu->registers[pair+1];
              uint16_t addr = (hi << 8) | lo;
              cpu->ip = addr;
            }
            break;
        }
      }
      break;
    case CMP:
      {
        uint16_t result = cpu->a - cpu->registers[field];
        if (result == 0) {
          cpu->f |= FLAG_Z;
        }
        if (((uint8_t)(result >> 8) & 0x01) == 1) {
          cpu->f |= FLAG_C;
        }
      }
      break;
    case BRCH:
      {
        // Immediate
        // relative
        // direct
        // conditionals?
        uint8_t lo = CPU_fetch(cpu);
        uint8_t hi = CPU_fetch(cpu);
        uint16_t addr = (hi << 8) | lo;
        switch(field) {
          case 0: // check Z flag set
            {
              if ((cpu->f & FLAG_Z) != 0) {
                cpu->ip = addr;
              }
            }
            break;
          case 1: // check Z flag clear
            {
              if ((cpu->f & FLAG_Z) == 0) {
                cpu->ip = addr;
              }
            }
            break;
          case 2: // check N flag
            {
              if ((cpu->f & FLAG_N) != 0) {
                cpu->ip = addr;
              }
            }
            break;
          case 3: // check N flag
            {
              if ((cpu->f & FLAG_N) == 0) {
                cpu->ip = addr;
              }
            }
            break;
          case 4: // check C flag
            {
              if ((cpu->f & FLAG_C) != 0) {
                cpu->ip = addr;
              }
            }
            break;
          case 5: // check C flag
            {
              if ((cpu->f & FLAG_C) == 0) {
                cpu->ip = addr;
              }
            }
            break;
          case 6: // check O flag
            {
              if ((cpu->f & FLAG_O) != 0) {
                cpu->ip = addr;
              }
            }
            break;
          case 7: // check O flag
            {
              if ((cpu->f & FLAG_O) == 0) {
                cpu->ip = addr;
              }
            }
            break;
        }
      }
      break;
    case STORE_I:
      // register to memory - operand
      {
        uint8_t lo = CPU_fetch(cpu);
        uint8_t hi = CPU_fetch(cpu);
        uint16_t addr = (hi << 8) | lo;
        cpu->memory[addr] = cpu->registers[field];
      }
      break;
    case LOAD_I:
      // memory (operand) to register
      // addressing
      {
        uint8_t lo = CPU_fetch(cpu);
        uint8_t hi = CPU_fetch(cpu);
        uint16_t addr = (hi << 8) | lo;
        cpu->registers[field] = cpu->memory[addr];
      }
      break;
    case SWAP:
      {
        uint8_t swap = cpu->registers[field];
        uint8_t start = 2 * field;
        cpu->registers[start] = cpu->registers[start + 1];
        cpu->registers[start + 1] = swap;
      }
      break;
    case SET:
      {
        uint8_t value = CPU_fetch(cpu);
        cpu->registers[field] = value;
      }
      break;
    case DEC:
      cpu->registers[field] -= 1;
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      }
      break;
    case INC:
      cpu->registers[field] += 1;
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      }
      break;
    case ADD:
      {
        uint8_t a = cpu->a;
        uint8_t b = cpu->registers[field];
        uint8_t carry = ((cpu->f & FLAG_C) != 0);
        uint16_t result = a + b + carry;
        cpu->a = result;
        if (cpu->a == 0) {
          cpu->f |= FLAG_Z;
        } else {
          cpu->f &= ~FLAG_Z;
        }

        if (isBitSet((~(a ^ b) & (a ^ result) & 0x80), 7)) {
          cpu->f |= FLAG_O;
        } else {
          cpu->f &= ~FLAG_O;
        }

        if (isBitSet(result, 8)) {
          cpu->f |= FLAG_C;
        } else {
          cpu->f &= ~FLAG_C;
        }

        if (isBitSet(result, 7)) {
          cpu->f |= FLAG_N;
        } else {
          cpu->f &= ~FLAG_N;
        }
      }
      break;
    case SUB:
      {
        uint8_t a = cpu->a;
        uint8_t b = cpu->registers[field];
        uint16_t carry = ((cpu->f & FLAG_C) != 0);
        int16_t result = a - (b+carry);
        cpu->a = result;
        if (cpu->a == 0) {
          cpu->f |= FLAG_Z;
        } else {
          cpu->f &= ~FLAG_Z;
        }

        if (isBitSet(((a ^ b) & (a ^ result) & 0x80), 7)) {
          cpu->f |= FLAG_O;
        } else {
          cpu->f &= ~FLAG_O;
        }

        if (result < 0) {
          cpu->f |= FLAG_C;
        } else {
          cpu->f &= ~FLAG_C;
        }

        if (isBitSet(result, 7)) {
          cpu->f |= FLAG_N;
        } else {
          cpu->f &= ~FLAG_N;
        }
      }
      break;
    case IMUL:
      {
        int8_t a = cpu->a;
        int8_t b = cpu->registers[field];
        int16_t result = a * b;

        cpu->a = result & 0xFF;
        cpu->b = (result & 0xFF00) >> 8;

        if (isBitSet((~(a ^ b) & (a ^ result) & 0x80), 7)) {
          cpu->f |= FLAG_O;
        } else {
          cpu->f &= ~FLAG_O;
        }
        if (result == 0) {
          cpu->f |= FLAG_Z;
        } else {
          cpu->f &= ~FLAG_Z;
        }
        if (result < 0) {
          cpu->f |= FLAG_N;
        } else {
          cpu->f &= ~FLAG_N;
        }
      }
      break;
    case MUL:
      {
        uint8_t a = cpu->a;
        uint8_t b = cpu->registers[field];
        uint16_t result = a * b;

        cpu->a = result & 0xFF;
        cpu->b = (result & 0xFF00) >> 8;

        if (isBitSet((~(a ^ b) & (a ^ result) & 0x80), 7)) {
          cpu->f |= FLAG_O;
        } else {
          cpu->f &= ~FLAG_O;
        }
        if (result == 0) {
          cpu->f |= FLAG_Z;
        } else {
          cpu->f &= ~FLAG_Z;
        }
      }
      break;
    case DIV:
      cpu->a /= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      }
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      } else {
        cpu->f &= ~FLAG_Z;
      }
      break;
    case MOD:
      cpu->a %= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      } else {
        cpu->f &= ~FLAG_Z;
      }
      break;
    case AND:
      cpu->a &= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      } else {
        cpu->f &= ~FLAG_Z;
      }
      break;
    case OR:
      cpu->a |= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      } else {
        cpu->f &= ~FLAG_Z;
      }
      break;
    case XOR:
      cpu->a ^= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      } else {
        cpu->f &= ~FLAG_Z;
      }
      break;
    case NOT:
      cpu->a = ~(cpu->a);
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      } else {
        cpu->f &= ~FLAG_Z;
      }
      break;
    case NOOP: break;
halt:
    case HALT:
    default: cpu->running = false;
  }
}

void CPU_init(CPU* cpu) {
  cpu->running = true;
  cpu->a = 0;
  cpu->b = 0;
  cpu->c = 0;
  cpu->d = 0;
  cpu->g = 0;
  cpu->h = 0;

  cpu->e = 0;
  cpu->f = 0x00;

  cpu->ip = 0;
  cpu->sp = 0;
  memset(cpu->memory, 0, MEMORY_SIZE);
}

void CPU_run(CPU* cpu) {
  // if halted
  while (cpu->running) {
    // fetch
    uint8_t instruction = CPU_fetch(cpu);
    // decode
    uint8_t opcode = instruction & 0x1F;
    uint8_t field = instruction >> 5;

    CPU_execute(cpu, opcode, field);
    // execute
  }
}


void CPU_dump(CPU* cpu) {
  printf("------ irx cpu dump ------\n\n");
  printf("# state\n");
  printf("Running: %s\n", cpu->running ? "true" : "false");
  printf("IP: 0x%04X\n", cpu->ip);
  printf("SP: 0x%04X\n", cpu->sp);
  printf("E: 0x%02X\t F: 0x%02X\n", cpu->e, cpu->f);

  printf("C:%i  Z:%i  I:%i  WP: %i\n", (cpu->f & FLAG_C) != 0, (cpu->f & FLAG_Z) != 0, (cpu->f & FLAG_I) != 0, (cpu->f & FLAG_WP) != 0);
  printf("O:%i  N:%i  U:%i  BRK:%i\n", (cpu->f & FLAG_O) != 0, (cpu->f & FLAG_N) != 0, (cpu->f & FLAG_U) != 0, (cpu->f & FLAG_BRK) != 0);

  printf("\n");
  printf("# registers\n\n");
  printf("A: 0x%02X\n", cpu->a);
  printf("B: 0x%02X\n", cpu->b);
  printf("C: 0x%02X\n", cpu->c);
  printf("D: 0x%02X\n", cpu->d);
  printf("G: 0x%02X\n", cpu->g);
  printf("H: 0x%02X\n", cpu->h);
  printf("\n");
  printf("--------------------------\n");
}

int main(int argc, char *argv[]) {
  CPU cpu;
  CPU_init(&cpu);

  /*
  uint8_t program[] = {
    OP(SET, 0), 4,
    OP(DEC, 0),
    OP(BRCH, 1), 2, 0, // Jump if zero
    OP(CLF, 0),
    OPZ(HALT)
  };
  */
  uint8_t program[] = {
    OP(SET, 0), 0xFF,
    OP(SET, 3), 0x02,
    OP(IMUL, 3),
    OPZ(HALT)
  };
  memcpy(cpu.memory, program, sizeof(program));

  CPU_run(&cpu);
  CPU_dump(&cpu);
  return 0;
}
