#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char *argv[]){
  // esto es de prueba nada mas para ver si estoy recibiendo por argv el nombre del pipe
  printf("%d\n",argc);
  for (size_t j = 0; j < argc; j++) {
    printf("%s\n",argv[j]);
  }

  //me guardo el nombre del fifo para abrirlo
  char *fifoName = argv[1];
  FILE * rd_stream;
  //messageR vendria a ser mi BUFFER
  char messageR[100];
  //n_elements es para saber si recibi el mensaje completo
  size_t n_elements;

  // abro el FIFO con permiso de lectura
  rd_stream = fopen(fifoName,"r");

  //valido si no hubo problema al abrirlo
  if (rd_stream == (FILE *)NULL) {
    printf("In slave process\n");
    printf("fopen returned NULL, not a valid stream\n");
    exit(102);
  }

  //leo el archivo que vino por el FIFO
  n_elements = fread(messageR,strlen("message to be sent by the pipe"),1,rd_stream);
  printf("%s\n",messageR);

  //cierro el FIFO
  fclose(rd_stream);
  return 0;
}
