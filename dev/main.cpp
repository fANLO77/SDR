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
        SoapySDRKwargs_set(&args, "uri", "ip:192.168.2.1");
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
        using Complex = std::complex<double>;

        int16_t tx_buff[2 * mtu];

        const int K = 128;
        const int16_t AMPLITUDE = 2000;
        const int SPS = 10;
        const int N_Upsampled = K * SPS;
        const int Len_Conv = N_Upsampled + SPS - 1;

        int bits[K] = {
            1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1
        };

        Complex symbols[K];

        for (int i = 0; i < K; i++)
        {
            symbols[i] = (bits[i] == 0)? Complex(1.0, 0.0) : Complex(-1.0, 0.0);
        }

        Complex upsampled[N_Upsampled] = {};

        for (int i = 0; i < K; i++)
        {
            upsampled[i * SPS] = symbols[i];
        }

        double h[SPS];

        for (int i = 0; i < SPS; i++)
        {
            h[i] = 1.0;
        }

        Complex conv_result[Len_Conv] = {};

        for (int n = 0; n < Len_Conv; n++)
        {
            for (int k = 0; k < N_Upsampled; k++)
            {
                int h_index = n - k;

                if (h_index >= 0 && h_index < SPS)
                {
                    conv_result[n] += upsampled[k] * h[h_index];
                }
            }
        }

        FILE *fp_tx = fopen("tx_data.txt", "w");

        if (!fp_tx)
        {
            perror("Error opening tx_data.txt");
            return EXIT_FAILURE;
        }

        fprintf(fp_tx, "# Sample_Index\tI_Value\tQ_Value\n");

        printf("Start transmitting...\n");

        void *tx_buffs[] = { tx_buff };

        size_t sample_index = 0;

        for (size_t buffers_sent = 0; buffers_sent < 500; buffers_sent++)
        {
            for (size_t n = 0; n < static_cast<size_t>(mtu); n++)
            {
                int idx = sample_index % Len_Conv;

                double I_real = conv_result[idx].real();
                double Q_real = conv_result[idx].imag();

                tx_buff[2 * n]     = static_cast<int16_t>(I_real * AMPLITUDE);
                tx_buff[2 * n + 1] = static_cast<int16_t>(Q_real * AMPLITUDE);

                sample_index++;
            }

            int flags = 0;

            int st = SoapySDRDevice_writeStream(sdr, stream, (const void * const *)tx_buffs, mtu, &flags, 0, timeoutUs
            );

            if (st < 0)
            {
                printf("TX Error, status: %i (%s)\n", st, SoapySDRDevice_lastError());
                break;
            }

            if (st != mtu)
            {
                printf("TX Warning: sent %i of %lu samples\n", st, (unsigned long)mtu);
            }
            for (int n = 0; n < st; n++)
            {
                size_t idx = sample_index - mtu + n;
                int conv_idx = idx % Len_Conv;
                fprintf(fp_tx, "%zu\t%.0f\t%.0f\n", idx, conv_result[conv_idx].real(), conv_result[conv_idx].imag());
            }
            if (buffers_sent % 50 == 0)
            {
                printf("Transmitted buffer %zu\n", buffers_sent);
            }
        }

        fclose(fp_tx);
        printf("TX data saved to tx_data.txt\n");
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