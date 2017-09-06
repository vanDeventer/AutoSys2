/*
 * AutoSys2.c
 *
 * Created: 13-Nov-16 11:36:25 AM
 * Updated: 06-Sept-2017
 * Author : Jan vanDeventer; email: jan.van.deventer@ltu.se
 */ 

/*
 * Purpose of this version:
 * The purpose of this version of the generate a PWM (pulse width modulation) signal that dims the display. 
 *	We use timer/counter 0.
*/

#define DISPLAYLENGTH 16	/* number of characters on the display */
#define DB_LED PB7	// Display Back-light's LED is on Port B, pin 7. This is a command to the compiler pre-processor.

#define LANG 1
#if LANG == 0
	#define GREETING "Hej! "
#elif LANG == 1
	#define GREETING "Hello"
#else
	#define GREETING "Error"
#endif

#include <avr/io.h>	// Standard IO header file
#include <stdlib.h>  /* This library is included for the atoi function to avoid the warning of implicit function declaration https://en.wikibooks.org/wiki/C_Programming/stdlib.h/atoi */
#include <avr/pgmspace.h>	// Contains some type definitions and functions.
#include <avr/interrupt.h>	// Contains ISR (Interrupt Service Routines) or interrupt handler details
#include <string.h>

#include "global.h"
#include "lcd.h"
// The above files are located in the same folder as the current file. They are also added to the Solution Explorer.

enum dStates {DBOOT, DADC, DUART, DTEXT, DSPI, DI2C, DCAN};	/* enumeration of states (C programming, p) */
#define DTOP DTEXT /* this needs to be updated when new states are added to enumeration of dState */
enum subMode {NORMAL, TEDIT, UECHO};
char *dbStateName[] = {GREETING, "ADC", "USART", "Text"}; /* initialization of Pointer Array (C programming, p113) */
	
volatile unsigned int dbState, subState;		/* display's state (activated by buttons)*/
volatile unsigned char buttons;		// This registers holds a copy of PINC when an external interrupt 6 has occurred.
volatile unsigned char bToggle = 0;	// This registers is a boolean that is set when an interrupt 6 occurs and cleared when serviced in the code.
volatile unsigned char LEDpattern, LEDperiod, LEDcountD;	// 3 bytes related to the 5 LEDs

volatile uint16_t adc_value;  //Allocate the double byte memory space into which the result of the 10 bits Analog to Digital Converter (ADC) is stored.

/* the following functions are initialization functions (initF) called only once in the main() function */

int initGPIO(void)
{
	//Set up input output direction on Port C and G
	DDRB |= (1<<DB_LED);	// Set the display back-light's IO pin an an output. Leave other bits as they were.
	DDRC = 0b00000111;		// Set the direction of the IO pins on Port C to output on the 3 least significant bits and input on the 5 higher ones. 5 buttons and 3 LEDs.
	DDRG |= 0b00000011;		// set the direction of the IO pins on Port G's lower 2 bytes as output (LEDs 1 & 2). Leave the other bits as they were.
	return(0);
}

int initExtInt(void)
{
	//Set up external Interrupts
	// The five Switches are ORed to Pin PE6 which is alternatively Int6
	EICRB |= (0<<ISC61) | (1<<ISC60);  //Any logical change to INT6 generates an interrupt
	EIMSK |= (1<<INTF6);
	return(6);
}


int initTimer2()
{
	/// Set up an internal Interrupt that will occur every 5 milliseconds.
	/// It uses the Timer Counter 2 in CTC mode with a pre-scaler of 256 and a value of 155 (it should be 155.25).
	// 
	TCCR2A = (1<<WGM21); // | (0<<WGM20);  //CTC mode
	//TCCR2A |= (0<<COM2A1) | (0<<COM2A0); // Mormal port operation, OC2A is disconnected.
	TCCR2A |= (1<<CS22) | (1<<CS21); //| (0<<CS20); /// Divide source frequency source by 256.
	TCNT2 = 0;	/// Make sure the timer counter is set to 0.
	OCR2A = 155;
	TIMSK2 = (1<<OCF2A); // Interrupt flag register to enable output compare.
	return(2);
}

