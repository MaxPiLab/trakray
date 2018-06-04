
//
// Example program for bcm2835 library
// Shows how to interface with SPI to transfer a number of bytes to and from an SPI device
//
// After installing bcm2835, you can build this 
// with something like:
// gcc -o spin spin.c -l bcm2835
// sudo ./spin
//
// Or you can test it before installing with:
// gcc -o spin -I ../../src ../../src/bcm2835.c spin.c
// sudo ./spin
//
// Author: Mike McCauley
// Copyright (C) 2012 Mike McCauley
// $Id: RF22.h,v 1.21 2012/05/30 01:51:25 mikem Exp $


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <bcm2835.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <syslog.h>
#include <time.h>
//#include <signal.h>
#include <unistd.h>


#define SPI_CRASH 1
#define SPI_RESET 22 // Wirint Pi pin 22
#define ReadyIn 21 // Physical pin 29, BCM pin 5, Wiring Pi pin 21
#define TX_RX_DELAY 1
#define TOGGLE_DELAY 1000
#define DEBUG 1
#define DEBUGBCM 0
#define MAXPI_BYTES 261
#define PI_SER_ST_INDEX 249

int socStatus = -1;

/* SPI device is crashed or not */
int isSPIDevCrashed(unsigned char read_data[]) {

    unsigned char SPI_DEV_ID[] = {0xE9,0x07,0x20,0x12};
    int i =0;
    int crashed = 0;

    for (i=0;i<=3;i++) {
        if(SPI_DEV_ID[i]!=read_data[i+5]) {
            crashed = 1;
            break;
        } else {
            crashed = 0;
        }
    }

    return crashed;
}


/**
    Signal Handler / can be used to gracefully shut down the 
*/
/*
void sig_handler(int signo)
{
   switch ( signo ) {
       case SIGINT:
        syslog(LOG_INFO,"received SIGINT");
        break;
       case SIGTERM:
        syslog(LOG_INFO,"recieved SIGTERM");
        break;
       default:
        printf( "recieved signal %d",signo);
   }
    syslog(LOG_INFO," gracefully shutdown \n");
   closelog();
    exit(0);
}*/


/**
 * @return - long  - returns the Serial number of the Pi
 * getPiSerial function reads the cpuinfo from /proc/cpuinfo and reads the Serial
 * and reutns the 32 bit long
 *
 * */

unsigned long getPiSerial() {

    char *recv_data =(char *) malloc(17);
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        //  strcpy(recv_data,"error");
        return -1 ;// recv_data;
    }
    char line[256];
    while (fgets(line, 256, f)) {
        if (strncmp(line, "Serial", 6) == 0) {
            strcpy(recv_data, strchr(line, ':') + 2);
        }

    }


    fclose(f);

    return (unsigned long)strtoul(recv_data,NULL,16);
}

/**
 * send all - used to send the half buffer 
 */
int send_all(int socket_in, const char *buffer, size_t length, int flags)
{
    ssize_t n;
    const char *p = buffer;
    //syslog(LOG_INFO,"send_all %zd  - %d \n", length, socket_in);
    while (length > 0)
    {
        printf(" calling send %s - p - %s \n", buffer, p);
        n = send(socket_in, p, length, flags);
        if( n < 0 ) {
            socStatus = -1;
            //syslog(LOG_ERR,"Send failed - %zd", n);
            return n;
        }
        printf("Send successful - %zd", n);
        if (n == 0) break;
        p += n;

        length -= n;


    }
    return (n <= 0) ? -1 : 0;
}

/*
 * is connected is used to check if soclet is connected
 *
 */

int isConnected(int socket_fd){

    if( socStatus == -1 ) {
        return -1;
    }

    int error = 0;
    //  printf("is connected called \n");
    socklen_t len = sizeof (error);

    int retval = getsockopt (socket_fd, SOL_SOCKET,SO_ERROR , &error, &len);

    //printf(" retval recieved %d with err  - %d ", retval , error );
    if (retval != 0) {

        fprintf(stderr, "error getting socket error code: %s\n", strerror(retval));
        //close(socket_fd);
        return -1;
    }

    if (error != 0) {
        fprintf(stderr, "socket error: %s\n", strerror(error));
        //close(socket_fd);
        return -2;
    }
    printf( "return val - %d \n", retval);

    return 1;
}


void printTimeTake( clock_t p_t2, clock_t  p_t1, char* msg ) {

    printf("%s - %f \n",msg, ((double) (p_t2 - p_t1)/CLOCKS_PER_SEC));


}
    
