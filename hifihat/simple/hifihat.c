//contains all necessary functions to operate the hifihat
//


#include <stdio.h>			// u.a. fuer printf()
#include <stdlib.h>	
#include <signal.h>		// verarbeiten von Signalen (z.B. STRG+C)
#include <wiringPi.h>
#include <sys/wait.h>		// u.a. usleep()
#include <sys/types.h>		// Variablentypen, z.B. ulong
#include <unistd.h>		// u.a. fuer NULL
#include <fcntl.h>			// u.a. open()
#include <string.h>		// stellt String-Funktionen bereit
#include <sys/stat.h>		// stat() fuer Dateigroesse
#include <sys/mman.h>		// u.a. mmap
#include <linux/i2c-dev.h>	// fuer I2C Funktionen
#include <sys/ioctl.h>		// fuer ioctl im Rahmen der I2C Nutzung
#include "hifihat.h"


// Schreibt einen 32 Bit Wert (value) an eine Speicheradresse
// (destination), wobei nur die Bitpositionen veraendert werden, die
// in der Maske (mask) 1 sind.
// Beispiel:
// Bit:      7654 3210
// Ziel alt: 1100 1100
// Maske:    0000 0110 (0x6)
// Wert:     1111 0011 (0xF3) - normalerweise aber alle Bits außerhalb
// 								der Maske 0
// Ziel neu: 1100 1010  Bits 1 und 2, dort wo in der Maske
// eine eins ist, uebernehmen den Inhalt von Wert

void writeMemMaskedBit(				// kein Rueckgabewert (da call-by-reference)
		volatile ulong *destination,	// [in/out] Zeiger auf die Speicheradresse
		ulong mask,						// Maske, nur die auf 1 gesetzten Bitpositionen werden veraendert
		ulong value)					// neuer Registerinhalt
{
	ulong current = *destination;	// Zwischenspeicher, verhindert dass zwischendurch im Ziel
									// kurzzeitig Nullen im Masken-Bereich geschrieben werden

	// die per Maske ausgewaehlten Bitpositionen des alten
	// Speicherinhaltes erst auf 0 setzen (bitweises Oder) und dann
	// mit dem neuen Werten ueberschreiben (mittels logischem Oder)
	ulong new = (current & (~mask)) | (value & mask);

	*destination = new;
	*destination = new; // Schreiben ueber Adressraumgrenzen
}

//=====================================================================

// Schreibt einen 32 Bit Wert (value) in ein Register (reg) des mittels
// fd adressierten I2C-ICs.
// Dabei werden nur die Bitpositionen veraendert, die in der Maske
// (mask) 1 sind.
// Funktion aehnlich writeMemMaskedBit(), siehe oben.

void writeI2cMaskedBit(	// kein Rueckgabewert (call-by-reference)
		int fd,				// File-Deskriptor fuer das anzusprechende I2C-IC
		int reg,			// Register, dessen Inhalt veraendert werden soll
		int mask,			// Maske, nur die auf 1 gesetzten Bitpositionen werden veraendert
		int value)			// neuer Registerinhalt
{
	int new_val = 0x00; // enthaelt am Ende den neuen (zu schreibenen) Registerinhalt
	int current = i2c_smbus_read_byte_data(fd, reg); // aktueller (ausgelesener) Registerinhalt

	new_val = (current & (~mask)) | (value & mask);
	i2c_smbus_write_byte_data(fd, reg, new_val);
}

//=====================================================================

// Erstellt einen File-Deskriptor fuer ein I2C-IC und prueft ob
// das IC erreichbar ist.

int i2cGetFD(			// Rueckgabewert: -1 bei Fehler, eine nicht-negative Zahl (File-Deskriptor) bei Erfolg
		int i2c_addr)	// Adresse des I2C-IC
{
	int fd; 				// File-Desktriptor
	unsigned long funcs;	// Rueckgabewert von ioctl (I2C Funktionen)

	//Geraetedatei oeffnen
	if ((fd = open("/dev/i2c-1", O_RDWR)) < 0) { // gilt fuer Raspberry Pi 3, aeltere Modelle: /dev/i2c-0
		perror("I2C open() fehlgeschlagen");
		fd = -1;
	}

	// I2C Funktionen pruefen (da nicht sichergestellt ist, dass alle
	// I2C Funktionen implementiert sind (siehe
	// www.kernel.org/doc.Documentation/i2c/functionality)
	else if (ioctl(fd,I2C_FUNCS, &funcs) < 0) {
		// Abfrage der angebotenen Funktionen fehlgeschlagen
		perror("ioctl() I2C_FUNCS fehlgeschlagen");
		fd = -1;
		}
//	else if(!(funcs & I2C_FUNC_I2C)) {
	else if(!funcs) {
		// Plain i2c-level Kommandos nicht verfuegbar
		printf("I2C Funktionen nicht verfuegbar\n");
		fd = -1;
	}
//	else if (!(funcs & (I2C_FUNC_SMBUS_BYTE))) {
	else if (!funcs) {
		// Byte-Funktionen nicht verfuegbar
		printf("I2C Funktionen read_byte/write_byte nicht verfuegbar\n");
		fd = -1;
	}
//	else if (!(funcs & (I2C_FUNC_SMBUS_BYTE_DATA))) {
	else if (!funcs) {
		// Byte-Data-Funktionen nicht verfuegbar
		printf("I2C Funktionen read_byte_data/write_byte_data nicht verfuegbar\n");
		fd = -1;
	}
	//Dem File-Desktriptor eine I2C Adresse zuordnen
	else if (ioctl(fd, I2C_SLAVE, i2c_addr) < 0) {
		// Zuordnung fehlgeschlagen
		perror("ioctl() I2C_SLAVE fehlgeschlagen");
		fd = -1;
	}
	//Zugriff auf das Geraet pruefen (Lesen des Registers 0x00)
	else if (i2c_smbus_read_byte_data(fd, 0x00) < 0) {
		// Geraet nicht erreichbar
		printf("I2C Geraet an Adresse %d (dezimal) nicht erreichbar.\n", i2c_addr);
		fd = -1;
	}
	return fd; //Geraet erreichbar fd >= 0 oder -1 bei Fehler
}

