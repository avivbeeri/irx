#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
   irx cpu core
   (instructions, registers, execution)
   */

#define STACK_SIZE 256
#define HALT SYS
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
  NOOP = 0x00,
  SYS,// vacant

  // ALU
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
  INC,
  DEC,
  RTL, // rotate left
  RTR, // rotate right

  // MEMORY
  SET,
  SWAP,
  LOAD_I, // Immediate address
  STORE_I,
  LOAD_IR, // Address in register pair.
  STORE_IR,

  COPY_IN, // to A
  COPY_OUT, // from A

  // Stack control
  STK,
  // Control flow
  JMP,
  BRCH,
  RET,
  CMP, // compare

  CLF, // clear flag
  SEF, // set flag
} OP;

// Based on 6502 format
typedef enum {
  FLAG_C = 1, // Carry
  FLAG_Z = 2, // Zero
  FLAG_I = 4, // Interrupt enabled
  FLAG_U2 = 8, // Write-protect-enable
  FLAG_BRK = 16, // Break (software interrupt)
  FLAG_U = 32, // unused
  FLAG_N = 64, // Negative
  FLAG_O = 128, // Overflow
} FLAG;

#define OPZ(opcode) opcode
#define OP(opcode, flag) (flag << 5) | opcode

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
        // direct
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
          case 4:
            {
              // CALL
              // absolute jump with PC push
              uint8_t lo = CPU_fetch(cpu);
              uint8_t hi = CPU_fetch(cpu);
              uint16_t addr = (hi << 8) | lo;

              PUSH_STACK((cpu->ip >> 8));
              PUSH_STACK((uint8_t)(cpu->ip & 0x00FF));
              cpu->ip = addr;
            }
            break;
          case 5:
          case 6:
          case 7:
            {
              // absolute CALL with PC push
              uint8_t pair = (field - 5)*2;
              uint8_t lo = cpu->registers[pair];
              uint8_t hi = cpu->registers[pair+1];
              uint16_t addr = (hi << 8) | lo;

              PUSH_STACK((cpu->ip >> 8));
              PUSH_STACK((uint8_t)(cpu->ip & 0x00FF));
              cpu->ip = addr;
            }
            break;
        }
      }
      break;
    case RET:
      {
        switch(field) {
          case 0:
            {
              uint8_t lo, hi;
              POP_STACK(lo);
              POP_STACK(hi);
              cpu->ip = (hi << 8) | lo;
            }
            break;
          case 1:
            // RETI
            {
              POP_STACK(cpu->f);
              uint8_t lo, hi;
              POP_STACK(lo);
              POP_STACK(hi);
              cpu->ip = (hi << 8) | lo;
            }
            break;
        }
      }
      break;
    case STK:
      {
        switch(field) {
          case 0:
            PUSH_STACK(cpu->a);
            break;
          case 1:
            POP_STACK(cpu->a);
            break;
        }
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
    case BRCH:
      {
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
        cpu->memory(WRITE, addr, cpu->registers[field]);
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
      }
      break;
    case SWAP:
      {
        uint8_t src = CPU_fetch(cpu);
        uint8_t swap = cpu->registers[field];
        cpu->registers[field] = cpu->registers[src];
        cpu->registers[src] = swap;
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
      cpu->a = ~(cpu->registers[field]);
      if (cpu->a == 0) {
        cpu->f |= FLAG_Z;
      } else {
        cpu->f &= ~FLAG_Z;
      }
      break;
    case NOOP: break;
    case SYS:
      {
        switch (field) {
          case 0: //HALT
halt:
            cpu->running = false;
            break;
          case 1: // enable interupts
          case 2: // disable interupts
            break;
          case 3: // DATA_IN
            CPU_readData(cpu);
            break;
          case 4: // DATA_OUT
            CPU_writeData(cpu);
            break;
          case 5: // enable interupts
            cpu->i = 0;
            break;
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

bool CPU_step(CPU* cpu) {
  if (!cpu->running) {
    return false;
  }

  if (isBitSet(cpu->f, 2) && cpu->i != 0) {
    // service interupt
    PUSH_STACK((cpu->ip >> 8));
    PUSH_STACK((uint8_t)(cpu->ip & 0x00FF));
    PUSH_STACK(cpu->f);
    uint8_t hi = cpu->memory(READ, 0x03, 0);
    uint8_t lo = cpu->memory(READ, 0x02, 0);
    cpu->ip = (hi << 8) | lo;
  }

  uint8_t instruction = CPU_fetch(cpu);

  // decode
  uint8_t opcode = instruction & 0x1F;
  uint8_t field = instruction >> 5;

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