int initADC(){
	//Set up analog to digital conversion (ADC) 
	//ADMUX register
	//AVcc with external capacitor on AREF pin (the 2 following lines)
	ADMUX &= ~(1<<REFS1);  //Clear REFS1 (although it should be 0 at reset)
	ADMUX |= (1<<REFS0);   //Set REFS0  
	ADMUX &= (0b11100000); //Single ended input on ADC0
	ADMUX &= ~(1<<ADLAR);  //Making sure ADLAR is zero (somehow it was set to 1)
	//The ACDC control and status register B ADCSRB
	ADCSRB &= ~(1<<ADTS2) & ~(1<<ADTS1) & ~(1<<ADTS0);  //Free running mode
	//The ADC control and status register A ADCSRA
	ADCSRA |= (1<<ADPS2) | (1<<ADPS1) |(1<<ADPS0);//set sampling frequency pre-scaler to a division by 128
	ADCSRA |= (1<<ADEN)  | (1<<ADATE) | (1<<ADIE);//enable ADC, able ADC auto trigger, enable ADC interrupt
	return(0);
}

int initDisplay(void)
{
	dbState = DBOOT;
	subState = NORMAL;
	lcdInit();	//initialize the LCD
	lcdClear();	//clear the LCD
	lcdHome();	//go to the home of the LCD
	lcdPrintData(GREETING, strlen(GREETING)); //Display the text on the LCD
	return(1);
}

/*! \brief Initializes the USART for the communication bus 
 * This function is used to initialize the USART which a baudrate
 * that needs to be sent in as a parameter Use the baudrate settings
 * specified in the ATMEGA128 baudrate setting.
 * \param baud The baudrate param from the ATMEGA32 datasheet.
 */
void usart1_init(unsigned int baudrate) {
	/* Set baud rate */
	UBRR1H = (unsigned char) (baudrate>>8);
	UBRR1L = (unsigned char) baudrate;
	/* Set frame format: 8data, no parity & 1 stop bits */
	UCSR1C = (0<<UMSEL0) | (0<<USBS0) | (1<<UCSZ1) | (1<<UCSZ0) | (0<<UCSZ2);
	/* Enable receiver, transmitter and receive interrupt */
	UCSR1B = (1<<RXEN1) | (1<<TXEN1);// | (1<<RXCIE1);
}

void TimerCounter0setup(int start)
{
	//Setup mode on Timer counter 0 to PWM phase correct
	TCCR0A = (0<<WGM01) | (1<<WGM00);
	//Set OC0A on compare match when counting up and clear when counting down
	TCCR0A |= (1<<COM0A1) | (1<<COM0A0);
	//Setup pre-scaller for timer counter 0
	TCCR0A |= (0<<CS02) | (0<<CS01) | (1<<CS00);  //Source clock IO, no pre-scaling

	//Setup output compare register A
	OCR0A = start;
}

/* end of the initialization functions, begin with functions called */

/*! \brief Send a character to the USART
 * Send a single character to the USART used for the communication bus
 * \param data The character you want to send
 */
unsigned char usart1_transmit(char  data ) {
	/* Wait for empty transmit buffer */
	while (!( UCSR1A & (1<<UDRE1)));
	/* Put data into buffer, sends the data */
	UDR1 = data;
	return 0;
}

/*! \brief Send a string of characters to the USART
 * Send a string of characters to the USART used for the communication bus
 * \param data The string of characters you wish to send
 * \param length The length of the string you wish to send
 */
unsigned char usart1_sendstring(char *data,unsigned char length) {
	int i;
	for (i=0;i<length;i++)
		usart1_transmit(*(data++));
	
	return 0;
}

/*! \brief Retrieve one character from the USART
 * Retrieve one character from the USART. This function will wait until
 * there is a character in the USART RX buffer
 * \return The character from the RX USART buffer
 */
