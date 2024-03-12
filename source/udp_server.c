/******************************************************************************
* File Name:   udp_server.c
*
* Description: This file contains declaration of task and functions related to
*              UDP server operation.
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

/* Cypress secure socket header file */
#include "cy_secure_sockets.h"

/* Ethernet connection manager header files */
#include "cy_ecm.h"
#include "cy_ecm_error.h"

/* UDP server task header file. */
#include "udp_server.h"

/* Standard C header files */
#include <inttypes.h>

/*******************************************************************************
* Macros
********************************************************************************/

/* Maximum number of connection retries to the ethernet network */
#define MAX_ETH_RETRY_COUNT                       (3u)

/* RTOS related macros for UDP server task. */
#define RTOS_TASK_TICKS_TO_WAIT                   (1000)

/* Length of the LED ON/OFF command issued from the UDP server. */
#define UDP_LED_CMD_LEN                           (1)

/* LED ON and LED OFF commands. */
#define LED_ON_CMD                                '1'
#define LED_OFF_CMD                               '0'
#define LED_ON_ACK_MSG                            "LED ON ACK"

/* Initial message sent to UDP Server to confirm client availability. */
#define START_COMM_MSG                            'A'

/* Interrupt priority of the user button. */
#define USER_BTN_INTR_PRIORITY                    (5)

/* Buffer size to store the incoming messages from server, in bytes. */
#define MAX_UDP_RECV_BUFFER_SIZE                  (20)

/*******************************************************************************
* Function Prototypes
********************************************************************************/
static cy_rslt_t create_udp_server_socket(void);
static cy_rslt_t udp_server_recv_handler(cy_socket_t socket_handle, void *arg);
static void isr_button_press( void *callback_arg, cyhal_gpio_event_t event);
void print_heap_usage(char *msg);

/* Establish ethernet connection to the network. */
static cy_rslt_t connect_to_ethernet(void);

/*******************************************************************************
* Global Variables
********************************************************************************/

/* Secure socket variables. */
cy_socket_sockaddr_t udp_server_addr, peer_addr;
cy_socket_t server_handle;

/* Flag variable to track client connection status,
 * set to True when START_COMM_MSG is received from client. */
bool client_connected = false;

/* Flags to tack the LED state and command. */
bool led_state = CYBSP_LED_STATE_OFF;

/* UDP Server task handle. */
extern TaskHandle_t server_task_handle;

/* Ethernet manager connection handle. */
static cy_ecm_t ecm_handle = NULL;

cyhal_gpio_callback_data_t cb_data =
{
.callback = isr_button_press,
.callback_arg = NULL
};

/*******************************************************************************
 * Function Name: udp_server_task
 *******************************************************************************
 * Summary:
 *  Task used to establish a connection to a remote UDP client.
 *
 * Parameters:
 *  void *args : Task parameter defined during task creation (unused)
 *
 * Return:
 *  void
 *
 *******************************************************************************/
void udp_server_task(void *arg)
{
    cy_rslt_t result;

    /* Variable to store number of bytes sent over UDP socket. */
    uint32_t bytes_sent = 0;

    /* Variable to receive LED ON/OFF command from the user button ISR. */
    uint32_t led_state_cmd = LED_OFF_CMD;

    /* Initialize the user button (CYBSP_USER_BTN) and register interrupt on falling edge. */
    cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);
    cyhal_gpio_register_callback(CYBSP_USER_BTN, &cb_data);
    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL, USER_BTN_INTR_PRIORITY, true);

    /* Connect to ethernet network. */
    result = connect_to_ethernet();
    if(result!= CY_RSLT_SUCCESS )
    {
        printf("\n Failed to connect to the ethernet network! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        CY_ASSERT(0);
    }

    /* Secure Sockets initialization */
    result = cy_socket_init();
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Secure Socket initialization failed!\n");
        CY_ASSERT(0);
    }
    else
    {
        printf("Secure Socket initialized.\n");
    }

    /* Create UDP Server*/
    result = create_udp_server_socket();
    if (result != CY_RSLT_SUCCESS)
    {
        printf("UDP Server Socket creation failed. Error: %"PRIu32"\n", result);
        CY_ASSERT(0);
    }

    /* Toggle the LED on/off command sent to client on every button press */
    while(true)
    {
        /* Wait until a notification is received from the user button ISR. */
        xTaskNotifyWait(0, 0, &led_state_cmd, portMAX_DELAY);

        /* Send LED ON/OFF command to UDP client. */
        if(client_connected)
        {
            result = cy_socket_sendto(server_handle, &led_state_cmd, UDP_LED_CMD_LEN, CY_SOCKET_FLAGS_NONE,
                                      &peer_addr, sizeof(cy_socket_sockaddr_t), &bytes_sent);
            if(result == CY_RSLT_SUCCESS )
            {
                if(led_state_cmd == LED_ON_CMD)
                {
                    printf("LED ON command sent to UDP client\n");
                }
                else
                {
                    printf("LED OFF command sent to UDP client\n");
                }
            }
            else
            {
                printf("Failed to send command to client. Error: %"PRIu32"\n", result);
            }
            
            print_heap_usage("After sending LED ON/OFF command to client\n");
        }
      }
 }

