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

// LCD pines
#define LCD_DATA PORTB
#define LCD_DATA_TRIS TRISB
#define LCD_RS PORTCbits.RC0
#define LCD_RW PORTCbits.RC1
#define LCD_E PORTCbits.RC2
#define LCD_RS_TRIS TRISCbits.TRISC0
#define LCD_RW_TRIS TRISCbits.TRISC1
#define LCD_E_TRIS TRISCbits.TRISC2

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
void LCD_Pulso(void) {
    LCD_E = 1;
    __delay_us(1);
    LCD_E = 0;
    __delay_us(50);
}

void LCD_Cmd(unsigned char cmd) {
    LCD_RS = 0;
    LCD_RW = 0;
    LCD_DATA = cmd;
    LCD_Pulso();
    __delay_ms(2);
}

void LCD_Dato(unsigned char dato) {
    LCD_RS = 1;
    LCD_RW = 0;
    LCD_DATA = dato;
    LCD_Pulso();
    __delay_us(50);
}

void LCD_Init(void) {
    LCD_DATA_TRIS = 0x00;  // Puerto B como salida
    LCD_RS_TRIS = 0;
    LCD_RW_TRIS = 0;
    LCD_E_TRIS = 0;
    
    LCD_RS = 0;
    LCD_RW = 0;
    LCD_E = 0;
    
    __delay_ms(20);
    
    LCD_Cmd(0x38);  // 8 bits, 2 líneas, 5x8
    LCD_Cmd(0x0C);  // Display ON, cursor OFF
    LCD_Cmd(0x06);  // Incrementar cursor
    LCD_Cmd(0x01);  // Limpiar display
    __delay_ms(2);
}

void LCD_CargarCGRAM(unsigned char posicion, unsigned char *sprite) {
    unsigned char i;
    unsigned char addr;
    
    // Dirección CGRAM: posición 0-7, cada una ocupa 8 bytes
    addr = 0x40 | ((posicion & 0x07) << 3);
    LCD_Cmd(addr);
    
    // Escribir 8 bytes en CGRAM
    for(i = 0; i < 8; i++) {
        LCD_Dato(sprite[i]);
    }
    
    // Regresar a DDRAM
    LCD_Cmd(0x80);
}

void LCD_MostrarChar(unsigned char fila, unsigned char col, unsigned char caracter) {
    unsigned char addr;
    
    if(fila == 0)
        addr = 0x80 + col;
    else
        addr = 0xC0 + col;
    
    LCD_Cmd(addr);
    LCD_Dato(caracter);
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

// ============ PARSEO JSON OPTIMIZADO ============
void JSON_Parse(void) {
    unsigned char c, i, temp[4], tempIdx;
    
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
    
    // Busca "goalType":
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'g')
            break;
    }
    while(UART_LeeBuffer() != ':');
    while(UART_LeeBuffer() != '"');
    
    // Lee tipo de meta: "time", "score" o "distance"
    c = UART_LeeBuffer();
    if(c == 't') {
        meta.tipo = 0;  // tiempo
    } else if(c == 's') {
        meta.tipo = 1;  // puntaje (score)
    } else if(c == 'd') {
        meta.tipo = 2;  // distancia
    }
    
    // Salta hasta "goalValue":
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'g')
            break;
    }
    while(UART_LeeBuffer() != ':');
    
    // Lee valor de meta (hasta 4 dígitos)
    tempIdx = 0;
    do {
        c = UART_LeeBuffer();
    } while(c == ' ');
    
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
    meta.tipo = 1;    // Por defecto: puntaje
    meta.valor = 100;
    
    UART_Escr_String("Sistema listo\r\n");
    
    LCD_Cmd(0x01);  // Limpiar LCD
    __delay_ms(2);
    
    while(1) {
        // Espera JSON completo
        if(buscaChar('}')) {
            JSON_Parse();
            
            // Cargar sprites en CGRAM
            LCD_CargarCGRAM(0, character);   // Posición 0
            LCD_CargarCGRAM(1, obstacle);    // Posición 1
            
            // Mostrar sprites en LCD
            LCD_MostrarChar(0, 0, 0);  // Muestra character (pos 0)
            LCD_MostrarChar(0, 2, 1);  // Muestra obstacle (pos 1)
            
            // Mostrar información de la meta en LCD
            LCD_MostrarChar(1, 0, 'M');
            LCD_MostrarChar(1, 1, ':');
            
            // Mostrar tipo de meta
            if(meta.tipo == 0) {
                LCD_MostrarChar(1, 2, 'T');  // Tiempo
            } else if(meta.tipo == 1) {
                LCD_MostrarChar(1, 2, 'P');  // Puntaje
            } else {
                LCD_MostrarChar(1, 2, 'D');  // Distancia
            }
            
            // Mostrar valor de meta (4 dígitos)
            LCD_MostrarChar(1, 4, (meta.valor / 1000) + '0');
            LCD_MostrarChar(1, 5, ((meta.valor / 100) % 10) + '0');
            LCD_MostrarChar(1, 6, ((meta.valor / 10) % 10) + '0');
            LCD_MostrarChar(1, 7, (meta.valor % 10) + '0');
            
            // Enviar confirmación JSON
            UART_Escr_String("{\"status\":\"ok\"}\r\n");
            
            // Limpiar buffer para siguiente mensaje
            bufferRead = bufferWrite;
        }
    }
}