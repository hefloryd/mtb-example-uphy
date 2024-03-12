/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for Ethernet UDP Server Example in ModusToolbox.
*              In this example, UDP server waits for a connection with a UDP
*              client. Once a message is received from UDP client, the server
*              allows the user to send LED ON/OFF command to the UDP client and
*              the client responds by sending an acknowledgement message to the
*              server.
*
* Related Document: See README.md
*
********************************************************************************
* Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/
/* Header file includes */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* FreeRTOS header file */
#include <FreeRTOS.h>
#include <task.h>

/* UDP server task header file. */
#include "udp_server.h"

/*******************************************************************************
* Macros
********************************************************************************/

/* RTOS related macros for UDP server task. */
#define UDP_SERVER_TASK_STACK_SIZE                (5 * 1024)
#define UDP_SERVER_TASK_PRIORITY                  (1)

#define UPHY_RUNMODE_PROFINET 0
#define UPHY_RUNMODE_ETHERNET_IP 1

/*******************************************************************************
* Global Variables
********************************************************************************/

/* UDP server task handle. */
TaskHandle_t server_task_handle;

#define app_main_priority      OS_PRIORITY_BELOWNORMAL
static TaskHandle_t app_task_hdl;


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  System entrance point. This function sets up user tasks and then starts
*  the RTOS scheduler.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/

/* Header file includes */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* FreeRTOS header file */
#include <FreeRTOS.h>
#include <task.h>

/* Cypress secure socket header file */
#include "cy_secure_sockets.h"

/* Ethernet connection manager header files */
#include "cy_ecm.h"
#include "cy_ecm_error.h"

/* UDP server task header file. */
#include "udp_server.h"

/* Standard C header files */
#include <inttypes.h>

#include "osal.h"

cy_rslt_t connect_to_ethernet(void);
int uphy_main (int argc, char * argv[]);

/* configure runmode */
#define UPHY_DEFAULT_MODE UPHY_RUNMODE_PROFINET

void uphy_task(void *)
{
#if UPHY_DEFAULT_MODE == UPHY_RUNMODE_PROFINET
    char * argv[2] = {"up_autostart", "profinet"};
#elif UPHY_DEFAULT_MODE == UPHY_RUNMODE_ETHERNET_IP
    char * argv[2] = {"up_autostart", "ethernetip"};
#endif

    cy_rslt_t result;

    /* Connect to ethernet network. */
    result = connect_to_ethernet();
    if(result!= CY_RSLT_SUCCESS )
    {
        printf("\n Failed to connect to the ethernet network! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        CY_ASSERT(0);
    }

    uphy_main(2, argv);
}

int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
    printf("\x1b[2J\x1b[;H");

    xTaskCreate( uphy_task, "uphy_main", 4000, ( void * ) NULL, OS_PRIORITY_HIGH, &app_task_hdl);

    /* Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Should never get here. */
    CY_ASSERT(0);
}


__attribute__((noreturn)) void exit (int __status)
{
   printf("exit called\n");
   CY_HALT();
   while (1)
      vTaskDelay(1000);
} ;

void HardFault_Handler      (void)
{
   printf("HardFault_Handler\n");

#if defined(PRINT_HEAP_USAGE)
   print_heap_usage();
#endif

   CY_HALT();
};

void NMIException_Handler     (void)
{
   printf("NMIException_Handler\n");
   CY_HALT();
};

void MemManage_Handler     (void)
{
   printf("MemManage_Handler\n");
   CY_HALT();
};

void BusFault_Handler       (void)
{
   printf("BusFault_Handler\n");
   CY_HALT();
};

void UsageFault_Handler       (void)
{
   printf("UsageFault_Handler\n");
   CY_HALT();
};

void DebugMon_Handler       (void)
{
   printf("DebugMon_Handler\n");
   CY_HALT();
};

void cy_halt(void)
{
	printf("*** HALT ! ***\n");
    CY_HALT();
}

void vApplicationMallocFailedHook( void )
{
   taskDISABLE_INTERRUPTS();
   CY_ASSERT(0U != 0U);
   CY_HALT();
   for( ;; )
   {
   }
}

void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                        char * pcTaskName )
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    CY_ASSERT(0U != 0U);
    CY_HALT();
    for( ;; )
    {
    }
}

/* [] END OF FILE */
