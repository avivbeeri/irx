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
  uint8_t memory[MEMORY_SIZE];

} CPU;

typedef enum {
  NOOP = 0,
  HALT,
  SET,
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  AND,
  OR,
  XOR,
  NOT,
  SWAP,
  LOAD_I,
  STORE_I,

  // Copy takes up multiple opcodes
  COPY_A = 0x10,
  COPY_B = 0x11,
  COPY_C = 0x12,
  COPY_D = 0x13,
  COPY_G = 0x14,
  COPY_H = 0x15,

  // Control flow
  JMP,


} OP;

#define OPZ(opcode) opcode
#define OP(opcode, flag) (flag << 5) | opcode

uint8_t CPU_fetch(CPU* cpu) {
  uint8_t data = cpu->memory[cpu->ip++];
  return data;
}

void CPU_execute(CPU* cpu, uint8_t opcode, uint8_t field) {
  switch (opcode) {
    case COPY_A:
    case COPY_B:
    case COPY_C:
    case COPY_D:
    case COPY_G:
    case COPY_H:
      // register to register
      {
        // A->B
        // A->C
        // A->D
        // A->G
        // A->H
        // B->A
        // B->C
        // B->D
        // B->G
        // B->H
        uint8_t dest = opcode & 0x07;
        uint8_t src = field;
        cpu->registers[dest] = cpu->registers[src];
      }
      break;
    case JMP:
      {
        // Immediate
        // relative
        // direct
        // conditionals?
        uint8_t value = CPU_fetch(cpu);
        switch(field) {
          case 0:
            {
              cpu->ip = value;
            }
            break;
          case 1: // check Z flag
            {
              if ((cpu->f & 0x01) != 0) {
                cpu->ip = value;
                printf("jumping");
              }
            }
            break;
          case 2: // check Z flag
            {
              if ((cpu->f & 0x01) == 0) {
                cpu->ip = value;
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
    case ADD:
      cpu->a += cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= 0x01;
      }
      break;
    case SUB:
      cpu->a -= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= 0x01;
        printf("Z\n");
      }
      break;
    case MUL:
      cpu->a *= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= 0x01;
      }
      break;
    case DIV:
      cpu->a /= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= 0x01;
      }
      break;
    case MOD:
      cpu->a %= cpu->registers[field];
      if (cpu->a == 0) {
        cpu->f |= 0x01;
      }
      break;
    case AND:
      cpu->a &= cpu->registers[field];
      break;
    case OR:
      cpu->a |= cpu->registers[field];
      break;
    case XOR:
      cpu->a ^= cpu->registers[field];
      break;
    case NOT:
      cpu->a = !cpu->a;
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
  cpu->f = 0;

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
  printf("IP: 0x%04x\n", cpu->ip);
  printf("SP: 0x%04x\n", cpu->sp);
  printf("E: 0x%02x\t F: 0x%02x\n", cpu->e, cpu->f);
  printf("\n");
  printf("# registers\n\n");
  printf("A: 0x%02x\n", cpu->a);
  printf("B: 0x%02x\n", cpu->b);
  printf("C: 0x%02x\n", cpu->c);
  printf("D: 0x%02x\n", cpu->d);
  printf("G: 0x%02x\n", cpu->g);
  printf("H: 0x%02x\n", cpu->h);
  printf("\n");
  printf("--------------------------\n");
}

int main(int argc, char *argv[]) {
  CPU cpu;
  CPU_init(&cpu);

  uint8_t program[] = {
    OP(SET, 0), 4,
    OP(SET, 1), 1,
    OP(SUB, 1),
    OP(JMP, 2), 4, // Jump if zero
    OPZ(HALT)
  };
  memcpy(cpu.memory, program, sizeof(program));

  CPU_run(&cpu);
  CPU_dump(&cpu);
  return 0;
}
