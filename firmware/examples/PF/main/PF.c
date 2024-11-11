/*! @mainpage Campímetro
 *
 * @section genDesc Descripción General
 *
 * Este proyecto implementa un campímetro, un dispositivo que utiliza una tira de LEDs y un motor paso a paso para iluminar y cubrir un rango completo de 360 grados. 
 * Los LEDs se encienden y apagan de forma ordenada para representar ángulos específicos, mientras que el motor paso a paso se encarga de rotar el dispositivo 
 * y cubrir toda la circunferencia.
 *
 * <a href="https://drive.google.com/...">Ejemplo de Operación</a>
 *
 * @section hardConn Conexión de Hardware
 *
 * | Periférico  | ESP32       |
 * |:-----------:|:------------|
 * | Pasos Motor | GPIO_2      |
 * | DIR Motor   | GPIO_3      |
 * | LED_PIN     | GPIO_8      |
 * 
 * @section changelog Historial de Cambios
 *
 * | Fecha       | Descripción                                    |
 * |:-----------:|:----------------------------------------------|
 * | 11/10/2024  | Creación del documento                        |
 * | 07/11/2024  | Finalizacion de la codificaion                |
 * | 08/11/2024  | generacion de la documentacion doxygen        |
 *
 * @note Autores: Sparvoli Tomás Mateo (tomas.sparvoli@ingenieria.uner.edu.ar), Perea Camila (camila.perea@ingenieria.uner.edu.ar)
 */

//==================[inclusiones]=============================================//

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "switch.h"
#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "timer_mcu.h"
#include "gpio_mcu.h"
#include "neopixel_stripe.h"
#include "ble_mcu.h"
#include "math.h"

#define BUILT_IN_RGB_LED_PIN          GPIO_8        /*!< NeoPixel en ESP32-C6-DevKitC-1 está conectado en GPIO_8 */
#define BUILT_IN_RGB_LED_LENGTH       1             /*!< NeoPixel en ESP32-C6-DevKitC-1 tiene un píxel */
#define LED_COUNT                     9             /*!< Número de LEDs en la tira */
#define START_ANGLE                   110           /*!< Ángulo inicial en grados */

//==================[definición de datos internos]=============================//

TaskHandle_t cambiomotor = NULL; /*!< Manejador de la tarea de control del motor */
TaskHandle_t presionoTecla = NULL; /*!< Manejador de la tarea de gestión de teclas */
TaskHandle_t enviar = NULL; /*!< Manejador de la tarea de envío de datos */

bool giro = false; /*!< Bandera de estado de rotación */
bool led = false; /*!< Bandera de estado del LED */
bool mapa = false; /*!< Bandera de estado del mapa */
long int angulo = 0; /*!< Ángulo actual */
long int angulo_led = 0; /*!< Ángulo del LED */

uint8_t current_led = 10; /*!< Índice del LED actual */
uint8_t current_angle = START_ANGLE; /*!< Ángulo actual en grados */
uint8_t teclas; /*!< Estado de las teclas */
uint8_t angle_selec; /*!< Ángulo seleccionado */
neopixel_color_t color; /*!< Color del LED NeoPixel */

//==================[declaración de funciones internas]=========================//

/**
 * @fn void FuncTimerA(void* param)
 * @brief Función de ISR del temporizador A.
 *
 * Envía una notificación a la tarea que gestiona la pulsación de teclas.
 *
 * @param param Parámetro opcional (no utilizado).
 */
void FuncTimerA(void* param) {
    vTaskNotifyGiveFromISR(presionoTecla, pdFALSE); 
}

/**
 * @fn void FuncTimerB(void* param)
 * @brief Función de ISR del temporizador B.
 *
 * Envía una notificación a la tarea encargada de mostrar la señal ECG.
 *
 * @param param Parámetro opcional (no utilizado).
 */
void FuncTimerB(void* param) {
    vTaskNotifyGiveFromISR(presionoTecla, pdFALSE); 
}

/**
 * @fn void enviar_datos()
 * @brief Envía datos seleccionados a través de BLE en un formato de cadena.
 *
 * Calcula y formatea las coordenadas basadas en el ángulo seleccionado y 
 * las envía a través de BLE como una cadena.
 */
void enviar_datos() {
    char msg[30];
    long int xcentrado = 110;
    long int ycentrado = 110;

    long int distancia_rectangularx = angle_selec * cos(angulo);
    long int distancia_rectangulary = angle_selec * sin(angulo);
    sprintf(msg, "*GX%ldY%ld*", xcentrado + distancia_rectangularx, ycentrado + distancia_rectangulary);
    BleSendString(msg);
    printf("Datos enviados: %s\n", msg);
    printf("Ángulo enviado: %ld\n", angulo);
    printf("LED seleccionado: %d\n", angle_selec);
}

