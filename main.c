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
#include <stddef.h>
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
#include "inc/hw_types.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "projdefs.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "message_buffer.h"

#define SAMPLE_SEQUENCE_NUM     3
#define OVERSAMPLING_FACTOR     64
#define SAMPLING_RATE_VAL       8000
#define MESSAGE_BUFFER_SIZE     10
#define EXAMPLE_RUNS            100
#define ERROR_RETURN_CODE       -1
#define SUCCESS_RETURN_CODE     0
#define MAX_BLOCK               (0xffff)

//**********************************GLOBALS SECTION**********************************

//to profile runtime
volatile uint32_t start_time = 0;
volatile uint32_t stop_time = 0;

//A-law encoding algo function declaration
int g711_encode(uint32_t);
void ADC_Init(void);
void Timer_Init(void);

uint32_t ui32Period;
uint32_t ui32SysClkFreq;

//global holding overall tick count
static uint32_t sample_count = 0;
static uint32_t ADC_data = 0;
uint32_t output_clock_rate_hz;

//Semaphores
SemaphoreHandle_t task1_sem = NULL;

//Service declarations
void NetworkingService(void *pvParameters);

//Queue handles and associated flags
static QueueHandle_t xQueue1, xQueue2; //queues to exchange data between ISR and task
static bool queue_switch_flag = false; //When cleared, use xQueue1. When set, use xQueue2

BaseType_t HigherPriorityTaskWoken = pdFALSE; //Needed for using semaphores and message queues

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

    //Check if xQueue1 is filled
    if((sample_count % MESSAGE_BUFFER_SIZE) == 0){
        queue_switch_flag = !queue_switch_flag; //flip flag
        //Give semaphore to networking task to begin
        xSemaphoreGiveFromISR(task1_sem, &HigherPriorityTaskWoken);
    }

    ADCProcessorTrigger(ADC0_BASE, SAMPLE_SEQUENCE_NUM); //start conversion by sending trigger
    sample_count++; //increment
}

//User-defined interrupt handler for ADC0 which handles sampling
void ADC0IntHandler(void)
{
    /*
     * Perform the following steps:
     * 1. Clear interrupt flag
     * 2. Read sampled data from register
     * 3. Perform G.711 encoding
     * 4. Send message data over FreeRTOS message buffer
     * */

    ADCIntClear(ADC0_BASE, SAMPLE_SEQUENCE_NUM);
    ADCSequenceDataGet(ADC0_BASE, SAMPLE_SEQUENCE_NUM, &ADC_data);

    int encoded_data = g711_encode(ADC_data);
    //Send data over the appropriate queue based on the flag value
    if(queue_switch_flag){
        xQueueSendFromISR(xQueue2, &encoded_data, &HigherPriorityTaskWoken);
        portYIELD_FROM_ISR(HigherPriorityTaskWoken);
    }
    else{
        xQueueSendFromISR(xQueue1, &encoded_data, &HigherPriorityTaskWoken);
        portYIELD_FROM_ISR(HigherPriorityTaskWoken);
    }
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

void NetworkingService(void *pvParameters)
{
    int received_data[MESSAGE_BUFFER_SIZE];
    int idx = 0;
    while(1){
        if(xSemaphoreTake(task1_sem, MAX_BLOCK) == pdTRUE){
            //Send data here
            while(idx < MESSAGE_BUFFER_SIZE){
                if(queue_switch_flag){
                    xQueueReceive(xQueue2, &(received_data[idx]), portMAX_DELAY);
                    idx++;
                }
                else{
                    xQueueReceive(xQueue1, &(received_data[idx]), portMAX_DELAY);
                    idx++;
                }
            }

            //clear idx
            idx = 0;
            //examine data here while debugging
        }
    }
}

//**********************************OTHER SUPPORT CODE**********************************

//Performs G.711 encoding on voice data from ADC. Has to be re-entrant
int g711_encode(uint32_t raw_data)
{
    const int bias = 0x84; //bias used to convert uint32_t ADC data to int8_t PCM data
    const int exponent = 132; //used to linearize the quantization curve
    const uint32_t mask = 0x7FFF; //mask to extract sign and magnitude of voice

    int sign = (raw_data & 0x8000) ? -1 : 1; //Check MSB, which is sign bit in signed integer and determine sign
    int magnitude = (raw_data & mask) >> 1; //extract magnitude and remove sign

    magnitude = (magnitude + exponent)/256; //Here we linearize the magnitude value
    magnitude = (magnitude > 127) ? 127 : magnitude; //saturate the magnitude value at MAX VAL of 127

    int pcm_sample = (magnitude * sign) + bias;
    return pcm_sample;
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


    //**********************************Initialization**********************************

    UARTStdioConfig(0, UART_BAUD_RATE, SYSTEM_CLOCK);//Initialize Serial UART port for debug
    UARTprintf("ADC test\r\n");

    //create queues
    xQueue1 = xQueueCreate(MESSAGE_BUFFER_SIZE, sizeof(int));
    xQueue2 = xQueueCreate(MESSAGE_BUFFER_SIZE, sizeof(int));

    ADCInit();//Call ADC initialization function
    TimerInit();



    // Initialize the GPIO pins for the Launchpad
    PinoutSet(false, false);

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
