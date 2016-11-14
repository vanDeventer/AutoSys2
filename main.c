/*
 * AutoSys2.c
 *
 * Created: 13-Nov-16 11:36:25 AM
 * Author : Jan vanDeventer; email: jan.van.deventer@ltu.se
 */ 

/*
 * Purpose of this version:
 * The purpose of this version of the program is to set up the General Purpose Inputs Outputs (GPIO) on ports B, C and G to read the buttons and light up the LEDs using a while loop.
*/

#define DB_LED PB7	// Display Backlight's LED is on Port B, pin 7. This is a command to the compiler pre-processor.

#include <avr/io.h>	// Standard IO header file


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
	unsigned char temp = 0x0F;		//Allocate memory for temp. It is initialized to 15 for demonstration purposes only.
	
	temp = initGPIO();				//Set up the data direction register for both ports C and G
	
	while(1)
	{
			temp = PINC;			// Copy Port C to temp.
			temp &= 0b11111000;		// Prepare to turn off Port C LEDs
			
			if (temp & 0b10000000)
			temp |= 0b00000100;		// Prepare to turn on Led5 if S5 is on
			if (temp & 0b01000000)
			temp |= 0b00000010;		// Prepare to turn on Led4 if S4 is on
			if (temp & 0b00100000)
			temp |= 0b00000001;		// Prepare to turn on Led3 if S3 is on
			
			PORTC = temp & 0b00000111;// Copy the last 3 bits of temp to Port C to turn on the LEDs.
			
			temp &= 0b11111000;		// Clear all LEDs so we do not turn on what we do not want
			PORTG &= 0b11111100;	// Turn off the two LEDs without affecting the other pins.
			
			if (temp & 0b00010000)
			temp |= 0b00000010;		// Prepare to turn on Led2 if S2 is on
			if (temp & 0b00001000)
			temp |= 0b00000001;		// Prepare to turn on Led1 if S1 is on
			
			PORTG |= (temp & 0b00000011);			// Copy the last 2 bits of temp to Port G to turn on the LEDs. DO NOT AFFECT THE UPPER BITS!
			
// NOTE: THE ABOVE LOGIC ALLOWS YOU TO HAVE MORE THAN 1 LED ON AT THE TIME.			

	}
}

