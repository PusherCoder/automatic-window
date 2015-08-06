/*************************************************************************
File Name:   Profiling
Name:        Automatic Window Control Program
Author:      Benjamin Degn
Created:     Jun 15, 2015
Modified:    Aug 05, 2015
Description:
    Controls the basic calibration and operation of the system.
*************************************************************************/

/*************************************************************************
                              GENRAL INCLUDES
*************************************************************************/

#include "PinChangeInt.h"
#include "TimerOne.h"

/*************************************************************************
                             DEFINED CONSTANTS
*************************************************************************/

/*------------------------------------------------------------------------
General Definitions
------------------------------------------------------------------------*/
#define ITERATION   100                 /* Iteration Division           */

/*------------------------------------------------------------------------
Boolean Values
------------------------------------------------------------------------*/
#define FALSE       0                   /* False                        */
#define TRUE        1                   /* True                         */

/*------------------------------------------------------------------------
Arudino Pin Definitions
------------------------------------------------------------------------*/
#define BUTTON      12                  /* Momentary Switch GPIO        */
#define CURRENT_INP A0                  /* Current Sense Analog Input   */
#define INPUT_A     2                   /* Motor Control Input A        */
#define INPUT_B     4                   /* Motor Control Input B        */
#define SPEED_CTRL  9                   /* Speed Control PWM            */
#define STATUS_LED  13                  /* Status LED GPIO              */

/*------------------------------------------------------------------------
Motor Direction
------------------------------------------------------------------------*/
#define BACK        0                   /* Reverse Direction            */
#define FORWARD     1                   /* Forward Direction            */

/*------------------------------------------------------------------------
Flags for state control
------------------------------------------------------------------------*/
#define FLAG_STOP   0x1                 /* Motor Just Stopped Flag      */
#define FLAG_SETUP  0x2                 /* Motor Setup Flag             */
#define FLAG_RUN    0x4                 /* Running Flag                 */
#define FLAG_PWRUP  0x8                 /* Power Up Flag                */
#define FLAG_CHECK  0x10                /* First Direction Checked      */
#define FLAG_GATE   0x20                /* Gate One-Time Operations     */

/*************************************************************************
                             GLOBAL VARIABLES
*************************************************************************/

int g_averaged_value;                   /* Averaged Current Value       */
int g_button;                           /* Button Input                 */
int g_current_return;                   /* Current Return Value         */
int g_flags;                            /* Target Flags                 */
int g_i;                                /* Iteration Variable           */
int g_j;                                /* Iteration Variable           */
int g_profiling_array[ 2 ][ 200 ];      /* Profiling Array              */
int g_state;                            /* Motor Direction State        */
int g_time_in_direction[ 2 ];           /* Time Spent Going in Direction*/
int g_time_iteration;                   /* Iteration Time Variable      */

/*************************************************************************
                                  METHODS
*************************************************************************/

/*------------------------------------------------------------------------

    PROCEDURE NAME: clear_flag

    DESCRIPTION: Clears the indicated flag.

------------------------------------------------------------------------*/

void clear_flag
    (
    int            flags                /* Flags to set                 */
    )
{
    g_flags ^= flags;
    
}  /* clear_flag() */


/*------------------------------------------------------------------------

    PROCEDURE NAME: get_flag

    DESCRIPTION: Returns true if the requested flag is set.

------------------------------------------------------------------------*/

int get_flag
    (
    int            flags                /* Flags to check against       */
    )
{
    return ( ( g_flags & flags ) == flags );
    
}  /* get_flag() */


/*------------------------------------------------------------------------

    PROCEDURE NAME: set_flag

    DESCRIPTION: Sets the indicated flag.

------------------------------------------------------------------------*/

void set_flag
    (
    int            flags                /* Flags to set                 */
    )
{
    g_flags |= flags;
    
}  /* set_flag() */


