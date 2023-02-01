/*==============================================================================
 File: PIANO2.c
 Date: January 31, 2023
 
 PIANO2 (PIC12F1840) hardware initialization function.
==============================================================================*/

#include	"xc.h"				// XC compiler general include file

#include	"stdint.h"			// Integer definition
#include	"stdbool.h"			// Boolean (true/false) definition

#include	"PIANO2.h"          // Include hardware constant definitions

void init(void)
{
	// Initialize oscillator
	
	OSCCON = 0b01101000;		// PLL off, 4 MHz HF internal oscillator
	
	// Initialize user ports and peripherals:

	OPTION_REG = 0b00101000;	// Weak pull-ups on, falling INT interrupt,
								// TRM0 internal from CPS, 1:1 (no pre-scaler)
	WPUA = 0b00001000;			// Enable weak pull-up on RA3 (S1 input)
    
    APFCON = 0b00000001;        // Configure PWM P1A output pin to RA5
	PORTA = 0;					// Clear port input registers and output latches
	LATA = 0;					// before configuring port pins
	ANSELA = 0b00010111;		// Set AN0-4 as analogue inputs for touch sensors

    PR2 = 0xFF;                 // Set PWM period
    CCP1CON = 0b00001100;       // Set CPP1 module to PWM mode, P1A out on RA5
    CCPR1L = 0;                 // PWM duty cycle = 0
    T2CON = 0b00000010;         // TMR2 off, 16:1 prescaler, 1:1 postscaler
    
	TRISA = 0b00011111;			// Set RA5 as digital output for piezo beeper
	
	CPSCON0 = 0b10001001;		// Enable Cap Sense module, fixed reference, TMR0
	CPSCON1 = 0;				// Select capacitive channel 0 (T1)

	WDTCON = 0b00001110;		// WDT off, div 4096 (~128ms period)

	// TODO Enable interrupts if needed
	
}