unsigned char usart1_receive(void ) {
	/* Wait for data to be received */
	while (!(UCSR1A & (1<<RXC1)));
	/* Get and return received data from buffer */
	return UDR1;
}

/*! \brief The USART receive loopback
 * This function does wait for a character in the RX buffer and returns
 * it to the transmit buffer.
 * \return The character from the RX USART buffer
 */
unsigned char usart1_receive_loopback(void ) {
	uint8_t rbuff;
	/* Wait for data to be received */
	while (!(UCSR1A & (1<<RXC1)));
	/* Get and return received data from buffer */
	rbuff = UDR1;
	usart1_transmit(rbuff);
	return rbuff;
}

/*! \brief Retrieve one character from the USART
 * Retrieve one character from the USART. With this function you will
 * need to poll the USART, it does NOT wait until a character is in the buffer.
 * \return The character from the RX USART buffer
 */
unsigned char poll_usart1_receive(void ) {
	/* Check if data is received */
	return ((UCSR1A & (1<<RXC1)));
}

int dbStateUp(void)
{
	if (++dbState > DTOP)
		dbState = DADC;
	lcdClear();	//clear the LCD
	lcdHome();	//go to the home of the LCD
	lcdPrintData(dbStateName[dbState], strlen(dbStateName[dbState])); //Display the text on the LCD
	return 0;
}

int dbStateDown(void)
{
	if (dbState-- <= DADC)
		dbState = DTOP;
	lcdClear();	//clear the LCD
	lcdHome();	//go to the home of the LCD
	lcdPrintData(dbStateName[dbState], strlen(dbStateName[dbState])); //Display the text on the LCD
	return 0;
}

int DbBOOThandler(void)
{
	switch(buttons & 0b11111000){
		case 0b10000000:			//S5 center button
			/* do nothing */
			break;
		case 0b01000000:			//S4  upper button
			dbStateUp();
			break;
		case 0b00100000:			//S3 left button
			/* do nothing */
			break;
		case 0b00010000:			//S2 lower button
			dbStateDown();
			break;
		case 0b00001000:			//S1 right button
			/* do nothing */
			break;
		default:
			/* button released, do nothing */
			break;
	}
	return 0;
}

int DbADChandler(void)
{
	switch(buttons & 0b11111000){
		case 0b10000000:			//S5 center button
			/* do nothing */
			break;
		case 0b01000000:			//S4  upper button
			dbStateUp();
			break;
		case 0b00100000:			//S3 left button
			/* change channel using ADMUX */
			break;
		case 0b00010000:			//S2 lower button
			dbStateDown();
			break;
		case 0b00001000:			//S1 right button
			/* change channel using ADMUX */			
			break;
		default:
			/* do nothing */
		break;
	}
	return 0;
}


int DbUSARThandler(void)
{
	switch(buttons & 0b11111000){
		case 0b10000000:			//S5 center button
			if (subState == NORMAL)
			{
				subState = UECHO;
				lcdGotoXY(8,0);     //Position the cursor on first line 8th position
				lcdPrintData("echo  ", 6); //Clear the lower part of the LCD
			}
			break;
		case 0b01000000:			//S4  upper button
			dbStateUp();
			break;
		case 0b00100000:			//S3 left button
			/* do nothing */
			break;
		case 0b00010000:			//S2 lower button
			dbStateDown();
			break;
		case 0b00001000:			//S1 right button
			/* do nothing */
			break;
		default:
			/* button released, do nothing */
			break;
	}
	return 0;
}


