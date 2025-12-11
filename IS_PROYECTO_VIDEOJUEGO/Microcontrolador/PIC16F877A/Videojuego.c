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

// ============ DEFINICIONES DE HARDWARE ============
#define SALTA PORTDbits.RD0
#define AGACHA PORTDbits.RD1

// Dimensiones del display
#define COLUMNAS 16
#define FILAS 2

// Caracteres personalizados en CGRAM
#define PERSONAJE 0
#define OBSTACULO 1

// Posición del score
#define SCORE_COL 14

// ============ OPTIMIZACIÓN: BUFFER UART REDUCIDO ============
// Reducido de 32 a 24 bytes - suficiente para JSON pequeño
#define BUFFER_SIZE 16
#define BUFFER_MASK 0x0F
volatile unsigned char uartBuffer[BUFFER_SIZE];
volatile unsigned char bufferWrite = 0;
volatile unsigned char bufferRead = 0;

// ============ ESTRUCTURA OPTIMIZADA DE CONFIGURACIÓN ============
typedef struct {
    unsigned char character[8];
    unsigned char obstacle[8];
    unsigned char goalType;
    unsigned int goalValue;
    unsigned char flags;  // Bit 0:charLoaded, Bit 1:obstLoaded, Bit 2:goalLoaded
} LevelConfig;

LevelConfig nivel;

// ============ ESTRUCTURA OPTIMIZADA DE TELEMETRÍA ============
typedef struct {
    unsigned int obstaclesEsquivados;
    unsigned int tiempoTranscurrido;
    unsigned char flags;  // Bit 0:resultado, Bit 1:partidaActiva
} GameTelemetry;

GameTelemetry telemetria;

// ============ VARIABLES DEL JUEGO - OPTIMIZADAS ============
// Buffer reducido - solo parte visible (13 columnas + 3 para score)
unsigned char displayBuffer[FILAS][13];  // Reducido de 16 a 13

// Variables de estado - empaquetadas
unsigned char Fila_Personaje = 1;
unsigned char Ult_Fila_Personaje = 1;
unsigned char Cont_Obstaculo = 1;
unsigned char puntuacion = 0;
unsigned char semilla = 0;

// Flags combinados en un byte
unsigned char gameFlags = 0;
#define GAME_ACTIVE     0x01  // Bit 0: Activo
#define GAME_INIT       0x02  // Bit 1: gameInitialized

// Variables de tiempo - optimizadas
volatile unsigned char timerTicks = 0;
unsigned char segundosJuego = 0;

// Buffer temporal - reducido de 5 a 4
unsigned char tempBuffer[4];

// ============ MACROS INLINE PARA VELOCIDAD ============
#define SET_FLAG(var, flag) ((var) |= (flag))
#define CLR_FLAG(var, flag) ((var) &= ~(flag))
#define CHK_FLAG(var, flag) ((var) & (flag))

#define IS_GAME_ACTIVE() CHK_FLAG(gameFlags, GAME_ACTIVE)
#define IS_GAME_INIT() CHK_FLAG(gameFlags, GAME_INIT)
#define SET_GAME_ACTIVE() SET_FLAG(gameFlags, GAME_ACTIVE)
#define CLR_GAME_ACTIVE() CLR_FLAG(gameFlags, GAME_ACTIVE)
#define SET_GAME_INIT() SET_FLAG(gameFlags, GAME_INIT)
#define CLR_GAME_INIT() CLR_FLAG(gameFlags, GAME_INIT)

// ============ PROTOTIPOS ============
void E_ENC(void);
void COMANDO(unsigned char valor);
void DIGITO(unsigned char valor);
void LCD_Init(void);
void LCD_Posicion(unsigned char col, unsigned char fila);
void LCD_Escr_String(const char *str);
void LCD_CargarSprites(void);

void UART_Init(void);
void UART_Escr(unsigned char dato);
void UART_Escr_String(const char *str);
unsigned char UART_Disp(void);
unsigned char UART_LeeBuffer(void);
unsigned char buscaChar(unsigned char c);
void UART_LimpiaBuffer(void);

void inicializar_juego(void);
void actualizar_pantalla_rapido(void);
void desplazar_mundo_rapido(void);
void generar_obstaculo(void);
void actualizar_score_rapido(void);
unsigned char random_number(unsigned char max);
void leer_botones_rapido(void);
unsigned char detectar_colision(void);
void evaluar_metas(void);
void inicializar_telemetria(void);
void enviar_telemetria(void);
void Timer1_Init(void);
void mostrar_victoria(void);
void mostrar_derrota(void);

