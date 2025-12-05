#include <xc.h>
#define _XTAL_FREQ 4000000UL

#pragma config FOSC = XT
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = OFF
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

// ============ OPTIMIZACIÓN DE MEMORIA ============
// Buffer circular más pequeño y eficiente
#define BUFFER_SIZE 32
#define BUFFER_MASK 0x1F
volatile unsigned char uartBuffer[BUFFER_SIZE];
volatile unsigned char bufferWrite = 0;
volatile unsigned char bufferRead = 0;

// Arrays para sprites (8 bytes cada uno) - sin redundancia
unsigned char character[8];
unsigned char obstacle[8];

// Estructura compacta para metas del juego (3 bytes total)
typedef struct {
    unsigned char tipo;      // 0=tiempo, 1=obstáculos (1 byte)
    unsigned int valor;      // Valor objetivo 0-9999 (2 bytes)
} GameGoal;

GameGoal meta;

// Buffer temporal multiuso - SE REUTILIZA en diferentes funciones
// Esto ahorra memoria al no tener múltiples buffers locales
#define TEMP_BUFFER_SIZE 5
unsigned char tempBuffer[TEMP_BUFFER_SIZE];

// ============ FUNCIONES LCD ============
void E_ENC(void) {
    PORTCbits.RC2 = 1;
    __delay_ms(5);
    PORTCbits.RC2 = 0;
    __delay_ms(2);
}

void COMANDO(unsigned char valor) {
    PORTB = valor;
    PORTCbits.RC0 = 0;  // RS = 0 (comando)
    PORTCbits.RC1 = 0;  // RW = 0 (escritura)
    E_ENC();
    __delay_ms(5);
}

void DIGITO(unsigned char valor) {
    PORTB = valor;
    PORTCbits.RC0 = 1;  // RS = 1 (dato)
    PORTCbits.RC1 = 0;  // RW = 0 (escritura)
    E_ENC();
    __delay_us(500);
}

void LCD_Init(void) {
    TRISB = 0x00;
    TRISCbits.TRISC0 = 0;
    TRISCbits.TRISC1 = 0;
    TRISCbits.TRISC2 = 0;
    
    __delay_ms(50);
    
    COMANDO(0x38);  // 8 bits, 2 líneas, 5x8
    COMANDO(0x0C);  // Display ON, cursor OFF
    COMANDO(0x01);  // Limpiar display
    COMANDO(0x06);  // Incrementar cursor
    __delay_ms(2);
}

// Función optimizada - usa índice directo sin multiplicación
void LCD_CargarCGRAM(unsigned char posicion, unsigned char *sprite) {
    unsigned char i;
    unsigned char addr;
    
    // DirecciónCGRAM: usa shift en lugar de multiplicación
    addr = 0x40 | ((posicion & 0x07) << 3);
    COMANDO(addr);
    
    // Escribir 8 bytes en CGRAM
    for(i = 0; i < 8; i++) {
        DIGITO(sprite[i]);
    }
    
    // Regresar a DDRAM
    COMANDO(0x80);
}

void LCD_Posicion(unsigned char col, unsigned char fila) {
    unsigned char addr;
    addr = (fila == 0) ? (0x80 + col) : (0xC0 + col);
    COMANDO(addr);
}

void LCD_Escr_String(const char *str) {
    while(*str) {
        DIGITO(*str++);
    }
}

// ============ FUNCIONES UART ============
void UART_Init(void) {
    TRISCbits.TRISC6 = 0;
    TRISCbits.TRISC7 = 1;
    ADCON1 = 0x07;
    
    SPBRG = 25;
    TXSTAbits.BRGH = 1;
    TXSTAbits.SYNC = 0;
    TXSTAbits.TXEN = 1;
    RCSTAbits.SPEN = 1;
    RCSTAbits.CREN = 1;
    
    PIE1bits.RCIE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
    
    // Inicializar índices del buffer
    bufferWrite = 0;
    bufferRead = 0;
}

void UART_Escr(unsigned char dato) {
    while(!TXSTAbits.TRMT);
    TXREG = dato;
}

void UART_Escr_String(const char *str) {
    while(*str) {
        UART_Escr(*str++);
    }
}

void __interrupt() ISR(void) {
    if(PIR1bits.RCIF) {
        // Manejo de error de overrun - limpia sin perder el dato actual
        if(RCSTAbits.OERR) {
            RCSTAbits.CREN = 0;
            RCSTAbits.CREN = 1;
        }
        // Escritura optimizada usando máscara
        uartBuffer[bufferWrite & BUFFER_MASK] = RCREG;
        bufferWrite++;
    }
}

