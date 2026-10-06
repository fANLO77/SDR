import numpy as np
import matplotlib.pyplot as plt
import os

# Настройки
FILENAME = "rx_data.txt"
SAMPLE_RATE = 1e6 

def load_iq_from_txt(filename):
    if not os.path.exists(filename):
        print(f"Ошибка: Файл '{filename}' не найден!")
        return None, None

    i_vals = []
    q_vals = []
    
    try:
        with open(filename, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = line.split('\t') 
                if len(parts) < 3:
                    parts = line.split()
                
                if len(parts) >= 3:
                    try:
                        
                        val_i = int(parts[1])
                        val_q = int(parts[2])
                        i_vals.append(val_i)
                        q_vals.append(val_q)
                    except ValueError:
                        continue
    except Exception as e:
        print(f"Ошибка при чтении файла: {e}")
        return None, None
    if not i_vals:
        print("Файл пуст или не удалось распарсить данные.")
        return None, None
    return np.array(i_vals), np.array(q_vals)
def main():
    print(f"Загрузка данных из {FILENAME}...")
    I, Q = load_iq_from_txt(FILENAME)   
    if I is None or Q is None:
        return
    num_samples = len(I)
    duration_sec = num_samples / SAMPLE_RATE  
    print(f"Загружено сэмплов: {num_samples}")
    print(f"Длительность записи: {duration_sec:.4f} сек")
    print(f"Мин/Макс I: {np.min(I)} / {np.max(I)}")
    print(f"Мин/Макс Q: {np.min(Q)} / {np.max(Q)}")
    signal_complex = I + 1j * Q
    amplitude = np.abs(signal_complex)

    plt.figure(figsize=(12, 6))
    t = np.arange(num_samples) / SAMPLE_RATE
    
    MAX_POINTS_TO_PLOT = 50000 
    if num_samples > MAX_POINTS_TO_PLOT:
        print(f"Предупреждение: Точек много ({num_samples}). Рисуем только первые {MAX_POINTS_TO_PLOT}.")
        t_plot = t[:MAX_POINTS_TO_PLOT]
        amp_plot = amplitude[:MAX_POINTS_TO_PLOT]
        title_suffix = f"(первые {MAX_POINTS_TO_PLOT})"
    else:
        t_plot = t
        amp_plot = amplitude
        title_suffix = ""

    plt.plot(t_plot, amp_plot, color='blue', linewidth=0.8)
    plt.title(f'Принятый сигнал: Амплитуда во времени {title_suffix}')
    plt.xlabel('Время [сек]')
    plt.ylabel('Амплитуда (ADU)')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    
    plt.figure(figsize=(12, 6))
    
    window = np.hamming(num_samples)
    fft_result = np.fft.fftshift(np.fft.fft(signal_complex * window))
    freqs = np.fft.fftshift(np.fft.fftfreq(num_samples, d=1/SAMPLE_RATE))

    magnitude_db = 20 * np.log10(np.abs(fft_result) + 1e-9)
    
    plt.plot(freqs / 1e6, magnitude_db, color='red', linewidth=0.8)
    plt.title('Спектр принятого сигнала (FFT)')
    plt.xlabel('Частота [МГц] (относительно несущей)')
    plt.ylabel('Мощность [дБ]')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xlim(-0.5, 0.5) 
    plt.show()

if __name__ == "__main__":
    main()