void JSON_Parse(void);
unsigned int strToUInt(unsigned char len);
unsigned char leerDigitos(void);
void enviarConfirmacion(void);
unsigned char validarConfiguracion(void);
void inicializarNivel(void);

// ============ FUNCIONES LCD - OPTIMIZADAS ============
void E_ENC(void) {
    PORTCbits.RC2 = 1;
    __delay_us(100);  // Reducido de 5ms a 100us
    PORTCbits.RC2 = 0;
}

void COMANDO(unsigned char valor) {
    PORTB = valor;
    PORTCbits.RC0 = 0;
    PORTCbits.RC1 = 0;
    E_ENC();
    __delay_us(50);  // Reducido de 5ms a 50us
}

void DIGITO(unsigned char valor) {
    PORTB = valor;
    PORTCbits.RC0 = 1;
    PORTCbits.RC1 = 0;
    E_ENC();
}

void LCD_Init(void) {
    TRISB = 0x00;
    TRISCbits.TRISC0 = 0;
    TRISCbits.TRISC1 = 0;
    TRISCbits.TRISC2 = 0;
    
    __delay_ms(50);
    
    COMANDO(0x38);
    COMANDO(0x0C);
    COMANDO(0x01);
    COMANDO(0x06);
    __delay_ms(2);
}

void LCD_CargarSprites(void) {
    unsigned char i;
    
    // Cargar personaje en CGRAM 0x00
    COMANDO(0x40);
    for(i = 0; i < 8; i++) DIGITO(nivel.character[i]);
    
    // Cargar obstáculo en CGRAM 0x01
    COMANDO(0x48);
    for(i = 0; i < 8; i++) DIGITO(nivel.obstacle[i]);
    
    COMANDO(0x80);
    SET_FLAG(nivel.flags, 0x03);  // Bits 0 y 1
}

void LCD_Posicion(unsigned char col, unsigned char fila) {
    COMANDO((fila == 0) ? (0x80 + col) : (0xC0 + col));
}

void LCD_Escr_String(const char *str) {
    while(*str) DIGITO(*str++);
}

// ============ FUNCIONES UART - OPTIMIZADAS ============
void UART_Init(void) {
    TRISCbits.TRISC6 = 0;
    TRISCbits.TRISC7 = 1;
    
    SPBRG = 25;
    TXSTAbits.BRGH = 1;
    TXSTAbits.SYNC = 0;
    TXSTAbits.TXEN = 1;
    RCSTAbits.SPEN = 1;
    RCSTAbits.CREN = 1;
    
    PIE1bits.RCIE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
}

void UART_Escr(unsigned char dato) {
    while(!TXSTAbits.TRMT);
    TXREG = dato;
}

void UART_Escr_String(const char *str) {
    while(*str) UART_Escr(*str++);
}

// ============ TIMER1 OPTIMIZADO ============
void Timer1_Init(void) {
    T1CONbits.TMR1ON = 0;
    T1CONbits.TMR1CS = 0;
    T1CONbits.T1CKPS0 = 1;
    T1CONbits.T1CKPS1 = 1;
    TMR1 = 0;
    PIE1bits.TMR1IE = 1;
    timerTicks = 0;
    segundosJuego = 0;
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
    
    if(PIR1bits.TMR1IF) {
        PIR1bits.TMR1IF = 0;
        if(++timerTicks >= 125) {
            timerTicks = 0;
            segundosJuego++;
            telemetria.tiempoTranscurrido = segundosJuego;
        }
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
        if(uartBuffer[temp & BUFFER_MASK] == c) return 1;
        temp++;
    }
    return 0;
}

void UART_LimpiaBuffer(void) {
    bufferRead = bufferWrite;
}

// ============ FUNCIONES DE CONVERSIÓN - OPTIMIZADAS ============
unsigned int strToUInt(unsigned char len) {
    unsigned int resultado = 0;
    unsigned char i = 0;
    
    while(i < len && tempBuffer[i] >= '0' && tempBuffer[i] <= '9') {
        resultado = (resultado << 3) + (resultado << 1) + (tempBuffer[i++] - '0');
    }
    return resultado;
}

unsigned char leerDigitos(void) {
    unsigned char c, idx = 0;
    
    do { c = UART_LeeBuffer(); } while(c == ' ' || c == ',');
    
    while(c >= '0' && c <= '9' && idx < 3) {
        tempBuffer[idx++] = c;
        c = UART_LeeBuffer();
    }
    return idx;
}