unsigned int DbTEXThandler(char *s, unsigned int position)
{
	switch(buttons & 0b11111000){
		case 0b10000000:			//S5 center button
			if (subState == NORMAL)
			{
				subState = TEDIT;
				lcdGotoXY(8,0);     //Position the cursor on first line 8th position
				lcdPrintData("input ", 6); //Clear the lower part of the LCD
			}
			else
			{
				subState = NORMAL;
				usart1_transmit('\r');
				usart1_sendstring(s, strlen(s));	/* transmit over USART 1 the string s */
				usart1_transmit('\n');
				lcdGotoXY(8,0);     //Position the cursor on first line 8th position
				lcdPrintData("      ", 6); //Clear the lower part of the LCD
			}		
			break;
		case 0b01000000:			//S4  upper button
			if (subState == TEDIT)
			{
				if (s[position] < 'z')	// if you did not reach the ASCII letter z or 0x7A, you can go to the next letter in the table
				s[position]++;
			}
			else
			{
				dbStateUp();
			}
			break;
		case 0b00100000:			//S3 left button
			if (subState == TEDIT)
			{
				if (position > 0)		//If you have not reached the right side of the display, you can move the cursor one step to the right
				{
					position--;
				}
			}
			break;
		case 0b00010000:			//S2 lower button
			if (subState == TEDIT)
			{
				if (s[position] > 'A')	// if you did not reach the ASCII letter A, you can go to the previous letter in the table
				{
					s[position]--;
				}
			}
			else
			{
				dbStateDown();
			}
			break;
		case 0b00001000:			//S1 right button
			if ((subState == TEDIT) && (position < DISPLAYLENGTH-1))		//If you have not reached the right side of the display with index starting at 0, you can move the cursor one step to the right
			{
				position++;
				s[position] = 'A';
				s[position+1] ='\0';
			}
			break;
		default:
		/* button released, do nothing */
		break;
	}
	return position;
}

void  flashLEDs()		//This function will scroll or invert the 5 LEDs (5 LSB) based on the three MSB (scroll left, invert, scroll right).
{	
	unsigned char temp;			//Allocate a temporary byte. Note! it is not the same byte as the byte named temp in the main routine. Do not confuse them even if they have the same name.
	if (LEDcountD != 0)
	{
		LEDcountD--;			// Decrement the countdown timer for another 5ms until it reaches 0
	} 
	else
	{
		LEDcountD = LEDperiod;	// Reset the countdown timer.
		temp = LEDpattern & 0b00011111; // Save the LED pattern.
		switch (LEDpattern & 0b11100000)
		{
			case 0b10000000:
				temp = temp<<1;
				if (temp & 0b00100000)
				{
					temp |= 0b00000001;
					temp &= ~0b00100000;	// Do not keep a bit set where there is supposed to be a flashing command.
				}
				break;
			case 0b01000000:
				temp = ~temp;		// Invert the light pattern.
				temp &= 0b00011111;	// Clear the flashing command.
				break;
			case 0b00100000:
				if (temp & 0b00000001)
				{
						temp |= 0b00100000;
				}
				temp = temp>>1;
				break;
			default:
				// Do nothing
				break;
	}
		LEDpattern = (LEDpattern & 0b11100000)|temp;	// Update the LEDpattern with the current pattern while keeping flashing commands
		PORTG = (PORTG & 0x11111100) | (temp & 0b00000011);	//Update the 2 Port G LEDs
		temp = temp >>2;
		PORTC = (PORTC & 0b11111000) | temp;	//Update the 3 Port C LEDs
	}
}

