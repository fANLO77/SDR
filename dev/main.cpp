#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <complex.h>

#define TX_FREQUENCY 840e6
#define RX_FREQUENCY 800e6
#define SAMPLE_RATE  1e6

int main(int argc, char *argv[])
{
    // Проверка аргумента командной строки
    if (argc < 2)
    {
        printf("Usage: %s rx\n", argv[0]);
        printf("  tx - передатчик (%.0f MHz)\n", TX_FREQUENCY / 1e6);
        printf("  rx - приёмник   (%.0f MHz)\n", RX_FREQUENCY / 1e6);
        return EXIT_FAILURE;
    }

    int is_tx;
    if (strcmp(argv[1], "tx") == 0)
    {
        is_tx = 1;
        printf("Mode: TRANSMITTER, freq = %.0f MHz\n", TX_FREQUENCY / 1e6);
    }
    else if (strcmp(argv[1], "rx") == 0)
    {
        is_tx = 0;
        printf("Mode: RECEIVER, freq = %.0f MHz\n", RX_FREQUENCY / 1e6);
    }
    else
    {
        printf("Unknown mode '%s'. Use 'tx' or 'rx'.\n", argv[1]);
        return EXIT_FAILURE;
    }

    SoapySDRKwargs args = {};
    SoapySDRKwargs_set(&args, "driver", "plutosdr");

    if (1) {
        SoapySDRKwargs_set(&args, "uri", "usb:");
    } else {
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.3.1");
    }

    SoapySDRKwargs_set(&args, "direct", "1");
    SoapySDRKwargs_set(&args, "loopback", "0");

    SoapySDRDevice *sdr = SoapySDRDevice_make(&args);
    SoapySDRKwargs_clear(&args);

    if (sdr == NULL)
    {
        printf("SoapySDRDevice_make fail: %s\n", SoapySDRDevice_lastError());
        return EXIT_FAILURE;
    }

    // Направление: TX или RX
    int direction = is_tx ? SOAPY_SDR_TX : SOAPY_SDR_RX;
    double freq   = is_tx ? TX_FREQUENCY : RX_FREQUENCY;

    // Настройка частоты и sample rate
    if (SoapySDRDevice_setSampleRate(sdr, direction, 0, SAMPLE_RATE) != 0)
    {
        printf("setSampleRate fail: %s\n", SoapySDRDevice_lastError());
    }
    if (SoapySDRDevice_setFrequency(sdr, direction, 0, freq, NULL) != 0)
    {
        printf("setFrequency fail: %s\n", SoapySDRDevice_lastError());
    }
    printf("Frequency: %lf Hz\n", SoapySDRDevice_getFrequency(sdr, direction, 0));

    // Усиление: для TX тише, для RX громче
    double gain_val = is_tx ? -10.0 : 40.0;
    if (SoapySDRDevice_setGain(sdr, direction, 0, gain_val) != 0)
    {
        printf("setGain fail: %s\n", SoapySDRDevice_lastError());
    }

    // Создание потока
    size_t channels[] = {0};
    size_t channel_count = sizeof(channels) / sizeof(channels[0]);

    SoapySDRStream *stream = SoapySDRDevice_setupStream(sdr, direction, SOAPY_SDR_CS16,channels, channel_count, NULL);
    if (stream == NULL)
    {
        printf("setupStream fail: %s\n", SoapySDRDevice_lastError());
        SoapySDRDevice_unmake(sdr);
        return EXIT_FAILURE;
    }

    size_t mtu = SoapySDRDevice_getStreamMTU(sdr, stream);
    printf("MTU buffer size: %lu samples\n", mtu);

    SoapySDRDevice_activateStream(sdr, stream, 0, 0, 0);

    const long timeoutUs = 400000;

    if (is_tx)
    {
        
        int16_t tx_buff[2 * mtu];

        const int steps = 8;
        const int step_len = mtu / steps;
        const int amp_min = 500;
        const int amp_max = 8000;

        for (int i = 0; i < 2 * (int)mtu; i += 2)
        {
            int step = (i / 2) / step_len;
            int amp = amp_min + (amp_max - amp_min) * step / (steps - 1);
            tx_buff[i] = amp; 
            tx_buff[i + 1] = amp; 
        }

        tx_buff[0] = 0x7fff;
        tx_buff[1] = 0x7fff;

        printf("Start transmitting...\n");
        void *tx_buffs[] = {tx_buff};

        for (size_t buffers_sent = 0; buffers_sent < 500; buffers_sent++)
        {
            int flags = 0;
            int st = SoapySDRDevice_writeStream(sdr, stream, (const void * const*)tx_buffs,mtu, &flags, 0, timeoutUs);
            if (st < 0)
            {
                printf("TX Error, status: %i (%s)\n", st, SoapySDRDevice_lastError());
            }
            else if ((size_t)st != mtu)
            {
                printf("TX Warning: sent %i of %lu samples\n", st, mtu);
            }

            if (buffers_sent % 50 == 0)
            {
                printf("Transmitted buffer %zu\n", buffers_sent);
            }
        }

        printf("Stopping transmission...\n");
    }
    else
    {
        int16_t *rx_buffer = (int16_t *)malloc(2 * mtu * sizeof(int16_t));
        if (!rx_buffer)
        {
            printf("Memory allocation failed\n");
            SoapySDRDevice_deactivateStream(sdr, stream, 0, 0);
            SoapySDRDevice_closeStream(sdr, stream);
            SoapySDRDevice_unmake(sdr);
            return EXIT_FAILURE;
        }

        FILE *fp_out = fopen("rx_data.txt", "w");
        if (!fp_out)
        {
            printf("Error opening file for writing\n");
            free(rx_buffer);
            SoapySDRDevice_deactivateStream(sdr, stream, 0, 0);
            SoapySDRDevice_closeStream(sdr, stream);
            SoapySDRDevice_unmake(sdr);
            return EXIT_FAILURE;
        }
        fprintf(fp_out, "# Sample_Index\tI_Value\tQ_Value\n");

        printf("Streaming started. Waiting for signal...\n");

        int flags;
        long long timeNs;
        size_t total_samples = 0;
        const size_t iteration_limit = 500;

        for (size_t i = 0; i < iteration_limit; i++)
        {
            void *buffs[] = {rx_buffer};

            int sr = SoapySDRDevice_readStream(sdr, stream, buffs, mtu, &flags, &timeNs, timeoutUs);

            if (sr >= 0)
            {
                for (int k = 0; k < sr; k++)
                {
                    fprintf(fp_out, "%zu\t%d\t%d\n", total_samples + k, rx_buffer[2 * k], rx_buffer[2 * k + 1]);
                }
                total_samples += sr;

                if (i % 50 == 0)
                {
                    printf("Packet #%zu/%zu | Samples: %d | Total: %zu\n", i + 1, iteration_limit, sr, total_samples);
                }
            }
            else if (sr == SOAPY_SDR_TIMEOUT)
            {
                continue;
            }
            else
            {
                printf("Read error code: %d (%s)\n", sr, SoapySDR_errToStr(sr));
                break;
            }
        }

        fclose(fp_out);
        printf("Reception finished. Saved %zu complex samples to 'rx_data.txt'\n", total_samples);
        free(rx_buffer);
    }

    SoapySDRDevice_deactivateStream(sdr, stream, 0, 0);
    SoapySDRDevice_closeStream(sdr, stream);
    SoapySDRDevice_unmake(sdr);

    printf("Done!\n");
    return EXIT_SUCCESS;
}