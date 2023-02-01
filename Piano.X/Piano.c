/*==============================================================================
 Project: Piano.c                       Link: mirobo.tech/piano2
 Date: January 31, 2023

 Piano with metronome program for the PIANO2 board. Press and release button S1
 to cycle through 3 operating modes: piano mode, metronome mode, and off mode.
 
 Piano Operation
 ===============
 When the batteries are first inserted, PIANO2 starts in piano mode. Press the
 painted 'keys' to play notes. The lowest note is A4 (concert A) and the
 ascending notes play the A major scale. Press both the highest and lowest
 keys at the same time to play the highest note, A5.
  
 Press and release S1 again to switch to metronome mode. In metronome mode each
 symbol above the keys controls a specific function:
 square - start and stop the metronome (sensed at the end of a beat period)
 arrows - decrease or increase the metronome beat frequency
 circle - enable a beat per measure count, cycling from 1 through 8, by changing
          metronome beat pitch (sensed at the end of a beat period, but changes
          after the end of each measure)
 
 Press and release S1 again to put PIANO2 into a low power mode (off mode). Note
 that PIANO2 never fully turns off and the batteries should be removed if it
 will not be used for an extended period of time.
  
 In this mode, the microcontroller is asleep, with it's core active but clock
 stopped. Periodically the WDT (Watch Dog Timer) will wake the core from sleep.
 
 Hardware and Software Features
 ==============================
 The low power sleep mode uses the microcontroller's WDT (Watch Dog Timer) to 
 periodically wake the core from sleep, so the program can check if S1 is being
 pressed and resume operation.
 
 PIANO2 uses the hardware CapSense module and TMR0 (Timer 0) to sense capacitive
 touch. Touching one of the four touch sensors (T1 - T4) causes the frequency of
 the CapSense oscillator to change, and its value is compared with the TMR0
 reference to determine if a touch occurred. To enable the four touch sensors on
 the 'keyboard' to produce seven notes, the Piano program checks if single or 
 adjacent touch sensors are pressed, as represented in the visual, below. To
 allow PIANO2 to play a full octave, an eighth note is played when both T1 and
 T4 are touched at the same time.

     |  T1   | |  T2   | |  T3   | |  T4   |
     |       | |       | |       | |       | 
     | note1 | | note3 | | note5 | | note7 |
     +-------+ +-------+ +-------+ +-------+
              |         |         |        |
            note2     note4     note6    note8* when touched with T1
 
 To make music note tones without using software time delays, the Piano program
 uses the microcontroller's PWM module and TMR2 (Timer 2) to generate 50% duty-
 cycle PWM waves at the required frequencies for each note. These are set as
 PWM period and on-time values were derived by experimentation.
 =============================================================================*/

#include    "xc.h"              // XC compiler general include file

#include    "stdint.h"          // Include integer definitions
#include    "stdbool.h"         // Include Boolean (true/false) definitions

#include	"PIANO2.h"          // Include hardware constants and functions

// Capacitive Sensing Module (CPS)/Touch sensor variables and threshold

unsigned int Ttemp;             // Temporary variable to initialize touch averages
unsigned char Tcount[4];		// CPS oscillator cycle counts for each touch sensor
unsigned char Tavg[4];			// Average count for each touch sensor
unsigned char Ttrip[4];			// Trip point for each touch sensor
unsigned char Tdelta[4];		// Difference of touch from average sensor count
const char Tthresh = 4;         // Sensor active threshold (below Tavg)

// Touch processing variables
unsigned char Tactive;			// Number of active touch targets (0 = none)
unsigned char Ttarget[4];       // Positions of active touch targets
unsigned char note = 0;         // Current note

// Piano operating mode constants and mode switch variables
#define off_mode 0
#define piano_mode 1
#define metronome_mode 2

bool modeSwitch = false;        // Mode switch in progress
unsigned char mode = piano_mode; // Current operating mode

// Metronome variables
bool beatOn = true;             // Metronome beating
bool settingChange = false;     // Key toggle boolean for setting changes
unsigned char beat = 0;         // Current beat count
unsigned char beats = 1;        // Beats per measure count
unsigned char bpm = 100;        // Starting metronome BPM (beats per minute)
unsigned char bpmIndex;         // Index to beatDelay table

// 40 BPM to 240 BPM metronome beat delay table (ms between beats)
const unsigned int beatDelay[41] = {
1500,1333,1200,1091,1000,923,857,800,
750,706,667,632,600,571,545,522,
500,480,462,444,429,414,400,387,
375,364,353,343,333,324,316,308,
300,293,286,279,273,267,261,255,
250 };

