#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);

  // TODO: Uncomment the code below to pass the first stage
  printf("$ ");
  char buffer[1024];
  fgets(buffer, sizeof(buffer), stdin);

  buffer[strlen(buffer) - 1] = '\0';
  printf("%s: command not found", buffer);

  return 0;
}
