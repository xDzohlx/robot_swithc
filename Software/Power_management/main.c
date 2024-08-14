/*
 * Power_management.c
 *
 * Created: 08/06/2024 15:41:55
 * Author : xDzohlx
 */ 

//To do mejorar la deteccion de boton por el ADC

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/cpufunc.h>
#include <util/delay.h>
#include <stdio.h>

typedef unsigned char bool;
#define false 0
#define true 1

#define boton_adc 0xe5

 enum pasos_de_encendido{
Apagado = 0,
Encendido_espera ,
led_1 ,
led_2 ,
led_3
}secuencia;

volatile uint16_t ADC_boton = 0x00;
uint16_t ADC_boton_calibracion = 0x00;
volatile uint16_t timer_1 = 0x00;
volatile uint8_t timer_2 = 0x00;

volatile uint8_t comparador = 0x00;

bool empezar_secuencia = false;
bool apagar = false;


void offsetsignals(){//etapa de autocalibracion por promedio, pequeño filtro digital
	//_delay_ms(1500);
	for (int i = 0;i<8;i++){//adquisición de señales, 8 valores en total
		_delay_ms(50);
		ADC_boton_calibracion += ADC_boton;
	}
	ADC_boton_calibracion = (ADC_boton_calibracion>>3);
	}
void setup(void)
{

	ccp_write_io((void *) & (CLKCTRL.MCLKCTRLB), (CLKCTRL_PDIV_2X_gc|CLKCTRL_PEN_bm));//Maxima frecuencia de lectura de pwm en señal de reloj per
	
	//pasos_de_encendido secuencia;
	
	secuencia = Encendido_espera;
	
	PORTF.DIRSET = PIN4_bm;//VCC enable PIN
	
	PORTA.DIRSET = PIN2_bm|PIN4_bm|PIN5_bm|PIN6_bm|PIN7_bm;//LEDS Y HSS
	
	PORTA.OUTCLR = PIN5_bm|PIN6_bm|PIN7_bm;//otros leds
	
	PORTA.OUTCLR = PIN4_bm;//HSS diag off
	
	PORTC.DIRSET = PIN2_bm;//LED 4
	
	PORTC.OUTCLR = PIN2_bm;//LED 4
	
	PORTA.PIN0CTRL  |= PORT_PULLUPEN_bm;
	
	//PORTA.OUTSET = PIN2_bm;//HSS en
	
	//Timer
	
	TCB0.CTRLA |= TCB_ENABLE_bm;
	TCB0.CCMP = 0xFA0;//Valor top 4000 interrupcion cada milisegundo
	TCB0.INTCTRL |= TCB_CAPT_bm;//habilita la interrupcion
	timer_1 = 0x00;
	
	//ADC
	VREF.CTRLA = VREF_ADC0REFSEL_4V34_gc;// Se selecciona la referencia a 2.5 volts par mejorar la resolucion de los sensores
	//ADC con referencia interna
	ADC0.CTRLA |= ADC_RESSEL_bm|ADC_FREERUN_bm;//RESOLUCION Y MODO DE MUESTRAS CONSECUTIVAS
	ADC0.CTRLB |= ADC_SAMPNUM_ACC32_gc;//MUESTRAS
	ADC0.CTRLC |= ADC_REFSEL_INTREF_gc|ADC_PRESC_DIV8_gc;//VOLTAJE DE REFERENCIA
	ADC0.CTRLD |= ADC_INITDLY_DLY16_gc;//CONFIGURACION DEL RELOJ DEL ADC
	//Canales de 0 al 7 despues a partir del 12 o 0x0C
	ADC0.MUXPOS = 0x01;//SELECCION DE CANAL DE ADC PD1
	ADC0.INTCTRL |= ADC_RESRDY_bm;//Habilitar interrupciones de resultado completo
	//ADC0.INTCTRL |= ADC_WCMP_bm;
	ADC0.CTRLA |= ADC_ENABLE_bm;//ENCENDIDO DE ADC
	ADC0.COMMAND |= ADC_STCONV_bm;//INICIO DE MUESTRAS
	
	CPUINT.LVL0PRI = ADC0_RESRDY_vect_num;
	
	
	//timer TCA
	TCA0.SINGLE.CTRLA |= TCA_SINGLE_CLKSEL_DIV2_gc;//fuente de reloj,
	TCA0.SINGLE.PER = 0x030;//Selección de resolucion de pwm y periodo total de pwm
	TCA0.SINGLE.CTRLB |= TCA_SINGLE_CMP2_bm|TCA_SINGLE_WGMODE_SINGLESLOPE_gc;//Habilitar comparador y seleccion de modo de de generacion de onda con modo de rampa sensilla TCA_SINGLE_CMP1EN_bm|
	//TCA0.SINGLE.CMP1 = 0x000;//registro de 16 bits para comparacion y pediodo de pwm
	TCA0.SINGLE.CMP2 = 0x018;

			//TCB2.CTRLA |= TCB_ENABLE_bm|TCB_CLKSEL_CLKDIV2_gc;
			//TCB2.CCMP = 0xFFFE;//Valor top 4000 interrupcion cada 10 milisegundos
			//TCB2.INTCTRL |= TCB_CAPT_bm;//habilita la interrupcion

	sei();
}
ISR(ADC0_RESRDY_vect){//solo 4 sensores para empezar
	//Canales de 0 al 7 despues a partir del 12 hasta sensor 14

	ADC_boton = ADC0.RES>>5;
}

