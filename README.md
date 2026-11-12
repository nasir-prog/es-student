# es-student

## Практика 2.2

[Проект 221-command-time](221-command-time/README.md). Логи в этом проекте — явно обозначенная программная модель без физической платы.

[![2.2.1](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-1.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-1.yml)
[![2.2.2](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-2.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-2.yml)
[![2.2.3](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-3.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-3.yml)
[![2.2.4](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-4.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-4.yml)
[![2.2.5](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-5.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-2-5.yml)

## Практики 2.3, 3.1 и 3.2

| Проект | Задания | Реализация |
| --- | --- | --- |
| [231-firmware-template](231-firmware-template/) | 2.3.1–2.3.8 | Текстовый протокол, API, задачи светодиода и кнопки, расчёт π, профилирование |
| [239-two-buttons](239-two-buttons/) | 2.3.9 и 2.3.9д | Две кнопки на GP15 и GP14, смена периода мигания, короткое и долгое нажатия |
| [311-adc](311-adc/) | 3.1.1–3.1.5 | Напряжение на GP26, температура, телеметрия, обработка данных и RC-цепь |
| [321-audio-recorder](321-audio-recorder/) | 3.2.1–3.2.7 и 3.2.7д | Таймер, DMA, 8- и 16-битный поток, ШИМ на GP20, сторожевой таймер и запись WAV |

[![2.3.1](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-1.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-1.yml)
[![2.3.2](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-2.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-2.yml)
[![2.3.3](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-3.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-3.yml)
[![2.3.4](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-4.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-4.yml)
[![2.3.5](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-5.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-5.yml)
[![2.3.6](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-6.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-6.yml)
[![2.3.7](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-7.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-7.yml)
[![2.3.9](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-9.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-9.yml)
[![2.3.9д](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-9d.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-2-3-9d.yml)
[![Учебный прогон 2.3–3.2](https://github.com/nasir-prog/es-student/actions/workflows/check-model.yml/badge.svg)](https://github.com/nasir-prog/es-student/actions/workflows/check-model.yml)

### Учебный прогон без платы

GitHub Actions собирает все четыре прошивки для RP2040 и выполняет статические проверки курса и учебный прогон. Прогон компилирует и запускает C-модули на компьютере с модельными входами, проверяет протокол, задачи и двоичные кадры, строит графики и создаёт WAV из синтетических сигналов. Артефакт `es-model-results` доступен на странице успешного запуска.

Логи, CSV, графики и WAV помечены как учебные. Значения напряжения, времени, температуры и звуковые сигналы получены из программной модели. Для 2.3.8 и заданий 3.1–3.2 используется проверка в этом репозитории. Статические проверки 2.3 отправляют свои результаты в кабинет курса через существующий секрет.

Запуск на Linux с GCC и Python:

```sh
python3 -m pip install matplotlib pyserial
bash offline/run.sh
```

Результаты сохраняются в `offline/model-output/`. Для сборки прошивки с установленным Pico SDK 2.3.0:

```sh
cmake -S 231-firmware-template -B 231-firmware-template/build
cmake --build 231-firmware-template/build
```

Скрипты сбора данных и статические проверки взяты из [книги курса ant-lab](https://github.com/ant-lab-ru/es-book) по [лицензии MIT](LICENSE-ant-lab).
