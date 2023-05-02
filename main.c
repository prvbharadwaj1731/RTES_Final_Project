/* FreeRTOS 10 Tiva Demo
 *
 * main.c
 *
 * Andy Kobyljanec
 * Modified by Pranav M Bharadwaj for ECEN5623 Final Project
 *
 * This is a simple demonstration project of FreeRTOS 8.2 on the Tiva Launchpad
 * EK-TM4C1294XL.  TivaWare driverlib sourcecode is included.
 */

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "drivers/pinout.h"
#include "utils/uartstdio.h"


// TivaWare includes
#include "driverlib/sysctl.h"
#include "driverlib/debug.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"

#include "inc/tm4c1294ncpdt.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"


//additional includes for ADC operation
#include "driverlib/adc.h"
#include "driverlib/pin_map.h"
#include "inc/hw_ints.h"
#include "inc/hw_adc.h"

// FreeRTOS includes
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "projdefs.h"

#define SAMPLE_SEQUENCE_NUM     3
#define OVERSAMPLING_FACTOR     64
#define SAMPLING_RATE_VAL       8000
#define BUFFER_SIZE             8
#define EXAMPLE_RUNS            100

//**********************************GLOBALS SECTION**********************************

//to profile runtime
volatile uint32_t start_time = 0;
volatile uint32_t stop_time = 0;

//A-law encoding algo function declaration
int8_t ALaw_Encode(int16_t);
void ADC_Init(void);
void Timer_Init(void);

uint32_t ui32Period;
uint32_t ui32SysClkFreq;

//global holding overall tick count
static uint32_t sample_count = 0;
static uint32_t ADC_data = 0;
uint32_t output_clock_rate_hz;

//Semaphores
SemaphoreHandle_t xsemS1 = NULL;
SemaphoreHandle_t xsemS2 = NULL;

//Service declarations
void EncodingService(void *pvParameters);
void NetworkingService(void *pvParameters);

//Buffers for data and associated flags
static uint32_t buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];
static uint32_t encoded_buffer[BUFFER_SIZE];
static bool buffer_switch_flag = false;

//**********************************INTERRUPT HANDLERS**********************************

//User-defined interrupt handler for Timer 0 ISR which handles sampling
void Timer0IntHandler(void)
{
    /*
     * Perform the following steps:
     * 1. Clear interrupt flag
     * 2. Start ADC conversion by sending trigger
     * 3. Increment sample count
     * */

    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); //Clear interrupt flag
    ADCProcessorTrigger(ADC0_BASE, SAMPLE_SEQUENCE_NUM); //start conversion by sending trigger

    if((sample_count % BUFFER_SIZE) == 0)

    sample_count++; //increment
}

//User-defined interrupt handler for ADC0 which handles sampling
void ADC0IntHandler(void)
{
    /*
     * Perform the following steps:
     * 1. Clear interrupt flag
     * 2. Read sampled data from register
     * */

    ADCIntClear(ADC0_BASE, SAMPLE_SEQUENCE_NUM);
    ADCSequenceDataGet(ADC0_BASE, SAMPLE_SEQUENCE_NUM, &ADC_data);

    //that's it
}

//**********************************PERIPHERAL INITIALIZATION**********************************

