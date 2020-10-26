#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include "stateMachines.h"
#include "emissor.h"
#include "macros.h"


struct termios oldtio,newtio;

int counter = 0;
volatile int STP=FALSE;


int llopen(int fd, int status) {
    
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

    if(status == TRANSMITTER) {
        // Installing Alarm Handler

        if(signal(SIGALRM,alarmHandler) || siginterrupt(SIGALRM,1)){
            printf("Signal instalation failed");
        }


        while(STP == FALSE && counter < MAXTRIES){

            if(sendMessage(C_SET) != -1){
                printf("%c message sent: %d \n",c,wr);
            }
            else{
                printf("Error sending message");
            }

            alarm(8); // Call an alarm to wait for the message

            if(receiveUA(fd) == 0){
                printf("Interaction received\n");
                STP = TRUE;
                counter = 0;
            }

            alarm(0);

        }

        //falta enviar a mensagem SET e esperar pela mensagem UA, com alarm
    }
    else if(status == RECEIVER) {
        if(readSetMessage(fd) == TRUE) {
            printf("READ SET MESSAGE CORRECTLY");
            if(sendMessage(fd, C_UA) == -1) {
                fprintf(stderr, "llopen - Error writing to serial port (Receiver)\n");
                return -1;
            }
            else {
                print("SEND UA MESSAGE");
            }
        }
        else {
            fprintf(stderr, "llopen - Error reading from serial port (Receiver)\n");
            return -1;
        }
    }
    return 0;
}

unsigned char getBCC2(unsigned char *mensagem, int size){

    unsigned char bcc2 = mensagem[0];
    for(int i = 0; i < size; i++){
        bcc2 ^= mensagem[i];
    }
    return bcc2;
}

unsigned char stuffBCC2(unsigned char bcc2){
    unsigned char stuffed[2];
    if(bcc2 == FLAG){
        stuffed[0] = ESCAPE_BYTE;
        stuffed[1] = ESCAPE_FLAG; 
    }
    else if(bcc2 == ESCAPE_BYTE){
        stuffed[0] = ESCAPE_BYTE;
        stuffed[1] = ESCAPE_ESCAPE; 
    }
    else{
        stuffed[0] = bcc2;
        stuffed[1] = NULL;
    }
    return stuffed;
    
}

int llwrite(int fd, char *buffer, int lenght) {
// escreve a trama e fica a espera de receber uma mensagem RR ou REJ para saber o que enviar a seguir

    while(STP == FALSE && counter < MAXTRIES){
            // Inventei aqui um pouco na declaração porque simplesmente sei que o array não é de tamanho constante.
            unsigned char *message = (unsigned char *)malloc(6 * sizeof(unsigned char)); 
            int trama = 0;

            message[0] = FLAG;
            message[1] = A_EE;

            if(trama = 0){
                message[2] = NS0;
            }
            else{
                message[2] = NS1;
            }
            message[3] = message[1] ^ message[2];

            // Começa a ler do 4 e o tamanho depende da mensagem a ser enviada
            int i = 4;
            int j = 0;
            for(int j = 0; j < lenght; j++){
                if(buffer[j] == FLAG){
                    message[i] = ESCAPE_BYTE;
                    message[i + 1] = ESCAPE_FLAG;
                    i+=2;
                }
                else if(buffer[j] == ESCAPE_BYTE){
                    message[i] = ESCAPE_BYTE;
                    message[i+1] = ESCAPE_ESCAPE;
                    i+=2;
                }
                else{
                    message[i] = buffer[i];
                    i++;
                }
            }


            // Processo de escrita
            tcflush(fd,TCIOFLUSH);

            // Para já ainda não sei qual é o tamanho
            int wr = write(fd,message,5);

            printf("SET message sent: %d \n",wr);

            alarm(8);

            if(receiveUA(fd) == 0){
                printf("Interaction received\n");
                STP = TRUE;
                counter = 0;
            }

            alarm(0);

        }


    

}

int llread(int fd, unsigned long *size) {
// le a trama
// tramas I, S ou U com cabecalho errado são ignoradas, sem qualquer acao
// caso trama I recebida sem erros detetados no cabecalho e no campo de dados:
// caso seja uma nova trama, a trama é aceite e passada à aplicação, e envia-se RR para o emissor para confirmar
// caso seja duplicado, descarta-se a trama e envia-se RR para o emissor 
// casos trama I sem erro no cabecalho mas com erro detetado pelo BCC no campo de dados:
// caso seja uma nova trama, a informacao e descartada mas envia-se REJ para o emissor para pedir a retransmissao
// caso seja duplicado, confirma-se com RR para o transmissor

    receiverRead_StateMachine(fd, *size); //nao sei se se passa o pointer ou nao;

    
}

int llclose(int fd, int status) {
//emissor:
// envia DISC, espera por DISC e envia UA
    if(status == TRANSMITTER) {

    }

//recetor:
// le a mensagem DISC enviada pelo emissor, envia DISC e recebe UA
    else if(status == RECEIVER) {
        if (receiveDISC(fd) == TRUE) {
            printf("READ DISC MESSAGE");
            if(sendMessage(fd, C_DISC)) {
                printf("SEND DISC MESSAGE");
                if(receiveUA(fd) == TRUE) {
                    printf("READ UA MESSAGE");
                }
                
                else {
                    fprintf(stderr, "llclose- Error reading UA message (Receiver)\n");
                    return -1;
                }
            }

            else {
                fprintf(stderr, "llclose- Error writing DISC message to serial port (Receiver)\n");
                return -1;
            }
        }

        else {
            fprintf(stderr, "llclose - Error reading DISC message (Receiver)\n");
            return -1;
        }
    }
    return 0;
}