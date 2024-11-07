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

#define CONFIG_BLINK_PERIOD 1000
#define LED_COUNT 11               // Número de LEDs en la tira (cada LED representa un ángulo)
#define START_ANGLE 110            // Ángulo inicial (110 grados)

/*==================[typedef]================================================*/

TaskHandle_t tecla_task_handle = NULL;
TaskHandle_t boton_task_handle = NULL;

uint8_t current_led = 10;
uint8_t current_angle = START_ANGLE;
uint8_t teclas;
uint8_t angle_selec;
neopixel_color_t color;

/*==================[function declarations]===============================*/

/**
 * @brief Función invocada en la interrupción del timer A
 */
void FuncTimerA(void* param);

/**
 * @brief Función invocada en la interrupción del timer B
 */
void FuncTimerB(void* param);

/**
 * @fn void LedControlTask(void *pvParameter);
 * @brief 
 * @param[in] void *pvParameter
 * @return
 */
void LedControlTask();

/**
 * @fn static void CambiarEstado(void *pvParameter);
 * @brief Función que cambia el estado de las variables encendido y hold ediante comandos enviados a través del puerto UART.
 * @param void *pvParameter 
 * @return 
 */
void CambiarEstado(void *pvParameter);

/*==================[internal functions declaration]=========================*/

void FuncTimerA(void* param){
    vTaskNotifyGiveFromISR(tecla_task_handle, pdFALSE);    /* Envía una notificación a la tarea asociada al LED_1 */
}

void FuncTimerB(void* param){
    vTaskNotifyGiveFromISR(boton_task_handle, pdFALSE);    /* Envía una notificación a la tarea asociada al LED_1 */
}

void LedControlTask()
{
            // Encender el LED correspondiente
            NeoPixelSetPixel(current_led, color);
            printf("Ángulo: %d grados\n", current_angle);
            angle_selec=current_angle;

            // Esperar 3 segundos
            vTaskDelay(3000 / portTICK_PERIOD_MS);

            // Apagar el LED anterior
            NeoPixelSetPixel(current_led, 0x000000);

            // Mover al siguiente LED
                current_led--;
                current_angle -= 10;
            
            // Si llegamos al último LED (ángulo 110), detener la secuencia
           /* if (current_led >= LED_COUNT) {
                is_running = false;
                current_led = 11;
            }*/       
    
}

void CambiarEstado(void *param)
{
    while (true) { 
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    uint8_t caracter;
    UartReadByte(UART_PC, &caracter);
    if (caracter=='b')
        {
            color =  NEOPIXEL_COLOR_WHITE;
        }
    if (caracter=='r')
        {
            color =  NEOPIXEL_COLOR_RED;
        }
    if (caracter=='l')
        {
            LedControlTask(); 
            caracter=0;
        }
    
    if (caracter=='p')
        {
        printf("Ángulo: %d grados\n", angle_selec);
            current_led=10;
            current_angle=110;
            caracter=0;
        }
    }
}

/*==================[external functions definition]==========================*/
void app_main(void)
{
    static neopixel_color_t color[11];

   /* ble_config_t ble_configuration = {
        "campimetro",
        LedControlTask
    };*/

    LedsInit();
   /* BleInit(&ble_configuration); */

    /* Se inicializa el LED RGB de la placa */
    NeoPixelInit(GPIO_19, 11, &color);
    NeoPixelAllOff();
    //NeoPixelAllColor(NeoPixelRgb2Color(100, 0, 0));

    // Inicialización del botón
    SwitchesInit();
    
    /* Inicialización de timers */
    timer_config_t timer_tecla = {
        .timer = TIMER_A,
        .period = CONFIG_BLINK_PERIOD,
        .func_p = FuncTimerA,
        .param_p = NULL
    };
    TimerInit(&timer_tecla);

     /* Inicialización de timers */
    timer_config_t timer_boton = {
        .timer = TIMER_B,
        .period = CONFIG_BLINK_PERIOD,
        .func_p = FuncTimerB,
        .param_p = NULL
    };
    TimerInit(&timer_boton);
    
    serial_config_t pantalla=
    {
        .port = UART_PC,
        .baud_rate = 9600,
        .func_p = CambiarEstado,
        .param_p = NULL,
    };
    UartInit (&pantalla);

   // Crear tareas
    xTaskCreate(CambiarEstado, "CambiarEstadoTask", 4096, NULL, 5, &tecla_task_handle);

    TimerStart(timer_tecla.timer);
}

/*==================[end of file]============================================*/