/*
 * FreeRTOS V202112.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://aws.amazon.com/freertos
 *
 */

/**
 * Creates two sets of two tasks.  The tasks within a set share a variable, access
 * to which is guarded by a semaphore.
 *
 * Each task starts by attempting to obtain the semaphore.  On obtaining a
 * semaphore a task checks to ensure that the guarded variable has an expected
 * value.  It then clears the variable to zero before counting it back up to the
 * expected value in increments of 1.  After each increment the variable is checked
 * to ensure it contains the value to which it was just set. When the starting
 * value is again reached the task releases the semaphore giving the other task in
 * the set a chance to do exactly the same thing.  The starting value is high
 * enough to ensure that a tick is likely to occur during the incrementing loop.
 *
 * An error is flagged if at any time during the process a shared variable is
 * found to have a value other than that expected.  Such an occurrence would
 * suggest an error in the mutual exclusion mechanism by which access to the
 * variable is restricted.
 *
 * The first set of two tasks poll their semaphore.  The second set use blocking
 * calls.
 *
 * \page SemTestC semtest.c
 * \ingroup DemoFiles
 * <HR>
 */

/*
 * Changes from V1.2.0:
 *
 + The tasks that operate at the idle priority now use a lower expected
 +    count than those running at a higher priority.  This prevents the low
 +    priority tasks from signaling an error because they have not been
 +    scheduled enough time for each of them to count the shared variable to
 +    the high value.
 +
 + Changes from V2.0.0
 +
 + Delay periods are now specified using variables and constants of
 +    TickType_t rather than unsigned long.
 +
 + Changes from V2.1.1
 +
 + The stack size now uses configMINIMAL_STACK_SIZE.
 + String constants made file scope to decrease stack depth on 8051 port.
 */

#include <stdlib.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Demo app include files. */
#include "semtest.h"
#include "print.h"

/* The value to which the shared variables are counted. */
#define semtstBLOCKING_EXPECTED_VALUE        ( ( unsigned long ) 0xfff )
#define semtstNON_BLOCKING_EXPECTED_VALUE    ( ( unsigned long ) 0xff )

#define semtstSTACK_SIZE                     configMINIMAL_STACK_SIZE

#define semtstNUM_TASKS                      ( 4 )

#define semtstDELAY_FACTOR                   ( ( TickType_t ) 10 )

/* The task function as described at the top of the file. */
static void prvSemaphoreTest( void * pvParameters );

/* Structure used to pass parameters to each task. */
typedef struct SEMAPHORE_PARAMETERS
{
    SemaphoreHandle_t xSemaphore;
    volatile unsigned long * pulSharedVariable;
    TickType_t xBlockTime;
} xSemaphoreParameters;

/* Variables used to check that all the tasks are still running without errors. */
static volatile short sCheckVariables[ semtstNUM_TASKS ] = { 0 };
static volatile short sNextCheckVariable = 0;

/* Strings to print if USE_STDIO is defined. */
const char * const pcPollingSemaphoreTaskError = "Guarded shared variable in unexpected state.\r\n";
const char * const pcSemaphoreTaskStart = "Guarded shared variable task started.\r\n";

/*-----------------------------------------------------------*/

void vStartSemaphoreTasks( unsigned portBASE_TYPE uxPriority )
{
    xSemaphoreParameters * pxFirstSemaphoreParameters, * pxSecondSemaphoreParameters;
    const TickType_t xBlockTime = ( TickType_t ) 100;

    /* Create the structure used to pass parameters to the first two tasks. */
    pxFirstSemaphoreParameters = ( xSemaphoreParameters * ) pvPortMalloc( sizeof( xSemaphoreParameters ) );

    if( pxFirstSemaphoreParameters != NULL )
    {
        /* Create the semaphore used by the first two tasks. */
        vSemaphoreCreateBinary( pxFirstSemaphoreParameters->xSemaphore );

        if( pxFirstSemaphoreParameters->xSemaphore != NULL )
        {
            /* Create the variable which is to be shared by the first two tasks. */
            pxFirstSemaphoreParameters->pulSharedVariable = ( unsigned long * ) pvPortMalloc( sizeof( unsigned long ) );

            /* Initialise the share variable to the value the tasks expect. */
            *( pxFirstSemaphoreParameters->pulSharedVariable ) = semtstNON_BLOCKING_EXPECTED_VALUE;

            /* The first two tasks do not block on semaphore calls. */
            pxFirstSemaphoreParameters->xBlockTime = ( TickType_t ) 0;

            /* Spawn the first two tasks.  As they poll they operate at the idle priority. */
            xTaskCreate( prvSemaphoreTest, "PolSEM1", semtstSTACK_SIZE, ( void * ) pxFirstSemaphoreParameters, tskIDLE_PRIORITY, ( TaskHandle_t * ) NULL );
            xTaskCreate( prvSemaphoreTest, "PolSEM2", semtstSTACK_SIZE, ( void * ) pxFirstSemaphoreParameters, tskIDLE_PRIORITY, ( TaskHandle_t * ) NULL );
        }
    }

    /* Do exactly the same to create the second set of tasks, only this time
     * provide a block time for the semaphore calls. */
    pxSecondSemaphoreParameters = ( xSemaphoreParameters * ) pvPortMalloc( sizeof( xSemaphoreParameters ) );

    if( pxSecondSemaphoreParameters != NULL )
    {
        vSemaphoreCreateBinary( pxSecondSemaphoreParameters->xSemaphore );

        if( pxSecondSemaphoreParameters->xSemaphore != NULL )
        {
            pxSecondSemaphoreParameters->pulSharedVariable = ( unsigned long * ) pvPortMalloc( sizeof( unsigned long ) );
            *( pxSecondSemaphoreParameters->pulSharedVariable ) = semtstBLOCKING_EXPECTED_VALUE;
            pxSecondSemaphoreParameters->xBlockTime = xBlockTime / portTICK_PERIOD_MS;

            xTaskCreate( prvSemaphoreTest, "BlkSEM1", semtstSTACK_SIZE, ( void * ) pxSecondSemaphoreParameters, uxPriority, ( TaskHandle_t * ) NULL );
            xTaskCreate( prvSemaphoreTest, "BlkSEM2", semtstSTACK_SIZE, ( void * ) pxSecondSemaphoreParameters, uxPriority, ( TaskHandle_t * ) NULL );
        }
    }
}
/*-----------------------------------------------------------*/

