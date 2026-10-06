#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex.h> 

int main(void)
{
    printf("Starting Receiver (TXT Mode)\n");

    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");
    
    if (1) {
        SoapySDRKwargs_set(&args, "uri", "usb:"); 
    } else {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1"); 
    }
    
    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "loopback", "0"); 

    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL)
    {
        printf("Error: Could not open device. %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }
    printf("Device opened successfully.\n");

    const double SAMPLE_RATE = 1e6;   
    const double FREQUENCY = 800e6;   

    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, SAMPLE_RATE) != 0)
    {
        printf("Warning: setSampleRate rx fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, FREQUENCY, NULL) != 0)
    {
        printf("Error: setFrequency rx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }
    printf("RX Configured: Freq=%.3f MHz, SR=%.3f Msps\n", FREQUENCY/1e6, SAMPLE_RATE/1e6);

    size_t channels[] = {0};
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);
    
    SoapySDRDevice_setGainMode(sdr, SOAPY_SDR_RX, 0, false); 
    
    double gain_val = 40.0; 
    if(SoapySDRDevice_setGain(sdr, SOAPY_SDR_RX, 0, gain_val) != 0 ){
        printf("Warning: setGain rx failed: %s\n", SoapySDRDevice_lastError());
    } else {
        printf("RX Gain set to %.1f dB\n", gain_val);
    }

    SoapySDRStream *rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, channels, channel_count, NULL);
    if (rxStream == NULL)
    {
        printf("Error: setupStream rx fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    size_t rx_mtu = SoapySDRDevice_getStreamMTU(sdr, rxStream);
    printf("RX MTU: %lu samples per buffer\n", rx_mtu);

    int16_t *rx_buffer = (int16_t *)malloc(2 * rx_mtu * sizeof(int16_t));
    if (!rx_buffer) {
        printf("Memory allocation failed\n");
        SoapySDRDevice_closeStream(sdr, rxStream);
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0);
    printf("Streaming started. Waiting for signal...\n");

    FILE *fp_out = fopen("rx_data.txt", "w"); 
    if (!fp_out) {
        printf("Error opening file for writing\n");
        free(rx_buffer);
        SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
        SoapySDRDevice_closeStream(sdr, rxStream);
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }
    
    fprintf(fp_out, "# Sample_Index\tI_Value\tQ_Value\n");

    const long timeoutUs = 400000; 
    int flags;
    long long timeNs;
    size_t total_samples = 0;
    const size_t iteration_limit = 500; 
    
    for (size_t i = 0; i < iteration_limit; i++)
    {
        void *buffs[] = {rx_buffer};
        
        int sr = SoapySDRDevice_readStream(sdr, rxStream, buffs, rx_mtu, &flags, &timeNs, timeoutUs);
        
        if (sr >= 0) {
            
            for (int k = 0; k < sr; k++) {
                int16_t I_val = rx_buffer[2 * k];     
                int16_t Q_val = rx_buffer[2 * k + 1]; 
                
                fprintf(fp_out, "%zu\t%d\t%d\n", total_samples + k, I_val, Q_val);
            }
            
            total_samples += sr;
            
            if (i % 50 == 0) {
                printf("Received packet #%zu/%zu | Samples in buf: %d | Total: %zu\n", 
                       i+1, iteration_limit, sr, total_samples);
            }
        } else if (sr == SOAPY_SDR_TIMEOUT) {
             continue;
        } else {
            printf("Read error code: %d (%s)\n", sr, SoapySDR_errToStr(sr));
            break;
        }
    }

    fclose(fp_out);
    printf("\nReception finished. Saved %zu complex samples to 'rx_data.txt'\n", total_samples);

    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0);
    SoapySDRDevice_closeStream(sdr, rxStream);
    SoapySDRDevice_unmake(sdr);
    free(rx_buffer);

    printf("Receiver stopped cleanly.\n");
    return EXIT_SUCCESS;
}