// Create a single metronome beat based on its beat count in the measure
void metronome_beat(unsigned int counts)
{
    if(beat == 0)                   // First beat is higher note
    {
        PR2 = 93;                   // Set PWM period
        CCPR1L = 47;                // Set PWM value
        TMR2ON = 1;                 // Enable tone output using PWM module
        __delay_ms(25);
        TMR2ON = 0;                 // Disable PWM output
    }
    else                            // Subsequent beets are low notes
    {
        PR2 = 111;
        CCPR1L = 56;
        TMR2ON = 1;
        __delay_ms(25);
        TMR2ON = 0;
    }
    beat++;                         // Increment beat counter after every beat
    if(beat == beats)
    {
        beat = 0;
    }
    for(counts; counts != 0; counts --) // Count BPM delay in ms (from table)
    {
        __delay_us(990);
    }
}

// Initialize and calibrate the touch sensor resting states
void init_touch(void)
{
	for(unsigned char i = 0; i != 4; i++)
	{
		CPSCON1 = i;				// Sense each of the 4 touch sensors in turn
		Ttemp = 0;					// Reset temporary counter
		for(unsigned char c = 16; c != 0; c--)
		{
			TMR0 = 0;				// Clear capacitive oscillator timer
			__delay_ms(1);			// Wait for fixed sensing time-base
			Ttemp += TMR0;			// Add capacitor oscillator count to temp
		}
		Tavg[i] = Ttemp / 16;		// Save average of 16 cycles
	}
}

// Read touch sensors and return number of active touch targets. The number of
// active touch targets is saved to Tactive, and Ttarget is set to the highest
// tripped touch target (1-4). If Tactive == 1, then Ttarget is the active
// touch sensor. If Tactive > 1, then compare Tdelta[(1-4)] values to determine
// the touch region.
unsigned char touch_input(void)
{
    Tactive = 0;                // Reset touch counter
    for(unsigned char i = 0; i != 4; i++)	// Check touch pads for new touch
    {
        CPSCON1 = i;            // Select each of the 4 touch sensors in turn
        TMR0 = 0;               // Clear cap oscillator cycle timer
        __delay_us(1000);       // Wait for fixed sensing time-base
        Tcount[i] = TMR0;       // Save current oscillator cycle count
        Tdelta[i] = (Tavg[i] - Tcount[i]);	// Calculate touch delta
        Ttrip[i] = Tavg[i] / 8; // Set trip point -12.5% below average
        if(Tcount[i] < (Tavg[i] - Ttrip[i]))    // Tripped?
        {
            Tactive ++;         // Increment active count for tripped sensors
            Ttarget[i] = 1;     // Save current touch target as real number
        }
        else                    // Not tripped?
        {
            Ttarget[i] = 0;
            if(Tcount[i] > Tavg[i]) // Average < count?
            {
                Tavg[i] = Tcount[i];    // Set average to prevent underflow
            }
            else                // Or, calculate new average
            {
                Tavg[i] = Tavg[i] - (Tavg[i] / 16) + (Tcount[i] / 16);                    
            }
        }
    }
    return(Tactive);
}

