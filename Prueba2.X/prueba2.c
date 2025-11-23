#include <xc.h>
#define _XTAL_FREQ 4000000UL

// CONFIG
#pragma config FOSC = XT        // Cristal XT (4MHz)
#pragma config WDTE = OFF       // Watchdog Timer deshabilitado
#pragma config PWRTE = ON       // Power-up Timer habilitado
#pragma config BOREN = OFF      // Brown-out Reset deshabilitado
#pragma config LVP = OFF        // Low Voltage Programming deshabilitado
#pragma config CPD = OFF        // Data EEPROM Code Protection
#pragma config WRT = OFF        // Flash Program Memory Write
#pragma config CP = OFF         // Flash Program Memory Code Protection

// Buffer pequeño para JSON (reducido de 128 a 32 bytes)
#define BUFFER_SIZE 32
char jsonBuffer[BUFFER_SIZE];
unsigned char bufferIndex = 0;

void UART_Init(void) {
    // Configurar pines
    TRISCbits.TRISC6 = 0;       // RC6/TX como salida
    TRISCbits.TRISC7 = 1;       // RC7/RX como entrada
    
    // Configurar pines analógicos como digitales
    ADCON1 = 0x07;              // Todos digitales
    
    // Configurar UART para 9600 baudios con 4MHz
    SPBRG = 25;                 // Valor para 9600 @ 4MHz con BRGH=1
    TXSTAbits.BRGH = 1;         // High speed (más preciso)
    TXSTAbits.SYNC = 0;         // Modo asíncrono
    TXSTAbits.TXEN = 1;         // Habilitar transmisión
    
    RCSTAbits.SPEN = 1;         // Habilitar puerto serial
    RCSTAbits.CREN = 1;         // Habilitar recepción continua
    
    // Inicializar buffer
    bufferIndex = 0;
}

void UART_Write(unsigned char data) {
    while(!TXSTAbits.TRMT);     // Esperar a que termine transmisión anterior
    TXREG = data;               // Enviar dato
}

void UART_Write_String(const char *str) {
    while(*str) {
        UART_Write(*str++);
    }
}

unsigned char UART_Read(void) {
    // Esperar a que llegue un dato
    while(!PIR1bits.RCIF) {
        // Verificar y limpiar error de overrun
        if(RCSTAbits.OERR) {
            RCSTAbits.CREN = 0;
            RCSTAbits.CREN = 1;
        }
    }
    return RCREG;
}

// Función simple para buscar el valor de "command" en JSON - optimizada
int buscar_valor_json(void) {
    // Búsqueda optimizada para "command":X
    for(unsigned char i = 0; i < bufferIndex - 9; i++) {
        // Buscar patron: "command":
        if(jsonBuffer[i] == '"' && jsonBuffer[i+1] == 'c' && jsonBuffer[i+2] == 'o' && 
           jsonBuffer[i+3] == 'm' && jsonBuffer[i+4] == 'm' && jsonBuffer[i+5] == 'a' && 
           jsonBuffer[i+6] == 'n' && jsonBuffer[i+7] == 'd' && jsonBuffer[i+8] == '"' && 
           jsonBuffer[i+9] == ':') {
            
            // Buscar el número después de los dos puntos
            for(unsigned char j = i + 10; j < bufferIndex; j++) {
                if(jsonBuffer[j] >= '0' && jsonBuffer[j] <= '9') {
                    return (int)(jsonBuffer[j] - '0'); // Convertir char a int
                }
            }
        }
    }
    return -1; // No encontrado
}

// Función para enviar respuesta JSON simple - corregida conversión
void enviar_respuesta_json(int valor) {
    // Enviar JSON manualmente sin usar sprintf
    UART_Write('{');
    UART_Write('"');
    UART_Write('r');
    UART_Write('e');
    UART_Write('s');
    UART_Write('p');
    UART_Write('o');
    UART_Write('n');
    UART_Write('s');
    UART_Write('e');
    UART_Write('"');
    UART_Write(':');
    UART_Write(' ');
    
    // Conversión corregida del valor
    UART_Write((unsigned char)('0' + (unsigned char)valor));
    
    UART_Write(',');
    UART_Write(' ');
    UART_Write('"');
    UART_Write('s');
    UART_Write('t');
    UART_Write('a');
    UART_Write('t');
    UART_Write('u');
    UART_Write('s');
    UART_Write('"');
    UART_Write(':');
    UART_Write(' ');
    UART_Write('"');
    UART_Write('s');
    UART_Write('u');
    UART_Write('c');
    UART_Write('c');
    UART_Write('e');
    UART_Write('s');
    UART_Write('s');
    UART_Write('"');
    UART_Write('}');
    UART_Write('\r');
    UART_Write('\n');
}

void main(void) {
    // Configurar LED en RC0 para debug
    TRISCbits.TRISC0 = 0;       // RC0 como salida
    PORTCbits.RC0 = 0;          // LED apagado inicialmente
    
    // Configurar LED adicional en RC1 para indicar transmisión
    TRISCbits.TRISC1 = 0;
    PORTCbits.RC1 = 0;

    // Inicializar UART
    UART_Init();
    
    // Pequeño delay para estabilizar
    __delay_ms(100);
    
    // Enviar mensaje de inicio
    UART_Write_String("PIC16F877A Listo\r\n");
    
    // Loop principal
    while(1) {
        // Verificar si hay datos disponibles
        if(PIR1bits.RCIF) {
            unsigned char dato = UART_Read();
            
            // Si es newline, procesar el JSON completo
            if(dato == '\n' || dato == '\r') {
                if(bufferIndex > 0) {
                    jsonBuffer[bufferIndex] = '\0';  // Terminar string
                    
                    // Indicar recepción con LED
                    PORTCbits.RC1 = 1;
                    __delay_ms(20);
                    PORTCbits.RC1 = 0;
                    
                    // Procesar JSON
                    int comando = buscar_valor_json();
                    
                    if(comando == 1) {
                        // Encender LED principal
                        PORTCbits.RC0 = 1;
                        
                        // Enviar respuesta JSON
                        enviar_respuesta_json(1);
                        
                    } else if(comando == 0) {
                        // Apagar LED
                        PORTCbits.RC0 = 0;
                        
                        // Enviar respuesta JSON
                        enviar_respuesta_json(0);
                    }
                    
                    // Limpiar buffer para siguiente mensaje
                    bufferIndex = 0;
                }
                
            } else if(bufferIndex < (BUFFER_SIZE - 1)) {
                // Almacenar carácter en buffer (excepto newline/carriage return)
                jsonBuffer[bufferIndex++] = dato;
            }
        }
        
        // Pequeño delay para evitar sobrecarga
        __delay_ms(1);
    }
}