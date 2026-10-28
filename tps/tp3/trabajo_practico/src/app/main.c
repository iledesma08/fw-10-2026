/**
 * @file    main.c
 *
 * Solo orquesta las tres demos. Si recibe --version imprime nombre y
 * version y termina sin correr nada mas (hito 13)
 */

#include <string.h>

#include "demo.h"
#include "print.h"

static const char PROGRAM_NAME[] = "building-process";
static const char PROGRAM_VERSION[] = "1.0.0";

int main(int argc, char *argv[]) {
  if (argc > 1 && strcmp(argv[1], "--version") == 0) {
    print_str(PROGRAM_NAME);
    print_str(" v");
    print_str(PROGRAM_VERSION);
    print_str("\n");
    return 0;
  }

  demo_math();
  demo_str();
  demo_embedded();

  return 0;
}
