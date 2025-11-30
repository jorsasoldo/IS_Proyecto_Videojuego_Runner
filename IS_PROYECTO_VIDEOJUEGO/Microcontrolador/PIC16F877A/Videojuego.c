#include <xc.h>
#define _XTAL_FREQ 4000000UL

#pragma config FOSC = XT        //Cristal (4MHz)
#pragma config WDTE = OFF       //Watchdog Timer deshabilitado
#pragma config PWRTE = ON       //Power-up Timer habilitado
#pragma config BOREN = OFF      //Brown-out Reset deshabilitado
#pragma config LVP = OFF        //Low Voltage Programming deshabilitado
#pragma config CPD = OFF        //Data EEPROM Code Protection
#pragma config WRT = OFF        //Flash Program Memory Write
#pragma config CP = OFF         //Flash Program Memory Code Protection

void UART_Init(void) 
{
    //Configura pines
    TRISCbits.TRISC6 = 0;       //RC6/TX como salida
    TRISCbits.TRISC7 = 1;       //RC7/RX como entrada
    
    //Configura todos los pines analagicos como digitales
    ADCON1 = 0x07;
    
    //Configurar UART para 9600 baudios con 4MHz
    SPBRG = 25;                 //Valor para 9600 con 4MHz
    TXSTAbits.BRGH = 1;         //Deteccion a alta velocidad
    TXSTAbits.SYNC = 0;         //Modo asincrono
    TXSTAbits.TXEN = 1;         //Habilita transmision
    
    RCSTAbits.SPEN = 1;         //Habilita el puerto serial
    RCSTAbits.CREN = 1;         //Habilita la recepcion continua
    
    //Inicializa buffer
    bufferIndex = 0;
}

void UART_Escr(unsigned char dato) 
{
    while(!TXSTAbits.TRMT);     //Espera a que termine transmision anterior
         
    TXREG = dato;               //Envia el dato
}

void UART_Escr_String(const char *str) 
{
    while(*str) 
    {
        UART_Escr(*str++);
    }
}

unsigned char UART_Read(void) 
{
    //Espera a que llegue un dato
    while(!PIR1bits.RCIF) 
    {
        //Verifica y limpia error de overrun
        if(RCSTAbits.OERR) 
        {
            RCSTAbits.CREN = 0;
            RCSTAbits.CREN = 1;
        }
    }
    return RCREG;
}
