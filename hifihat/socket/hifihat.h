/*
 * hifihat.h
 *
 * Version 1.0
 * Datum: 28.02.2017
 * Erstellt von: Sebastian Albers
 * Modifiziert von: Julius Rauscher
 * Beschreibung: Headerdatei zur Operation des Hifihat
 * .c. Beschreibung siehe dort.
 */

//=====================================================================

#ifndef HIFIHAT_H_
#define HIFIHAT_H_
#endif /* HIFIHAT_H_ */

//=====================================================================

// Hier koennen die Geraeteadressen von ADC und DAC angegeben werden.

#define I2C_ADDR_ADC 0x4a // I2C Adresse des ADC
#define I2C_ADDR_DAC 0x4c // I2C Adresse des DAC

//=====================================================================

#define LINE_LENGTH 250		// maximale Laenge einer Zeile der DSP-Konfigurationsdatei (Puffergroesse)

//=====================================================================

// Boolean gibt es in C nicht, hier per Integer nachbilden

#define BOOL int
//#define FALSE 0
//#define TRUE 1

//=====================================================================

// Kommandozeilenparameter wird in der main-Funktion in diese
// symbolischen Konstanten uebersetzt

#define I2S_NONE_SET 0
#define I2S_FILE_READ 1
#define I2S_FILE_WRITE 2
#define I2S_FORWARD 3
#define I2S_FILTER 4
#define I2S_PASSIVE 5
#define I2S_DSP_CONFIG 6
#define I2S_DAC_RESET 7

//=====================================================================

// Startadresse der mmap() Funktion. Erklärung:
// BCM2835-ARM-Peripherals.pdf, Seite 6: Physikalische Adressen fuer
// Peripheriegeraete (wie GPIO und I2S Schnittstellen. Fuer Raspberry
// Pi Modelle mit 512MB Ram (z.B. Modell B+) lautet die Adresse laut
// Datenblatt 0x2000 0000. Das entspricht der Adresse oberhalb von
// 512 MB Ram. Fuer das Modell 3 muss jedoch die Adresse 0x3F00 0000
// gewaehlt werden. Dies entspricht der Adresse oberhalb von
// 1 GB - 16 MB Ram. D.h. die oberen 16 MB Ram sind nicht nutzbar,
// statt dessen liegen hier die Adressen der Peripheriegeraete.

#define BCMxxxx_PERI_BASE 0x3F000000

//=====================================================================

// Adressen fuer die GPIO Schnittstellen
// Die Bezeichnungen entsprechen den Bezeichnungen im Datenblatt.

#define GPIO_BASE (0x200000)	// Startadresse des GPIO-Registers relativ zu mmap (ergibt 0x3F000000 + 0x200000)

#define GPIO_GPFSEL1 ((GPIO_BASE + 0x4)/4)	// GPFSEL1-Register Adresse, relativ zu mmap
#define GPIO_GPFSEL2 ((GPIO_BASE + 0x8)/4)	// GPFSEL2-Register Adresse, relativ zu mmap

// Die folgenden Makros helfen zusammen mit der Funktion
// writeMemMaskedBit() bei der einfachen Ansprache der diversen ueber
// Register gesteuerten Peripheriegeraete des Raspberry.
// Es gibt jeweils 3 Zeilen fuer eine Funktion. Z.B. kann der GPIO
// Pin 18 wie folgt beeinflusst werden:
// Pin 18 wird gesteuert ueber das Feld FSEL18 (Bits 24 bis 26) in
// Register GPFSEL1 (S. 92 im Datenblatt).
// Das erste Makro definiert die Bitposition im Register. Mit
// Makro zwei kann ab dieser Bitposition ein neuer Wert geschrieben
// werden. Mit Makro drei wird eine Maske definiert (hier die
// 3 Bits (0x7) ab Position 24. Die Makros zwei und drei koennen der
// Funktion writeMemMaskedBit() uebergeben werden, wodurch die nur die
// relevanten Bereiche des Registers GPFSEL1 mit einem neuen Wert
// ueberschrieben werden und der Rest des Registers unveraendert
// bleibt.