// ============ PARSEO JSON - OPTIMIZADO ============
void JSON_Parse(void) {
    unsigned char c, i, len;
    
    nivel.flags = 0;
    
    // Buscar inicio
    while(!buscaChar('{'));
    while(UART_LeeBuffer() != '{');
    
    // CHARACTER
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'c') break;
    }
    while(UART_LeeBuffer() != '[');
    
    for(i = 0; i < 8; i++) {
        len = leerDigitos();
        nivel.character[i] = (unsigned char)strToUInt(len);
    }
    SET_FLAG(nivel.flags, 0x01);
    
    // OBSTACLE
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'o') break;
    }
    while(UART_LeeBuffer() != '[');
    
    for(i = 0; i < 8; i++) {
        len = leerDigitos();
        nivel.obstacle[i] = (unsigned char)strToUInt(len);
    }
    SET_FLAG(nivel.flags, 0x02);
    
    // GOALTYPE
    while(1) {
        c = UART_LeeBuffer();
        if(c == 'g' && UART_LeeBuffer() == 'o') break;
    }
    while(UART_LeeBuffer() != ':');
    while(UART_LeeBuffer() != '"');
    nivel.goalType = (UART_LeeBuffer() == 't') ? 0 : 1;
    
    // GOALVALUE
    while(1) {
        c = UART_LeeBuffer();
        if(c == 'g' && UART_LeeBuffer() == 'o') {
            while(UART_LeeBuffer() != ':');
            break;
        }
    }
    nivel.goalValue = strToUInt(leerDigitos());
    SET_FLAG(nivel.flags, 0x04);
}

void enviarConfirmacion(void) {
    UART_Escr_String("{\"status\":\"loaded\",\"character\":\"");
    UART_Escr_String(CHK_FLAG(nivel.flags, 0x01) ? "ok" : "error");
    UART_Escr_String("\",\"obstacle\":\"");
    UART_Escr_String(CHK_FLAG(nivel.flags, 0x02) ? "ok" : "error");
    UART_Escr_String("\",\"goal\":\"");
    UART_Escr_String(CHK_FLAG(nivel.flags, 0x04) ? "ok" : "error");
    UART_Escr_String("\"}\r\n");
}

unsigned char validarConfiguracion(void) {
    return (nivel.flags == 0x07 && nivel.goalValue > 0 && nivel.goalValue < 1000);
}

void inicializarNivel(void) {
    unsigned char i;
    for(i = 0; i < 8; i++) {
        nivel.character[i] = 0;
        nivel.obstacle[i] = 0;
    }
    nivel.goalType = 1;
    nivel.goalValue = 10;
    nivel.flags = 0;
}

// ============ TELEMETRÍA - OPTIMIZADA ============
void inicializar_telemetria(void) {
    telemetria.obstaclesEsquivados = 0;
    telemetria.tiempoTranscurrido = 0;
    telemetria.flags = 0x02;  // partidaActiva = 1
    segundosJuego = 0;
    timerTicks = 0;
    TMR1 = 0;
    T1CONbits.TMR1ON = 1;
}

void enviar_telemetria(void) {
    unsigned char buffer[4], idx, temp;
    unsigned int val;
    
    UART_Escr_String("{\"obstacles\":");
    
    val = telemetria.obstaclesEsquivados;
    idx = 0;
    if(val == 0) {
        buffer[idx++] = '0';
    } else {
        while(val > 0) {
            buffer[idx++] = (val % 10) + '0';
            val /= 10;
        }
        // Invertir
        temp = 0;
        while(temp < idx/2) {
            unsigned char t = buffer[temp];
            buffer[temp] = buffer[idx-1-temp];
            buffer[idx-1-temp] = t;
            temp++;
        }
    }
    for(temp = 0; temp < idx; temp++) UART_Escr(buffer[temp]);
    
    UART_Escr_String(",\"time\":");
    
    val = telemetria.tiempoTranscurrido;
    idx = 0;
    if(val == 0) {
        buffer[idx++] = '0';
    } else {
        while(val > 0) {
            buffer[idx++] = (val % 10) + '0';
            val /= 10;
        }
        temp = 0;
        while(temp < idx/2) {
            unsigned char t = buffer[temp];
            buffer[temp] = buffer[idx-1-temp];
            buffer[idx-1-temp] = t;
            temp++;
        }
    }
    for(temp = 0; temp < idx; temp++) UART_Escr(buffer[temp]);
    
    UART_Escr_String(",\"result\":\"");
    UART_Escr_String(CHK_FLAG(telemetria.flags, 0x01) ? "win" : "lose");
    UART_Escr_String("\"}\r\n");
    
    T1CONbits.TMR1ON = 0;
    CLR_FLAG(telemetria.flags, 0x02);
}

