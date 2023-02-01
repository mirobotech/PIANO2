/*==============================================================================
 File: PIANO2.h
 Date: January 31, 2023
 
 PIANO2 (PIC12F1840) symbolic constant definitions and function prototypes.
==============================================================================*/

// I/O Port definitions

#define S1			RA3			// Switch S1 input
#define P1			RA5			// Piezo beeper P1 output
#define BEEPER		RA5			// Piezo beeper P1 output

//Capacitive touch sensor input channel constants

unsigned char T1 = 0;           // CPS channel constants for each touch sensor
unsigned char T2 = 1;           // Used to select channels in CPSCON1 register
unsigned char T3 = 2;
unsigned char T4 = 3;

// Clock frequency definitions for delay macros and simulation

#define _XTAL_FREQ	4000000     // Set clock frequency for time delay calculations
#define FCY	_XTAL_FREQ/4        // Processor instruction cycle time

// TODO - Add function prototypes for all functions in PIANO2.c here:

void init(void);                // Initialization function prototype