// Función inline optimizada para verificar datos disponibles
unsigned char UART_Disp(void) {
    return (bufferWrite != bufferRead);
}

unsigned char UART_LeeBuffer(void) {
    unsigned char dato;
    while(!UART_Disp());
    dato = uartBuffer[bufferRead & BUFFER_MASK];
    bufferRead++;
    return dato;
}

// Búsqueda optimizada sin variables adicionales
unsigned char buscaChar(unsigned char c) {
    unsigned char temp = bufferRead;
    while(temp != bufferWrite) {
        if(uartBuffer[temp & BUFFER_MASK] == c)
            return 1;
        temp++;
    }
    return 0;
}

// Función eficiente para limpiar buffer
void UART_LimpiaBuffer(void) {
    bufferRead = bufferWrite;  // Sincroniza índices
}

// Función optimizada para buscar inicio de JSON
void UART_BuscaInicioJSON(void) {
    unsigned char c;
    unsigned int timeout = 0;
    
    while(timeout < 5000) {
        if(UART_Disp()) {
            c = UART_LeeBuffer();
            if(c == '{') {
                // Retrocede el índice con manejo circular
                bufferRead = (bufferRead == 0) ? (BUFFER_SIZE - 1) : (bufferRead - 1);
                return;
            }
        }
        __delay_us(100);
        timeout++;
    }
}

// ============ FUNCIONES DE CONVERSIÓN OPTIMIZADAS ============
// Convierte string ASCII a número usando el buffer temporal global
// Reutiliza tempBuffer en lugar de crear variable local
unsigned int strToUInt(unsigned char len) {
    unsigned int resultado = 0;
    unsigned char i;
    
    for(i = 0; i < len && tempBuffer[i] != '\0'; i++) {
        // Optimización: multiplicación por 10 usando shifts y suma
        // resultado * 10 = (resultado << 3) + (resultado << 1)
        resultado = (resultado << 3) + (resultado << 1) + (tempBuffer[i] - '0');
    }
    
    return resultado;
}

// Lee dígitos y los almacena en tempBuffer
// Retorna la cantidad de dígitos leídos
unsigned char leerDigitos(void) {
    unsigned char c, idx = 0;
    
    // Salta espacios y comas
    do {
        c = UART_LeeBuffer();
    } while(c == ' ' || c == ',');
    
    // Lee dígitos
    while(c >= '0' && c <= '9' && idx < TEMP_BUFFER_SIZE - 1) {
        tempBuffer[idx++] = c;
        c = UART_LeeBuffer();
    }
    
    tempBuffer[idx] = '\0';
    return idx;
}

// ============ PARSEO JSON ULTRA-OPTIMIZADO ============
void JSON_Parse(void) {
    unsigned char c, i, len;
    unsigned int intentos = 0;
    
    // Espera inicio de JSON
    while(!buscaChar('{') && intentos < 1000) {
        __delay_ms(10);
        intentos++;
    }
    
    if(intentos >= 1000) {
        return; // Timeout
    }
    
    // Lee '{'
    while(UART_LeeBuffer() != '{');
    
    // ===== PARSEO DE CHARACTER =====
    // Busca "character":[
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'c')
            break;
    }
    while(UART_LeeBuffer() != '[');
    
    // Lee 8 valores - OPTIMIZADO: usa función auxiliar
    for(i = 0; i < 8; i++) {
        len = leerDigitos();
        character[i] = (unsigned char)strToUInt(len);
    }
    
    // ===== PARSEO DE OBSTACLE =====
    // Busca "obstacle":[
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'o')
            break;
    }
    while(UART_LeeBuffer() != '[');
    
    // Lee 8 valores - REUTILIZA las mismas funciones
    for(i = 0; i < 8; i++) {
        len = leerDigitos();
        obstacle[i] = (unsigned char)strToUInt(len);
    }
    
    // ===== PARSEO DE GOALTYPE =====
    // Busca "goalType":
    while(1) {
        c = UART_LeeBuffer();
        if(c == 'g') {
            c = UART_LeeBuffer();
            if(c == 'o')
                break;
        }
    }
    
    // Salta hasta el valor
    while(UART_LeeBuffer() != ':');
    while(UART_LeeBuffer() != '"');
    
    // Lee tipo: 't' = tiempo (0), 'o' = obstáculos (1)
    c = UART_LeeBuffer();
    meta.tipo = (c == 't') ? 0 : 1;
    
    // ===== PARSEO DE GOALVALUE =====
    // Busca "goalValue":
    while(1) {
        c = UART_LeeBuffer();
        if(c == 'g') {
            c = UART_LeeBuffer();
            if(c == 'o') {
                while(UART_LeeBuffer() != ':');
                break;
            }
        }
    }
    
    // Lee valor de meta - REUTILIZA funciones optimizadas
    len = leerDigitos();
    meta.valor = strToUInt(len);
}

