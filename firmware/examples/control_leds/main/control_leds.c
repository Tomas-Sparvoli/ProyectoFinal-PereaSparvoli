/*! @mainpage ejemplo Bluetooth LED-RGB
 *
 * @section genDesc General Description
 *
 * Este proyecto ejemplifica el uso del módulo de comunicación Bluetooth Low Energy (BLE) 
 * junto con el manejo de tiras de LEDs RGB. 
 * Permite manejar la tonalidad e intensidad del LED RGB incluído en la placa ESP-EDU, 
 * mediante una aplicación móvil.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 02/04/2024 | Document creation		                         |
 *
 * @author Camila Perea (camila.perea@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led.h"
#include "neopixel_stripe.h"
#include "ble_mcu.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
#include "switch.h"

/*==================[macros and definitions]=================================*/

#define CONFIG_BLINK_PERIOD 500
#define LED_BT	LED_1

#define BUILT_IN_RGB_LED_PIN          GPIO_8        /*> ESP32-C6-DevKitC-1 NeoPixel it's connected at GPIO_8 */
#define BUILT_IN_RGB_LED_LENGTH       1             /*> ESP32-C6-DevKitC-1 NeoPixel has one pixel */

#define LED_COUNT 9               // Número de LEDs en la tira (cada LED representa un ángulo)
#define START_ANGLE 110           // Ángulo inicial (110 grados)
#define ANGLE_STEP (START_ANGLE / LED_COUNT)  // Paso entre cada LED en grados
//#define BUTTON_PIN GPIO_0         // Pin del botón

/*==================[typedef]================================================*/

TaskHandle_t led_task_handle = NULL;

bool is_running = false;
uint8_t current_led = 0;
uint8_t current_angle = START_ANGLE;
uint8_t teclas;


/*==================[function declarations]===============================*/

/**
 * @brief Función invocada en la interrupción del timer A
 */
void FuncTimer(void* param){
    vTaskNotifyGiveFromISR(led_task_handle, pdFALSE);    /* Envía una notificación a la tarea asociada al LED_1 */
}

/**
 * @fn void LedControlTask(void *pvParameter);
 * @brief 
 * @param[in] void *pvParameter
 * @return
 */
void LedControlTask(void *pvParameter);

/**
 * @fn static void CambiarEstado(void *pvParameter);
 * @brief Función que cambia el estado de las variables encendido y hold ediante comandos enviados a través del puerto UART.
 * @param void *pvParameter 
 * @return 
 */
void CambiarEstado(void *pvParameter);

/**
 * @fn static void ButtonTask(void *pvParameter);
 * @brief Función que controla el botón para iniciar/detener la secuencia de LEDs.
 * @param void *pvParameter 
 * @return 
 */
void ButtonTask(void *pvParameter);

/**
 * @fn static void DisplayAngle(uint8_t angle);
 * @brief Función que muestra el ángulo actual en una pantalla (simulado con printf).
 * @param void *pvParameter 
 * @return 
 */
void DisplayAngle(uint8_t angle);

/*==================[internal functions declaration]=========================*/

void LedControlTask(void *pvParameter){
    uint8_t i = 1;
    neopixel_color_t color [11];
    while(true){ 

    if(is_running == true){
        
        // Encender el LED correspondiente
        NeoPixelSetPixel(current_led,  NEOPIXEL_COLOR_RED);
        // Esperar 3 segundos
        vTaskDelay(3000 / portTICK_PERIOD_MS);

        // Apagar el LED anterior
        NeoPixelSetPixel(current_led -1, 0x000000);

        // Mover al siguiente LED
            current_led++;
            current_angle -= ANGLE_STEP;

        // Si llegamos al último LED (ángulo 0), detener la secuencia
        if (current_led >= LED_COUNT) {
            is_running = false;
            current_led = 0;
            current_angle = START_ANGLE;
        }
        else {
            // Suspender la tarea si no está corriendo
            //vTaskSuspend(NULL);
    }
    }
}
}

void CambiarEstado(void *param)
{
	uint8_t caracter;
	UartReadByte(UART_PC, &caracter);
	if (caracter == 'l')
		is_running = !is_running;
}

void ButtonTask(void *pvParameter){
while (true) {
        teclas = SwitchesRead();  // Actualizar el estado del botón
        // Detectar si el botón fue presionado
        switch(teclas){
        case SWITCH_1:
            if (is_running) {
                // Si ya está corriendo, detener y mostrar el ángulo
                is_running = false;
                DisplayAngle(current_angle);
                //vTaskSuspend(led_task_handle);  // Suspender la tarea de control de LEDs
            } else {
                // Si no está corriendo, iniciar la secuencia
                is_running = true;
                current_led = 0;
                current_angle = START_ANGLE;
                vTaskResume(led_task_handle);  // Reanudar la tarea de control de LEDs
            }
            // Esperar un poco para evitar rebotes de botón
            vTaskDelay(500 / portTICK_PERIOD_MS);
        break;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);  // Pequeño retardo para evitar saturar la CPU
    }
}

void DisplayAngle(uint8_t angle) {
    printf("Ángulo detenido: %d grados\n", angle);
}

/*==================[external functions definition]==========================*/
void app_main(void){
    static neopixel_color_t color;
    ble_config_t ble_configuration = {
        "campimetro",
        LedControlTask
    };

    LedsInit();
    BleInit(&ble_configuration);

    /* Se inicializa el LED RGB de la placa */
    NeoPixelInit(GPIO_9, 11, &color);
    NeoPixelAllOff();
    NeoPixelAllColor(NeoPixelRgb2Color(100, 0, 0));

    // Inicialización del botón
    SwitchesInit();
    
    /* Inicialización de timers */
    timer_config_t timer_led = {
        .timer = TIMER_A,
        .period = CONFIG_BLINK_PERIOD,
        .func_p = FuncTimer,
        .param_p = NULL
    };

   // Crear tareas
    xTaskCreate(LedControlTask, "LedControlTask", 2048, NULL, 5, &led_task_handle);
    xTaskCreate(ButtonTask, "ButtonTask", 1024, NULL, 5, NULL);
}

/*==================[end of file]============================================*/