/**
 * @fn void LedControlTask()
 * @brief Controla el comportamiento del LED NeoPixel según el ángulo actual.
 *
 * Enciende el LED NeoPixel correspondiente al ángulo actual, espera un 
 * periodo de tiempo, lo apaga y luego se mueve al siguiente ángulo.
 */
void LedControlTask() {
    NeoPixelSetPixel(current_led, color);
    printf("Ángulo: %d grados\n", current_angle);
    angle_selec = current_angle;

    vTaskDelay(3000 / portTICK_PERIOD_MS);
    NeoPixelSetPixel(current_led, 0x000000);
    current_led--;
    current_angle -= 10;     
}

/**
 * @fn void cambioEstado(void *param)
 * @brief Monitorea la entrada UART y actualiza las variables de estado.
 *
 * Dependiendo del carácter recibido, actualiza las variables de rotación, mapa, 
 * y color.
 *
 * @param param Parámetro opcional (no utilizado).
 */
void cambioEstado(void *param) {
    while(true) {    
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint8_t caracter;
        UartReadByte(UART_PC, &caracter);

        if (caracter == 'a') {
            giro = true;
            vTaskNotifyGiveFromISR(cambiomotor, pdFALSE);
        } else if (caracter == 'm') {    
            mapa = !mapa;
            enviar_datos();
            current_led = 10;
            current_angle = 110;
        } else if (caracter == 'b') {
            color = NEOPIXEL_COLOR_WHITE;
            printf("Color seleccionado: blanco\n");
        } else if (caracter == 'r') {
            color = NEOPIXEL_COLOR_RED;
            printf("Color seleccionado: rojo\n");
        } else if (caracter == 'l') {
            LedControlTask(); 
        }
    }
}

/**
 * @fn void motorpaso()
 * @brief Función de control del motor paso a paso.
 *
 * Rota el motor en una dirección o en la otra dependiendo de los límites 
 * del ángulo.
 */
void motorpaso() {
    GPIOInit(GPIO_2, GPIO_OUTPUT);
    GPIOInit(GPIO_3, GPIO_OUTPUT);

    while(1) {        
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (giro) {
            if (angulo > 330) {    
                GPIOOff(GPIO_3);    
                for (int i = 0; i < 800; i++) {
                    GPIOOn(GPIO_2);    
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    GPIOOff(GPIO_2);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }    
                angulo = 0;
            } else {    
                GPIOOn(GPIO_3);
                for (int i = 0; i < 100; i++) {
                    GPIOOn(GPIO_2);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    GPIOOff(GPIO_2);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }    
                angulo += 45;
            }
            giro = false;
        }
    }
}

/**
 * @fn void app_main(void)
 * @brief Función principal que inicializa hardware y tareas.
 *
 * Inicializa BLE, UART, temporizadores y NeoPixel, y crea tareas para el 
 * control del motor, control del LED, y gestión de entrada UART.
 */
void app_main(void) {
    static neopixel_color_t color[11];
    NeoPixelInit(GPIO_19, 11, &color);
    NeoPixelAllOff();

    timer_config_t timer_leds = {
        .timer = TIMER_B,
        .period = 100000,
        .func_p = FuncTimerA,
        .param_p = NULL
    };
    TimerInit(&timer_leds);

    timer_config_t timer_estado = {
        .timer = TIMER_B,
        .period = 100000,
        .func_p = FuncTimerB,
        .param_p = NULL
    };
    TimerInit(&timer_estado);

    serial_config_t pantalla = {
        .port = UART_PC,
        .baud_rate = 9600,
        .func_p = cambioEstado,
        .param_p = NULL,
    };
    UartInit(&pantalla);

    ble_config_t ble_configuration = {
        "Campimetro",
        NULL
    };
	BleInit(&ble_configuration);


xTaskCreate(&cambioEstado,"cambia",4096,NULL,5,&presionoTecla);
xTaskCreate(&motorpaso, "cambio del motor", 4096, NULL, 5, &cambiomotor);

TimerStart(timer_estado.timer);
TimerStart(timer_leds.timer);
LedsInit();
   while (1)
   { 
	vTaskDelay(1000 / portTICK_PERIOD_MS);   
	switch(BleStatus()){
	    case BLE_OFF:
                LedOff(LED_1);
        break;
        case BLE_DISCONNECTED:
                LedToggle(LED_1);
        break;
        case BLE_CONNECTED:
                LedOn(LED_1);
        break;
        }
   }


}
//==================[end of file]============================================//