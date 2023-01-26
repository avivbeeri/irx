#include <pthread.h>
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
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char buf[256];
uint8_t bufPtr = 0;
uint8_t readPtr = 0;

#define CTRL_KEY(k) ((k) & 0x1f)
void* SERIAL_thread(void *data) {
  CPU* cpu = data;

  while (cpu->running) {
    char c = '\0';
    size_t count = 0;
    if ((count = read(STDIN_FILENO, &c, 1)) == -1 && errno != EAGAIN) die("read");
    if (iscntrl(c)) {
      if (c == CTRL_KEY('q')) {
        cpu->running = false;
      }
    }
    if (count > 0) {
      buf[bufPtr++] = c;
      CPU_raiseInterrupt(cpu, 0);
    }
  }
  return NULL;
}

void TERM_run(CPU* cpu) {
  while (cpu->running) {
    CPU_step(cpu);
  }
}

uint8_t SERIAL_io(enum DIRECTION dir, uint8_t value) {
  if (dir == READ) {
    return buf[readPtr++];
  }
  if (dir == WRITE) {
    write(STDOUT_FILENO, &value, 1);
    return 0;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  enableRawMode();

  CPU cpu;
  CPU_init(&cpu);
  CPU_registerBusCallback(&cpu, 0, SERIAL_io);
  pthread_t thread;
  pthread_create(&thread, NULL, SERIAL_thread, &cpu);

  uint8_t program[] = {
    // Little-endian execution start address.
    0x04, 0x00,
    // Little-endian execution interupt
    0x0A, 0x00,
    // Main loop
    OP(SEF, 2),
    OP(SET, 7), 0x00,
    OP(JMP, 0), 0x07, 0x00,
    // Interrupt
    // clear interupt count
    OP(SYS, 5),
    // Read from device bus
    OP(SYS, 3),
    // store character
    OP(COPY_OUT, 1),
    // write to terminal
    OP(SYS, 4),
    // RETI
    OP(RET, 1),
  };

  CPU_loadMemory(&cpu, &program, sizeof(program));
  TERM_run(&cpu);
  pthread_join(thread, NULL);
  disableRawMode();
  write(STDOUT_FILENO, "\n\r", 1);
  CPU_dump(&cpu);
  return 0;
}