void dummy_data_for_initialization(void) {
    /*there is bug in SPI device. for that we need to do this. */
    
    unsigned char bufInit[] = {0x0b}; // Dummy data for initialization.
    bcm2835_spi_transfern(bufInit, sizeof(bufInit));
    delay(TX_RX_DELAY);
}



int main(int argc, char **argv)
{
    clock_t t1,t2,t3,t4;

    
    // Initializing syslog 
    //openlog("slog", LOG_PID|LOG_CONS, LOG_USER);
    // syslog(LOG_INFO, "A different kind of Hello world ... ");
    /*if (signal(SIGINT, sig_handler) == SIG_ERR)
        syslog(LOG_ERR,"can't catch SIGINT");

    if (signal(SIGTERM, sig_handler) == SIG_ERR)
        syslog(LOG_ERR,"can't catch SIGTERM");

    if( signal(SIGKILL,sig_handler) == SIG_ERR) 
	syslog(LOG_ERR,"cant catch SIGKILL");
    */
    // If you call this, it will not actually access the GPIO
    // Use for testing
    if (DEBUGBCM) {
        bcm2835_set_debug(1);
    }
   
    // run as sudo
    if (!bcm2835_init())
    {
      printf("bcm2835_init failed. Are you running as root??\n");
      return 1;
    }

    if (!bcm2835_spi_begin())
    {
      printf("bcm2835_spi_begin failed. Are you running as root??\n");
      return 1;
    }
    
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);    // 32 = 7.8125MHz on Rpi2, 12.5MHz on RPI3  
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default
    

    long serPi = getPiSerial();
    // Send a some bytes to the slave and simultaneously read 
    // some bytes back from the slave
    // Most SPI devices expect one or 2 bytes of command, after which they will send back
    // some data. In such a case you will have the command bytes first in the mpi_rpi_tx_rx_data,
    // followed by as many 0 bytes as you expect returned data bytes. After the transfer, you 
    // Can the read the reply bytes from the mpi_rpi_tx_rx_data.
    // If you tie MISO to MOSI, you should read back what was sent.
    
#ifdef ENABLE_SERVER_SEND
    char ipaddr[20] = "192.168.1.164";
    int port = 5019;
    
    // Initializing syslog 
    //openlog("slog", LOG_PID|LOG_CONS, LOG_USER);
    // syslog(LOG_INFO, "A different kind of Hello world ... ");
    //closelog();

    if ( argc >= 2 ) {   
        // argc should be 2 for correct execution
        // We print argv[0] assuming it is the program name
        printf( " args 1 =  " );
        strcpy(ipaddr, argv[1]);
        if( argc == 3 )
            port = atoi(argv[2]);
    }

    int clientSocket, bytesTrasfered;
    struct sockaddr_in serverAddr;
    socklen_t addr_size;
   
    //clientConnect:
    clientSocket = socket(PF_INET, SOCK_STREAM, 0);