#define GPIO_GPFSEL1__FSEL18__BASE 24 // FSL18 Start-Bit im Register GPFSEL1 (fuer GPIO 18)
#define GPIO_GPFSEL1__FSEL18__VAL(val) (val << GPIO_GPFSEL1__FSEL18__BASE) // neuer Wert
#define GPIO_GPFSEL1__FSEL18__MASK (0x7 << GPIO_GPFSEL1__FSEL18__BASE) // Maske

#define GPIO_GPFSEL1__FSEL19__BASE 27
#define GPIO_GPFSEL1__FSEL19__VAL(val) (val << GPIO_GPFSEL1__FSEL19__BASE)
#define GPIO_GPFSEL1__FSEL19__MASK (0x7 << GPIO_GPFSEL1__FSEL19__BASE)

#define GPIO_GPFSEL2__FSEL20__BASE 0
#define GPIO_GPFSEL2__FSEL20__VAL(val) (val << GPIO_GPFSEL2__FSEL20__BASE)
#define GPIO_GPFSEL2__FSEL20__MASK (0x7 << GPIO_GPFSEL2__FSEL20__BASE)

#define GPIO_GPFSEL2__FSEL21__BASE 3
#define GPIO_GPFSEL2__FSEL21__VAL(val) (val << GPIO_GPFSEL2__FSEL21__BASE)
#define GPIO_GPFSEL2__FSEL21__MASK (0x7 << GPIO_GPFSEL2__FSEL21__BASE)

//=====================================================================

// Adressen fuer die PCM/I2S Schnittstelle
// Die Bezeichnungen entsprechen den Bezeichnungen im Datenblatt.

#define PCM_BASE (0x203000) // Startadresse PCM-Register relativ zu mmap

#define PCM_CS_A ((PCM_BASE + 0x0)/4) // CS_A Register
#define PCM_FIFO_A ((PCM_BASE + 0x4)/4) // FIFO Register
#define PCM_MODE_A ((PCM_BASE + 0x8)/4) // MODE_A Register
#define PCM_RXC_A ((PCM_BASE + 0xc)/4) // RXC_A Register
#define PCM_TXC_A ((PCM_BASE + 0x10)/4) // TXC_A Register

// Erklaerung zu den Makros siehe oben bei GPIO
#define PCM_CS_A__SYNC__BASE 24
#define PCM_CS_A__SYNC__VAL(val) (val << PCM_CS_A__SYNC__BASE)
#define PCM_CS_A__SYNC__MASK (1 << PCM_CS_A__SYNC__BASE)

#define PCM_CS_A__RXSEX__BASE 23
#define PCM_CS_A__RXSEX__VAL(val) (val << PCM_CS_A__RXSEX__BASE)
#define PCM_CS_A__RXSEX__MASK (1 << PCM_CS_A__RXSEX__BASE)

#define PCM_CS_A__RXD__BASE 20
#define PCM_CS_A__RXD__VAL(val) (val << PCM_CS_A__RXD__BASE)
#define PCM_CS_A__RXD__MASK (1 << PCM_CS_A__RXD__BASE)

#define PCM_CS_A__TXD__BASE 19
#define PCM_CS_A__TXD__VAL(val) (val << PCM_CS_A__TXD__BASE)
#define PCM_CS_A__TXD__MASK (1 << PCM_CS_A__TXD__BASE)

#define PCM_CS_A__RXERR__BASE 16
#define PCM_CS_A__RXERR__VAL(val) (val << PCM_CS_A__RXERR__BASE)
#define PCM_CS_A__RXERR__MASK (1 << PCM_CS_A__RXERR__BASE)

#define PCM_CS_A__TXERR__BASE 15
#define PCM_CS_A__TXERR__VAL(val) (val << PCM_CS_A__TXERR__BASE)
#define PCM_CS_A__TXERR__MASK (1 << PCM_CS_A__TXERR__BASE)

#define PCM_CS_A__RXCLR__BASE 4
#define PCM_CS_A__RXCLR__VAL(val) (val << PCM_CS_A__RXCLR__BASE)
#define PCM_CS_A__RXCLR__MASK (1 << PCM_CS_A__RXCLR__BASE)

