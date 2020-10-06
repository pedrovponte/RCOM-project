/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07


#define ERROR -1

volatile int STOP=FALSE;

enum state {START,FLAG_RCV,A_RCV,C_RCV,BCC_OK,STOP};
enum state current = START;

int receiverInteraction(int serialPort){

    char c; // char reaad. Changes the state

    //Colocar aqui o código da espera pelo byte


    //State Machine
    switch(current){
        case START:
            if(c == FLAG){
                puts("Flag Received");
                current = FLAG_RCV;
            }
            break;
        case FLAG_RCV:
            if(c == A){
                puts("A Received");
                current = A_RCV;
            }
            else if(c == FLAG);
            else{
                current = START;
            }
            break;
        case A_RCV:
            if(c == C_SET) current = C_RCV;
            break;
        case C_RCV:
          break;
        case BCC_OK:
          break;
        case STOP:
          break;
        default:
          break;
  }

}

int main(int argc, char** argv)
{
  int fd,c;
  struct termios oldtio,newtio;
  char *buf = NULL;
  int i, sum = 0, speed = 0;

  if ( (argc < 2) || 
        ((strcmp("/dev/ttyS0", argv[1])!=0) && 
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }


  /*
  Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C.
  */


  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }

  if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */



  /* 
  VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
  leitura do(s) pr�ximo(s) caracter(es)
  */



  tcflush(fd, TCIOFLUSH);

  if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");



  /*for (i = 0; i < 255; i++) {
    buf[i] = 'a';
  }


  buf[25] = '\n';*/

  size_t n;
  ssize_t n_bytes;

  printf("Enter a message: ");

  n_bytes = getline(&buf, &n, stdin);

  if(n_bytes == -1) {
    fprintf(stderr, "Error in reading from stdin");
    exit(-2);
  }

  buf[n_bytes - 1] = '\0';

  n_bytes = write(fd,buf,n_bytes * sizeof(buf[0]));   

  if(n_bytes == -1) {
    fprintf(stderr, "Failed to write in serial port");
    exit(-3);
  }

  printf("%zd bytes written\n", n_bytes);


  /* 
  O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar 
  o indicado no gui�o 
  */

  char message[4096];
  char byte;
  n = 0;

  while(1) {
    n_bytes = read(fd, &byte, 1);

    if(n_bytes == -1) {
      fprintf(stderr, "Error reading return message");
      exit(-4);
    }

    message[n++] = byte;

    if(byte == '\0') {
      break;
    }
  }

  printf("\nWaiting for a message...\n");
  printf("Message returned: %s\n", message);

  sleep(2);

  free(buf);

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
