/*************************************************************************
File Name:   Profiling
Name:        Automatic Window Control Program
Author:      Benjamin Degn
Created:     Jun 15, 2015
Modified:    Jun 15, 2015
Description:
    Controls the basic calibration and operation of the system.
*************************************************************************/

/*************************************************************************
                              GENRAL INCLUDES
*************************************************************************/

#include "TimerOne.h"

/*************************************************************************
                             DEFINED CONSTANTS
*************************************************************************/

#define BACK        0                   /* Reverse Direction            */
#define BUTTON      12                  /* Momentary Switch GPIO        */
#define CURRENT_INP A0                  /* Current Sense Analog Input   */
#define INPUT_A     2                   /* Motor Control Input A        */
#define INPUT_B     4                   /* Motor Control Input B        */
#define ITERATION   100                 /* Iteration Division           */
#define FORWARD     1                   /* Forward Direction            */
#define SPEED_CTRL  9                   /* Speed Control PWM            */
#define STATUS_LED  13                  /* Status LED GPIO              */

#define FLAG_STOP   0x1                 /* Motor Just Stopped Flag      */

/*************************************************************************
                             GLOBAL VARIABLES
*************************************************************************/

int g_averaged_value;                   /* Averaged Current Value       */
int g_button;                           /* Button Input                 */
int g_current_return;                   /* Current Return Value         */
int g_flags;                            /* Target Flags                 */
int g_i;                                /* Iteration Variable           */
int g_j;                                /* Iteration Variable           */
int g_state;                            /* Motor Direction State        */

/*************************************************************************
                                  METHODS
*************************************************************************/

/*------------------------------------------------------------------------

    PROCEDURE NAME: setup

    DESCRIPTION: Called at start-up before any processor functions are
                 initiated.

------------------------------------------------------------------------*/

void setup
    (
    void
    )
{
    /*--------------------------------------------------------------------
    Open the serial port
    --------------------------------------------------------------------*/
    Serial.begin(9600);
    while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
    }

    /*--------------------------------------------------------------------
    Set up the GPIO ports
    --------------------------------------------------------------------*/
    pinMode( INPUT_A, OUTPUT );
    pinMode( INPUT_B, OUTPUT );
    pinMode( STATUS_LED, OUTPUT );
    pinMode( BUTTON, INPUT );

    /*--------------------------------------------------------------------
    Set up Timer
    --------------------------------------------------------------------*/
    Timer1.initialize( 250000 );        /* Half-second timer            */
    Timer1.attachInterrupt( timer1callback );

    /*--------------------------------------------------------------------
    Set up the PWM
    --------------------------------------------------------------------*/
    Timer1.pwm( SPEED_CTRL, 512 );

    /*--------------------------------------------------------------------
    Set up the global variables
    --------------------------------------------------------------------*/
    g_averaged_value = 0;
    g_flags          = 0;
    g_i              = 0;
    g_j              = 0;
    g_state          = FORWARD;

    Serial.println("Power Up Complete!");
}

/*------------------------------------------------------------------------

    PROCEDURE NAME: timer1callback

    DESCRIPTION: Callback for the Timer 1 interrupt.

------------------------------------------------------------------------*/

void timer1callback
    (
    void
    )
{
    //digitalWrite( STATUS_LED, digitalRead( STATUS_LED ) ^ 1 );
    g_averaged_value /= g_i;
    g_i = 0;
    Serial.write('\r');
    Serial.print( g_averaged_value );
    g_averaged_value = 0;
}

/*------------------------------------------------------------------------

    PROCEDURE NAME: loop

    DESCRIPTION: Called in an infinite loop after the setup() function.

------------------------------------------------------------------------*/

void loop
    (
    void
    )
{
    /*--------------------------------------------------------------------
    Read the input from the momentary switch and the analog current sense
    --------------------------------------------------------------------*/
    g_button = digitalRead( BUTTON );
    g_current_return = analogRead( CURRENT_INP );

    /*--------------------------------------------------------------------
    Increment the iteration divisor and increment the current return until
    the timer ISR prints it out to reset it.
    --------------------------------------------------------------------*/
    if( ++g_j > 100 ) {
        g_averaged_value += g_current_return;
        g_i++;
        g_j = 0;
    }

    /*--------------------------------------------------------------------
    Move the motor when the button is depressed, stop the motor and
    change direction whenever it's released
    --------------------------------------------------------------------*/
    if( g_button == HIGH ) {
        if( !(g_flags & FLAG_STOP) ) {
            if( g_state == FORWARD ) {
                digitalWrite( INPUT_B, LOW );
                digitalWrite( INPUT_A, HIGH );
            } else {
                digitalWrite( INPUT_B, HIGH );
                digitalWrite( INPUT_A, LOW );
            }
            g_flags ^= FLAG_STOP;
        }
    } else {
        if( g_flags & FLAG_STOP ) {
            g_state ^= 1;
            digitalWrite( INPUT_A, LOW );
            digitalWrite( INPUT_B, LOW );
            g_flags ^= FLAG_STOP;
        }
    }
}
