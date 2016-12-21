/*
 * AutoSys2.c
 *
 * Created: 13-Nov-16 11:36:25 AM
 * Updated: 21-Dec-16
 * Author : Jan vanDeventer; email: jan.van.deventer@ltu.se
 */ 

/*
 * Purpose of this version:
 * The purpose of this version of the program is to introduce Internal Interrupts (using Timer x).
*/

#define DB_LED PB7	// Display Backlight's LED is on Port B, pin 7. This is a command to the compiler pre-processor.

#include <avr/io.h>	// Standard IO header file
#include <avr/pgmspace.h>	// Contains some type definitions and functions.
#include <avr/interrupt.h>	// Contains ISR (Interrupt Service Routines) or interrupt handler details
// The above files should be in C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\avr8\avr8-gnu-toolchain\avr\include\avr\

volatile unsigned char buttons;		// This registers holds a copy of PINC when an external interrupt 6 has occurred.
volatile unsigned char bToggle = 0;	// This registers is a boolean that is set when an interrupt 6 occurs and cleared when serviced in the code.
//These registers is available outside of the main loop (i.e., to the interrupt handlers)

int initGPIO(void)
{
	//Set up input output direction on Port C and G
	DDRB |= (1<<DB_LED);	// Set the display backlight's IO pin an an output. Leave other bits as they were.
	DDRC = 0b00000111;		// Set the direction of the IO pins on Port C to output on the 3 least significant bits and input on the 5 higher ones. 5 buttons and 3 LEDs.
	DDRG |= 0b00000011;		// set the direction of the IO pins on Port G's lower 2 bytes as output (LEDs 1 & 2). Leave the other bits as they were.
	return(0);
}

int iniExtInt(void)
{
	//Set up external Interrupts
	// The five Switches are ORed to Pin PE6 which is alternatively Int6
	EICRB |= (0<<ISC61) | (1<<ISC60);  //Any logical change to INT6 generates an interrupt
	EIMSK |= (1<<INTF6);
	return(6);
}


int initTimer2()
{
	//Set up external Interrupts
	// The five Switches are ORed to Pin PE6 which is alternatively Int6
	EICRB |= (0<<ISC61) | (1<<ISC60);  //Any logical change to INT6 generates an interrupt
	EIMSK |= (1<<INTF6);
	return(6);
}

int main(void)
{
	unsigned char temp = 0x0F;		// Allocate memory for temp. It is initialized to 15 for demonstration purposes only.
	
	temp = initGPIO();				// Set up the data direction register for both ports C and G
	temp = iniExtInt();				// Setup external interrupts
	sei();					// Set Global Interrupts
	
	while(1)
	{
		if (bToggle)
		{
			switch(buttons & 0b11111000)
			{
				case 0b10000000:			//S5 center button
					PORTC |= 0b00000100;	//Turn on Led5 if S5 is on
					break;
				case 0b01000000:			//S4  upper button
					PORTC |= 0b00000010;	 //Turn on Led4 if S4 is on
					break;
				case 0b00100000:			//S3 left button
					PORTC |= 0b00000001;	//Turn on Led3 if S3 is on
					break;
				case 0b00010000:			//S2 lower button
					PORTG |= 0b00000010;	//Turn on Led2 if S2 is on
					break;
				case 0b00001000:			//S1 right button
					PORTG |= 0b00000001;	//Turn on Led1 if S1 is on
					break;
				default:
					PORTC &= 0b11111000;	//Turn off Port C LEDs
					PORTG &= 0x11111100;	//Turn off Port G LEDs
				break;
			}
		bToggle = 0;
		}			
// NOTE: THE ABOVE LOGIC ALLOWS YOU TO HAVE ONLY 1 LED ON AT THE TIME. This would be the case if you had a joystick.			

	}
}

SIGNAL(SIG_INTERRUPT6)  //Execute the following code if an INT6 interrupt has been generated. It is kept short.
{
	bToggle = 1;
	buttons = PINC;
}