// Die Funktion wartet zwei PCM-Takte (BCK) und beendet sich danach.
// Die Wartezeit wird gestartet durch Aendern des Registerinhaltes
// PCM_CS_A, Bitposition 24.
// Nach zwei Takten kann der geaenderte Inhalt zurueckgelesen werden.
// Funktioniert nur, wenn das Interface eingeschaltet ist (und BCK
// empfangen wird oder selbst erstellt wird).

void waitForSyncBit()
{
	if(p_mmap[PCM_CS_A] & PCM_CS_A__SYNC__MASK) {
		// Sync Bit ist aktuell gesetzt - Sync Bit loeschen
		writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__SYNC__MASK,
				(0 << PCM_CS_A__SYNC__BASE));
		while ((p_mmap[PCM_CS_A] & PCM_CS_A__SYNC__MASK) && abort_flag == FALSE)
			;	// warten bis Sync Bit als geloescht zurueck gemeldet wird (nach 2 PCM/I2S Takten)
	}
	else {
		// Sync Bit ist aktuell nicht gesetzt - Sync Bit setzen
		writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__SYNC__MASK,
				(1 << PCM_CS_A__SYNC__BASE));
		while ((!(p_mmap[PCM_CS_A] & PCM_CS_A__SYNC__MASK)) && abort_flag == FALSE)
			;	// warten bis Sync Bit als gesetzt zurueck gemeldet wird (nach 2 PCM/I2S Takten)
	}
	return;
}



//=====================================================================

//Die Funktion stoppt die I2S Empfangs- und Sendefunktion, loescht die Fifos und startet die
//Empfangsfunktion nach Ablauf einer Wartezeit neu.

void i2sRestartStream(){
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXON__MASK, PCM_CS_A__TXON__VAL(0)); // TX OFF
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXON__MASK, PCM_CS_A__RXON__VAL(0)); // RX OFF
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXCLR__MASK, PCM_CS_A__TXCLR__VAL(1)); // TX Fifo loeschen
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXCLR__MASK, PCM_CS_A__RXCLR__VAL(1)); // RX Fifo loeschen
	waitForSyncBit();
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXON__MASK, PCM_CS_A__RXON__VAL(1)); // RX ON
	return;
}


//=====================================================================

//Filter in Festkommaarithmetik (keine Nachkommastelle), 24 Bit Werte

