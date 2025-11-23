#include <xc.h>
#define _XTAL_FREQ 4000000UL

// CONFIG (ajusta según tu configuración de proyecto)
#pragma config FOSC = XT        // Cristal XT (4MHz)
#pragma config WDTE = OFF       // Watchdog Timer deshabilitado
#pragma config PWRTE = ON       // Power-up Timer habilitado
#pragma config BOREN = OFF      // Brown-out Reset deshabilitado
#pragma config LVP = OFF        // Low Voltage Programming deshabilitado
#pragma config CPD = OFF        // Data EEPROM Code Protection
#pragma config WRT = OFF        // Flash Program Memory Write
#pragma config CP = OFF         // Flash Program Memory Code Protection

void UART_Init(void) {
    TRISCbits.TRISC6 = 0;
    TRISCbits.TRISC7 = 1;
    ADCON1 = 0x07;
    
    // Configuración CORRECTA para 9600 @ 4MHz
    SPBRG = 25;
    TXSTAbits.BRGH = 1;         // High speed - IMPORTANTE
    TXSTAbits.SYNC = 0;
    TXSTAbits.TXEN = 1;
    
    RCSTAbits.SPEN = 1;
    RCSTAbits.CREN = 1;
    
    // Pequeño delay para estabilizar
    __delay_ms(10);
}

void UART_Write(unsigned char data) {
    while(!TXSTAbits.TRMT);     // Esperar a que termine transmisión anterior
    TXREG = data;               // Enviar dato
}

unsigned char UART_Read(void) {
    // Esperar a que llegue un dato
    while(!PIR1bits.RCIF) {
        // Verificar y limpiar error de overrun mientras espera
        if(RCSTAbits.OERR) {
            RCSTAbits.CREN = 0;     // Deshabilitar recepción
            RCSTAbits.CREN = 1;     // Rehabilitar recepción
        }
        // Verificar y limpiar error de framing
        if(RCSTAbits.FERR) {
            unsigned char dummy = RCREG;  // Leer para limpiar error
        }
    }
    
    return RCREG;               // Leer y retornar dato
}

void main(void) {
    // Configurar LED en RC0 para debug
    TRISCbits.TRISC0 = 0;       // RC0 como salida
    PORTCbits.RC0 = 0;          // LED apagado
    
    // Inicializar UART
    UART_Init();
    
    /*// Parpadeo de inicio (3 veces) para verificar que el PIC funciona
    for(unsigned char i = 0; i < 3; i++) {
        PORTCbits.RC0 = 1;
        __delay_ms(100);
        PORTCbits.RC0 = 0;
        __delay_ms(100);
    }*/
    
    // Enviar mensaje de inicio
    __delay_ms(100);
    UART_Write('S');
    UART_Write('T');
    UART_Write('A');
    UART_Write('R');
    UART_Write('T');
    UART_Write('\r');
    UART_Write('\n');
    
    // Loop infinito - Echo simple
    while(1) {
        // Esperar y recibir un carácter
        unsigned char dato = UART_Read();
        
        // Parpadear LED para indicar recepción
        //PORTCbits.RC0 = 1;
        //__delay_ms(20);
        //PORTCbits.RC0 = 0;
        
        // Devolver el mismo carácter (echo)
        UART_Write(dato);
        
        // Si recibe '1', mantener LED encendido
        if(dato == '1') {
            PORTCbits.RC0 = 1;
        }
        // Si recibe '0', apagar LED
        else if(dato == '0') {
            PORTCbits.RC0 = 0;
        }
    }
}

// Nota: Si usas XC8, asegúrate de tener las siguientes configuraciones:
// - Optimization: -O0 (sin optimización) o -O1
// - Memory model: Free version compatible