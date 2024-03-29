#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
   irx cpu core
   (instructions, registers, execution)
   */

#define STACK_SIZE 256
#define POP_STACK(dest) do { cpu->sp--; dest = cpu->memory(READ, 0xFFFF - cpu->sp, 0); } while(0)
#define PUSH_STACK(src) do { cpu->memory(WRITE, 0xFFFF - cpu->sp, src); cpu->sp++; } while(0)
#define SET_IP(low, high) do { cpu->ip = ((high) << 8) | low; } while(0)

enum DIRECTION { READ, WRITE };
typedef uint8_t (*MEM_callback)(enum DIRECTION, uint16_t, uint8_t);
typedef uint8_t (*BUS_callback)(enum DIRECTION, uint8_t);
typedef struct BUS_t {
  BUS_callback callback[256];
} BUS;

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
      uint8_t sp; // stack pointer
    };
  };

  uint16_t ip;
  uint8_t f; // flags
  uint8_t i; // interupt status

  BUS bus;
  MEM_callback memory;
} CPU;

typedef enum {
  SYS = 0x00,
  JMP = 0x80,

  NOOP = 0x00,
  HALT = 0x01,
  DATA_IN = 0x02,
  DATA_OUT = 0x03,
  CLEAR_INT = 0x04,
  RET = 0x05,
  RETI = 0x06,
  SWAP = 0x07,


  CLF = 0x01,
  SEF = 0x81,

  PUSH = 0x02,
  POP = 0x82,

  COPY_IN = 0x03,
  COPY_OUT = 0x83,

  INC = 0x04,
  DEC = 0x84,

  RTL = 0x05,
  RTR = 0x85,

  SHL = 0x06,
  SHR = 0x86,

  LOAD_I = 0x07,
  LOAD_R = 0x87,

  STORE_I = 0x08,
  STORE_R = 0x88,

  U1 = 0x89,

  U2 = 0x0A,
  U3 = 0x8A,

  BRCH = 0x0B,
  SET = 0x8B,

  NOT = 0x0C,
  XOR = 0x8C,

  AND = 0x0D,
  OR = 0x8D,

  ADD = 0x0E,
  MUL = 0x8E,

  SUB = 0x0F,
  CMP = 0x8F,
} OP;

// Based on 6502 format
typedef enum {
  FLAG_C = 1, // Carry
  FLAG_Z = 2, // Zero
  FLAG_N = 4, // Negative
  FLAG_O = 8, // Overflow
  FLAG_I = 16, // Interrupt enabled
  FLAG_BRK = 32, // Break (software interrupt)
  FLAG_U2 = 64, // Write-protect-enable
  FLAG_U = 128, // unused
} FLAG;

#define OPZ(opcode) opcode
#define OP(opcode, flag) (opcode | (flag << 4))

uint8_t CPU_fetch(CPU* cpu) {
  uint8_t data = cpu->memory(READ, cpu->ip++, 0);
  return data;
}

bool isBitSet(uint16_t value, uint8_t position) {
  return (value & (1 << position)) != 0;
}

void CPU_writeData(CPU* cpu) {
  uint8_t addr = cpu->e;
  if (cpu->bus.callback[addr] != NULL) {
    cpu->bus.callback[addr](WRITE, cpu->a);
  }
}