// Main Piano program starts here
int main(void)
{
	init();						// Initialize oscillator, I/O, and peripherals
	init_touch();				// Calibrate capacitive touch sensor averages
		
	while(1)                    // Main program loop
	{
		SWDTEN = 0;				// Disable Watch Dog Timer
		
        // Nap during off mode. Use the Watch Dog Timer to wake from nap and
        // check if the button is pressed. If pressed, switch to piano mode.
        while(mode == off_mode)
        {
            CPSON = 0;              // Disable CapSense module
            SWDTEN = 1;				// Enable Watch Dog Timer
            SLEEP();				// Nap to save power. Wake up every ~128 ms.
            
            SWDTEN = 0;             // Disable WDT
            if(S1 == 0 && modeSwitch == 0)  // Check for button press after wake-up
            {
                SWDTEN = 0;         // Disable Watch Dog
                CPSON = 1;          // Enable CapSense module
                modeSwitch = true;
                mode = piano_mode;  // Switch to piano mode
            }
            
            if(S1 == 1)
            {
                modeSwitch = false;
            }
        }

        // Piano mode - determine which sensors are touched and play the note
        while(mode == piano_mode)
        {
            if(touch_input() > 0)   // Check for touch sensor activity
            {
                if(Ttarget[0] == 1 && Ttarget[3] == 1)  // Left and right keys
                {
                    note = 8;
                }
                else if(Ttarget[0] == 1 && Ttarget[1] == 0)  // Right-most key
                {
                    note = 7;
                }
                else if(Ttarget[0] == 1 && Ttarget[1] == 1)
                {
                    note = 6;
                }
                else if(Ttarget[0] == 0 && Ttarget[1] == 1 && Ttarget[2] == 0)
                {
                    note = 5;
                }
                else if(Ttarget[1] == 1 && Ttarget[2] == 1)
                {
                    note = 4;
                }
                else if(Ttarget[1] == 0 && Ttarget[2] == 1 && Ttarget[3] == 0)
                {
                    note = 3;
                }
                else if(Ttarget[2] == 1 && Ttarget[3] == 1)
                {
                    note = 2;
                }
                else if(Ttarget[3] == 1 && Ttarget[2] == 0)    // Left-most key
                {
                    note = 1;
                }
            }
            else
            {
                note = 0;
            }
		
            if(S1 == 0 && modeSwitch == 0)  // Check for mode switch
            {
                modeSwitch = true;
                mode = metronome_mode;
                beatOn = true;
            }
            
            if(S1 == 1)                 // Reset mode switch activity
            {
                modeSwitch = false;
            }

            if(note == 8)               // Note A5
            {
                PR2 = 69;               // Set PWM period
                CCPR1L = 35;            // Set PWM value
                TMR2ON = 1;             // Enable PWM module to play note
            }
            else if(note == 7)          // Note G#5
            {
                PR2 = 74;
                CCPR1L = 38;
                TMR2ON = 1;
            }
            else if(note == 6)          // Note F#5
            {
                PR2 = 83;
                CCPR1L = 42;
                TMR2ON = 1;
            }
            else if(note == 5)          // Note E5
            {
                PR2 = 93;
                CCPR1L = 47;
                TMR2ON = 1;
            }
            else if(note == 4)          // Note D5
            {
                PR2 = 105;
                CCPR1L = 53;
                TMR2ON = 1;
            }
            else if(note == 3)          // Note C#5
            {
                PR2 = 111;
                CCPR1L = 56;
                TMR2ON = 1;
            }
            else if(note == 2)          // Note B4
            {
                PR2 = 125;
                CCPR1L = 63;
                TMR2ON = 1;
           }
            else if(note == 1)          // Note A4
            {
                PR2 = 140;
                CCPR1L = 71;
                TMR2ON = 1;
            }
            else
            {
                TMR2ON = 0;             // Disable PWM module
            }
        }
        
        // Metronome mode - make beats, use touch sensors to start and stop the
        // metronome, modify BPM rate (Beats per Minute), and control beats per
        // measure (from 1 to 8) to make different beat tones.
        while(mode == metronome_mode)
        {
            if(beatOn == true)              // Make beats if metronome is running
            {
                bpmIndex = bpm - 40;        // Convert from BPM to delay using
                if(bpmIndex == 0)           // table look-up
                {
                    metronome_beat(beatDelay[0] - 20);
                }
                else
                {
                    bpmIndex = bpmIndex / 5;
                    metronome_beat(beatDelay[bpmIndex] - 20);
                }
            }
            
            if(S1 == 0 && modeSwitch == 0)  // Check for mode switch
            {
                modeSwitch = true;
                mode = off_mode;
            }
            
            if(S1 == 1)                     // Reset mode switch activity
            {
                modeSwitch = false;
            }
            
            if(touch_input() > 0)
            {
                if(Ttarget[0] == 1 && settingChange == false)   // Beat/measure
                {
                    settingChange = true;
                    beats++;
                    if(beats >= 9)
                    {
                        beats = 1;
                        beat = 0;
                    }
                }
                else if(Ttarget[1] == 1)        // Increase bpm in steps of 5
                {
                    if(bpm < 240)
                    {
                        bpm += 5;
                    }
                }
                else if(Ttarget[2] == 1)        // Decrease bpm in steps of 5
                {
                    if(bpm > 60)
                    {
                        bpm -= 5;
                    }
                }
                else if(Ttarget[3] == 1 && settingChange == false)
                {
                    settingChange = true;
                    beatOn = !beatOn;           // Toggle beats on or off
                }
            }
            else
            {
                settingChange = 0;
            }
        }
	}
}