#define PCM_CS_A__TXCLR__BASE 3
#define PCM_CS_A__TXCLR__VAL(val) (val << PCM_CS_A__TXCLR__BASE)
#define PCM_CS_A__TXCLR__MASK (1 << PCM_CS_A__TXCLR__BASE)

#define PCM_CS_A__TXON__BASE 2
#define PCM_CS_A__TXON__VAL(val) (val << PCM_CS_A__TXON__BASE)
#define PCM_CS_A__TXON__MASK (1 << PCM_CS_A__TXON__BASE)

#define PCM_CS_A__RXON__BASE 1
#define PCM_CS_A__RXON__VAL(val) (val << PCM_CS_A__RXON__BASE)
#define PCM_CS_A__RXON__MASK (1 << PCM_CS_A__RXON__BASE)

#define PCM_CS_A__EN__BASE 0
#define PCM_CS_A__EN__VAL(val) (val << PCM_CS_A__EN__BASE)
#define PCM_CS_A__EN__MASK (1 << PCM_CS_A__EN__BASE)

#define PCM_MODE_A__CLKM__BASE 23
#define PCM_MODE_A__CLKM__VAL(val) (val << PCM_MODE_A__CLKM__BASE)
#define PCM_MODE_A__CLKM__MASK (1 << PCM_MODE_A__CLKM__BASE)

#define PCM_MODE_A__CLKI__BASE 22
#define PCM_MODE_A__CLKI__VAL(val) (val << PCM_MODE_A__CLKI__BASE)
#define PCM_MODE_A__CLKI__MASK (1 << PCM_MODE_A__CLKI__BASE)

#define PCM_MODE_A__FSM__BASE 21
#define PCM_MODE_A__FSM__VAL(val) (val << PCM_MODE_A__FSM__BASE)
#define PCM_MODE_A__FSM__MASK (1 << PCM_MODE_A__FSM__BASE)

#define PCM_MODE_A__FSI__BASE 20
#define PCM_MODE_A__FSI__VAL(val) (val << PCM_MODE_A__FSI__BASE)
#define PCM_MODE_A__FSI__MASK (1 << PCM_MODE_A__FSI__BASE)

#define PCM_RXC_A__CH1EN__BASE 30
#define PCM_RXC_A__CH1EN__VAL(val) (val << PCM_RXC_A__CH1EN__BASE)
#define PCM_RXC_A__CH1EN__MASK (1 << PCM_RXC_A__CH1EN__BASE)

#define PCM_RXC_A__CH1WID__BASE 16
#define PCM_RXC_A__CH1WID__VAL(val) (val << PCM_RXC_A__CH1WID__BASE)
#define PCM_RXC_A__CH1WID__MASK (0xf << PCM_RXC_A__CH1WID__BASE)

#define PCM_RXC_A__CH1WEX__BASE 31
#define PCM_RXC_A__CH1WEX__VAL(val) (val << PCM_RXC_A__CH1WEX__BASE)
#define PCM_RXC_A__CH1WEX__MASK (1 << PCM_RXC_A__CH1WEX__BASE)

#define PCM_RXC_A__CH1POS__BASE 20
#define PCM_RXC_A__CH1POS__VAL(val) (val << PCM_RXC_A__CH1POS__BASE)
#define PCM_RXC_A__CH1POS__MASK (0x3ff << PCM_RXC_A__CH1POS__BASE)

#define PCM_RXC_A__CH2EN__BASE 14
#define PCM_RXC_A__CH2EN__VAL(val) (val << PCM_RXC_A__CH2EN__BASE)
#define PCM_RXC_A__CH2EN__MASK (1 << PCM_RXC_A__CH2EN__BASE)

#define PCM_RXC_A__CH2WID__BASE 0
#define PCM_RXC_A__CH2WID__VAL(val) (val << PCM_RXC_A__CH2WID__BASE)
#define PCM_RXC_A__CH2WID__MASK (0xf << PCM_RXC_A__CH2WID__BASE)

