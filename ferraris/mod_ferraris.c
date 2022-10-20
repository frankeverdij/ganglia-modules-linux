/*******************************************************************************
* Copyright (C) 2007 Novell, Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  - Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
*  - Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
*  - Neither the name of Novell, Inc. nor the names of its
*    contributors may be used to endorse or promote products derived from this
*    software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL Novell, Inc. OR THE CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* Author: Brad Nicholes (bnicholes novell.com)
******************************************************************************/

/*
 * The ganglia metric "C" interface, required for building DSO modules.
 */
#include <gm_metric.h>

#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <apr_thread_proc.h>
#include <pigpiod_if2.h>

#define GPIO_PIN 17
#define GPIO_RESET_PIN 27
#define INTERVAL_MS 50
#define ROTATIONS_PER_KWH 400.0
#define POWER_TICK 3600*1000/ROTATIONS_PER_KWH

/*
 * Declare ourselves so the configuration routines can find and know us.
 * We'll fill it in at the end of the module.
 */
extern mmodule ferraris_module;

static int pi;
static float instant_power = 0.0;
static float cum_daily_energy_use = 0.0;

static apr_pool_t *pool;
static apr_thread_t *gpio_thread;
static apr_status_t status;

static void * APR_THREAD_FUNC get_IP(apr_thread_t * thread, void *data) {        

    set_mode(pi, GPIO_PIN, PI_INPUT);
    set_mode(pi, GPIO_RESET_PIN, PI_INPUT);

    int old, interval = 1;
    int tick = 0;
    int now = PI_HIGH;

    while (1) {
	if (gpio_read(pi, GPIO_RESET_PIN))
	    tick = 0;
        old = now;
        now = gpio_read(pi, GPIO_PIN);

        if ((old == PI_HIGH) && (now == PI_LOW)) {
	    if (interval > 4) {
                tick++;
                instant_power = (float)POWER_TICK*1000/(interval*INTERVAL_MS);
                cum_daily_energy_use = (float)tick/ROTATIONS_PER_KWH;
            }
            interval = 0;
        } else
            interval ++;

        time_sleep(INTERVAL_MS/1000.0); 
    }

    return NULL;
}

static int ferr_metric_init ( apr_pool_t *p )
{
    pi = pigpio_start(NULL, NULL);

    pool = p;
    apr_thread_create(&gpio_thread, NULL, get_IP, NULL, pool);

    /* Initialize the metadata storage for each of the metrics and then
     *  store one or more key/value pairs.  The define MGROUPS defines
     *  the key for the grouping attribute. */
    MMETRIC_INIT_METADATA(&(ferraris_module.metrics_info[0]),p);
    MMETRIC_ADD_METADATA(&(ferraris_module.metrics_info[0]),MGROUP,"energy");
    MMETRIC_INIT_METADATA(&(ferraris_module.metrics_info[1]),p);
    MMETRIC_ADD_METADATA(&(ferraris_module.metrics_info[1]),MGROUP,"energy");

    return 0;
}

static void ferr_metric_cleanup ( void )
{
    apr_thread_join(&status, gpio_thread);
    pigpio_stop(pi);
}

static g_val_t ferr_metric_handler ( int metric_index )
{
    g_val_t val;

    /* The metric_index corresponds to the order in which
       the metrics appear in the metric_info array
    */
    switch (metric_index) {
    case 0:
        val.f = instant_power;
        break;
    case 1:
        val.f = cum_daily_energy_use;
        break;
    default:
        val.f = 0.0; /* default fallback */
    }

    return val;
}

static Ganglia_25metric ferr_metric_info[] = 
{
    {0, "Instant_Power", 180, GANGLIA_VALUE_FLOAT, "Watt", "both", "%1.f", UDP_HEADER_SIZE+8, "Instantaneous power"},
    {0, "Cumulative_daily_energy_use", 180, GANGLIA_VALUE_FLOAT, "KWh", "both", "%.3f", UDP_HEADER_SIZE+8, "Cumulative energy use per day"},
    {0, NULL}
};

mmodule ferraris_module =
{
    STD_MMODULE_STUFF,
    ferr_metric_init,
    ferr_metric_cleanup,
    ferr_metric_info,
    ferr_metric_handler,
};