long firFilter(				// Rueckgabe: (long) (sample_return>>23) Ausgabewert des FIR-Filters als 24 Bit-Wert (LSB)
		long *p_coefficients,	// [in] Zeiger auf Filter-Koeffizienten (Koeffizienten sind konstant)
		long *p_buffer,			// [in/out] Zeiger auf Ringpuffer der Datenwerte
		long *p_offset,			// [in/out] Zeiger auf Offsetwert (Offset legt die Position des neuen Datenwertes im Ringpuffer fest
		long n_coefficients,	// Anzahl der Koeffizienten (definiert die Anzahl der Speicherplaetze fuer Koeffizienten und Puffer)
		long value_read)		// neu eingelesener Datenwert, wird an Position Offset in den Ringspeicher eingefuegt
{

	long long  sample_return = 0; 			// Rueckgabewert (Aufsummieren der einzelnen Multiplikationen) (64bit Wert)
	long *p_coeff = p_coefficients;			// Zeigt (anfaenglich) auf den ersten Koeffizienten
	long *p_coefficients_end = p_coefficients + n_coefficients;	// Zeigt auf den letzten Koeffizienten
	long *p_sample = p_buffer + *p_offset;	// Zeigt (anfaenglich) auf den Speicherplatz fuer das neue Datum

	*p_sample = value_read; // Datum einfuegen (ueberschreibt das aelteste Datum)

	// Durchlaufen des Ringpuffers in absteigender Reihenfolge und Multiplizieren des jeweiligen Wertes mit einem Koeffizienten.
	// Der Pointer des Ringspeichers wird bei jedem Durchlauf auf die naechst niedrigere Adresse gesetzt und der Pointer der
	// Koeffizienten auf die naechst hoehere Adresse.
	// Wird das untere Ende des Ringspeichers erreicht, endet die Schleife.
	while(p_sample >= p_buffer)
		sample_return += (long long) *p_sample-- * (long long) *p_coeff++;

	p_sample = p_buffer + n_coefficients-1; //zeigt nun auf das obere Ende des Ringspeichers

	// Durchlaufen des Ringpuffers in absteigender Reihenfolge und Multiplizieren des jeweiligen Wertes mit einem Koeffizienten.
	// Wird das obere Ende des Koeffizientenspeichers erreicht, endet die Schleife. Damit ist der Ringpuffer
	// einmal vollstaendig durchlaufen (da Ringspeicher und Koeffizientenspeicher gleich viele Speicherstellen besitzen.
	while(p_coeff < p_coefficients_end)
		sample_return += (long long) *p_sample-- * (long long) *p_coeff++;

	// Offsetwert um eins erhöhen. Falls der Offset das obere Ende des Ringpuffers erreicht hat, wird er fuer den
	// naechsten Durchlauf wieder auf 0 gesetzt.
	if(++(*p_offset) >= n_coefficients)
		*p_offset = 0;

	return (long) (sample_return>>23); //23 statt 24 mal schieben wg. 2 Vorzeichenbits
}



//Diese Funktion entnimmt I2S Daten aus dem Empfangsfifo und schreibt sie nach FIR-Filterung in den Sendefifo.