#endif //ENABLE_SERVER_SEND

    int count = 1;
    
    // setting up wiring pi RESET Pin & ReadyIn Pin
    wiringPiSetup();
    pinMode(ReadyIn, INPUT);
    pinMode(SPI_RESET, OUTPUT);
    digitalWrite (SPI_RESET, LOW);
    delay(TOGGLE_DELAY);
    digitalWrite (SPI_RESET, HIGH);
    delay(TOGGLE_DELAY);

    printf("Starting\n") ;
    printf("Stage1 Initiating SPI connection\n") ;

   /* unsigned char bufInit[] = {0x0b}; // Dummy data for initialization.
    bcm2835_spi_transfern(bufInit, sizeof(bufInit));
    delay(TX_RX_DELAY);*/
    dummy_data_for_initialization();
    
    //SPICRASHED1 START
    if(SPI_CRASH) {
        unsigned char SPI_ID_R1[] = { 0x0b, 0x10, 0x70, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Data to readback
        bcm2835_spi_transfern(SPI_ID_R1, sizeof(SPI_ID_R1));
        delay(TX_RX_DELAY);
        // buf will now be filled with the data that was read from the slave
        if(DEBUG) {
        printf("Read from SPI_ID_R1: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", SPI_ID_R1[0],SPI_ID_R1[1],SPI_ID_R1[2],SPI_ID_R1[3],SPI_ID_R1[4],SPI_ID_R1[5], SPI_ID_R1[6], SPI_ID_R1[7], SPI_ID_R1[8]);
        }

        if(isSPIDevCrashed(SPI_ID_R1)) {
            /* toggle SPI_RESET Pin */
            digitalWrite (SPI_RESET, LOW);
            delay(TOGGLE_DELAY);
            
            digitalWrite (SPI_RESET, HIGH);
            delay(TOGGLE_DELAY);
            /*
            unsigned char bufInit_R2[] = {0x0b}; // Dummy data for initialization.
            bcm2835_spi_transfern(bufInit_R2, sizeof(bufInit_R2));
            delay(TX_RX_DELAY); */
            dummy_data_for_initialization();
            unsigned char SPI_ID_R2[] = { 0x0b, 0x10, 0x70, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Data to readback
            bcm2835_spi_transfern(SPI_ID_R2, sizeof(SPI_ID_R2));
            delay(TX_RX_DELAY);
            // buf will now be filled with the data that was read from the slave
            if(DEBUG) {
            printf("Read from SPI_ID_R2: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", SPI_ID_R2[0],SPI_ID_R2[1],SPI_ID_R2[2],SPI_ID_R2[3],SPI_ID_R2[4],SPI_ID_R2[5], SPI_ID_R2[6], SPI_ID_R2[7], SPI_ID_R2[8]);
            }
            
            if(isSPIDevCrashed(SPI_ID_R2)) {
                printf("SPI Device crashed1\n");
            } else {
                printf("SPI Device is fine1\n");    
            }
        } else {
            printf("SPI Device is fine2\n");
        }
    }
    //SPICRASHED1 END

    unsigned char bufM0[] = {0x02,0x10,0x80,0x00,0xff,0xff,0xfd,0xff,0x00};
    //spi_write_word(0x108000,0xfffdffff); line # 109 from photon code
    bcm2835_spi_transfern(bufM0, sizeof(bufM0));
    delay(TX_RX_DELAY);

    if(DEBUG) {
        unsigned char buf0[] = { 0x0b, 0x10, 0x80, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Data to readback
        bcm2835_spi_transfern(buf0, sizeof(buf0));
        delay(TX_RX_DELAY);
        // buf will now be filled with the data that was read from the slave
        printf("Read from SPI0: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", buf0[0],buf0[1],buf0[2],buf0[3],buf0[4],buf0[5], buf0[6], buf0[7], buf0[8]);
    }

    unsigned char bufM1[] = {0x02,0x10,0x80,0x1c,0x48,0xaa,0x0a,0x00,0x00};
    //spi_write_word(0x10801c,0xaaa48);  line # 152 from photon code 
    bcm2835_spi_transfern(bufM1, sizeof(bufM1));
    delay(TX_RX_DELAY);    
 
#ifdef ENABLE_SERVER_SEND
    serverAddr.sin_family = AF_INET;
    
    /* Set port number, using htons function to use proper byte order */
    serverAddr.sin_port = htons(port);
    
    /* Set IP address to localhost */
    serverAddr.sin_addr.s_addr = inet_addr(ipaddr);
    
    /* Set all bits of the padding field to 0 */
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
                       
    /*---- Connect the socket to the server using the address struct ----*/
    addr_size = sizeof serverAddr;

#endif //ENABLE_SERVER_SEND


    while(1)
    {
        printf("Entering server Send \n");
#ifdef ENABLE_SERVER_SEND
        int retConnection = isConnected(clientSocket);
        if(0 > retConnection ){

            if( -2 == retConnection ) 
                clientSocket = socket(PF_INET, SOCK_STREAM, 0);
            //waitfor connect
            //
            //
            do {

                socStatus = connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);

            } while ( socStatus !=  0 );
        }

#endif //ENABLE_SERVER_SEND

        printf("Stage2\n");
        
        unsigned char bufM2[] = {0x02,0x10,0x70,0x70,0x00,0x00,0x00,0x00,0x00};
        //spi_write_word(0x107070,0x0); line # 228 from photon code
        bcm2835_spi_transfern(bufM2, sizeof(bufM2));
        delay(TX_RX_DELAY);

#ifdef PROFILE
        t1 = clock(); /* time starts now */
#endif //PROFILE
        printf("Waiting for ReadyIn\n");
        while(digitalRead(ReadyIn) == 0);  //Wait for Device to make Pin to 1
        printf("Stage3\n");
#ifdef PROFILE
        t2 = clock(); //millis();
#endif //PROFILE    
        unsigned char bufM3[] = {0x02,0x10,0x80,0x1c,0x40,0xaa,0x0a,0x00,0x00};
        //spi_write_word(0x10801c,0xaaa40);  //GPIO7 (SPIS_IO1/SPIS_MISO) is in SPI mode, line # 233 from photon code
        bcm2835_spi_transfern(bufM3, sizeof(bufM3));
        delay(TX_RX_DELAY);
    
        if(DEBUG) {
            unsigned char buf3[] = {0x0b,0x10,0x80,0x1c,0xff,0xff,0xff,0xff,0xff};
            bcm2835_spi_transfern(buf3, sizeof(buf3));
            delay(TX_RX_DELAY);
            // buf will now be filled with the data that was read from the slave
            printf("Read from SPI3: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", buf3[0],buf3[1],buf3[2],buf3[3],buf3[4],buf3[5], buf3[6], buf3[7], buf3[8]);
        }

        unsigned char bufM4[] = {0x02,0x10,0x80,0x08,0x00,0x00,0x00,0x00,0x00};
        //spi_write_word(0x108008,0x0);        //GPIO7 (SPIS_IO1/SPIS_MISO) drive to 0 , line # 234 from photon code
        bcm2835_spi_transfern(bufM4, sizeof(bufM4));
        delay(TX_RX_DELAY);

        // Read Location Data - START
        /*  {0x0b,0x18,0x00,0x00,0xFF, 0xFF, 0xFF, 0xFF, 0xFF} */
        unsigned char mpi_rpi_tx_rx_data[MAXPI_BYTES] = {0x0b,0x18, 0x00, 0x00, 0x00 };
        memset(&mpi_rpi_tx_rx_data[5],0xFF,MAXPI_BYTES - 5);

        bcm2835_spi_transfern(mpi_rpi_tx_rx_data, sizeof(mpi_rpi_tx_rx_data));
        delay(TX_RX_DELAY);
        
        if (DEBUG) {
        printf("Read location from SPI: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", mpi_rpi_tx_rx_data[0],mpi_rpi_tx_rx_data[1],mpi_rpi_tx_rx_data[2],mpi_rpi_tx_rx_data[3],mpi_rpi_tx_rx_data[4],mpi_rpi_tx_rx_data[5], mpi_rpi_tx_rx_data[6], mpi_rpi_tx_rx_data[7], mpi_rpi_tx_rx_data[8]);
        }
        
        mpi_rpi_tx_rx_data[PI_SER_ST_INDEX+3] = (int)((serPi >> 24) & 0xFF) ;
        mpi_rpi_tx_rx_data[PI_SER_ST_INDEX+2]= (int)((serPi >> 16) & 0xFF) ;
        mpi_rpi_tx_rx_data[PI_SER_ST_INDEX+1] = (int)((serPi >> 8) & 0XFF);
        mpi_rpi_tx_rx_data[PI_SER_ST_INDEX] = (int)((serPi & 0XFF));
      
        unsigned  char  rx_tx [256];
       
        memcpy(rx_tx, &mpi_rpi_tx_rx_data[5],256);

      // Read Location Data - END

        //SPICRASHED2? START
        if(SPI_CRASH) {
            
            unsigned char SPI_ID_R3[] = { 0x0b, 0x10, 0x70, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Data to readback
            bcm2835_spi_transfern(SPI_ID_R3, sizeof(SPI_ID_R3));
            delay(TX_RX_DELAY);
            
            // buf will now be filled with the data that was read from the slave
            if(DEBUG) {
            printf("Read from SPI_ID_R3: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", SPI_ID_R3[0],SPI_ID_R3[1],SPI_ID_R3[2],SPI_ID_R3[3],SPI_ID_R3[4],SPI_ID_R3[5], SPI_ID_R3[6], SPI_ID_R3[7], SPI_ID_R3[8]);
            }

            if(isSPIDevCrashed(SPI_ID_R3)) {

                digitalWrite (SPI_RESET, LOW);
                delay(TOGGLE_DELAY);

                digitalWrite (SPI_RESET, HIGH);
                delay(TOGGLE_DELAY);
                /*unsigned char bufInit_R4[] = {0x0b}; // Dummy data for initialization.
                bcm2835_spi_transfern(bufInit_R4, sizeof(bufInit_R4));
                delay(TX_RX_DELAY);*/
                dummy_data_for_initialization();

                unsigned char SPI_ID_R4[] = { 0x0b, 0x10, 0x70, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Data to readback
                bcm2835_spi_transfern(SPI_ID_R4, sizeof(SPI_ID_R4));
                delay(TX_RX_DELAY);
                
                // buf will now be filled with the data that was read from the slave
                if(DEBUG) {
                printf("Read from SPI_ID_R4: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", SPI_ID_R4[0],SPI_ID_R4[1],SPI_ID_R4[2],SPI_ID_R4[3],SPI_ID_R4[4],SPI_ID_R4[5], SPI_ID_R4[6], SPI_ID_R4[7], SPI_ID_R4[8]);
                }

                if(isSPIDevCrashed(SPI_ID_R4)) {
                    printf("SPI Device crashed2\n");
                } else {
                    printf("SPI Device is fine3\n");
                    unsigned char bufSPIM0[] = {0x02,0x10,0x80,0x00,0xff,0xff,0xfd,0xff,0x00};
                    //spi_write_word(0x108000,0xfffdffff); line # 109 from photon
                    bcm2835_spi_transfern(bufSPIM0, sizeof(bufSPIM0));
                    delay(TX_RX_DELAY);

                    if(DEBUG) {
                        unsigned char bufSPI0[] = { 0x0b, 0x10, 0x80, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Data to readback
                        bcm2835_spi_transfern(bufSPI0, sizeof(bufSPI0));
                        delay(TX_RX_DELAY);
                        // buf will now be filled with the data that was read from the slave
                        printf("Read from bufSPI0: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", bufSPI0[0],bufSPI0[1],bufSPI0[2],bufSPI0[3],bufSPI0[4],bufSPI0[5], bufSPI0[6], bufSPI0[7], bufSPI0[8]);
                    }

                    unsigned char bufSPIM1[] = {0x02,0x10,0x80,0x1c,0x48,0xaa,0x0a,0x00,0x00};
                    //spi_write_word(0x10801c,0xaaa48);  line # 152 
                    bcm2835_spi_transfern(bufSPIM1, sizeof(bufSPIM1));
                    delay(TX_RX_DELAY);    
                }
            } else {
                printf("SPI Device is fine4\n");
            }
        }   
        //SPICRASHED2? END
        
        unsigned char bufM5[] = {0x02,0x10,0x80,0x1c,0x48,0xaa,0x0a,0x00,0x00};
        //spi_write_word(0x10801c,0xaaa48);  //GPIO7 (SPIS_IO1/SPIS_MISO) is in GPIO mode, line 248 from Photon Code
        bcm2835_spi_transfern(bufM5, sizeof(bufM5));
        delay(TX_RX_DELAY);
    
        if(DEBUG) {
            unsigned char buf5[] = {0x0b,0x10,0x80,0x1c,0xff,0xff,0xff,0xff,0xff};
            bcm2835_spi_transfern(buf5, sizeof(buf5));
            delay(TX_RX_DELAY);

            // buf will now be filled with the data that was read from the slave
            printf("Read from SPI5: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", buf5[0],buf5[1],buf5[2],buf5[3],buf5[4],buf5[5], buf5[6], buf5[7], buf5[8]);
        }

        unsigned char bufM6[] = {0x02,0x10,0x70,0x70,0x01,0xc0,0x01,0xc0,0x00};
        //spi_write_word(0x107070,0xC001C001);  //Write 0xC001C001 to Device to proceed, line 249 from Photon Code
        bcm2835_spi_transfern(bufM6, sizeof(bufM6));
        delay(TX_RX_DELAY);

        if(DEBUG) {
            unsigned char buf6[] = {0x0b,0x10,0x70,0x70,0xff,0xff,0xff,0xff,0xff};
            bcm2835_spi_transfern(buf6, sizeof(buf6));
            delay(TX_RX_DELAY);

            // buf will now be filled with the data that was read from the slave
            printf("Read from SPI6: %02X %02X %02X %02X %02X %02X  %02X  %02X  %02X \n", buf6[0],buf6[1],buf6[2],buf6[3],buf6[4],buf6[5], buf6[6], buf6[7], buf6[8]);
        }
#ifdef PROFILE
        t3 = clock();//millis();
#endif //PROFILE
#ifdef ENABLE_SERVER_SEND
        printf("Sending 256 bytes...\n");
        int retSocVal =  send_all(clientSocket, rx_tx, sizeof(rx_tx), MSG_CONFIRM) ;
        if( retSocVal == -1 )
        {
            fprintf(stderr, "socket() send failed: %s\n", strerror(errno));
            //syslog(LOG_ERR,  "socket() send failed: %s\n", strerror(errno));
            close(clientSocket);
            socStatus = -1;
            clientSocket = socket(PF_INET, SOCK_STREAM, 0);
        }
#endif //

       
        count++;
        printf("Count = %d \n",count);
#ifdef PROFILE
        t4 = clock();//millis();
        printTimeTake(t2, t1, "T2 - T1 ");
        printTimeTake(t3,t2, "T3 -T2 ");
        printTimeTake(t4,t3, "T4 -T3 ");

#endif //PROFILE

        }

    bcm2835_spi_end();
    bcm2835_close();
    return 0;
}

