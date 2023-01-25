#include <ctype.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "cpu.c"

struct termios orig_termios;
void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);
  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
//  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

void TERM_run(CPU* cpu) {
  while (cpu->running) {
    CPU_step(cpu);
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    if (c == 'q') break;
  }
}

int main(int argc, char *argv[]) {
  enableRawMode();

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
  TERM_run(&cpu);
  CPU_dump(&cpu);
  return 0;
}
