/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * http://www.rt-labs.com
 * Copyright 2022 rt-labs AB, Sweden.
 * See LICENSE file in the project root for full license information.
 ********************************************************************/

#include "cyhal.h"
#include "cybsp.h"

#include "options.h"
#include "up_api.h"
#include "up_util.h"
#include "model.h"

#include "uphy_demo_app.h"

#include "math.h"
#include "osal.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#ifndef UPHY_DEMO_BUSTYPE
#define UPHY_DEMO_BUSTYPE UP_BUSTYPE_PROFINET
/*#define UPHY_DEMO_BUSTYPE UP_BUSTYPE_ETHERNETIP*/
#endif

extern cy_rslt_t connect_to_ethernet (void);

/* U-Phy callbacks */
static void cb_avail (up_t * up, void * user_arg);
static void cb_sync (up_t * up, void * user_arg);
static void cb_param_write_ind (up_t * up, void * user_arg);
static void cb_status_ind (up_t * up, uint32_t status, void * user_arg);
static void cb_status_ind (up_t * up, uint32_t status, void * user_arg);
static void cb_error_ind (up_t * up, up_error_t error_code, void * user_arg);
static void cb_profinet_signal_led_ind (up_t * up, void * user_arg);

static up_busconf_t up_busconf;

static up_cfg_t cfg = {
   .device = &up_device,
   .busconf = &up_busconf,
   .vars = up_vars,
   .sync = cb_sync,
   .avail = cb_avail,
   .param_write_ind = cb_param_write_ind,
   .status_ind = cb_status_ind,
   .error_ind = cb_error_ind,
   .profinet_signal_led_ind = cb_profinet_signal_led_ind,
};

/* Use EVK user LEDs and BTNs are only mapped to process data for the DIGIO
 * device. */
static bool is_digio_sample_device = true;

static TaskHandle_t uphy_task_hdl;

static const char * error_code_to_str (up_error_t error_code)
{
   switch (error_code)
   {
   case UP_ERROR_NONE:
      return "UP_ERROR_NONE";
   case UP_ERROR_CORE_COMMUNICATION:
      return "UP_ERROR_CORE_COMMUNICATION";
   case UP_ERROR_PARAMETER_WRITE:
      return "UP_ERROR_PARAMETER_WRITE";
   case UP_ERROR_PARAMETER_READ:
      return "UP_ERROR_PARAMETER_READ";
   case UP_ERROR_INVALID_PROFINET_MODULE_ID:
      return "UP_ERROR_INVALID_PROFINET_MODULE_ID";
   case UP_ERROR_INVALID_PROFINET_SUBMODULE_ID:
      return "UP_ERROR_INVALID_PROFINET_SUBMODULE_ID";
   case UP_ERROR_INVALID_PROFINET_PARAMETER_INDEX:
      return "UP_ERROR_INVALID_PROFINET_PARAMETER_INDEX";
   default:
      return "UNKNOWN";
   }
}

/*
 * Callback indicating that output data (from PLc) is available.
 * Called every U-Phy cycle.
 */
static void cb_avail (up_t * up, void * user_arg)
{
   up_read_outputs (up);

   /* Apply process data to actual device outputs  */
   if (is_digio_sample_device)
   {
      digio_set_output (up_data.O8.Output_8_bits.value);
   }
}
/*
 * Callback indicating that input data (to PLC) shall be updated.
 * Called every U-Phy cycle.
 */
static void cb_sync (up_t * up, void * user_arg)
{
   if (is_digio_sample_device)
   {
      up_data.I8.Input_8_bits.value = digio_get_input();
   }
   up_write_inputs (up);
}

static void cb_param_write_ind (up_t * up, void * user_arg)
{
   uint16_t slot_ix;
   uint16_t param_ix;
   binary_t data;
   up_param_t * p;

   while (up_param_get_write_req (up, &slot_ix, &param_ix, &data) == 0)
   {
      p = &up_device.slots[slot_ix].params[param_ix];
      memcpy (up_vars[p->ix].value, data.data, data.dataLength);
   }
}

static void cb_status_ind (up_t * up, uint32_t status, void * user_arg)
{
   led_set_running_mode ((bool)(status & UP_CORE_RUNNING));
}

static void cb_error_ind (up_t * up, up_error_t error_code, void * user_arg)
{
   printf (
      "U-Phy Core Error: error_code=%" PRIi16 " %s\n",
      error_code,
      error_code_to_str (error_code));
}

static void cb_profinet_signal_led_ind (up_t * up, void * user_arg)
{
   led_profinet_signal();
}

void up_app_main (up_t * up)
{
   if (up_init_device (up) != 0)
   {
      printf ("Failed to configure device\n");
      exit (EXIT_FAILURE);
   }

   if (up_util_init (&up_device, up, up_vars) != 0)
   {
      printf ("Failed to init up utils\n");
      exit (EXIT_FAILURE);
   }

   if (up_start_device (up) != 0)
   {
      printf ("Failed to start device\n");
      exit (EXIT_FAILURE);
   }

   /* Write input signals to set initial values and status */
   up_write_inputs (up);

   printf ("Run event loop\n");

   extern bool up_worker (up_t * up);
   while (up_worker (up) == true)
      ;

   printf ("Unexpected error in U-Phy library.\n");
   printf ("Restart device\n");
}

up_t * up_app_init (void)
{
   up_t * up;

   /* User LEDs and buttons are only mapped to process io data for the default
    * DIGIO sample
    */
   if (strcmp (cfg.device->name, "U-Phy DIGIO Sample") != 0)
   {
      is_digio_sample_device = false;
   }

   cfg.device->bustype = UPHY_DEMO_BUSTYPE;

#if (UPHY_DEMO_BUSTYPE == UP_BUSTYPE_PROFINET)
   up_busconf.profinet = up_profinet_config;
#elif (UPHY_DEMO_BUSTYPE == UP_BUSTYPE_ETHERNETIP)
   up_busconf.ethernetip = up_ethernetip_config;
#elif (UPHY_DEMO_BUSTYPE == UP_BUSTYPE_MOCK)
   up_busconf.mock = up_mock_config;
#else
#error "UPHY_DEMO_BUSTYPE is not defined or bus is not supported"
#endif

   extern void up_core_init (void); // TBD - why not in header-file
   up_core_init();

   up = up_init (&cfg);

   return up;
}

void uphy_task (void *)
{
   up_t * up;

   printf ("Starting U-Phy Demo\n");
   printf ("Active device model: \"%s\"\n", cfg.device->name);

   /* Connect to ethernet network. */
   /* TBD - I think this shall be moved to porting layer or somehow
    * automatically handled by lib.
    * For example when running Profinet DHCP shall not be initiated by
    * configured ip used. */
   cy_rslt_t result = connect_to_ethernet();
   if (result != CY_RSLT_SUCCESS)
   {
      printf (
         "\n Failed to connect to the ethernet network! Error code: "
         "0x%08" PRIx32 "\n",
         (uint32_t)result);
      CY_ASSERT (0);
   }

   printf ("Init U-Phy Device \n");
   up = up_app_init();

   printf ("Run U-Phy Device \n");
   up_app_main (up);

   printf ("error - unexptected return from up_app_main()\n");
   CY_ASSERT (0);
}

void start_demo (void)
{
   xTaskCreate (
      uphy_task,
      "uphy_task",
      5000,
      (void *)NULL,
      OS_PRIORITY_HIGH,
      &uphy_task_hdl);
}
