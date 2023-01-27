#include "cpu.c"

#define ROM_SIZE (16)
#define MEMORY_SIZE (64 * 1024 - ROM_SIZE)

uint8_t RAM[MEMORY_SIZE];
uint8_t ROM[ROM_SIZE];

uint8_t accessMemory(enum DIRECTION dir, uint16_t addr, uint8_t value) {
  if (dir == READ) {
    if (addr < ROM_SIZE) {
      return ROM[addr];
    } else {
      return RAM[addr - ROM_SIZE];
    }
  } else {
    if (addr >= ROM_SIZE) {
      RAM[addr - ROM_SIZE] = value;
    }
  }
  return 0;
}

void CPU_run(CPU* cpu) {
  while (cpu->running) {
    CPU_step(cpu);
  }
}

int main(int argc, char *argv[]) {
  CPU cpu;
  CPU_init(&cpu);
  CPU_registerMemCallback(&cpu, accessMemory);

  uint8_t program[] = {
    // Little-endian execution start address.
    0x04, 0x00,
    // Little-endian execution interupt
    0x0C, 0x00,
    OP(SET, 0), 0x07,
    OP(SET, 1), 0x00,
    OP(JMP, 4), 0x0C, 0x00,
    OPZ(HALT),
    OP(SWAP, 0), 0x01,
    OPZ(RET)
  };

  memcpy(ROM, &program, sizeof(program));
  CPU_run(&cpu);
  CPU_dump(&cpu);
  return 0;
}