/*------------------------------------------------------------------------

    PROCEDURE NAME: set_motor

    DESCRIPTION: Sets the motor output.
    
------------------------------------------------------------------------*/

void set_motor
    (
    int            enable,               /* Enable motor                */
    int            motor_direction       /* Set motor direction         */
    )
{
    if( enable ) {
        if( motor_direction == FORWARD ) {
            digitalWrite( INPUT_B, LOW );
            digitalWrite( INPUT_A, HIGH );
        } else {
            digitalWrite( INPUT_B, HIGH );
            digitalWrite( INPUT_A, LOW );
        }
    } else {
        digitalWrite( INPUT_B, LOW );
        digitalWrite( INPUT_A, LOW );
        g_state ^= 1;
    }   
            
}  /* set_motor() */


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
    Serial.begin( 9600 );
    while ( !Serial ) {
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
    Timer1.initialize( 250000 );        /* Quarter-second timer         */
    Timer1.attachInterrupt( timer1_callback );

    /*--------------------------------------------------------------------
    Set up button callback
    --------------------------------------------------------------------*/
    PCintPort::attachInterrupt( BUTTON, button_callback, CHANGE );

    /*--------------------------------------------------------------------
    Set up the PWM
    --------------------------------------------------------------------*/
    Timer1.pwm( SPEED_CTRL, 512 );

    /*--------------------------------------------------------------------
    Set up the global variables
    --------------------------------------------------------------------*/
    g_averaged_value = 0;
    g_flags          = FLAG_PWRUP;
    g_i              = 0;
    g_j              = 0;
    g_state          = FORWARD;
    g_time_iteration = 0;

    Serial.println( "Power Up Complete!" );
    Serial.println( "HOLD BUTTON UNTIL FULLY OPEN" );
    
}  /* setup() */


/*------------------------------------------------------------------------

    PROCEDURE NAME: button_callback

    DESCRIPTION: Callback for the Button interrupt.

------------------------------------------------------------------------*/

void button_callback
    (
    void
    )
{
    /*--------------------------------------------------------------------
    Debounce the pin; return if pin reads high
    --------------------------------------------------------------------*/
    g_button = digitalRead( BUTTON );
    delay( 2 );
    if( g_button != digitalRead( BUTTON ) ) return;
    
    if( get_flag( FLAG_PWRUP ) ) {
        /*----------------------------------------------------------------
        Wait for button to record motor time
        ----------------------------------------------------------------*/
        if( g_button == HIGH ) {
            if( !get_flag( FLAG_GATE ) ) {
                /*--------------------------------------------------------
                Set motor to correct direction
                --------------------------------------------------------*/
                set_motor( TRUE, g_state );
                
                /*--------------------------------------------------------
                Setup flags
                --------------------------------------------------------*/
                set_flag( FLAG_STOP );
                set_flag( FLAG_SETUP );
                set_flag( FLAG_GATE );
            }
            
        /*----------------------------------------------------------------
        Change direction when motor stops
        ----------------------------------------------------------------*/
        } else if( get_flag( FLAG_STOP ) ) {
            /*------------------------------------------------------------
            Stop the motor and print the elapsed time
            ------------------------------------------------------------*/
            Serial.print( "Took " );
            Serial.print( (float)( g_time_in_direction[ g_state ] / 4 ) );
            Serial.print( " seconds to move window.\n\r" );
            set_motor( FALSE, g_state );
            
            /*------------------------------------------------------------
            Reset time iteration
            ------------------------------------------------------------*/
            g_time_iteration = 0;
            
            /*------------------------------------------------------------
            If only first direction has run, switch directions
            ------------------------------------------------------------*/
            if( !get_flag( FLAG_CHECK ) ) {
                set_flag( FLAG_CHECK );
                clear_flag( FLAG_STOP );
                clear_flag( FLAG_GATE );
                clear_flag( FLAG_SETUP );
                Serial.println( "HOLD BUTTON UNTIL FULLY CLOSED" );
                
            /*------------------------------------------------------------
            If both directions have run, quit powerup sequence
            ------------------------------------------------------------*/
            } else {
                clear_flag( FLAG_CHECK );
                clear_flag( FLAG_PWRUP );
                clear_flag( FLAG_GATE );
                clear_flag( FLAG_STOP );
                clear_flag( FLAG_SETUP );
                set_flag( FLAG_RUN );
                Serial.println( "CALIBRATION COMPLETE" );
            }
        }
    }
    
    if( get_flag( FLAG_RUN ) ) {
        /*----------------------------------------------------------------
        Wait for button to activate motor
        ----------------------------------------------------------------*/
        if( g_button == HIGH ) {
            if( !get_flag( FLAG_GATE ) ) {
                /*--------------------------------------------------------
                Set motor to correct direction
                --------------------------------------------------------*/
                set_motor( TRUE, g_state );
                
                /*--------------------------------------------------------
                Setup flags
                --------------------------------------------------------*/
                set_flag( FLAG_GATE );
            }
        } 
    }

}  /* button_callback() */