/*******************************************************************************
 * Function Name: connect_to_ethernet
 *******************************************************************************
 * Summary:
 *  Connect to ethernet, retries up to a
 *  configured number of times until the connection succeeds.
 *
  * Parameters:
 *  None
 *
 * Return:
 *  cy_result result: Result of the operation.
 *  
 *******************************************************************************/
cy_rslt_t connect_to_ethernet(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    uint8_t retry_count = 0;

    /* Variables used by Ethernet connection manager.*/
    cy_ecm_phy_config_t ecm_phy_config;
    cy_ecm_ip_address_t ip_addr;

    #if ENABLE_STATIC_IP_ADDRESS
    cy_ecm_ip_setting_t static_ip_addr;

    static_ip_addr.ip_address.version = CY_ECM_IP_VER_V4;
    static_ip_addr.ip_address.ip.v4 = UDP_STATIC_IP_ADDR;
    static_ip_addr.gateway.version = CY_ECM_IP_VER_V4;
    static_ip_addr.gateway.ip.v4 = UDP_STATIC_GATEWAY;
    static_ip_addr.netmask.version = CY_ECM_IP_VER_V4;
    static_ip_addr.netmask.ip.v4 = UDP_NETMASK;
    #endif

    ecm_phy_config.interface_speed_type = CY_ECM_SPEED_TYPE_RGMII;
    ecm_phy_config.mode = CY_ECM_DUPLEX_AUTO;
    ecm_phy_config.phy_speed = CY_ECM_PHY_SPEED_AUTO;

    /* Initialize ethernet connection manager. */
    result = cy_ecm_init();
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Ethernet connection manager initialization failed! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        CY_ASSERT(0);
    }
    else
    {
        printf("Ethernet connection manager initialized.\n");
    }


    /* To change the MAC address,enter the desired MAC as the second parameter 
    in cy_ecm_ethif_init() instead of NULL. Default MAC address(00-03-19-45-00-00) 
    is used when NULL is passed. */
    result =  cy_ecm_ethif_init(CY_ECM_INTERFACE_ETH1, NULL, &ecm_phy_config, &ecm_handle);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Ethernet interface initialization failed! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        CY_ASSERT(0);
    }

    /* Establish a connection to the ethernet network */
    while(1)
    {
        #if ENABLE_STATIC_IP_ADDRESS
        /* Connect to the ethernet network with the assigned static IP address */
        result = cy_ecm_connect(ecm_handle, &static_ip_addr, &ip_addr);
        #else
        /* Connect to the ethernet network with the dynamically allocated IP address by DHCP */
        result = cy_ecm_connect(ecm_handle, NULL, &ip_addr);
        #endif
        if(result != CY_RSLT_SUCCESS)
        {
            retry_count++;
            if (retry_count >= MAX_ETH_RETRY_COUNT)
            {
                printf("Exceeded max ethernet connection attempts\n");
                return result;
            }
            printf("Connection to ethernet network failed. Retrying...\n");
            continue;
        }
        else
        {
            printf("Successfully connected to Ethernet.\n");
            printf("IP Address Assigned: %d.%d.%d.%d\n", (uint8)ip_addr.ip.v4,(uint8)(ip_addr.ip.v4 >> 8), (uint8)(ip_addr.ip.v4 >> 16),
                    (uint8)(ip_addr.ip.v4 >> 24));

            /* IP address and UDP port number of the UDP server */
            udp_server_addr.ip_address.ip.v4 = ip_addr.ip.v4;
            udp_server_addr.ip_address.version = CY_SOCKET_IP_VER_V4;
            udp_server_addr.port = UDP_SERVER_PORT;
            
            break;
        }
    }
    return result;

}

/*******************************************************************************
 * Function Name: create_udp_server_socket
 *******************************************************************************
 * Summary:
 *  Function to create a socket and set the socket options
 *
 *******************************************************************************/
