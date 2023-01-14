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
  LOAD,
  STORE,
  COPY,

} OP;

#define OPZ(opcode) opcode
#define OP(opcode, flag) (flag << 5) | opcode

uint8_t CPU_fetch(CPU* cpu) {
  uint8_t data = cpu->memory[cpu->ip++];
  return data;
}

void CPU_execute(CPU* cpu, uint8_t opcode, uint8_t type) {
  switch (opcode) {
    case COPY:
      // register to register
      {
      }
      break;
    case STORE:
      // register to memory
      {
      }
      break;
    case LOAD:
      // memory to register
      {
      }
      break;
    case SWAP:
      {
        uint8_t swap = cpu->registers[type];
        uint8_t start = 2 * type;
        cpu->registers[start] = cpu->registers[start + 1];
        cpu->registers[start + 1] = swap;
      }
      break;
    case SET:
      {
        uint8_t value = CPU_fetch(cpu);
        cpu->registers[type] = value;
      }
      break;
    case ADD:
      cpu->a += cpu->registers[type];
      break;
    case SUB:
      cpu->a -= cpu->registers[type];
      break;
    case MUL:
      cpu->a *= cpu->registers[type];
      break;
    case DIV:
      cpu->a /= cpu->registers[type];
      break;
    case MOD:
      cpu->a %= cpu->registers[type];
      break;
    case AND:
      cpu->a &= cpu->registers[type];
      break;
    case OR:
      cpu->a |= cpu->registers[type];
      break;
    case XOR:
      cpu->a ^= cpu->registers[type];
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
    uint8_t type = instruction >> 5;

    CPU_execute(cpu, opcode, type);
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
    OP(SET, 0), 2,
    OP(SET, 1), 6,
    OP(SWAP, 0),
    OPZ(HALT)
  };
  memcpy(cpu.memory, program, sizeof(program));

  CPU_run(&cpu);
  CPU_dump(&cpu);
  return 0;
}
