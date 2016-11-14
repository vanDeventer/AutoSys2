/*
 * AutoSys2.c
 *
 * Created: 13-Nov-16 11:36:25 AM
 * Updated: 14-Nov-16 @ 08:40
 * Author : Jan vanDeventer; email: jan.van.deventer@ltu.se
 */ 

/*
 * Purpose of this version:
 *	The purpose of this program is to give students the experience of working with multiple files. In this case delay. h and delay.c need to be added.
 *	This program turns on the center LED and then goes outwards.
 *	Compilation gives one warning.
 */

#define DB_LED PB7	// Display Backlight's LED is on Port B, pin 7. This is a command to the compiler pre-processor.

#include <avr/io.h>	// Standard IO header file
#include "delay.h"


int initGPIO(void)
{
	//Set up input output direction on Port C and G
	DDRB |= (1<<DB_LED);	// Set the display backlight's IO pin an an output. Leave other bits as they were.
	DDRC = 0b00000111;		// Set the direction of the IO pins on Port C to output on the 3 least significant bits and input on the 5 higher ones. 5 buttons and 3 LEDs.
	DDRG |= 0b00000011;		// set the direction of the IO pins on Port G's lower 2 bytes as output (LEDs 1 & 2). Leave the other bits as they were.
	return(0);
}



int main(void)
{
	unsigned char temp = 0xFF;		//Allocate memory for temp. It is initialized to 255 for demonstration purposes only.
	
	temp = initGPIO();				//Set up the data direction register for both ports C and G
	
	while(1)
	{
		PORTC = 0b000000001;	//Tun on Led3
		PORTG &= 0x00;			//Turn off all Leds on PortG
		delay_s(1);				//Wait 1 second
		
		PORTC = 0b00000010;		//Turn on Led4 on PortC
		PORTG = 0x02;			//Turn on Led2 on PortG
		delay_ms(255);			//Wait 255 ms
		
		PORTC = 0b00000100;		//Turn on Led5 on PortC
		PORTG = 0x01;			//Turn on Led1 on PortG
		delay_ms(255);			//Wait 255 ms
		delay_ms(255);			//Wait 255 ms
		
		if (PINB & (1<<DB_LED))		// If the display backlight is on, then 
		{
			PORTB &= ~(1<<DB_LED);	// Turn off the display backlight.
		} 
		else
		{
			PORTB |= (1<<DB_LED);	// Turn on turn display backlight.
		}
	}
}