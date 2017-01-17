/*
 * AutoSys2.c
 *
 * Created: 13-Nov-16 11:36:25 AM
 * Updated: 21-Dec-16
 * Author : Jan vanDeventer; email: jan.van.deventer@ltu.se
 */ 

/*
 * Purpose of this version:
 * The purpose of this version of the program is to build on what we have done with an example that flashes and swipes the 5 LEDs.
*/

#define DB_LED PB7	// Display Backlight's LED is on Port B, pin 7. This is a command to the compiler pre-processor.

#include <avr/io.h>	// Standard IO header file
#include <avr/pgmspace.h>	// Contains some type definitions and functions.
#include <avr/interrupt.h>	// Contains ISR (Interrupt Service Routines) or interrupt handler details
// The above files should be in C:\Program Files (x86)\Atmel\Studio\7.0\toolchain\avr8\avr8-gnu-toolchain\avr\include\avr\

volatile unsigned char buttons;		// This registers holds a copy of PINC when an external interrupt 6 has occurred.
volatile unsigned char bToggle = 0;	// This registers is a boolean that is set when an interrupt 6 occurs and cleared when serviced in the code.
//These registers is available outside of the main loop (i.e., to the interrupt handlers)
volatile unsigned char LEDpattern, LEDperiod, LEDcountD;	// 3 bytes related to the 5 LEDs

int initGPIO(void)
{
	//Set up input output direction on Port C and G
	DDRB |= (1<<DB_LED);	// Set the display backlight's IO pin an an output. Leave other bits as they were.
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

int  flashLEDs()
{
	unsigned char temp;			//Allocate a temporary byte. Note! it is not the same byte as the byte named temp in the main routine. Do not confuse them even if they have the same name.
	if (LEDcountD != 0)
	{
		LEDcountD--;		// Decrement the countdown timer for another 5ms until it reaches 0
	} 
	else
	{
		LEDcountD = LEDperiod;	// Reset the countdown timer
		temp = LEDpattern & 0b00011111;
		switch (LEDpattern & 0b11100000)
		{
			case 0b10000000:
				temp = temp<<1;
				if (temp & 0b00100000)
				{
					temp |= 0b00000001;
					temp &= ~0b00100000;	// Do not keep a bit set where there is supposed to be a flashing command
				}
				break;
			case 0b01000000:
				temp = ~temp;		// Invert the light pattern
				temp &= 0b00011111;	// Clear the flashing command
				break;
			case 0b00100000:
				if (temp & 0b00000001)
				{
						temp |= 0b00100000;
				}
				temp = temp>>1;
				break;
		}
		LEDpattern = (LEDpattern & 0b11100000)|temp;	// Update the LEDpattern with the current pattern while keeping flashing commands
		PORTG = (PORTG & 0x11111100) | (temp & 0b00000011);	//Update the 2 Port G LEDs
		temp = temp >>2;
		PORTC = (PORTC & 0b11111000) | temp;	//Update the 3 Port C LEDs
	}
}

int main(void)
{
	unsigned char temp = 0x0F;		// Allocate memory for temp. It is initialized to 15 for demonstration purposes only.
	LEDpattern = 0b01000100;
	LEDperiod = 100;
	LEDcountD = 0;
	
	temp = initGPIO();				// Set up the data direction register for both ports C and G
	temp = initExtInt();			// Setup external interrupts
	temp = initTimer2();			// Setup 5ms internal interrupt
	sei();							// Set Global Interrupts
	
	while(1)
	{
	temp++;
	}			
}

SIGNAL(SIG_INTERRUPT6)  //Execute the following code if an INT6 interrupt has been generated. It is kept short.
{
	bToggle = 1;
	buttons = PINC;
}



SIGNAL(SIG_OUTPUT_COMPARE2) // This loop is executed every 5 ms (depending on the compare and match of timer 2)
{	
	if (bToggle)
	{
		switch(buttons & 0b11111000)
		{
			case 0b10000000:			//S5 center button
			//PORTC |= 0b00000100;	//Turn on Led5 if S5 is on
			LEDpattern = (LEDpattern & 0b00011111) | 0b01000000; // Swipe from Led1 to Led 5
			LEDcountD = 0;				// Do it next time Flash is executed
			break;
			case 0b01000000:			//S4  upper button
			//PORTC |= 0b00000010;	 //Turn on Led4 if S4 is on
			LEDperiod++;				// Increase the number of loops to blink
			LEDcountD = 0;				// Do it next time Flash is executed
			break;
			case 0b00100000:			//S3 left button
			//PORTC |= 0b00000001;	//Turn on Led3 if S3 is on
			LEDpattern = (LEDpattern & 0b00011111) | 0b10000000; // Alternate the LEDS
			LEDcountD = 0;				// Do it next time Flash is executed
			break;
			case 0b00010000:			//S2 lower button
			//PORTG |= 0b00000010;	//Turn on Led2 if S2 is on
			LEDperiod--;
			LEDcountD = 0;				// Do it next time Flash is executed
			break;
			case 0b00001000:			//S1 right button
			//PORTG |= 0b00000001;	//Turn on Led1 if S1 is on
			LEDpattern = (LEDpattern & 0b00011111) | 0b00100000; // Swipe from Led5 to Led1
			LEDcountD = 0;				// Do it next time Flash is executed
			break;
			default:
			//PORTC &= 0b11111000;	//Turn off Port C LEDs
			//PORTG &= 0x11111100;	//Turn off Port G LEDs
			break;
		}
		bToggle = 0;
   }
   flashLEDs();
}