int i2sFilter(						// Rueckgabewert: EXIT_SUCCESS bei fehlerfreiem Abschluss der Funktion, sonst EXIT_FAILURE
		//volatile ulong* p_mmap,
		char* coeffs_l_path,
		char* coeffs_r_path)	// [in/out] Zeiger auf den per mmap() geteilten Speicher
{
	int ret = EXIT_SUCCESS;	//Rueckgabewert
	ulong cnt = 0;			// Zaehlvariable
	ulong tx_error_cnt = 0;	// Zaehlvariable
	ulong rx_error_cnt = 0;	// Zaehlvariable
	FILE *p_file = NULL;	// Dateipointer (Auslesen der Koeffizienten)
	long *p_ch_left = NULL;	// (Ring-) Puffer fuer linken Kanal
	long *p_ch_right = NULL;// (Ring-) Puffer fuer rechten Kanal
	long offset_left = 0;	// Offset fuer den Ringpuffer (mit jedem neuen Sample (fuer re/li zusammen) um 1 erhoehen)
	long offset_right = 0;	// Offset fuer den Ringpuffer (mit jedem neuen Sample (fuer re/li zusammen) um 1 erhoehen)
	long *p_coefficients_left = NULL;	// h(k) (24 Bit Woerter in 32 Bit verpackt), aus Datei lesen
	long *p_coefficients_right = NULL;	// h(k) (24 Bit Woerter in 32 Bit verpackt), aus Datei lesen
	long n_coefficients_left = 0;		// Anzahl der Koeffizienten, aus Datei auslesen
	long n_coefficients_right = 0;		// Anzahl der Koeffizienten, aus Datei auslesen
	struct stat attributes;	// enthaelt spaeter u.a. die Dateigroesse der Datei mit den Filterkoeffizienten

	// Koeffizienten auslesen links
	// Datei oeffnen
	if((p_file = fopen(coeffs_l_path, "r")) == NULL) {
		printf("Kann Datei\n%s\nnicht oeffnen. Ende\n", coeffs_l_path);
		return EXIT_FAILURE;
	}

	// Anzahl der Koeffizienten lesen, der Lesezeiger zeigt anschliessend auf den ersten Koeffizienten.
	cnt = fread(&n_coefficients_left, sizeof(long), 1, p_file);
	if(cnt != 1) {
		printf("Fehler beim Lesen der Koeffizientenanzahl (links), Ende\n");
		fclose(p_file);
		return EXIT_FAILURE;
	}
	printf("Anzahl Koeffizienten links soll: %ld\n", n_coefficients_left);

	// Speicher reservieren
	p_coefficients_left = (long *) calloc(n_coefficients_left, sizeof(long));
	if (p_coefficients_left == NULL) {
		perror("Fehler Speicherreservierung Array fuer Koeffizienten (links), Ende\n");
		fclose(p_file);
		return EXIT_FAILURE;
	}

	// Dateieigenschaften ermitteln (fuer Dateigroesse im naechsten Abschnitt benoetigt)
	stat(coeffs_l_path, &attributes);

	// Alle Koeffizienten lesen
	cnt = fread(p_coefficients_left, sizeof(long), n_coefficients_left, p_file);
	if(cnt != n_coefficients_left || (cnt+1)*sizeof(long) != (long) attributes.st_size) {
		printf("Fehler beim Lesen der Koeffizienten (links), Ende\n");
		free(p_coefficients_left);
		fclose(p_file);
		return EXIT_FAILURE;
	}
	printf("Anzahl Koeffizienten links ist: %lu\n", cnt);
	fclose(p_file);

	// Koeffizienten auslesen rechts (Ablauf wie zuvor fuer links, Kommentare siehe oben)
	if((p_file = fopen(coeffs_r_path, "r")) == NULL) {
		printf("Kann Datei \n%s\nnicht oeffnen. Ende\n", coeffs_r_path);
		free(p_coefficients_left);
		return EXIT_FAILURE;
	}

	cnt = fread(&n_coefficients_right, sizeof(long), 1, p_file);
	if(cnt != 1) {
		printf("Fehler beim Lesen der Koeffizientenanzahl (rechts), Ende\n");
		free(p_coefficients_left);
		fclose(p_file);
		return EXIT_FAILURE;
	}
	printf("Anzahl Koeffizienten rechts soll: %ld\n", n_coefficients_right);

	p_coefficients_right = (long *) calloc(n_coefficients_right, sizeof(long));
	if (p_coefficients_right == NULL) {
		perror("Fehler Speicherreservierung Array fuer Koeffizienten (rechts), Ende\n");
		free(p_coefficients_left);
		fclose(p_file);
		return EXIT_FAILURE;
	}

	stat(coeffs_r_path, &attributes);

	cnt = fread(p_coefficients_right, sizeof(long), n_coefficients_right, p_file);
	if(cnt != n_coefficients_right || (cnt+1)*sizeof(long) != (long) attributes.st_size) {
		printf("Fehler beim Lesen der Koeffizienten (rechts), Ende\n");
		free(p_coefficients_left);
		free(p_coefficients_right);
		fclose(p_file);
		return EXIT_FAILURE;
	}
	printf("Anzahl Koeffizienten rechts ist: %lu\n", cnt);
	fclose(p_file);


	// Ringpuffer reservieren links
	p_ch_left = (long *) calloc(n_coefficients_left, sizeof(long));
	if (p_ch_left == NULL) {
		perror("Fehler Speicherreservierung Array fuer Ringpuffer (links), Ende\n");
		free(p_coefficients_left);
		free(p_coefficients_right);
		return EXIT_FAILURE;
	}

	// Ringpuffer reservieren rechts
	p_ch_right = (long *) calloc(n_coefficients_right, sizeof(long));
	if (p_ch_right == NULL) {
		perror("Fehler Speicherreservierung Array fuer Ringpuffer (rechts), Ende\n");
		free(p_coefficients_left);
		free(p_coefficients_right);
		free(p_ch_left);
		return EXIT_FAILURE;
	}


	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__EN__MASK, PCM_CS_A__EN__VAL(1)); //EN
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXCLR__MASK, PCM_CS_A__TXCLR__VAL(1)); //TX Fifo loeschen
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXCLR__MASK, PCM_CS_A__RXCLR__VAL(1)); //RX Fifo loeschen
	waitForSyncBit();
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXON__MASK, PCM_CS_A__RXON__VAL(1)); //RX ON


	cnt = 0;
	offset_left = 0; // starten ohne Offset
	offset_right = 0;

	usleep(5000);
	
	// Datenweiterleitung mit Filterung solange, bis Benutzer abbricht
	while(!abort_flag) {
		// linker Kanal
		while ((!(p_mmap[PCM_CS_A] & PCM_CS_A__RXD__MASK)) && !abort_flag) //warte solang RX Fifo leer ist (= Bit gesetzt)
			; // warten
		// Empfangsfifo auslesen, Ausgangswert berechnen und in Sendefifo schreiben
		p_mmap[PCM_FIFO_A] = firFilter(p_coefficients_left, p_ch_left, &offset_left, n_coefficients_left, (long) p_mmap[PCM_FIFO_A]);

		// rechter Kanal
		while ((!(p_mmap[PCM_CS_A] & PCM_CS_A__RXD__MASK)) && !abort_flag) //warte solang RX Fifo leer ist (= Bit gesetzt)
			; // warten
		// Empfangsfifo auslesen, Ausgangswert berechnen und in Sendefifo schreiben
		p_mmap[PCM_FIFO_A] = firFilter(p_coefficients_right, p_ch_right, &offset_right, n_coefficients_right, (long) p_mmap[PCM_FIFO_A]);

		// nach 2x32 Eintraegen ist Sendefifo voll und meldet Fehler bei weiterem Datum, daher bei 2x31 Eintraegen (0..30) TX aktivieren
		if(cnt == 30)
			writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXON__MASK, (0x1 << PCM_CS_A__TXON__BASE)); // TX ON

		// falls der Empfangs-Fifo uebergelaufen ist: Warnung ausgeben, Stream neu starten und Rueckgabewert auf Fehler setzen
		if (p_mmap[PCM_CS_A] & PCM_CS_A__RXERR__MASK) {
			rx_error_cnt++;
			printf("RX Fifo Error... Schleife: %lu  RX Error Anzahl: %lu, Neustart.\n", cnt, rx_error_cnt);
			writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXERR__MASK, PCM_CS_A__RXERR__VAL(1)); // Flag zuruecksetzen (1 schreiben)
			i2sRestartStream();
			cnt = 0;
			ret = EXIT_FAILURE;
		}
		// falls der Sende-Fifo leergelaufen ist: Warnung ausgeben, Stream neu starten und Rueckgabewert auf Fehler setzen
		if (p_mmap[PCM_CS_A] & PCM_CS_A__TXERR__MASK) {
			tx_error_cnt++;
			printf("TX Fifo Error... Schleife: %lu  TX Error Anzahl: %lu, Neustart.\n", cnt, tx_error_cnt);
			writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXERR__MASK, PCM_CS_A__TXERR__VAL(1)); // Flag zuruecksetzen (1 schreiben)
			i2sRestartStream();
			cnt = 0;
			ret = EXIT_FAILURE;
		}
		cnt++;
	}
	free(p_ch_left);
	free(p_ch_right);
	free(p_coefficients_left);
	free(p_coefficients_right);

		return ret;
}

