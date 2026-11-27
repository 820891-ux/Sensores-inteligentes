# Integración del firmware

Este archivo es un complemento propuesto, no el sketch original de la práctica. No incluye el modelo entrenado ni la biblioteca generada por Edge Impulse, que deben exportarse desde el proyecto original. No se ha compilado ni ensayado con la placa.

Copia `led_detection.h` junto al sketch de inferencia continua de la biblioteca exportada. Añade:

```cpp
#include "led_detection.h"
```

En `setup()` añade `keyword_led::begin();`. Tras una inferencia correcta, donde `result` sea el resultado de Edge Impulse, añade:

```cpp
for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    keyword_led::detection(result.classification[i].label,
                           result.classification[i].value);
}
keyword_led::update();
```

Llama también a `keyword_led::update()` en cada iteración del `loop()`. La duración del LED es nominal y depende de la frecuencia de actualización; las detecciones consecutivas prolongan el encendido. No se utiliza `delay()`, para evitar bloquear adicionalmente el procesamiento del audio.
