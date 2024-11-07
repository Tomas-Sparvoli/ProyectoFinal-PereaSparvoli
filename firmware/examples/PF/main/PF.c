/*! @mainpage PF
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 12/09/2023 | Document creation		                         |
 *
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 *
 */

//==================[inclusions]=============================================//
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

#define BUILT_IN_RGB_LED_PIN          GPIO_8        /*> ESP32-C6-DevKitC-1 NeoPixel it's connected at GPIO_8 */
#define BUILT_IN_RGB_LED_LENGTH       1             /*> ESP32-C6-DevKitC-1 NeoPixel has one pixel */

#define LED_COUNT 9               // Número de LEDs en la tira (cada LED representa un ángulo)
#define START_ANGLE 110           // Ángulo inicial (110 grados)
#define ANGLE_STEP (START_ANGLE / LED_COUNT)  // Paso entre cada LED en grados
//#define BUTTON_PIN GPIO_0         // Pin del botón


//==================[internal data definition]===============================//

TaskHandle_t cambiomotor = NULL;
TaskHandle_t presionoTecla = NULL;
TaskHandle_t enviar = NULL;


bool giro = false;
bool led = false;
bool mapa = false;
long int angulo = 0;
long int angulo_led = 0;


uint8_t current_led = 10;
uint8_t current_angle = START_ANGLE;
uint8_t teclas;
uint8_t angle_selec;
neopixel_color_t color;

 
//==================[internal functions declaration]=========================//

void FuncTimerA(void* param){
    vTaskNotifyGiveFromISR( presionoTecla, pdFALSE); 
}

/**
 * @brief Función que se ejecuta en la interrupción del temporizador B.
 *
 * Envía una notificación a la tarea encargada de mostrar la señal ECG.
 *
 * @param param Parámetro opcional (no utilizado).
 */

void FuncTimerB(void* param){
    vTaskNotifyGiveFromISR( presionoTecla, pdFALSE); 
}

/**
 * @brief Función que cambia el estado de las variables encendido y hold basándose en el input UART.
 * 
 * Esta función permite cambiar el estado de las variables mediante comandos enviados a través del puerto UART.
 * 
 * @param param Parámetro opcional (no se utiliza en esta función).
 */

void enviar_datos()
{
	char msg [30];

	long int xcentrado=110;
	long int ycentrado=110;

	long int distancia_rectangularx=angle_selec*cos(angulo);
	long int distancia_rectangulary=angle_selec*sin(angulo );
    sprintf(msg,"*GX%ldY%ld*",xcentrado+distancia_rectangularx,ycentrado+distancia_rectangulary);
    BleSendString(msg);
	printf("se enviaron los datos\r\n");
	printf("el angulo enviado fue de %ld\r\n",angulo);
	printf("el led seleccionado es de %d\r\n",angle_selec);
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
    
}

void cambioEstado(void *param)
{
	while(true)
	{	
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		uint8_t caracter;
		UartReadByte(UART_PC, &caracter);
		if (caracter == 'a')
			{
				giro = true;
				vTaskNotifyGiveFromISR (cambiomotor, pdFALSE);
				caracter = 0;
			}
		if (caracter == 'm')
			{	
				mapa=!mapa;
				enviar_datos();
				printf("Ángulo: %d grados\n", l64a);
            	current_led=10;
            	current_angle=110;
            	caracter = 0;			
			}
		if (caracter=='b')
        {
            color =  NEOPIXEL_COLOR_WHITE;
			printf("el color seleccionado es el blanco");
			caracter=0;
		}
    if (caracter=='r')
        {
            color =  NEOPIXEL_COLOR_RED;
			printf("el color seleccionado es el rojo");
			caracter=0;
        }
    if (caracter=='l')
        {
            LedControlTask(); 
            caracter=0;
        }

	}

}


void motorpaso()
{

	GPIOInit(GPIO_2,GPIO_OUTPUT); //pin de cantidad de pasos del motor
	GPIOInit(GPIO_3,GPIO_OUTPUT); //pin de direccionamiento del motor
	
	while(1)
	{		
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		if (giro==true)
			{
			if (angulo > 330)
				{	
					GPIOOff(GPIO_3);	
					for (int i = 0; i < 800; i++) 
						{
						GPIOOn(GPIO_2);	
						vTaskDelay(100 / portTICK_PERIOD_MS);
						GPIOOff(GPIO_2);
						vTaskDelay(100/portTICK_PERIOD_MS);
						}	
					angulo = 0;
				}	
				else 
				{ 	
					
					GPIOOn(GPIO_3); //seleccionio el modo de giro contrario a la aguja del reloj
					printf("entra al +45 grados\n\r");
					for (int i = 0; i < 100; i++) //cantidad de pasos necesarios para que el motor gire 30 grados en ese periodo de tiempo 
					{
						GPIOOn(GPIO_2);	//cmabio el estado logico de la salida del motor
						vTaskDelay(100 / portTICK_PERIOD_MS); //velocidad de giro
						GPIOOff(GPIO_2); //cambio el estado de la salida del motor
						vTaskDelay(100 / portTICK_PERIOD_MS);
					}	
					angulo = angulo + 45;
					printf("el angulo es :%ld\r\n",angulo);
					//printf("termina los 30 grados\n\r");
				}
				giro=false;
				printf("cambio el estado a falso\n\r");

			}
	}
}



//==================[external functions definition]==========================//

void app_main(void)
{
	static neopixel_color_t color[11];

	 /* Se inicializa el LED RGB de la placa */
    NeoPixelInit(GPIO_19, 11, &color);
    NeoPixelAllOff();

	timer_config_t timer_leds = 
	{
		.timer = TIMER_B,
		.period = 100000,
		.func_p = FuncTimerA,
		.param_p = NULL
	};
	TimerInit(&timer_leds);

	timer_config_t timer_estado = 
	{
		.timer = TIMER_B,
		.period = 100000,
		.func_p = FuncTimerB,
		.param_p = NULL
	};
	TimerInit(&timer_estado);

	serial_config_t pantalla = 
	{
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