// ============ PANTALLAS - OPTIMIZADAS ============
void mostrar_victoria(void) {
    unsigned char i;
    
    COMANDO(0x01);
    __delay_ms(2);
    
    LCD_Posicion(4, 0);
    LCD_Escr_String("YOU WIN!");
    
    LCD_Posicion(0, 1);
    if(nivel.goalType == 1) {
        LCD_Escr_String("Obst:");
        if(telemetria.obstaclesEsquivados >= 10)
            DIGITO('0' + (telemetria.obstaclesEsquivados / 10));
        DIGITO('0' + (telemetria.obstaclesEsquivados % 10));
        LCD_Escr_String(" T:");
        if(telemetria.tiempoTranscurrido >= 10)
            DIGITO('0' + (telemetria.tiempoTranscurrido / 10));
        DIGITO('0' + (telemetria.tiempoTranscurrido % 10));
        DIGITO('s');
    } else {
        LCD_Escr_String("Tiempo:");
        if(telemetria.tiempoTranscurrido >= 10)
            DIGITO('0' + (telemetria.tiempoTranscurrido / 10));
        DIGITO('0' + (telemetria.tiempoTranscurrido % 10));
        DIGITO('s');
    }
    
    for(i = 0; i < 3; i++) {
        __delay_ms(500);
        COMANDO(0x08);
        __delay_ms(300);
        COMANDO(0x0C);
    }
    __delay_ms(2000);
}

void mostrar_derrota(void) {
    unsigned char i;
    
    COMANDO(0x01);
    __delay_ms(2);
    
    LCD_Posicion(3, 0);
    LCD_Escr_String("GAME OVER");
    
    LCD_Posicion(0, 1);
    LCD_Escr_String("O:");
    if(telemetria.obstaclesEsquivados >= 10)
        DIGITO('0' + (telemetria.obstaclesEsquivados / 10));
    DIGITO('0' + (telemetria.obstaclesEsquivados % 10));
    
    LCD_Escr_String(" T:");
    if(telemetria.tiempoTranscurrido >= 10)
        DIGITO('0' + (telemetria.tiempoTranscurrido / 10));
    DIGITO('0' + (telemetria.tiempoTranscurrido % 10));
    DIGITO('s');
    
    if(nivel.goalType == 1) {
        LCD_Escr_String(" /");
        if(nivel.goalValue >= 10)
            DIGITO('0' + (nivel.goalValue / 10));
        DIGITO('0' + (nivel.goalValue % 10));
    }
    
    for(i = 0; i < 5; i++) {
        __delay_ms(200);
        COMANDO(0x08);
        __delay_ms(150);
        COMANDO(0x0C);
    }
    __delay_ms(2000);
}

// ============ FUNCIONES DEL JUEGO - ULTRA OPTIMIZADAS ============
void inicializar_juego(void) {
    unsigned char fil, col;
    
    COMANDO(0x01);
    __delay_ms(2);
    
    LCD_CargarSprites();
    
    for(fil = 0; fil < FILAS; fil++)
        for(col = 0; col < 13; col++)
            displayBuffer[fil][col] = ' ';
    
    Fila_Personaje = 1;
    Ult_Fila_Personaje = 1;
    Cont_Obstaculo = 0;
    puntuacion = 0;
    
    displayBuffer[1][0] = PERSONAJE;
    
    SET_FLAG(gameFlags, GAME_ACTIVE | GAME_INIT);
    
    inicializar_telemetria();
}

void leer_botones_rapido(void) {
    if(SALTA && Fila_Personaje) {
        displayBuffer[Ult_Fila_Personaje][0] = ' ';
        Fila_Personaje = 0;
        Ult_Fila_Personaje = 0;
        displayBuffer[0][0] = PERSONAJE;
        __delay_ms(15);  // Antirebote reducido
    }
    else if(AGACHA && !Fila_Personaje) {
        displayBuffer[0][0] = ' ';
        Fila_Personaje = 1;
        Ult_Fila_Personaje = 1;
        displayBuffer[1][0] = PERSONAJE;
        __delay_ms(15);
    }
}

void generar_obstaculo(void) {
    if(displayBuffer[0][12] == ' ' && displayBuffer[1][12] == ' ') {
        displayBuffer[random_number(2)][12] = OBSTACULO;
    }
}

