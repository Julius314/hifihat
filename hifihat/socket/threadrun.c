// Thread implementation of hifihat to change coefficients during operation


//kompilieren: gcc -Ofast run.c hifihat.c -o XXX -li2c -lpthread

// Einbinden von Headerdateien
#include <stdio.h>			// u.a. fuer printf()
#include <unistd.h>		// u.a. fuer NULL
#include <stdlib.h>		// u.a. EXIT_FAILURE/EXIT_SUCCESS
#include <string.h>		// stellt String-Funktionen bereit
#include <signal.h>		// verarbeiten von Signalen (z.B. STRG+C)

#include <pthread.h>

#include <sys/socket.h> 
#include <netinet/in.h> 
//#include<wiringPi.h>
#include "hifihat.h"		// eigene Headerdatei einbinden (u.a. I2C Adressen)

#define PORT 3141 // port for socket listener

//redeclaration of variable from header file
volatile sig_atomic_t abort_flag;
volatile ulong *p_mmap = NULL;
volatile int socketInt = 0;

//default filter coefficients
char *c_l = "/home/pi/hifihat/Coeffs/48000/42_comp_bb_l.raw";
char *c_r = "/home/pi/hifihat/Coeffs/48000/42_comp_bb_r.raw";

//function responsible for filtering
void *f_filter(void *vargp){
	int ret = EXIT_SUCCESS;

	while(ret == EXIT_SUCCESS && !abort_flag){
		socketInt = 0;
		ret = i2sFilter(c_l,c_r);
	}
}

//function responsible for socket listening
//changing the coefficients
// interrupting the filter 
void *f_socket(void *vargp){
    int server_fd, new_socket, valread; 
    struct sockaddr_in address; 
    int opt = 1; 
    int addrlen = sizeof(address); 
    char buffer[1024] = {0}; 
    char *response = "Wrong message sent."; 
       
    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                  &opt, sizeof(opt))) 
    { 
        perror("setsockopt failed"); 
        exit(EXIT_FAILURE); 
    } 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons( PORT ); 
       
    // Forcefully attaching socket to the port 
    if (bind(server_fd, (struct sockaddr *)&address,  
                                 sizeof(address))<0) 
    { 
        perror("socket bind failed"); 
        exit(EXIT_FAILURE); 
    }
    while(!abort_flag){
	
        if (listen(server_fd, 3) < 0) 
        { 
            perror("socket listen error"); 
            exit(EXIT_FAILURE); 
        }
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
                        (socklen_t*)&addrlen))<0) 
        { 
            perror("socket accept error"); 
            exit(EXIT_FAILURE); 
        }
		valread = read( new_socket , buffer, 1024); 


        //printf("%s\n\n",buffer ); 

		if(strcmp(buffer,"c0") == 0){
			response = "Switching to flat response.\n";
			c_l = "/home/pi/hifihat/Coeffs/48000/42_comp_l.raw";
			c_r = "/home/pi/hifihat/Coeffs/48000/42_comp_r.raw";

			socketInt = 1;
		}			
		else if(strcmp(buffer,"c1") == 0){
			response = "Switching to flat bass boost.\n";
			c_l = "/home/pi/hifihat/Coeffs/48000/42_comp_bb_l.raw";
			c_r = "/home/pi/hifihat/Coeffs/48000/42_comp_bb_r.raw";

			socketInt = 1;
		}			
		else if(strcmp(buffer,"c2") == 0){
			response = "Switching to less base.\n";
			c_l = "/home/pi/hifihat/Coeffs/48000/42_nobase_l.raw";
			c_r = "/home/pi/hifihat/Coeffs/48000/42_nobase_r.raw";

			socketInt = 1;
		}		
		else if(strcmp(buffer,"c3") == 0){
			response = "Switching to flat unequalized.\n";
			c_l = "/home/pi/hifihat/Coeffs/48000/42_cross_l.raw";
			c_r = "/home/pi/hifihat/Coeffs/48000/42_cross_r.raw";

			socketInt = 1;
		}			
		else{
			response = "Wrong message sent.\n";
			socketInt = 0;
		}

		printf(response);
		send(new_socket , response , strlen(response) , 0 ); 
	}
}



int main(				// Rueckgabewert: EXIT_SUCCESS bei fehlerfreiem Abschluss der Funktion, sonst EXIT_FAILURE
		int argc,		// Anzahl der per Kommandozeile uebergebenen Argumente
		char *argv[])	// [in] Enthaelt die einzelnen Argumente
{
	int fd_adc_i2c = 0;				// File-Deskriptor fuer den ADC
	int fd_dac_i2c = 0;				// File-Deskriptor fuer den DAC
	int ret = EXIT_SUCCESS;			// Rueckgabewert
	ulong f_sample = 48000;				// Samplerate fuer die Wandler
	int cnt = 0;

    signal(SIGINT, signalHandler); // Signalhandler registrieren fuer SIGINT (STRG+C)

	pthread_t t_filter;
	pthread_t t_socket;

	ret = hifihatSetup(fd_adc_i2c, fd_dac_i2c, f_sample);
    
	pthread_create(&t_filter, NULL, f_filter,NULL);
    pthread_create(&t_socket, NULL, f_socket, NULL);
	pthread_join(t_filter,NULL);
	
	pthread_kill(t_socket,SIGINT); //stops the socket block
	//pthread_join(t_socket,NULL);

	hifihatShutdown(fd_adc_i2c,fd_dac_i2c);
	
	return ret;
		
}	

// Setzt das globale Abbrechen Flag auf TRUE, der Signaltyp wird
// nicht ausgewertet.
void signalHandler( 	// kein Rueckgabewert (da globale Variable)
		int signal)		// Signaltyp
{
	abort_flag = 1;
	printf("\nWird beendet...\n");
}