#define PCM_RXC_A__CH2WEX__BASE 15
#define PCM_RXC_A__CH2WEX__VAL(val) (val << PCM_RXC_A__CH2WEX__BASE)
#define PCM_RXC_A__CH2WEX__MASK (1 << PCM_RXC_A__CH2WEX__BASE)

#define PCM_RXC_A__CH2POS__BASE 4
#define PCM_RXC_A__CH2POS__VAL(val) (val << PCM_RXC_A__CH2POS__BASE)
#define PCM_RXC_A__CH2POS__MASK (0x3ff << PCM_RXC_A__CH2POS__BASE)

#define PCM_TXC_A__CH1EN__BASE 30
#define PCM_TXC_A__CH1EN__VAL(val) (val << PCM_TXC_A__CH1EN__BASE)
#define PCM_TXC_A__CH1EN__MASK (1 << PCM_TXC_A__CH1EN__BASE)

#define PCM_TXC_A__CH1WID__BASE 16
#define PCM_TXC_A__CH1WID__VAL(val) (val << PCM_TXC_A__CH1WID__BASE)
#define PCM_TXC_A__CH1WID__MASK (0xf << PCM_TXC_A__CH1WID__BASE)

#define PCM_TXC_A__CH1WEX__BASE 31
#define PCM_TXC_A__CH1WEX__VAL(val) (val << PCM_TXC_A__CH1WEX__BASE)
#define PCM_TXC_A__CH1WEX__MASK (1 << PCM_TXC_A__CH1WEX__BASE)

#define PCM_TXC_A__CH1POS__BASE 20
#define PCM_TXC_A__CH1POS__VAL(val) (val << PCM_TXC_A__CH1POS__BASE)
#define PCM_TXC_A__CH1POS__MASK (0x3ff << PCM_TXC_A__CH1POS__BASE)

#define PCM_TXC_A__CH2EN__BASE 14
#define PCM_TXC_A__CH2EN__VAL(val) (val << PCM_TXC_A__CH2EN__BASE)
#define PCM_TXC_A__CH2EN__MASK (1 << PCM_TXC_A__CH2EN__BASE)

#define PCM_TXC_A__CH2WID__BASE 0
#define PCM_TXC_A__CH2WID__VAL(val) (val << PCM_TXC_A__CH2WID__BASE)
#define PCM_TXC_A__CH2WID__MASK (0xf << PCM_TXC_A__CH2WID__BASE)

#define PCM_TXC_A__CH2WEX__BASE 15
#define PCM_TXC_A__CH2WEX__VAL(val) (val << PCM_TXC_A__CH2WEX__BASE)
#define PCM_TXC_A__CH2WEX__MASK (1 << PCM_TXC_A__CH2WEX__BASE)

#define PCM_TXC_A__CH2POS__BASE 4
#define PCM_TXC_A__CH2POS__VAL(val) (val << PCM_TXC_A__CH2POS__BASE)
#define PCM_TXC_A__CH2POS__MASK (0x3ff << PCM_TXC_A__CH2POS__BASE)

//=====================================================================

// Globale Variable

// Wird in signalHandler() gesetzt wenn der User ein
// SIGTERM (STRG+C) an das Programm schickt

// #ifndef I2S_DEMO
// #define I2S_DEMO

extern volatile sig_atomic_t abort_flag;
extern volatile ulong *p_mmap; //Zeiger auf den per mmap() geteilten speicher
extern volatile int socketInt;

//#endif
//=====================================================================

// Funktionendeklarationen
// Beschreibungen siehe .c-Datei

void signalHandler(int);
void writeMemMaskedBit(volatile ulong *, ulong, ulong value);
void writeI2cMaskedBit(	int, int, int,	int);
int  i2cGetFD(int);
void waitForSyncBit();
void i2sRestartStream();
int  i2sForward();
long firFilter(long *, long *, long *, long, long);
int  i2sFilter(char *, char *);

int hifihatSetup(int,int,ulong);
void hifihatShutdown(int,int);
//=====================================================================