void CPU_readData(CPU* cpu) {
  uint8_t addr = cpu->e;
  if (cpu->bus.callback[addr] == NULL) {
    cpu->a = 0;
  } else {
    cpu->a = cpu->bus.callback[addr](READ, 0);
  }
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

    case SHL:
      {
        uint8_t value = cpu->registers[field];
        if (((value >> 8) & 0x01) == 1) {
          cpu->f |= FLAG_C;
        }
        cpu->registers[field] = value << 1;
      }
      break;
    case SHR:
      {
        uint8_t value = cpu->registers[field];
        cpu->registers[field] = value >> 1;
        cpu->f &= ~FLAG_C;
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
        cpu->f &= ~(1 << field);
      }
      break;
    case SEF:
      {
        cpu->f |= (1 << field);
      }
      break;
    case JMP:
      {
        if (field & 0x4) {
          PUSH_STACK((cpu->ip >> 8));
          PUSH_STACK((uint8_t)(cpu->ip & 0x00FF));
        }
        uint8_t lo, hi;
        if ((field & 0x3) == 0x3) {
          lo = CPU_fetch(cpu);
          hi = CPU_fetch(cpu);
        } else {
          int pair = (field & 0x3) * 2;
          lo = cpu->registers[pair];
          hi = cpu->registers[pair+1];
        }
        uint16_t addr = (hi << 8) | lo;
        cpu->ip = addr;
      }
      break;
    case PUSH:
      {
        PUSH_STACK(cpu->registers[field]);
      }
      break;
    case POP:
      {
        POP_STACK(cpu->registers[field]);
      }
      break;
    case BRCH:
      {
        uint8_t lo = CPU_fetch(cpu);
        uint8_t hi = CPU_fetch(cpu);
        uint16_t addr = (hi << 8) | lo;
        uint8_t flag = (field / 2);
        uint8_t mode = (field % 2);
        if ((cpu->f & (1 << flag)) != (mode << flag)) {
          cpu->ip = addr;
        }

        /*
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
        */
      }
      break;
    case CMP:
      {
        uint8_t a = cpu->a;
        uint8_t b = cpu->registers[field];
        uint16_t carry = ((cpu->f & FLAG_C) != 0);
        int16_t result = a - (b+carry);
        if (result == 0) {
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
    case STORE_I:
      // register to memory - operand
      {
        uint8_t lo = CPU_fetch(cpu);
        uint8_t hi = CPU_fetch(cpu);
        uint16_t addr = (hi << 8) | lo;
        cpu->memory(WRITE, addr, cpu->registers[field]);
        cpu->f &= ~FLAG_Z;
        cpu->f &= ~FLAG_C;
        cpu->f &= ~FLAG_N;
        cpu->f &= ~FLAG_O;
      }
      break;
    case STORE_R:
      // Pick memory address from register pair
      // store register value to memory at address
      {
        uint8_t src = CPU_fetch(cpu);
        uint8_t pair = src * 2;
        uint8_t lo = cpu->registers[pair];
        uint8_t hi = cpu->registers[pair+1];
        uint16_t addr = (hi << 8) | lo;
        cpu->memory(WRITE, addr, cpu->registers[field]);

        cpu->f &= ~FLAG_Z;
        cpu->f &= ~FLAG_C;
        cpu->f &= ~FLAG_N;
        cpu->f &= ~FLAG_O;
      }
      break;
    case LOAD_I:
      // memory (operand) to register
      // addressing
      {
        uint8_t lo = CPU_fetch(cpu);
        uint8_t hi = CPU_fetch(cpu);
        uint16_t addr = (hi << 8) | lo;
        cpu->registers[field] = cpu->memory(READ, addr, 0);
        cpu->f &= ~FLAG_Z;
        cpu->f &= ~FLAG_C;
        cpu->f &= ~FLAG_N;
        cpu->f &= ~FLAG_O;
      }
      break;
    case LOAD_R:
      // Pick memory address from register pair
      // read the contents of address to register
      {
        uint8_t src = CPU_fetch(cpu);
        uint8_t pair = src * 2;
        uint8_t lo = cpu->registers[pair];
        uint8_t hi = cpu->registers[pair+1];
        uint16_t addr = (hi << 8) | lo;

        cpu->registers[field] = cpu->memory(READ, addr, 0);

        cpu->f &= ~FLAG_Z;
        cpu->f &= ~FLAG_C;
        cpu->f &= ~FLAG_N;
        cpu->f &= ~FLAG_O;
      }
      break;
    case SET:
      {
        uint8_t value = CPU_fetch(cpu);
        cpu->registers[field] = value;
        if (cpu->a == 0) {
          cpu->f |= FLAG_Z;
        } else {
          cpu->f &= ~FLAG_Z;
        }
      }
      break;
    case DEC:
      {
        uint8_t result = cpu->registers[field] -= 1;
        if (cpu->a == 0) {
          cpu->f |= FLAG_Z;
        } else {
          cpu->f &= ~FLAG_Z;
        }

        if (isBitSet(result, 7)) {
          cpu->f |= FLAG_N;
        } else {
          cpu->f &= ~FLAG_N;
        }
      }
      break;
    case INC:
      {
        uint8_t result = cpu->registers[field] += 1;
        if (cpu->a == 0) {
          cpu->f |= FLAG_Z;
        } else {
          cpu->f &= ~FLAG_Z;
        }

        if (isBitSet(result, 7)) {
          cpu->f |= FLAG_N;
        } else {
          cpu->f &= ~FLAG_N;
        }
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
      cpu->a = ~(cpu->registers[field]);
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      } else {
        cpu->f &= ~FLAG_Z;
      }
      break;
    case SYS:
      {
        switch (field) {
          case HALT:
            cpu->running = false;
            break;
          case DATA_IN:
            CPU_readData(cpu);
            break;
          case DATA_OUT:
            CPU_writeData(cpu);
            break;
          case CLEAR_INT:
            cpu->i = 0;
            break;
          case RET:
            {
              uint8_t lo, hi;
              POP_STACK(lo);
              POP_STACK(hi);
              cpu->ip = (hi << 8) | lo;
            }
            break;
          case RETI:
            {
              POP_STACK(cpu->f);
              uint8_t lo, hi;
              POP_STACK(lo);
              POP_STACK(hi);
              cpu->ip = (hi << 8) | lo;
            }
            break;
          case SWAP:
            {
              uint8_t operand = CPU_fetch(cpu);
              uint8_t src = operand & 0xF;
              uint8_t dest = (operand & 0xF0) >> 4;
              uint8_t swap = cpu->registers[dest];
              cpu->registers[dest] = cpu->registers[src];
              cpu->registers[src] = swap;

              cpu->f &= ~FLAG_Z;
              cpu->f &= ~FLAG_C;
              cpu->f &= ~FLAG_N;
              cpu->f &= ~FLAG_O;
            }
            break;
          case NOOP: break;
        }
      }
      break;
    default: cpu->running = false;
  }
}

uint8_t CPU_defaultMemAccess(enum DIRECTION dir, uint16_t addr, uint8_t value) {
  return 0;
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
  cpu->memory = CPU_defaultMemAccess;
}

void CPU_prime(CPU* cpu) {
  uint8_t lo = cpu->memory(READ, 0x00, 0);
  uint8_t hi = cpu->memory(READ, 0x01, 0);
  cpu->ip = (hi << 8) | lo;
}

bool CPU_step(CPU* cpu) {
  if (!cpu->running) {
    return false;
  }

  if ((cpu->f & FLAG_I) != 0 && (cpu->i != 0)) {
    // service interupt
    PUSH_STACK((cpu->ip >> 8));
    PUSH_STACK((uint8_t)(cpu->ip & 0x00FF));
    PUSH_STACK(cpu->f);
    uint8_t lo = cpu->memory(READ, 0x02, 0);
    uint8_t hi = cpu->memory(READ, 0x03, 0);
    cpu->ip = (hi << 8) | lo;
  }

  uint8_t instruction = CPU_fetch(cpu);

  // decode
  uint8_t opcode = instruction & 0x8F;
  uint8_t field = (instruction & 0x70) >> 4;

  CPU_execute(cpu, opcode, field);
  return cpu->running;
}

void CPU_registerMemCallback(CPU* cpu, MEM_callback callback) {
  cpu->memory = callback;
}

void CPU_registerBusCallback(CPU* cpu, uint8_t addr, BUS_callback callback) {
  cpu->bus.callback[addr] = callback;
}

void CPU_raiseInterrupt(CPU* cpu, uint8_t addr) {
  if (cpu->i < 255) {
    cpu->i++;
  }
}

void CPU_dump(CPU* cpu) {
  printf("------ irx cpu dump ------\n\n");
  printf("# state\n");
  printf("Running: %s\n", cpu->running ? "true" : "false");
  printf("IP: 0x%04X\n", cpu->ip);
  printf("SP: 0x%04X\n", cpu->sp);
  printf("E: 0x%02X\t F: 0x%02X\n", cpu->e, cpu->f);

  printf("C:%i  Z:%i  I:%i  U2: %i\n", (cpu->f & FLAG_C) != 0, (cpu->f & FLAG_Z) != 0, (cpu->f & FLAG_I) != 0, (cpu->f & FLAG_U2) != 0);
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