cy_rslt_t create_udp_server_socket(void)
{
    cy_rslt_t result;

    /* Variable used to set socket options. */
    cy_socket_opt_callback_t udp_recv_option = {
            .callback = udp_server_recv_handler,
            .arg = NULL
    };

    /* Create a UDP server socket. */
    result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_DGRAM, CY_SOCKET_IPPROTO_UDP, &server_handle);
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }

    /* Register the callback function to handle messages received from UDP client. */
    result = cy_socket_setsockopt(server_handle, CY_SOCKET_SOL_SOCKET,
            CY_SOCKET_SO_RECEIVE_CALLBACK,
            &udp_recv_option, sizeof(cy_socket_opt_callback_t));
    if (result != CY_RSLT_SUCCESS)
    {
        return result;
    }

    /* Bind the UDP socket created to Server IP address and port. */
    result = cy_socket_bind(server_handle, &udp_server_addr, sizeof(udp_server_addr));
    if (result == CY_RSLT_SUCCESS)
    {
         printf("Socket bound to port: %d\n", udp_server_addr.port);
    }

    return result;
}

/*******************************************************************************
 * Function Name: udp_server_recv_handler
 *******************************************************************************
 * Summary:
 *  Callback function to handle incoming  message from UDP client
 *
 *******************************************************************************/
cy_rslt_t udp_server_recv_handler(cy_socket_t socket_handle, void *arg)
{
    cy_rslt_t result;

    /* Variable to store the number of bytes received. */
    uint32_t bytes_received = 0;

    /* Buffer to store data received from Client. */
    char message_buffer[MAX_UDP_RECV_BUFFER_SIZE] = {0};

    /* Receive incoming message from UDP server. */
    result = cy_socket_recvfrom(server_handle, message_buffer, MAX_UDP_RECV_BUFFER_SIZE,
                                CY_SOCKET_FLAGS_NONE, &peer_addr, NULL,
                                &bytes_received);

    message_buffer[bytes_received] = '\0';

    if(result == CY_RSLT_SUCCESS)
    {
        if(START_COMM_MSG == message_buffer[0])
        {
            client_connected = true;
            printf("UDP Client available on IP Address: %d.%d.%d.%d \n", (uint8)peer_addr.ip_address.ip.v4,
                    (uint8)(peer_addr.ip_address.ip.v4 >> 8), (uint8)(peer_addr.ip_address.ip.v4 >> 16),
                    (uint8)(peer_addr.ip_address.ip.v4 >> 24));
        }
        else
        {
            printf("\nAcknowledgement from UDP Client:\n");

            /* Print the message received from UDP client. */
            printf("%s",message_buffer);

            /* Set the LED state based on the acknowledgement received from the UDP client. */
            if(strcmp(message_buffer, LED_ON_ACK_MSG) == 0)
            {
                led_state = CYBSP_LED_STATE_ON;
            }
            else
            {
                led_state = CYBSP_LED_STATE_OFF;
            }
            printf("\n");
        }

    }
    else
    {
        printf("Failed to receive message from client. Error: %"PRIu32"\n", result);
        return result;
    }
    
    print_heap_usage("After receiving ACK from client\n");

    printf("===============================================================\n");
    printf("Press the user button to send LED ON/OFF command to the UDP client\n");

    return result;
}

/*******************************************************************************
 * Function Name: isr_button_press
 *******************************************************************************
 *
 * Summary:
 *  GPIO interrupt service routine. This function detects button presses and
 *  sets the command to be sent to UDP client.
 *
 * Parameters:
 *  void *callback_arg : pointer to the variable passed to the ISR
 *  cyhal_gpio_event_t event : GPIO event type
 *
 * Return:
 *  None
 *
 *******************************************************************************/
void isr_button_press( void *callback_arg, cyhal_gpio_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Variable to hold the LED ON/OFF command to be sent to the UDP client. */
    uint32_t led_state_cmd;

    /* Set the command to be sent to UDP client. */
    if(led_state == CYBSP_LED_STATE_ON)
    {
        led_state_cmd = LED_OFF_CMD;
    }
    else
    {
        led_state_cmd = LED_ON_CMD;
    }

    /* Set the flag to send command to UDP client. */
    xTaskNotifyFromISR(server_task_handle, led_state_cmd,
                      eSetValueWithoutOverwrite, &xHigherPriorityTaskWoken);

    /* Force a context switch if xHigherPriorityTaskWoken is now set to pdTRUE. */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* [] END OF FILE */

