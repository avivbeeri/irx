#include "cpu.c"

void CPU_run(CPU* cpu) {
  while (cpu->running) {
    CPU_step(cpu);
  }
}

int main(int argc, char *argv[]) {
  CPU cpu;
  CPU_init(&cpu);

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

  CPU_loadMemory(&cpu, &program, sizeof(program));
  CPU_run(&cpu);
  CPU_dump(&cpu);
  return 0;
}
