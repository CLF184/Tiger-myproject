#include <stdio.h>
#include "light_sensor.h"
#include "um_adc.h"

/**
 * ADC range constants
 * Light sensor characteristics: stronger light, lower resistance, lower ADC value
 * Weaker light, higher resistance, higher ADC value
 */
#define ADC_MAX_VALUE 4095  // 12-bit ADC maximum value
#define ADC_MIN_VALUE 0     // ADC minimum value

int light_sensor_init(void)
{
    // printf("Light sensor initialization completed\n");
    return 0;
}

int light_sensor_read(int *value)
{
    if (value == NULL) {
        fprintf(stderr, "Error: Invalid parameter pointer\n");
        return -1;
    }

    // Read light sensor value from ADC_2 channel
    int ret = get_adc_data(ADC_2, value);
    
    if (ret != ADC_OK) {
        fprintf(stderr, "Error: Unable to read ADC data, error code: %d\n", ret);
        return -1;
    }

    return 0;
}

float light_sensor_to_percentage(int adc_value)
{
    // Ensure ADC value is within valid range
    if (adc_value < ADC_MIN_VALUE) {
        adc_value = ADC_MIN_VALUE;
    } else if (adc_value > ADC_MAX_VALUE) {
        adc_value = ADC_MAX_VALUE;
    }
    
    // Convert ADC value to percentage, light intensity is inversely proportional to ADC value
    float percentage = (1.0f - ((float)adc_value / ADC_MAX_VALUE)) * 100.0f;
    
    return percentage;
}