int main(void){
	unsigned char temp = 0x0F;		// Allocate memory for temp. It is initialized to 15 for demonstration purposes only.
	 char text[10],uText[10];					//Allocate an array of 10 bytes to store text
	 unsigned char textEcho, i;
	 
	unsigned char cursor = 0;				/* allocate a variable to keep track of the cursor position and initialize it to 0 */
	char tLine[DISPLAYLENGTH + 1];	/* allocate a consecutive array of 16 characters where your text will be stored with an end of string */
	tLine[0] = 'A';					/* initialize the first ASCII character to A or 0x41 */
	tLine[1] = '\0';				/* initialize the second character to be an end of text string */
	
	LEDpattern = 0b01000100;		// Flash the pattern the LED pattern 00100
	LEDperiod = 100;				// LEDperiod x initTimer2 period.
	LEDcountD = 0;
	
	temp = initGPIO();				// Set up the data direction register for both ports C and G
	temp = initExtInt();			// Setup external interrupts
	temp = initTimer2();			// Setup 5ms internal interrupt
	temp = initADC();				// Setup the Analog to Digital Converter
	temp = initDisplay();
	usart1_init(51);				//BAUDRATE_19200 (look at page s183 and 203)
	TimerCounter0setup(127);

	
	ADCSRA |= (1<<ADSC);			//Start ADC
	sei();							// Set Global Interrupts
	
	while(1){						// LOOP: does not do much more than increase temp.
		temp++;
		ADCSRA &= ~(1<<ADIE);		//disable ADC interrupt to prevent value update during the conversion
		itoa(adc_value, text, 9);	//Convert the unsigned integer to an ascii string; look at 3.6 "The C programming language"
		OCR0A = adc_value >> 2;		// using the top 8 bits of the ADC, load OCR0A to compare to the timer Counter 0 to generate aPWM
		ADCSRA |= (1<<ADIE);		//re-enable ADC interrupt
		if (bToggle)			//This is set to true only in the interrupt service routine at the bottom of the code
		{
			switch (dbState){
				case DBOOT:
					DbBOOThandler();
					break;
				case DADC:
					DbADChandler();
					break;
				case DUART:
					DbUSARThandler();
					i = 0;
					break;
				case DTEXT:
					cursor = DbTEXThandler(tLine, cursor);
					break;
				default:
					break;
			}
			bToggle = 0;			//
		}
		
		if (dbState == DTEXT)
		{
			lcdGotoXY(0, 1);     //Position the cursor on
			lcdPrintData(tLine, strlen(tLine)); //Display the text on the LCD
		} 
		else if (dbState == DUART)
		{
			if (subState == UECHO)
			{
				if (i < 10)
				{
					textEcho = usart1_receive();
					if ((textEcho == '\r') || (textEcho == '\n'))
					{
						subState = NORMAL;
					}
					else
					{
						usart1_transmit(textEcho);
						uText[i] = textEcho;
						uText[++i] = '\0';
						lcdGotoXY(0, 1);     //Position the cursor on
						lcdPrintData(uText, strlen(uText)); //Display the text on the LCD
					}
				}
				else
				{
					subState = NORMAL;
				}	
			}
			else
			{
				lcdGotoXY(8,0);     //Position the cursor on first line 8th position
				lcdPrintData("      ", 6); //Clear the lower part of the LCD
			}
		}
		else
		{
			lcdGotoXY(5, 1);     //Position the cursor on
			lcdPrintData("      ", 6); //Clear the lower part of the LCD
			lcdGotoXY(5, 1);     //Position the cursor on
			lcdPrintData(text, strlen(text)); //Display the text on the LCD
			usart1_sendstring(text, strlen(text));	/* transmit over USART 1 the value of the ADC */
			usart1_transmit('\n');
			usart1_transmit('\r');
		}

	}			
}


/* the functions below are Interrupt Service Routines, they are never called by software */

SIGNAL(SIG_INTERRUPT6)  //Execute the following code if an INT6 interrupt has been generated. It is kept short.
{
	bToggle = 1;
	buttons = PINC;
}


SIGNAL(SIG_OUTPUT_COMPARE2){ // This loop is executed every 5 ms (depending on the compare and match of timer 2)	
	// Update the LED sequence
	cli();					// Disable the global interrupts to prevent accidental corruption of the results while the two bytes.
	
	if (adc_value>852){		
		LEDpattern = 0b00011111;
	} 
	else if(adc_value>682){
		LEDpattern = 0b00001111;
	}
	else if(adc_value>511){
		LEDpattern = 0b00000111;
	}
	else if(adc_value>341){
		LEDpattern = 0b00000011;
	}
	else if(adc_value>170){
		LEDpattern = 0b00000001;
	}
	else{
		LEDpattern = 0b00000000;
	};
			 
	sei();					// re-enable the global interrupts

	flashLEDs();
}

ISR(ADC_vect){
	adc_value = ADCL;		//Load the low byte of the ADC result
	adc_value += (ADCH<<8); //shift the high byte by 8bits to put the high byte in the variable
}