void desplazar_mundo_rapido(void) {
    unsigned char fil, col;
    for(fil = 0; fil < FILAS; fil++) {
        for(col = 0; col < 12; col++)
            displayBuffer[fil][col] = displayBuffer[fil][col + 1];
        displayBuffer[fil][12] = ' ';
    }
}

void actualizar_pantalla_rapido(void) {
    unsigned char fil, col;
    for(fil = 0; fil < FILAS; fil++) {
        LCD_Posicion(0, fil);
        for(col = 0; col < 13; col++)
            DIGITO(displayBuffer[fil][col]);
    }
}

void actualizar_score_rapido(void) {
    LCD_Posicion(SCORE_COL, 0);
    DIGITO('0' + (puntuacion / 10));
    DIGITO('0' + (puntuacion % 10));
}

unsigned char random_number(unsigned char max) {
    semilla += TMR0;
    semilla = (semilla * 13 + 17) & 0xFF;
    return semilla % max;
}

unsigned char detectar_colision(void) {
    return (displayBuffer[Fila_Personaje][0] == OBSTACULO);
}

void evaluar_metas(void) {
    if(nivel.goalType == 1) {
        if(telemetria.obstaclesEsquivados >= nivel.goalValue) {
            CLR_GAME_ACTIVE();
            SET_FLAG(telemetria.flags, 0x01);
            mostrar_victoria();
            enviar_telemetria();
            CLR_GAME_INIT();
            LCD_Posicion(0, 0);
            LCD_Escr_String("Esperando");
            LCD_Posicion(0, 1);
            LCD_Escr_String("config...");
        }
    } else {
        if(telemetria.tiempoTranscurrido >= nivel.goalValue) {
            CLR_GAME_ACTIVE();
            SET_FLAG(telemetria.flags, 0x01);
            mostrar_victoria();
            enviar_telemetria();
            CLR_GAME_INIT();
            LCD_Posicion(0, 0);
            LCD_Escr_String("Esperando");
            LCD_Posicion(0, 1);
            LCD_Escr_String("config...");
        }
    }
}

// ============ FUNCIÓN PRINCIPAL - OPTIMIZADA ============
void main(void) {
    // Configuración rápida
    TRISB = 0x00;
    TRISC = 0x00;
    TRISD = 0x03;
    PORTB = PORTC = PORTD = 0x00;
    
    OPTION_REGbits.T0CS = 1;
    OPTION_REGbits.PSA = 0;
    OPTION_REGbits.PS = 0b111;
    
    ADCON1 = 0x07;
    CMCON = 0x07;
    
    UART_Init();
    LCD_Init();
    Timer1_Init();
    inicializarNivel();
    
    semilla = TMR0;
    gameFlags = 0;
    
    LCD_Posicion(0, 0);
    LCD_Escr_String("Esperando");
    LCD_Posicion(0, 1);
    LCD_Escr_String("config...");
    
    while(1) {
        // Esperar configuración
        if(buscaChar('}') && !IS_GAME_INIT()) {
            __delay_ms(50);
            JSON_Parse();
            
            if(validarConfiguracion()) {
                LCD_CargarSprites();
                enviarConfirmacion();
                inicializar_juego();
                UART_LimpiaBuffer();
            }
        }
        
        // Loop del juego optimizado
        if(IS_GAME_INIT() && IS_GAME_ACTIVE()) {
            leer_botones_rapido();
            
            if(++Cont_Obstaculo >= 4) {
                Cont_Obstaculo = 0;
                generar_obstaculo();
            }
            
            desplazar_mundo_rapido();
            actualizar_pantalla_rapido();
            
            static unsigned char lastScore = 0;
            if(puntuacion != lastScore) {
                actualizar_score_rapido();
                lastScore = puntuacion;
            }
            
            if(detectar_colision()) {
                CLR_GAME_ACTIVE();
                CLR_FLAG(telemetria.flags, 0x01);
                mostrar_derrota();
                enviar_telemetria();
                CLR_GAME_INIT();
                LCD_Posicion(0, 0);
                LCD_Escr_String("Esperando");
                LCD_Posicion(0, 1);
                LCD_Escr_String("config...");
            }
            
            if(displayBuffer[0][0] == OBSTACULO || displayBuffer[1][0] == OBSTACULO) {
                puntuacion++;
                telemetria.obstaclesEsquivados++;
            }
            
            evaluar_metas();
            
            __delay_ms(100);  // Velocidad mejorada
        }
        
        __delay_ms(5);
    }
}