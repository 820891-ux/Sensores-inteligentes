
#include <Clasificacion_IMU2_inferencing.h>
#include <Arduino_LSM9DS1.h>
#include <string.h>

// =============================================
// CONFIGURACION
// =============================================

#define CONVERT_G_TO_MS2 9.80665f
#define UMBRAL_CONFIANZA 0.70f

static bool debug_nn = false;

// Buffer para las muestras de la IMU
static float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// =============================================
// CONTROL LED RGB
// =============================================

// LED RGB activo en nivel bajo
// 255 = encendido, 0 = apagado

void establecerColor(uint8_t rojo,
                    uint8_t verde,
                    uint8_t azul) {

    analogWrite(LEDR, 255 - rojo);
    analogWrite(LEDG, 255 - verde);
    analogWrite(LEDB, 255 - azul);
}

void apagarLED() {
    establecerColor(0, 0, 0);
}

void actualizarLED(const char* clase, float confianza) {

    if (confianza < UMBRAL_CONFIANZA) {
        apagarLED();
        Serial.println("Confianza insuficiente");
        return;
    }

    if (strcmp(clase, "roll_positivo") == 0) {
        establecerColor(255, 0, 0);       // ROJO
    }
    else if (strcmp(clase, "roll_negativo") == 0) {
        establecerColor(255, 40, 120);    // ROSA
    }
    else if (strcmp(clase, "pitch_positivo") == 0) {
        establecerColor(0, 255, 0);       // VERDE
    }
    else if (strcmp(clase, "pitch_negativo") == 0) {
        establecerColor(255, 255, 0);     // AMARILLO
    }
    else if (strcmp(clase, "yaw_positivo") == 0) {
        establecerColor(0, 0, 255);       // AZUL
    }
    else if (strcmp(clase, "yaw_negativo") == 0) {
        establecerColor(140, 0, 255);     // MORADO
    }
    else {
        apagarLED();                       // REPOSO
    }
}

// =============================================
// SETUP
// =============================================

void setup() {

    Serial.begin(115200);

    // Evitar bloqueo permanente si no hay USB
    unsigned long inicio = millis();

    while (!Serial && millis() - inicio < 2000) {
    }

    // LED RGB
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);

    apagarLED();

    Serial.println("Edge Impulse - Clasificacion IMU2");

    // Inicializar IMU
    if (!IMU.begin()) {
        Serial.println("ERROR: IMU no inicializada");
        while (1) {
            apagarLED();
            delay(100);
        }
    }

    // Comprobar canales del modelo
    if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 6) {
        Serial.println("ERROR: modelo no tiene 6 canales");
        while (1);
    }

    Serial.println("IMU inicializada");

    Serial.print("Frecuencia: ");
    Serial.print(1000.0f / EI_CLASSIFIER_INTERVAL_MS);
    Serial.println(" Hz");

    Serial.print("Muestras por ventana: ");
    Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / 6);

    Serial.println("Sistema preparado");
}

// =============================================
// LOOP PRINCIPAL
// =============================================

void loop() {

    // =========================================
    // 1. CAPTURA DE LOS 6 CANALES
    // =========================================

    const unsigned long intervalo_us =
        (unsigned long)(EI_CLASSIFIER_INTERVAL_MS * 1000.0f);

    unsigned long siguiente_muestra = micros();

    for (size_t ix = 0;
         ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
         ix += 6) {

        // Esperar al siguiente instante de muestreo
        while ((int32_t)(micros() - siguiente_muestra) < 0) {
        }

        siguiente_muestra += intervalo_us;

        float ax, ay, az;
        float gx, gy, gz;

        // Esperar lecturas disponibles
        while (!IMU.accelerationAvailable() ||
               !IMU.gyroscopeAvailable()) {
        }

        // Leer acelerometro y giroscopio
        IMU.readAcceleration(ax, ay, az);
        IMU.readGyroscope(gx, gy, gz);

        // Aceleracion en m/s2
        buffer[ix + 0] = ax * CONVERT_G_TO_MS2;
        buffer[ix + 1] = ay * CONVERT_G_TO_MS2;
        buffer[ix + 2] = az * CONVERT_G_TO_MS2;

        // Giroscopio en grados/segundo
        buffer[ix + 3] = gx;
        buffer[ix + 4] = gy;
        buffer[ix + 5] = gz;
    }

    // =========================================
    // 2. CONVERTIR BUFFER EN SIGNAL
    // =========================================

    signal_t signal;

    int err = numpy::signal_from_buffer(
        buffer,
        EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
        &signal
    );

    if (err != 0) {
        Serial.print("ERROR creando señal: ");
        Serial.println(err);
        apagarLED();
        return;
    }

    // =========================================
    // 3. EJECUTAR CLASIFICADOR
    // =========================================

    ei_impulse_result_t result = { 0 };

    err = run_classifier(&signal, &result, debug_nn);

    if (err != EI_IMPULSE_OK) {
        Serial.print("ERROR clasificador: ");
        Serial.println(err);
        apagarLED();
        return;
    }

    // =========================================
    // 4. MOSTRAR PREDICCIONES
    // =========================================

    Serial.println("\n--- PREDICCIONES ---");

    float mayor_probabilidad = -1.0f;
    size_t mejor_clase = 0;

    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {

        Serial.print(result.classification[i].label);
        Serial.print(": ");
        Serial.println(result.classification[i].value, 4);

        if (result.classification[i].value >
            mayor_probabilidad) {

            mayor_probabilidad =
                result.classification[i].value;

            mejor_clase = i;
        }
    }

    // =========================================
    // 5. IDENTIFICAR MOVIMIENTO
    // =========================================

    const char* movimiento =
        result.classification[mejor_clase].label;

    Serial.print("\nMovimiento detectado: ");
    Serial.println(movimiento);

    Serial.print("Confianza: ");
    Serial.println(mayor_probabilidad, 4);

    Serial.print("Tiempo de inferencia: ");
    Serial.print(
        result.timing.dsp +
        result.timing.classification
    );
    Serial.println(" ms");

    // =========================================
    // 6. ACTUALIZAR LED RGB
    // =========================================

    actualizarLED(movimiento, mayor_probabilidad);
    delay(1000);
}
