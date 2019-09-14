#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

int
main(void){
  //message es lo que envio por el FIFO
  char message[] = "message to be sent by the pipe";
  //fifo es el nombre del FIFO y lo que voy a mandar por el EXECVP
  char fifo[] = "na";
  //el formato que debe tener el segundo parametro del EXECVP
  char *args[] = {"./hijo","na", NULL};


  FILE *wr_stream;
  pid_t pid;
  size_t n_elements;
  int i;
  // creo el FIFO
  if((i = mkfifo(fifo,0666)) != 0){
    printf("%d\n",i);
    printf("******** Unable to create a FIFO ********\n");
    exit(1);
  }

    // abro el FIFO
    wr_stream = fopen(fifo,"w");
    //falla al abrir el FIFO
    if (wr_stream == (FILE *)NULL) {
      printf("In Parent process\n");
      printf("fopen returned NULL, not a valid stream\n");
      exit(100);
    }
    // si llegue aca es porque estoy dentro del FIFO
    n_elements = fwrite(message,strlen(message)+1,1,wr_stream);
    if (n_elements != strlen(message)+1) {
      printf("Error writing the message in the pipe\n");
      exit(101);
    }
    //le mando por argumento args al ejecutable ./hijo que vendria a ser el slave
    execvp("./hijo",args);

    //cierro el FIFO
    fclose(wr_stream);
    //elimino el FIFO
    unlink(wr_stream);


  return 0;
}
