// runs the hifihat with the coefficients provided in c_l & c_r

//kompilieren: gcc -Ofast run.c hifihat.c -o XXX -li2c

// Einbinden von Headerdateien
#include <stdio.h>			// u.a. fuer printf()
#include <unistd.h>		// u.a. fuer NULL
#include <stdlib.h>		// u.a. EXIT_FAILURE/EXIT_SUCCESS
#include <string.h>		// stellt String-Funktionen bereit
#include <signal.h>		// verarbeiten von Signalen (z.B. STRG+C)
//#include<wiringPi.h>
#include "hifihat.h"		// eigene Headerdatei einbinden (u.a. I2C Adressen)


//redeclaration of variable from header file
volatile sig_atomic_t abort_flag;
volatile ulong *p_mmap = NULL;

int main(				// Rueckgabewert: EXIT_SUCCESS bei fehlerfreiem Abschluss der Funktion, sonst EXIT_FAILURE
		int argc,		// Anzahl der per Kommandozeile uebergebenen Argumente
		char *argv[])	// [in] Enthaelt die einzelnen Argumente
{
	int fd_adc_i2c = 0;				// File-Deskriptor fuer den ADC
	int fd_dac_i2c = 0;				// File-Deskriptor fuer den DAC
	int ret = EXIT_SUCCESS;			// Rueckgabewert
	ulong f_sample = 48000;				// Samplerate fuer die Wandler
	int cnt = 0;
    char *c_l = "/home/pi/hifihat/Coeffs/48000/42_comp_bb_l.raw";
    char *c_r = "/home/pi/hifihat/Coeffs/48000/42_comp_bb_r.raw";

    signal(SIGINT, signalHandler); // Signalhandler registrieren fuer SIGINT (STRG+C)


    ret = hifihatSetup(fd_adc_i2c, fd_dac_i2c, f_sample);
    
    while(ret == EXIT_SUCCESS && !abort_flag){
		ret = i2sFilter(c_l,c_r);
	}
		
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

