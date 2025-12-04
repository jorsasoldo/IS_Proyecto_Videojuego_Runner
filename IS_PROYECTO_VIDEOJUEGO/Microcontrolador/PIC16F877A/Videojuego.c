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

// Buffer circular reducido
#define BUFFER_SIZE 32
#define BUFFER_MASK 0x1F
volatile unsigned char uartBuffer[BUFFER_SIZE];
volatile unsigned char bufferWrite = 0;
volatile unsigned char bufferRead = 0;

// Arrays para sprites (8 bytes cada uno)
unsigned char character[8];
unsigned char obstacle[8];

// Estructura de datos para metas del juego
typedef struct {
    unsigned char tipo;      // 0=tiempo, 1=puntaje, 2=distancia
    unsigned int valor;      // Valor objetivo (0-9999)
} GameGoal;

GameGoal meta;

// ============ FUNCIONES LCD ============
void E_ENC(void) {
    PORTCbits.RC2 = 1;  // E = 1
    __delay_ms(5);
    PORTCbits.RC2 = 0;  // E = 0
    __delay_ms(2);
}

void COMANDO(unsigned char VALOR) {
    PORTB = VALOR;
    PORTCbits.RC0 = 0;  // RS = 0 (comando)
    PORTCbits.RC1 = 0;  // RW = 0 (escritura)
    E_ENC();
    __delay_ms(5);
}

void DIGITO(unsigned char VALOR) {
    PORTB = VALOR;
    PORTCbits.RC0 = 1;  // RS = 1 (dato)
    PORTCbits.RC1 = 0;  // RW = 0 (escritura)
    E_ENC();
    __delay_us(500);
}

void LCD_Init(void) {
    // Configurar puertos
    TRISB = 0x00;  // Puerto B como salida (D0-D7)
    TRISCbits.TRISC0 = 0;  // RC0 (RS) como salida
    TRISCbits.TRISC1 = 0;  // RC1 (RW) como salida
    TRISCbits.TRISC2 = 0;  // RC2 (E) como salida
    
    __delay_ms(50);
    
    COMANDO(0x38);  // 8 bits, 2 líneas, 5x8
    COMANDO(0x0C);  // Display ON, cursor OFF
    COMANDO(0x01);  // Limpiar display
    COMANDO(0x06);  // Incrementar cursor
    __delay_ms(2);
}

void LCD_CargarCGRAM(unsigned char posicion, unsigned char *sprite) {
    unsigned char i;
    unsigned char addr;
    
    // Dirección CGRAM: posición 0-7, cada una ocupa 8 bytes
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
    
    if(fila == 0)
        addr = 0x80 + col;
    else
        addr = 0xC0 + col;
    
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
        if(RCSTAbits.OERR) {
            RCSTAbits.CREN = 0;
            RCSTAbits.CREN = 1;
        }
        uartBuffer[bufferWrite & BUFFER_MASK] = RCREG;
        bufferWrite++;
    }
}

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

unsigned char buscaChar(unsigned char c) {
    unsigned char temp = bufferRead;
    while(temp != bufferWrite) {
        if(uartBuffer[temp & BUFFER_MASK] == c)
            return 1;
        temp++;
    }
    return 0;
}

// Limpia el buffer UART descartando todos los datos actuales
void UART_LimpiaBuffer(void) {
    bufferRead = bufferWrite;
}

// Espera hasta encontrar el inicio de un JSON válido
void UART_BuscaInicioJSON(void) {
    unsigned char c;
    unsigned int timeout = 0;
    
    // Busca '{' descartando todo lo demás
    while(timeout < 5000) {
        if(UART_Disp()) {
            c = UART_LeeBuffer();
            if(c == '{') {
                // Encontró inicio de JSON, retrocede el índice de lectura
                if(bufferRead > 0) {
                    bufferRead--;
                } else {
                    bufferRead = BUFFER_SIZE - 1;
                }
                return;
            }
        }
        __delay_us(100);
        timeout++;
    }
}