//ADC Init
void ADCInit(void)
{
//   //
//   SysCtlPeripheralReset(SYSCTL_PERIPH_GPIOE); //always reset first for safety
//   SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
//
//   SysCtlPeripheralReset(SYSCTL_PERIPH_ADC0);
//   SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
//
//   //Set GPIO-E Pin PE3 to ADC0 input using GPIOPinType
//   GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
//
//   //Disable any previous sequence settings for safety
//   ADCSequenceDisable(ADC0_BASE, SAMPLE_SEQUENCE_NUM);
//
//   //We will currently use ADC in single sample mode, with the processor triggering it to take a sample every time necessary
//   //Use sample sequence 3 with default priority, since we are using only 1 (SS3 has a FIFO depth of 1 and performs 1 sampling process every time it is triggered
//   ADCSequenceConfigure(ADC0_BASE, SAMPLE_SEQUENCE_NUM, ADC_TRIGGER_PROCESSOR, 0);
//   //Configure step 0 on sample sequence 3, and configure interrupt flag (ADC_CTL_IE) to indicate processor that sampling is complete
//   ADCSequenceStepConfigure(ADC0_BASE, SAMPLE_SEQUENCE_NUM, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
//
//   //ADCHardwareOversampleConfigure(ADC0_BASE, OVERSAMPLING_FACTOR);
//
//   //Enable the sequence
//   ADCSequenceEnable(ADC0_BASE, SAMPLE_SEQUENCE_NUM);



   /*
    * Perform the following steps:
    * 1. Enable ADC0 peripheral on channel 1 and input AIN0 at pin PE3 (this is GPIOE)
    * 2. Set conversion rate/sampling rate
    * 3. User sequencer 3 (single-sample per trigger)
    * 4. Enable sequencer 3
    * 5. Set interrupt and enable them
    * */
   SysCtlPeripheralReset(SYSCTL_PERIPH_GPIOE); //always reset first for safety
   SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

   SysCtlPeripheralReset(SYSCTL_PERIPH_ADC0);
   SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
   GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

   ADCSequenceDisable(ADC0_BASE, SAMPLE_SEQUENCE_NUM); //Disable any previous sequence settings for safety
   ADCSequenceConfigure(ADC0_BASE, SAMPLE_SEQUENCE_NUM, ADC_TRIGGER_PROCESSOR, 0);
   ADCSequenceStepConfigure(ADC0_BASE, SAMPLE_SEQUENCE_NUM, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);

   ADCSequenceEnable(ADC0_BASE, SAMPLE_SEQUENCE_NUM);

   ADCIntClear(ADC0_BASE, SAMPLE_SEQUENCE_NUM);
   IntEnable(INT_ADC0SS3);
   ADCIntEnable(ADC0_BASE, SAMPLE_SEQUENCE_NUM);
}

void TimerInit(void)
{
    //Timer0 A initialization
    //Credits to the following link posted by Shreyan Prabhu: https://e2e.ti.com/support/microcontrollers/arm-based-microcontrollers-group/arm-based-microcontrollers/f/arm-based-microcontrollers-forum/686112/ek-tm4c1294xl-tm4c1294-tiva-c-series-run-timer-every-ms
    //SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    //GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);

    ui32Period = (output_clock_rate_hz / SAMPLING_RATE_VAL); //this division factors yields a timer with period = 125 ms or 8 KHz
    TimerLoadSet(TIMER0_BASE, TIMER_A, ui32Period);

    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    IntMasterEnable();

    TimerEnable(TIMER0_BASE, TIMER_A);
}

//**********************************SERVICE DEFINITIONS**********************************

void EncodingService(void *pvParameters)
{
    while(1){
        if(xSemaphoreTake(xsemS1, MAX_BLOCK) == pdTRUE){
            //Perform encoding here
        }
    }
}

void NetworkingService(void *pvParameters)
{
    while(1){
        if(xSemaphoreTake(xsemS2, MAX_BLOCK) == pdTRUE){
            //Send data here
        }
    }
}

//**********************************MAIN PROGRAM CODE**********************************
// Main function
int main(void)
{
    // Initialize system clock to 120 MHz
    output_clock_rate_hz = ROM_SysCtlClockFreqSet(
                               (SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
                                SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480),
                               SYSTEM_CLOCK);
    ASSERT(output_clock_rate_hz == SYSTEM_CLOCK);


    //Clear buffers
    memset(buffer1, 0, sizeof(buffer1));
    memset(buffer2, 0, sizeof(buffer2));
    memset(encoded_buffer, 0, sizeof(encoded_buffer));

    //**********************************Initialization**********************************

    UARTStdioConfig(0, UART_BAUD_RATE, SYSTEM_CLOCK);//Initialize Serial UART port for debug
    UARTprintf("ADC test\r\n");
    ADCInit();//Call ADC initialization function
    TimerInit();



    // Initialize the GPIO pins for the Launchpad
    PinoutSet(false, false);

    xTaskCreate(EncodingService, (const portCHAR *)"Encoding Service",
                        configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

    xTaskCreate(NetworkingService, (const portCHAR *)"Networking Service",
                        configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);



    vTaskStartScheduler();

    while(1){
        UARTprintf("ADC value = %d\r\n", ADC_data);
    }
    
    // Code should never reach this point
    return 0;
}


/*  ASSERT() Error function
 *
 *  failed ASSERTS() from driverlib/debug.h are executed in this function
 */
void __error__(char *pcFilename, uint32_t ui32Line)
{
    // Place a breakpoint here to capture errors until logging routine is finished
    while (1)
    {
    }
}
