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

volatile int STP=FALSE;
extern unsigned char rcv;
extern int counter = 0;


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

        int counter = 0;
        while(STP == FALSE && counter < MAXTRIES){
            int wr;
            if((wr = sendMessage(fd,C_SET)) != ERROR){
                printf("C_SET message sent: %d \n",wr);
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

unsigned char* stuffBCC2(unsigned char bcc2,unsigned int *size){
    unsigned char* stuffed;
    if(bcc2 == FLAG){
        stuffed = malloc(2*sizeof(unsigned char));
        stuffed[0] = ESCAPE_BYTE;
        stuffed[1] = ESCAPE_FLAG; 
        (*size) = 2;
    }
    else if(bcc2 == ESCAPE_BYTE){
        stuffed = malloc(2*sizeof(unsigned char));
        stuffed[0] = ESCAPE_BYTE;
        stuffed[1] = ESCAPE_ESCAPE;
        (*size) = 2; 
    }
    else{
        stuffed = malloc(sizeof(unsigned char));
        stuffed[0] = bcc2;
        (*size) = 1;
    }
    
    return stuffed;
    
    
}

int llwrite(int fd, char *buffer, int length) {
// escreve a trama e fica a espera de receber uma mensagem RR ou REJ para saber o que enviar a seguir
    unsigned char bcc2;
    unsigned int sizebcc2 = 1;
    unsigned int messageSize = length+6;
    unsigned char *bcc2Stuffed = (unsigned char *)malloc(sizeof(unsigned char));
    unsigned char *message = (unsigned char *)malloc(messageSize * sizeof(unsigned char));
    int trama = 0;

    bcc2 = getBCC2(buffer,length);
    bcc2Stuffed = stuffBCC2(bcc2, &sizebcc2);

    
    // Inicio do preenchimento da mensagem
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
    for(int j = 0; j < length; j++){
        if(buffer[j] == FLAG){
            message = (unsigned char *)realloc(message, ++messageSize);
            message[i] = ESCAPE_BYTE;
            message[i + 1] = ESCAPE_FLAG;
            i+=2;
        }
        else if(buffer[j] == ESCAPE_BYTE){
            message = (unsigned char *)realloc(message, ++messageSize);
            message[i] = ESCAPE_BYTE;
            message[i+1] = ESCAPE_ESCAPE;
            i+=2;
        }
        else{
            message[i] = buffer[i];
            i++;
        }
    }

    if(sizebcc2 == 2){
        messageSize++;
        realloc(message,(messageSize)*sizeof(unsigned char));
        message[i] = bcc2Stuffed[0];
        message[i + 1]  = bcc2Stuffed[1];
        i+=2;
    }
    else{
        message[i] = bcc2;
        i++;
    }
    message[i] = FLAG;

    //Mensagem preenchida Trama I feita
    

    int counter = 0;

    STP = FALSE;

    // Envio da trama
    do {
        // Processo de escrita
        tcflush(fd,TCIOFLUSH);

        counter++;

        // Para já ainda não sei qual é o tamanho
        int wr = write(fd,message,messageSize);

        printf("SET message sent: %d \n",wr);

        alarm(TIMEOUT);

        // Mudar o processo de espera não é receiveUA
        if(readMessage(fd) == 0){
            printf("Interaction received\n");
        }

        // Tratar do rcv
        if((rcv == RR0 && trama == 1) || (rcv == RR1 && trama == 0)) {
            counter = 0;
            trama = (trama + 1) % 2;
            STP = FALSE;
            if(rcv == RR0) {
                printf("TRANSMITTER: Received RR0\n");
            }
            else {
                printf("TRANSMITTER: Received RR1\n");
            }
            break;
        }

        else if(rcv == REJ0 || rcv == REJ1) {
            STP = TRUE;
            if(rcv == REJ0) {
                printf("TRANSMITTER: Received REJ0");
            }
            else {
                printf("TRANSMITTER: Received REJ1");
            }
        }

        else {
            printf("TRANSMITTER: Received an invalid message");
        }

        // alarm(0); //ao fazer isto, o alarme nao e cancelado e perde-se o efeito do counter??
    } while(STP || counter < MAXTRIES); //verificar esta condicao

    return 0;
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
    unsigned char ret;

    if(status == TRANSMITTER) {
        if(sendMessage(fd, C_DISC)) {
            printf("TRANSMITTER: Send DISC\n");
        }

        ret = receiveDISC(fd);

        while (ret != C_DISC) { //adaptar maquinas de estado 
            ret = receiveDISC(fd);
        } 

        printf("TRANSMITTER: Read DISC\n");

        if(sendMessage(fd, C_UA)) {
            printf("TRANSMITTER: Send UA\n");
        }
        tcsetattr(fd, TCSANOW, &oldtio);
    }

//recetor:
// le a mensagem DISC enviada pelo emissor, envia DISC e recebe UA
    else if(status == RECEIVER) {
        if (receiveDISC(fd) == C_DISC) {
            printf("RECEIVER: Read DISC\n");
            if(sendMessage(fd, C_DISC)) {
                printf("RECEIVER: Send DISC\n");
                if(receiveUA(fd) == TRUE) {
                    printf("RECEIVER: Read UA\n");
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
        tcsetattr(fd, TCSANOW, &oldtio);
    }
    return 0;
}