// Diese Funktion entnimmt I2S Daten aus dem Empfangsfifo und schreibt
// sie unveraendert in den Sendefifo.
// Rueckgabewert: EXIT_SUCCESS bei fehlerfreiem Abschluss der Funktion, sonst EXIT_FAILURE

int i2sForward()
{
	int ret = EXIT_SUCCESS;	// Rueckgabe-Wert
	ulong cnt = 0;			// Zaehl-Variable
	ulong tx_error_cnt = 0;	// Zaehl-Variable
	ulong rx_error_cnt = 0;	// Zaehl-Variable

	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__EN__MASK, PCM_CS_A__EN__VAL(1)); //EN
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXCLR__MASK, PCM_CS_A__TXCLR__VAL(1)); //TX Fifo loeschen
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXCLR__MASK, PCM_CS_A__RXCLR__VAL(1)); //RX Fifo loeschen
	waitForSyncBit();
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXON__MASK, (0x1 << PCM_CS_A__RXON__BASE)); //RX ON
	//TX ON nicht hier, sondern weiter unten nach dem Fuellen des Fifo

	cnt = 0;
	//Datenweiterleitung solange, bis Benutzer abbricht
	while(!abort_flag) {
		//Datenwort fuer linken Kanal aus Empfangs-Fifo auslesen und den Sende-Fifo schreiben, sofern Platz ist
		while ((!(p_mmap[PCM_CS_A] & PCM_CS_A__RXD__MASK)) && !abort_flag) //warte solang RX Fifo leer ist (= Bit gesetzt)
			; // warten
		p_mmap[PCM_FIFO_A] = (ulong) p_mmap[PCM_FIFO_A];

		//Datenwort fuer rechten Kanal aus Empfangs-Fifo auslesen und den Sende-Fifo schreiben, sofern Platz ist
		while ((!(p_mmap[PCM_CS_A] & PCM_CS_A__RXD__MASK)) && !abort_flag) //warte solang RX Fifo leer ist (= Bit gesetzt)
			; //warten
		p_mmap[PCM_FIFO_A] = (ulong) p_mmap[PCM_FIFO_A];

		// nach 2x32 Eintraegen ist Sendefifo voll und meldet Fehler bei weiterem Datum, daher bei 2x31 Eintraegen (0..30) TX aktivieren
		if(cnt == 30)
			writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXON__MASK, PCM_CS_A__TXON__VAL(1)); //TX ON

		//falls der Empfangs-Fifo uebergelaufen ist: Warnung ausgeben, Stream neu starten und Rueckgabewert auf Fehler setzen
		if (p_mmap[PCM_CS_A] & PCM_CS_A__RXERR__MASK) {
			rx_error_cnt++;
			printf("RX Fifo Error... Schleife: %lu  RX Error Anzahl: %lu, Neustart.\n", cnt, rx_error_cnt);
			writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXERR__MASK, PCM_CS_A__RXERR__VAL(1)); // Flag zuruecksetzen (1 schreiben)
			i2sRestartStream();
			cnt = 0;
			ret = EXIT_FAILURE;
		}
		//falls der Sende-Fifo leergelaufen ist: Warnung ausgeben, Stream neu starten und Rueckgabewert auf Fehler setzen
		if (p_mmap[PCM_CS_A] & PCM_CS_A__TXERR__MASK) {
			tx_error_cnt++;
			printf("TX Fifo Error... Schleife: %lu  TX Error Anzahl: %lu, Neustart.\n", cnt, tx_error_cnt);
			writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXERR__MASK, PCM_CS_A__TXERR__VAL(1)); // Flag zuruecksetzen (1 schreiben)
			i2sRestartStream();
			cnt = 0;
			ret = EXIT_FAILURE;
		}
		cnt++;
	}

	return ret;
}