/*------------------------------------------------------------------------

    PROCEDURE NAME: timer1_callback

    DESCRIPTION: Callback for the Timer 1 interrupt.

------------------------------------------------------------------------*/

void timer1_callback
    (
    void
    )
{
    if( get_flag( FLAG_SETUP ) ) {
        /*----------------------------------------------------------------
        Average the value over the number of samples
        ----------------------------------------------------------------*/
        g_averaged_value /= g_i;
        
        /*----------------------------------------------------------------
        Add the value to the array of values
        ----------------------------------------------------------------*/
        g_profiling_array[ g_state ][ g_time_iteration++ ] = g_averaged_value;
        
        /*----------------------------------------------------------------
        Print current value for debug
        ----------------------------------------------------------------*/
        Serial.println( g_averaged_value );
        
        /*----------------------------------------------------------------
        Reset iteration variables
        ----------------------------------------------------------------*/
        g_averaged_value = 0;
        g_i              = 0;
        
        /*----------------------------------------------------------------
        Increment time values
        ----------------------------------------------------------------*/
        g_time_in_direction[ g_state ]++;
    }
    
    /*--------------------------------------------------------------------
    Stop motor when running and time has elapsed
    --------------------------------------------------------------------*/
    if( get_flag( FLAG_RUN ) && get_flag( FLAG_GATE ) ) {
        if( ++g_time_iteration == g_time_in_direction[ g_state ] ) {
            set_flag( FLAG_STOP );
        }
    }

}  /* timer1_callback() */

/*------------------------------------------------------------------------

    PROCEDURE NAME: loop

    DESCRIPTION: Called in an infinite loop after the setup() function.

------------------------------------------------------------------------*/

void loop
    (
    void
    )
{
    if( get_flag( FLAG_SETUP ) || get_flag( FLAG_RUN ) ) {
        /*----------------------------------------------------------------
        Get current if in a setup or running state
        ----------------------------------------------------------------*/
        g_current_return = analogRead( CURRENT_INP );

        /*----------------------------------------------------------------
        Increment the iteration divisor and increment the current return
        until the timer ISR prints it out to reset it.
        ----------------------------------------------------------------*/
        if( ++g_j > ITERATION ) {
            g_averaged_value += g_current_return;
            g_i++;
            g_j = 0;
        }
    }
    
    if( get_flag( FLAG_RUN ) ) {
        /*----------------------------------------------------------------
        Stop the motor when it has run for the correct time
        ----------------------------------------------------------------*/
        if( get_flag( FLAG_STOP ) ) {
            /*------------------------------------------------------------
            Stop the motor and clear the flags
            ------------------------------------------------------------*/
            set_motor( FALSE, g_state );
            g_time_iteration = 0;
            clear_flag( FLAG_GATE );
            clear_flag( FLAG_STOP );
        }
    }
    
}  /* loop() */