// ============ FUNCIÓN DE LIBERACIÓN DE RECURSOS ============
void limpiarRecursos(void) {
    unsigned char i;
    
    // Limpia buffer UART
    UART_LimpiaBuffer();
    
    // Limpia buffer temporal (opcional, se sobreescribirá)
    for(i = 0; i < TEMP_BUFFER_SIZE; i++) {
        tempBuffer[i] = 0;
    }
    
    // Limpia buffers de hardware
    if(RCSTAbits.OERR) {
        RCSTAbits.CREN = 0;
        RCSTAbits.CREN = 1;
    }
    
    // Flush de buffers hardware
    while(PIR1bits.RCIF) {
        i = RCREG;  // Lee y descarta
    }
}

// ============ FUNCIÓN DE DISPLAY OPTIMIZADA ============
// Usa variables locales mínimas y reutiliza registros
void mostrarConfiguracion(void) {
    unsigned char digito;
    
    // Limpiar LCD
    COMANDO(0x01);
    __delay_ms(2);
    
    // Cargar sprites en CGRAM
    LCD_CargarCGRAM(0, character);
    LCD_CargarCGRAM(1, obstacle);
    
    // Línea 1: Mostrar sprites
    LCD_Posicion(0, 0);
    LCD_Escr_String("Char:");
    DIGITO(0);
    LCD_Escr_String(" Obst:");
    DIGITO(1);
    
    // Línea 2: Mostrar meta
    LCD_Posicion(0, 1);
    LCD_Escr_String("Meta:");
    
    // Tipo de meta
    DIGITO((meta.tipo == 0) ? 'T' : 'O');
    DIGITO('=');
    
    // Valor de meta (4 dígitos) - optimizado con división inline
    digito = meta.valor / 1000;
    DIGITO(digito + '0');
    
    digito = (meta.valor / 100) % 10;
    DIGITO(digito + '0');
    
    digito = (meta.valor / 10) % 10;
    DIGITO(digito + '0');
    
    digito = meta.valor % 10;
    DIGITO(digito + '0');
}

// ============ FUNCIÓN PRINCIPAL ============
void main(void) {
    unsigned char i;
    
    // Inicialización
    UART_Init();
    LCD_Init();
    
    // Inicializar sprites vacíos - optimizado con memset inline
    for(i = 0; i < 8; i++) {
        character[i] = 0;
        obstacle[i] = 0;
    }
    
    // Inicializar meta con valores por defecto
    meta.tipo = 1;    // Obstáculos
    meta.valor = 10;
    
    // Limpia todos los recursos
    limpiarRecursos();
    
    UART_Escr_String("Sistema listo\r\n");
    
    // Mensaje de bienvenida
    LCD_Posicion(0, 0);
    LCD_Escr_String("Esperando");
    LCD_Posicion(0, 1);
    LCD_Escr_String("config...");
    
    // ===== LOOP PRINCIPAL OPTIMIZADO =====
    while(1) {
        // Verifica si hay JSON completo
        if(buscaChar('}')) {
            // Pequeña espera para asegurar recepción completa
            __delay_ms(100);
            
            // Parsea configuración
            JSON_Parse();
            
            // Muestra en LCD
            mostrarConfiguracion();
            
            // Envía confirmación JSON
            __delay_ms(100);
            UART_Escr_String("{\"status\":\"ok\"}\r\n");
            
            // CRÍTICO: Limpia todos los recursos después de procesar
            limpiarRecursos();
            
            // Pausa antes de siguiente iteración
            __delay_ms(100);
        }
        
        // Pausa pequeña para no saturar CPU
        __delay_ms(10);
        
        // Mantenimiento preventivo del buffer cada N iteraciones
        // Esto previene acumulación de datos basura
        static unsigned char contador = 0;
        if(++contador > 100) {
            contador = 0;
            // Si el buffer tiene datos antiguos sin procesar, límpialo
            if(UART_Disp() && !buscaChar('{')) {
                limpiarRecursos();
            }
        }
    }
}