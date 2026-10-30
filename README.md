<p align="center">
  <img src="Reports/images/plutosdr-banner.svg" alt="PlutoSDR — лабораторный проект по SDR" width="100%" />
</p>

<p align="center">
  <a href="#быстрый-старт"><img src="https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white" alt="CMake" /></a>
  <a href="#что-используется"><img src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white" alt="C++" /></a>
  <a href="#что-используется"><img src="https://img.shields.io/badge/SDR-ADALM--PLUTO-7B3FE4" alt="ADALM-PLUTO" /></a>
  <img src="https://img.shields.io/badge/status-lab%20work-success" alt="Статус: лабораторная работа" />
</p>

> Учебный проект по программно-определяемому радио: от первых приёмников в **GNU Radio** до управления ADALM-PLUTO через **SoapySDR**, формирования и передачи `I/Q`-отсчётов и сохранения принятого сигнала.

## Проект

| Часть | Что делает |
|:--- |:--- |
| [`main.cpp`](dev/main.cpp) | Единая программа: передаёт I/Q-сигнал или принимает поток, в зависимости от флага. |
| [`plot_rx.py`](dev/plot_rx.py) | Строит графики I и Q из записанных файлов. |
| [`Отчет по 1-2 неделе`](Reports/report_1_2.md) | Отчёт по сборке FM-радиоприёмника на базе ADALM-Pluto. |
| [`Отчет по 3 неделе`](Reports/report_3.md) | Отчет по настройке ADALM-Pluto и программной генерации сигналов. |
| [`Отчет по 4-6 неделе`](Reports/report_4_5_6.md) | Отчет по настройке ADALM-Pluto и программной генерации сигналов. |
| [`Отчет по 7-8 неделе`](Reports/report_7_8.md) | Отчет по формированию и передачи BPSK-сигнала на базе ADALM-Pluto. |

## Требования

Для сборки проекта необходимы:

* ОС Linux (например, Ubuntu).
* Git.
* CMake версии 3.16 или выше.
* Компилятор C++ (G++).
* Библиотека SoapySDR.
* Устройство PlutoSDR для работы с радиосигналом.

## Клонирование проекта

Склонируйте репозиторий вместе со всеми подключёнными библиотеками:

```bash
git clone --recurse-submodules https://github.com/fANLO77/SDR.git
```
## Сборка проекта

Соберите проект:

```bash
cd dev
mkdir build && cd build
cmake ..
make -j16 # nproc - количество потоков, например make -j16
```

После успешной сборки запустите исполняемый файл:

```bash
./main tx   # передатчик на 840 МГц
./main rx   # приёмник на 800 МГц
```

## Повторная сборка

После изменения исходного кода повторная конфигурация обычно не требуется. Для пересборки выполните из каталога `/dev`:

```bash
cmake ...
```

При необходимости полностью пересоздать каталог сборки:

```bash
rm -rf build
mkdir build && cd build
cmake ..
make -j16 # nproc - количество потоков, например make -j16
```