ISR(TCB0_INT_vect){//contador de milisegundos, para generador de trayectorias
	timer_1++;

	if (empezar_secuencia)
	{
		if ((timer_1>1250)){//&&()&&(ADC_boton>0xbb)
			secuencia++;
			timer_1 = 0x00;
			if (secuencia > led_3){
				secuencia = led_3;
			}
		}
	}else{

		if ((timer_1>1250)){//&&(ADC_boton>0xbb)
			if (secuencia!=Apagado)
			{
				secuencia--;
			}
			
			timer_1 = 0x00;
		}
	}
	
	TCB0.INTFLAGS &= ~TCB_CAPT_bp;
}
	

int main(void)
{
    setup();
	
	PORTC.OUTSET = PIN2_bm;
	offsetsignals();
	_delay_ms(1500);
	PORTF.OUTSET = PIN4_bm;//VCC enable
	
	PORTC.OUTCLR = PIN2_bm;
    while (1) 
    {
//!(PORTA.IN & PIN0_bm)

	if (apagar)
	{
		if ((ADC_boton>boton_adc)){
			empezar_secuencia = false;
			}else{
			empezar_secuencia = true;
		}
		
	}else
	{
		if ((ADC_boton>boton_adc)){
			empezar_secuencia = true;
		}else{
			empezar_secuencia = false;
		}
	}


	//if ((ADC_boton>boton_adc))//Se requiere detectar cambios en la señal el voltaje de ADC boton no es estatico
	//{
		//empezar_secuencia=true;
		//if (apagar)
		//{
			//empezar_secuencia= false;
		//}
	//}else{
		//
		//if ((secuencia!=led_3))
		//{
			//empezar_secuencia= false;
		//}
		//if (apagar)
		//{
			//empezar_secuencia = true;
		//}
		//
	//}
	
	switch(secuencia){//maquina de estados para encender el HSS
		case Apagado:
			PORTA.OUTCLR = PIN2_bm;//HSS off
			PORTF.OUTCLR = PIN4_bm;//VCC off power off
			PORTC.OUTCLR = PIN2_bm;//lED 1 ON
			PORTA.OUTCLR = PIN7_bm;//LED 2 OFF
			PORTA.OUTCLR = PIN6_bm;//LED 3 OFF
			PORTA.OUTCLR = PIN5_bm;//LED 4 OFF
			
			TCA0.SINGLE.CMP2 = 0x00;
			
			while (1)
			{
				cli();
				asm("NOP");
			}
		break;
		case Encendido_espera:
			PORTC.OUTSET = PIN2_bm;//lED 1 ON
			PORTA.OUTCLR = PIN7_bm;//LED 2 OFF
			PORTA.OUTCLR = PIN6_bm;//LED 3 OFF
			PORTA.OUTCLR = PIN5_bm;//LED 4 OFF
			if (!apagar)
			{
				TCA0.SINGLE.CMP2 = 0x018;//Para encendido suave, carga de capacitores
				TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;//Habilitar pwm
			}
		//_delay_ms(7500);
		break;
		case led_1:
			PORTC.OUTSET = PIN2_bm;//lED 1 ON
			PORTA.OUTSET = PIN7_bm;//LED 2 ON
			PORTA.OUTCLR = PIN6_bm;//LED 3 OFF
			PORTA.OUTCLR = PIN5_bm;//LED 4 OFF
		//_delay_ms(7500);
		break;
		case led_2:
			PORTC.OUTSET = PIN2_bm;//lED 1 ON
			PORTA.OUTSET = PIN7_bm;//LED 2 ON
			PORTA.OUTSET = PIN6_bm;//LED 3 ON
			PORTA.OUTCLR = PIN5_bm;//LED 4 OFF
		//_delay_ms(7500);
		break;
		case led_3:
			TCA0.SINGLE.CMP2 = 0x030;
			PORTA.OUTSET = PIN6_bm;
			if ((ADC_boton>boton_adc))
			{
				}else{
				apagar = true;
				}		
			PORTC.OUTSET = PIN2_bm;//lED 1 ON
			PORTA.OUTSET = PIN7_bm;//LED 2 ON
			PORTA.OUTSET = PIN6_bm;//LED 3 ON
			PORTA.OUTSET = PIN5_bm;//LED 4 ON
		break;
		default:
		break;
	}
    }

}