// ============ PARSEO JSON OPTIMIZADO ============
void JSON_Parse(void) {
    unsigned char c, i, temp[4], tempIdx;
    unsigned int intentos = 0;
    
    // Espera y busca inicio de JSON válido
    while(!buscaChar('{') && intentos < 1000) {
        __delay_ms(10);
        intentos++;
    }
    
    if(intentos >= 1000) {
        return; // Timeout esperando JSON
    }
    
    // Espera '{'
    while(UART_LeeBuffer() != '{');
    
    // Busca "character":[
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'c')
            break;
    }
    while(UART_LeeBuffer() != '[');
    
    // Lee 8 valores para character
    for(i = 0; i < 8; i++) {
        tempIdx = 0;
        do {
            c = UART_LeeBuffer();
        } while(c == ' ' || c == ',');
        
        while(c >= '0' && c <= '9') {
            temp[tempIdx++] = c;
            c = UART_LeeBuffer();
        }
        temp[tempIdx] = '\0';
        
        character[i] = 0;
        for(tempIdx = 0; temp[tempIdx] != '\0'; tempIdx++) {
            character[i] = character[i] * 10 + (temp[tempIdx] - '0');
        }
    }
    
    // Busca "obstacle":[
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'o')
            break;
    }
    while(UART_LeeBuffer() != '[');
    
    // Lee 8 valores para obstacle
    for(i = 0; i < 8; i++) {
        tempIdx = 0;
        do {
            c = UART_LeeBuffer();
        } while(c == ' ' || c == ',');
        
        while(c >= '0' && c <= '9') {
            temp[tempIdx++] = c;
            c = UART_LeeBuffer();
        }
        temp[tempIdx] = '\0';
        
        obstacle[i] = 0;
        for(tempIdx = 0; temp[tempIdx] != '\0'; tempIdx++) {
            obstacle[i] = obstacle[i] * 10 + (temp[tempIdx] - '0');
        }
    }
    
    // Busca "goalType": (busca 'g' de goalType)
    while(1) {
        c = UART_LeeBuffer();
        if(c == 'g') {
            c = UART_LeeBuffer();
            if(c == 'o') // goalType o goalValue
                break;
        }
    }
    
    // Verifica si es goalType (siguiente char es 'a')
    c = UART_LeeBuffer(); // 'a' de goalType
    while(UART_LeeBuffer() != ':');
    while(UART_LeeBuffer() != '"');
    
    // Lee tipo de meta: "time" u "obstacles"
    c = UART_LeeBuffer();
    if(c == 't') {
        meta.tipo = 0;  // tiempo (time)
    } else if(c == 'o') {
        meta.tipo = 1;  // obstáculos (obstacles)
    }
    
    // Salta hasta "goalValue":
    while(1) {
        c = UART_LeeBuffer();
        if(c == 'g') {
            c = UART_LeeBuffer();
            if(c == 'o') { // "goalValue"
                while(UART_LeeBuffer() != ':');
                break;
            }
        }
    }
    
    // Lee valor de meta (hasta 4 dígitos)
    tempIdx = 0;
    do {
        c = UART_LeeBuffer();
    } while(c == ' ' || c == ',');
    
    while(c >= '0' && c <= '9') {
        temp[tempIdx++] = c;
        c = UART_LeeBuffer();
    }
    temp[tempIdx] = '\0';
    
    // Convierte string a número (unsigned int)
    meta.valor = 0;
    for(tempIdx = 0; temp[tempIdx] != '\0'; tempIdx++) {
        meta.valor = meta.valor * 10 + (temp[tempIdx] - '0');
    }
}

// ============ FUNCIÓN PRINCIPAL ============
void main(void) {
    unsigned char i;
    
    UART_Init();
    LCD_Init();
    
    // Inicializar sprites vacíos
    for(i = 0; i < 8; i++) {
        character[i] = 0;
        obstacle[i] = 0;
    }
    
    // Inicializar meta
    meta.tipo = 1;    // Por defecto: obstáculos
    meta.valor = 10;
    
    UART_Escr_String("Sistema listo\r\n");
    
    // Mensaje de bienvenida en LCD
    LCD_Posicion(0, 0);
    LCD_Escr_String("Esperando");
    LCD_Posicion(0, 1);
    LCD_Escr_String("config...");
    
    while(1) {
        // Espera JSON completo
        if(buscaChar('}')) {
            // Limpia el buffer antes de parsear para evitar datos viejos
            __delay_ms(100); // Espera a que termine de llegar todo el JSON
            
            JSON_Parse();
            
            // Limpiar LCD
            COMANDO(0x01);
            __delay_ms(2);
            
            // Cargar sprites en CGRAM
            LCD_CargarCGRAM(0, character);   // Posición 0
            LCD_CargarCGRAM(1, obstacle);    // Posición 1
            
            // Mostrar sprites en LCD (línea 1)
            LCD_Posicion(0, 0);
            LCD_Escr_String("Char:");
            DIGITO(0);  // Muestra character (pos 0 CGRAM)
            LCD_Escr_String(" Obst:");
            DIGITO(1);  // Muestra obstacle (pos 1 CGRAM)
            
            // Mostrar información de la meta en LCD (línea 2)
            LCD_Posicion(0, 1);
            LCD_Escr_String("Meta:");
            
            // Mostrar tipo de meta
            if(meta.tipo == 0) {
                DIGITO('T');  // Tiempo
            } else {
                DIGITO('O');  // Obstáculos
            }
            
            DIGITO('=');
            
            // Mostrar valor de meta (4 dígitos)
            DIGITO((meta.valor / 1000) + '0');
            DIGITO(((meta.valor / 100) % 10) + '0');
            DIGITO(((meta.valor / 10) % 10) + '0');
            DIGITO((meta.valor % 10) + '0');
            
            // Enviar confirmación JSON
            __delay_ms(100); // Pequeña pausa antes de responder
            UART_Escr_String("{\"status\":\"ok\"}\r\n");
            
            // Limpiar buffer completamente para siguiente mensaje
            UART_LimpiaBuffer();
            
            // Pequeña pausa antes de volver a esperar
            __delay_ms(100);
        }
        
        __delay_ms(10); // Pausa pequeña para no saturar el loop
    }
}