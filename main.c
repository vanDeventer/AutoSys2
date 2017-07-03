/*
 * AutoSys2.c
 *
 * Created: 13-Nov-16 11:36:25 AM
 * Updated: 03-July-2017
 * Author : Jan vanDeventer; email: jan.van.deventer@ltu.se
 */ 

/*
 * Purpose of this version:
 * The purpose of this version of the program is to use one of the Analog to Digital Converter (ADC) channel (ADC0).
 * More information on interrupts can be found at http://www.avr-tutorials.com/interrupts/about-avr-8-bit-microcontrollers-interrupts and http://www.nongnu.org/avr-libc/user-manual/group__avr__interrupts.html 
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

volatile uint16_t adc_value;  //Allocate the double byte memory space into which the result of the 10 bits Analog to Digital Converter (ADC) is stored.


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
	
	LEDpattern = 0b01000100;		// Flash the pattern the LED pattern 00100
	LEDperiod = 100;				// LEDperiod x initTimer2 period.
	LEDcountD = 0;
	
	temp = initGPIO();				// Set up the data direction register for both ports C and G
	temp = initExtInt();			// Setup external interrupts
	temp = initTimer2();			// Setup 5ms internal interrupt
	temp = initADC();				// Setup the Analog to Digital Converter
	ADCSRA |= (1<<ADSC);			//Start ADC

	sei();							// Set Global Interrupts
	
	while(1){						// LOOP: does not do much more than increase temp.
		temp++;
	}			
}

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