//Diese Funktion stellt die nötigen Voraussetzungen für die Verwendung des HifiHat her. 
//		int fd_mmap = 0;			// File-Deskriptor fuer Shared Memory
//	int fd_adc_i2c = 0;				// File-Deskriptor fuer den ADC
//	int fd_dac_i2c = 0;				// File-Deskriptor fuer den DAC
// ulong f_sample = 48000			// Sampling Rate

//returns the pointer necessary to carry out filtering

//volatile ulong * 
int hifihatSetup(//volatile ulong * p_mmap,
				int fd_adc_i2c,
				int fd_dac_i2c,
				ulong f_sample){

	int fd_mmap = 0;
	int i2s_action = I2S_FILTER;	// speichert den vom Nutzer gewaehlten Funktionswunsch (siehe #define I2S_...)
	int ret = EXIT_SUCCESS;			// Rueckgabewert
	FILE *p_file = NULL;			// Zeiger fuer Datei-Handle
//	volatile ulong *p_mmap = NULL;	// Zeiger auf den per mmap() geteilten Speicher
	ulong cnt = 0;					// Zaehl-Variable
	
	
	printf("\nHiFi-HAT Interface\n\n");	
	
	
	if((p_file = fopen("/run/lock/hifihat.lock", "r"))) {
		printf("Fehler: Lock-Datei /run/lock/hifihat.lock existiert schon, weitere Instanz aktiv?\n");
		printf("Datei ggf. manuell loeschen. Programmende.\n");
		fclose(p_file);
		return EXIT_FAILURE;
	} else {
		p_file = fopen("/run/lock/hifihat.lock", "w");
		fprintf(p_file, "%d (PID that created this file)",getpid());
		fclose(p_file);
	}
	
	
		// I2C Repeated Start Condition bzw. Combined Read/Write setzen (PCM1863 benoetigt dies um Registerinhalte auszulesen)
	p_file = fopen("/sys/module/i2c_bcm2708/parameters/combined", "w"); //Datei ueberschreiben
	if(p_file == NULL) {
		perror("Fehler bei Zugriff auf /sys/module/i2c_bcm2708/parameters/combined");
		printf("Ende\n");
		ret = EXIT_FAILURE;
	} else {
		cnt = fwrite("Y", sizeof(char), 1, p_file); //Combined write/read aktivieren
		if(cnt != 1) {
				printf("Fehler beim Schreiben in Datei /sys/module/i2c_bcm2708/parameters/combined, Ende");
				fclose(p_file);
				ret = EXIT_FAILURE;
		}
	fclose(p_file);
	}
	
	if (ret != EXIT_FAILURE) {
		// DAC File-Deskriptor erstellen und IC konfigurieren
		if ((fd_dac_i2c = i2cGetFD(I2C_ADDR_DAC)) < 0) {
			printf("DAC konnte nicht angesprochen werden. Ende\n");
			ret = EXIT_FAILURE;
		} else {
			writeI2cMaskedBit(fd_dac_i2c, 0x02, 0x01, 0x00); // DAC aus Powerdown aufwecken
			usleep(5000);

			// DEBUG Mute bei fehlenden Eingangsdaten abschalten
			//writeI2cMaskedBit(fd_dac_i2c, 0x41, 0x01, 0x00); // rechter Kanal automute aus
			//writeI2cMaskedBit(fd_dac_i2c, 0x41, 0x02, 0x00); // linker Kanal automute aus
			//printf("Debug/Messen: DAC Automute ausgeschaltet. Fuer normalen Betrieb einschalten...\n");
		}
	}

	if (ret != EXIT_FAILURE) {
		// ADC File-Deskriptor erstellen und IC konfigurieren
		if ((fd_adc_i2c = i2cGetFD(I2C_ADDR_ADC)) < 0) {
			printf("ADC konnte nicht angesprochen werden. Ende\n");
			ret = EXIT_FAILURE;
		} else {
			writeI2cMaskedBit(fd_adc_i2c, 0x70, 0x01, 0x00); // ADC aus Standby aufwecken
			usleep(5000);
			writeI2cMaskedBit(fd_adc_i2c, 0x20, 0x10, 0x10); // ADC auf Master
			writeI2cMaskedBit(fd_adc_i2c, 0x06, 0x3f, 0x48); // linken Kanal auf Klinkenbuchse
			writeI2cMaskedBit(fd_adc_i2c, 0x07, 0x3f, 0x48); // rechten Kanal auf Klinkenbuchse
		}
	}

	if (ret != EXIT_FAILURE) {
		// Shared Memory einrichten (16 MB Adressumfang, lt. Datenblatt Raspberry Pi)
		if((fd_mmap=open("/dev/mem",O_RDWR|O_SYNC)) == -1) {
			perror("I2S open");
			ret = EXIT_FAILURE;
		} else if((p_mmap = (ulong*) mmap(0, 0xFFFFFF, PROT_READ|PROT_WRITE,MAP_SHARED,fd_mmap,BCMxxxx_PERI_BASE)) < 0) {
				perror("mmap Zugriff fehlgeschlagen\n");
				ret = EXIT_FAILURE;
		}
	}

	// GPIOs auf PCM/I2S (ALT0) schalten
	writeMemMaskedBit(&p_mmap[GPIO_GPFSEL1], GPIO_GPFSEL1__FSEL18__MASK, GPIO_GPFSEL1__FSEL18__VAL(0x4)); // Pin 18: (Bits 26..24): 100 setzen
	writeMemMaskedBit(&p_mmap[GPIO_GPFSEL1], GPIO_GPFSEL1__FSEL19__MASK, GPIO_GPFSEL1__FSEL19__VAL(0x4)); // Pin 19: (Bits 27..29): 100 setzen
	writeMemMaskedBit(&p_mmap[GPIO_GPFSEL2], GPIO_GPFSEL2__FSEL20__MASK, GPIO_GPFSEL2__FSEL20__VAL(0x4)); // Pin 20: (Bits 0..2): 100 setzen
	writeMemMaskedBit(&p_mmap[GPIO_GPFSEL2], GPIO_GPFSEL2__FSEL21__MASK, GPIO_GPFSEL2__FSEL21__VAL(0x4)); // Pin 21: (Bits 3..5): 100 setzen

	// I2S Schnittstelle Raspberry Pi konfigurieren
	// Bevor Parameter geaendert werden I2S Schnittstelle anhalten
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__EN__MASK, PCM_CS_A__EN__VAL(0));
	// RX OFF
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXON__MASK, PCM_CS_A__RXON__VAL(0));
	// TX OFF
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__TXON__MASK, PCM_CS_A__TXON__VAL(0));
	usleep(1000);

	// PCM_CLK auf Eingang / Slave
	writeMemMaskedBit(&p_mmap[PCM_MODE_A], PCM_MODE_A__CLKM__MASK, PCM_MODE_A__CLKM__VAL(1));
	// PCM_FS auf Eingang / Slave
	writeMemMaskedBit(&p_mmap[PCM_MODE_A], PCM_MODE_A__FSM__MASK, PCM_MODE_A__FSM__VAL(1));

	// RX Sign Extend setzen
	writeMemMaskedBit(&p_mmap[PCM_CS_A], PCM_CS_A__RXSEX__MASK, PCM_CS_A__RXSEX__VAL(1));

	// CLK invertieren (und damit Samplen bei fallender Flanke von BCK/PCM_CLK
	writeMemMaskedBit(&p_mmap[PCM_MODE_A], PCM_MODE_A__CLKI__MASK, PCM_MODE_A__CLKI__VAL(1));

	// FS invertieren (und damit linken Kanal (1) samplen bei fallender Flanke von LRCK/PCM_FS
	writeMemMaskedBit(&p_mmap[PCM_MODE_A], PCM_MODE_A__FSI__MASK, PCM_MODE_A__FSI__VAL(1));

	// Empfangsdaten konfigurieren
	writeMemMaskedBit(&p_mmap[PCM_RXC_A], PCM_RXC_A__CH1WID__MASK, PCM_RXC_A__CH1WID__VAL(0)); //Bit 19..16: 0000
	writeMemMaskedBit(&p_mmap[PCM_RXC_A], PCM_RXC_A__CH1WEX__MASK, PCM_RXC_A__CH1WEX__VAL(1)); //Bit 31: 1

	// CH1POS = 1 Position des ersten Bit im Frame setzen (hier nach 1 Takt, d.h. 1 setzen)
	writeMemMaskedBit(&p_mmap[PCM_RXC_A], PCM_RXC_A__CH1POS__MASK, PCM_RXC_A__CH1POS__VAL(1)); //Bit 29..20: 0000000001

	// CH2WID + CH2WEX 24-8 = 16 = 10000
	writeMemMaskedBit(&p_mmap[PCM_RXC_A], PCM_RXC_A__CH2WID__MASK, PCM_RXC_A__CH2WID__VAL(0)); //Bit 3..0: 0000
	writeMemMaskedBit(&p_mmap[PCM_RXC_A], PCM_RXC_A__CH2WEX__MASK, PCM_RXC_A__CH2WEX__VAL(1)); //Bit 15: 1

	// CH2POS = 1 Position des ersten Bit im Frame setzen (hier nach 32+1, d.h. 33 setzen)
	writeMemMaskedBit(&p_mmap[PCM_RXC_A], PCM_RXC_A__CH2POS__MASK, PCM_RXC_A__CH2POS__VAL(0x21)); //Bit 13..4: 0000100001

	// CH1 und 2 EN
	writeMemMaskedBit(&p_mmap[PCM_RXC_A], PCM_RXC_A__CH1EN__MASK, PCM_RXC_A__CH1EN__VAL(1)); //Bit 30 setzen CH1
	writeMemMaskedBit(&p_mmap[PCM_RXC_A], PCM_RXC_A__CH2EN__MASK, PCM_RXC_A__CH2EN__VAL(1)); //Bit 14 setzen CH2

	// Sendedaten konfigurieren
	// CH1WID + CH1WEX 24-8 = 10000
	writeMemMaskedBit(&p_mmap[PCM_TXC_A], PCM_TXC_A__CH1WID__MASK, PCM_TXC_A__CH1WID__VAL(0)); //Bit 19..16: 0000
	writeMemMaskedBit(&p_mmap[PCM_TXC_A], PCM_TXC_A__CH1WEX__MASK, PCM_TXC_A__CH1WEX__VAL(1)); //Bit 31: 1

	// CH1POS = 1 Position des ersten Bit im Frame setzen (hier nach 1 Takt)
	writeMemMaskedBit(&p_mmap[PCM_TXC_A], PCM_TXC_A__CH1POS__MASK, PCM_TXC_A__CH1POS__VAL(1)); //Bit 29..20: 0000000001

	// CH2WID + CH2WEX 24 = 10000
	writeMemMaskedBit(&p_mmap[PCM_TXC_A], PCM_TXC_A__CH2WID__MASK, PCM_TXC_A__CH2WID__VAL(0)); //Bit 3..0: 0000
	writeMemMaskedBit(&p_mmap[PCM_TXC_A], PCM_TXC_A__CH2WEX__MASK, PCM_TXC_A__CH2WEX__VAL(1)); //Bit 15: 1

	// CH2POS = 1 Position des ersten Bit im Frame setzen (hier nach 32+1)
	writeMemMaskedBit(&p_mmap[PCM_TXC_A], PCM_TXC_A__CH2POS__MASK, PCM_TXC_A__CH2POS__VAL(0x21)); //Bit 13..4: 0000100001

	// CH1 und 2 EN
	writeMemMaskedBit(&p_mmap[PCM_TXC_A], PCM_TXC_A__CH1EN__MASK, PCM_TXC_A__CH1EN__VAL(1)); //Bit 30 setzen CH1
	writeMemMaskedBit(&p_mmap[PCM_TXC_A], PCM_TXC_A__CH2EN__MASK, PCM_TXC_A__CH2EN__VAL(1)); //Bit 14 setzen CH2

	
	//Set the samplerate
	switch(f_sample){
		case 44100:
			writeI2cMaskedBit(fd_dac_i2c, 0x08, 0x20, 0x1 << 5 );	// GPIO6 aktivieren
			writeI2cMaskedBit(fd_dac_i2c, 0x55, 0x1F, 0x02 << 0 );	// GPIO6 auf Ausgang
			writeI2cMaskedBit(fd_dac_i2c, 0x56, 0x20, 0x1 << 5 );	// GPIO6 high
		case 48000:
			writeI2cMaskedBit(fd_dac_i2c, 0x08, 0x20, 0x1 << 5 );	// GPIO6 aktivieren
			writeI2cMaskedBit(fd_dac_i2c, 0x55, 0x1F, 0x02 << 0 );	// GPIO6 auf Ausgang
			writeI2cMaskedBit(fd_dac_i2c, 0x56, 0x20, 0x0 << 5 );	// GPIO6 low
	}
	//Samplerate :48000
	writeI2cMaskedBit(fd_adc_i2c, 0x26, 0x7F, 7 << 0); // BCK = 1/8 MCLK
	
	usleep(50000); // 50ms warten (z.B. bei Wechsel der Samplerate brauchen die Wandler einen Moment)

	printf("\nSetup complete!\n\n");	
	
	return ret;
//	return p_mmap;

}


//Shutdown function
void hifihatShutdown(int fd_adc_i2c, int fd_dac_i2c){

	writeI2cMaskedBit(fd_dac_i2c, 0x02, 0x01, 0x01); // DAC auf Powerdown
	usleep(5000);
	writeI2cMaskedBit(fd_adc_i2c, 0x70, 0x01, 0x01); // ADC auf Standby
	
	if(fd_dac_i2c >= 0)
		close(fd_dac_i2c);
	if(fd_adc_i2c >= 0)
		close(fd_adc_i2c);
	printf("\nRemoving /run/lock/hifihat.lock\n");
	remove("/run/lock/hifihat.lock");
	
	//free allocated memory for coefficient path
	//free();
}
