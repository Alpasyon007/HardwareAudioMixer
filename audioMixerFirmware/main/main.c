#include "esp_adc/adc_oneshot.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/uart.h>
#include <math.h>
#include <stdio.h>

// Define the number of potentiometers
#define NUM_POTS 5

// Define ADC channels for each potentiometer
const adc_channel_t pot_adc_channels[NUM_POTS] = {
	ADC_CHANNEL_0, // GPIO36
	ADC_CHANNEL_3, // GPIO39
	ADC_CHANNEL_6, // GPIO34
	ADC_CHANNEL_7, // GPIO35
	ADC_CHANNEL_4, // GPIO32
};

#define ADC_RESOLUTION 4096.0 // 12-bit resolution for ADC

#define UART_PORT	   UART_NUM_0 // UART0 is typically connected to the USB-to-serial
#define BAUD_RATE	   115200	  // Set the baud rate

#define USE_UART	   0 // Set to 1 to send data over UART, 0 to print to console

void app_main(void) {
#if USE_UART
	const uart_config_t uart_config = {
		.baud_rate = BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity	   = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};
	uart_driver_install(UART_PORT, 256, 0, 0, NULL, 0); // Install UART driver
	uart_param_config(UART_PORT, &uart_config);			// Configure UART
#endif

	// ADC one-shot configuration
	adc_oneshot_unit_handle_t		  adc2_handle;
	const adc_oneshot_unit_init_cfg_t init_config = {
		.unit_id = ADC_UNIT_1,				 // Use ADC2
		.clk_src = ADC_DIGI_CLK_SRC_DEFAULT, // Default clock source
	};
	adc_oneshot_new_unit(&init_config, &adc2_handle);

	// Configure all potentiometer ADC channels
	const adc_oneshot_chan_cfg_t channel_config = {
		.atten	  = ADC_ATTEN_DB_12,	 // 12 dB attenuation to read voltages up to ~3.9V
		.bitwidth = ADC_BITWIDTH_DEFAULT // 12-bit width (0 to 4095)
	};

	for(int i = 0; i < NUM_POTS; ++i) {
		adc_oneshot_config_channel(adc2_handle, pot_adc_channels[i], &channel_config);
	}

	while(1) {
		int adc_raw[NUM_POTS] = {0};

		// Read ADC values for each potentiometer
		for(int i = 0; i < NUM_POTS; ++i) {
			adc_oneshot_read(adc2_handle, pot_adc_channels[i], &adc_raw[i]);
		}

		// Convert the ADC values to percentages (8-bit values ranging from 0 to 100)
		uint8_t percentages[NUM_POTS] = {0};
		for(int i = 0; i < NUM_POTS; ++i) {
			percentages[i] = (uint8_t)ceil((adc_raw[i] / ADC_RESOLUTION) * 100);
		}

#if(!USE_UART)
		printf("\nPotentiometer values: ");
		for(int i = 0; i < NUM_POTS; ++i) {
			printf("%d ", percentages[i]);
		}
#endif

#if USE_UART
		// Send binary data over UART (which will appear as serial data over USB)
		uart_write_bytes(UART_PORT, (const char*)percentages, NUM_POTS);
#endif
		// Wait for 500 ms before reading again
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}

	// Clean up ADC after usage
	adc_oneshot_del_unit(adc2_handle);
}