static void prvSemaphoreTest( void * pvParameters )
{
    xSemaphoreParameters * pxParameters;
    volatile unsigned long * pulSharedVariable, ulExpectedValue;
    unsigned long ulCounter;
    short sError = pdFALSE, sCheckVariableToUse;

    /* See which check variable to use.  sNextCheckVariable is not semaphore
     * protected! */
    portENTER_CRITICAL();
    sCheckVariableToUse = sNextCheckVariable;
    sNextCheckVariable++;
    portEXIT_CRITICAL();

    /* Queue a message for printing to say the task has started. */
    vPrintDisplayMessage( &pcSemaphoreTaskStart );

    /* A structure is passed in as the parameter.  This contains the shared
     * variable being guarded. */
    pxParameters = ( xSemaphoreParameters * ) pvParameters;
    pulSharedVariable = pxParameters->pulSharedVariable;

    /* If we are blocking we use a much higher count to ensure loads of context
     * switches occur during the count. */
    if( pxParameters->xBlockTime > ( TickType_t ) 0 )
    {
        ulExpectedValue = semtstBLOCKING_EXPECTED_VALUE;
    }
    else
    {
        ulExpectedValue = semtstNON_BLOCKING_EXPECTED_VALUE;
    }

    for( ; ; )
    {
        /* Try to obtain the semaphore. */
        if( xSemaphoreTake( pxParameters->xSemaphore, pxParameters->xBlockTime ) == pdPASS )
        {
            /* We have the semaphore and so expect any other tasks using the
             * shared variable to have left it in the state we expect to find
             * it. */
            if( *pulSharedVariable != ulExpectedValue )
            {
                vPrintDisplayMessage( &pcPollingSemaphoreTaskError );
                sError = pdTRUE;
            }

            /* Clear the variable, then count it back up to the expected value
             * before releasing the semaphore.  Would expect a context switch or
             * two during this time. */
            for( ulCounter = ( unsigned long ) 0; ulCounter <= ulExpectedValue; ulCounter++ )
            {
                *pulSharedVariable = ulCounter;

                if( *pulSharedVariable != ulCounter )
                {
                    if( sError == pdFALSE )
                    {
                        vPrintDisplayMessage( &pcPollingSemaphoreTaskError );
                    }

                    sError = pdTRUE;
                }
            }

            /* Release the semaphore, and if no errors have occurred increment the check
             * variable. */
            if( xSemaphoreGive( pxParameters->xSemaphore ) == pdFALSE )
            {
                vPrintDisplayMessage( &pcPollingSemaphoreTaskError );
                sError = pdTRUE;
            }

            if( sError == pdFALSE )
            {
                if( sCheckVariableToUse < semtstNUM_TASKS )
                {
                    ( sCheckVariables[ sCheckVariableToUse ] )++;
                }
            }

            /* If we have a block time then we are running at a priority higher
             * than the idle priority.  This task takes a long time to complete
             * a cycle	(deliberately so to test the guarding) so will be starving
             * out lower priority tasks.  Block for some time to allow give lower
             * priority tasks some processor time. */
            vTaskDelay( pxParameters->xBlockTime * semtstDELAY_FACTOR );
        }
        else
        {
            if( pxParameters->xBlockTime == ( TickType_t ) 0 )
            {
                /* We have not got the semaphore yet, so no point using the
                 * processor.  We are not blocking when attempting to obtain the
                 * semaphore. */
                taskYIELD();
            }
        }
    }
}
/*-----------------------------------------------------------*/

/* This is called to check that all the created tasks are still running. */
BaseType_t xAreSemaphoreTasksStillRunning( void )
{
    static short sLastCheckVariables[ semtstNUM_TASKS ] = { 0 };
    portBASE_TYPE xTask, xReturn = pdTRUE;

    for( xTask = 0; xTask < semtstNUM_TASKS; xTask++ )
    {
        if( sLastCheckVariables[ xTask ] == sCheckVariables[ xTask ] )
        {
            xReturn = pdFALSE;
        }

        sLastCheckVariables[ xTask ] = sCheckVariables[ xTask ];
    }

    return xReturn;
}