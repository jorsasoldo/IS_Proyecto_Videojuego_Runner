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
#define SALTA PORTDbits.RD0      // Botón para saltar
#define AGACHA PORTDbits.RD1     // Botón para agacharse

// Dimensiones del display
#define COLUMNAS 16
#define FILAS 2

// Caracteres personalizados en CGRAM
#define PERSONAJE 0
#define OBSTACULO 1

// Posición del score en pantalla
#define SCORE_COL 14

// ============ BUFFER UART OPTIMIZADO ============
#define BUFFER_SIZE 32
#define BUFFER_MASK 0x1F
volatile unsigned char uartBuffer[BUFFER_SIZE];
volatile unsigned char bufferWrite = 0;
volatile unsigned char bufferRead = 0;

// ============ ESTRUCTURA DE CONFIGURACIÓN DE NIVEL ============
typedef struct {
    unsigned char character[8];     // Sprite del personaje
    unsigned char obstacle[8];      // Sprite del obstáculo
    unsigned char goalType;         // 0=tiempo, 1=obstáculos
    unsigned int goalValue;         // Valor objetivo (0-9999)
    unsigned char characterLoaded;  // Flag de validación
    unsigned char obstacleLoaded;   // Flag de validación
    unsigned char goalLoaded;       // Flag de validación
} LevelConfig;

LevelConfig nivel;

// ============ ESTRUCTURA DE TELEMETRÍA ============
typedef struct {
    unsigned int obstaclesEsquivados;  // Total de obstáculos esquivados
    unsigned int tiempoTranscurrido;   // Tiempo en segundos (aproximado)
    unsigned char resultado;           // 0=lose, 1=win
    unsigned char partidaActiva;       // Flag de partida en curso
} GameTelemetry;

GameTelemetry telemetria;

// ============ VARIABLES DEL JUEGO ============
// Buffer de pantalla
unsigned char displayBuffer[FILAS][COLUMNAS];

// Variables de estado del juego
unsigned char Fila_Personaje = 1;        // 0 = arriba, 1 = abajo
unsigned char Columna_Personaje = 0;     // Columna fija del jugador
unsigned char Ult_Fila_Personaje = 1;    // Última fila del jugador
unsigned char Cont_Frame = 0;            // Contador de frames
unsigned char Activo = 1;                // Juego activo
unsigned char Cont_Obstaculo = 1;        // Contador de obstáculos
unsigned char puntuacion = 0;            // Puntuación actual
unsigned char semilla = 0;               // Semilla para aleatorios
unsigned char gameInitialized = 0;       // Flag de inicialización

// Variables para tiempo
volatile unsigned int timerTicks = 0;    // Contador de ticks del timer
unsigned int segundosJuego = 0;          // Segundos transcurridos

// Buffer temporal multiuso
#define TEMP_BUFFER_SIZE 5
unsigned char tempBuffer[TEMP_BUFFER_SIZE];

// ============ PROTOTIPOS DE FUNCIONES ============
// LCD
void E_ENC(void);
void COMANDO(unsigned char valor);
void DIGITO(unsigned char valor);
void LCD_Init(void);
void LCD_Posicion(unsigned char col, unsigned char fila);
void LCD_Escr_String(const char *str);
unsigned char LCD_CargarPersonaje(void);
unsigned char LCD_CargarObstaculo(void);
void LCD_CargarTodosSprites(void);

// UART
void UART_Init(void);
void UART_Escr(unsigned char dato);
void UART_Escr_String(const char *str);
unsigned char UART_Disp(void);
unsigned char UART_LeeBuffer(void);
unsigned char buscaChar(unsigned char c);
void UART_LimpiaBuffer(void);

// Juego
void inicializar_juego(void);
void reiniciar_juego(void);
void actualizar_pantalla(void);
void desplazar_mundo(void);
void generar_obstaculo(void);
void actualizar_score(void);
unsigned char random_number(unsigned char max);
void leer_botones(void);
unsigned char detectar_colision(void);
void evaluar_metas(void);
void inicializar_telemetria(void);
void enviar_telemetria(void);
void Timer1_Init(void);
void actualizar_tiempo(void);

// Parseo JSON
void JSON_Parse(void);
unsigned int strToUInt(unsigned char len);
unsigned char leerDigitos(void);
void enviarConfirmacion(void);
unsigned char validarConfiguracion(void);
void inicializarNivel(void);
void limpiarRecursos(void);

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

// ============ ESCRITURA EN CGRAM ============
unsigned char LCD_CargarPersonaje(void) {
    unsigned char i;
    unsigned char addr = 0x40;
    
    COMANDO(addr);
    __delay_us(100);
    
    for(i = 0; i < 8; i++) {
        DIGITO(nivel.character[i]);
        __delay_us(50);
    }
    
    COMANDO(0x80);
    __delay_us(100);
    
    nivel.characterLoaded = 1;
    return 1;
}

unsigned char LCD_CargarObstaculo(void) {
    unsigned char i;
    unsigned char addr = 0x40 | (1 << 3);
    
    COMANDO(addr);
    __delay_us(100);
    
    for(i = 0; i < 8; i++) {
        DIGITO(nivel.obstacle[i]);
        __delay_us(50);
    }
    
    COMANDO(0x80);
    __delay_us(100);
    
    nivel.obstacleLoaded = 1;
    return 1;
}

void LCD_CargarTodosSprites(void) {
    LCD_CargarPersonaje();
    LCD_CargarObstaculo();
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

// ============ TIMER1 PARA MEDICIÓN DE TIEMPO ============
void Timer1_Init(void) {
    T1CONbits.TMR1ON = 0;     // Timer1 OFF inicialmente
    T1CONbits.TMR1CS = 0;     // Reloj interno (FOSC/4)
    T1CONbits.T1CKPS0 = 1;    // Prescaler 1:8
    T1CONbits.T1CKPS1 = 1;
    TMR1 = 0;                 // Limpiar contador
    PIE1bits.TMR1IE = 1;      // Habilitar interrupción Timer1
    timerTicks = 0;
    segundosJuego = 0;
}

void actualizar_tiempo(void) {
    // Aproximación: cada 125000 ticks = 1 segundo con prescaler 1:8 a 4MHz
    // (4MHz / 4 / 8) = 125kHz → 125000 ticks = 1 seg
    if(timerTicks >= 125) {  // Ajustado para desbordamientos de Timer1
        timerTicks = 0;
        segundosJuego++;
        telemetria.tiempoTranscurrido = segundosJuego;
    }
}

void __interrupt() ISR(void) {
    // Interrupción UART
    if(PIR1bits.RCIF) {
        if(RCSTAbits.OERR) {
            RCSTAbits.CREN = 0;
            RCSTAbits.CREN = 1;
        }
        uartBuffer[bufferWrite & BUFFER_MASK] = RCREG;
        bufferWrite++;
    }
    
    // Interrupción Timer1 para tiempo
    if(PIR1bits.TMR1IF) {
        PIR1bits.TMR1IF = 0;
        timerTicks++;
        actualizar_tiempo();
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

void UART_LimpiaBuffer(void) {
    bufferRead = bufferWrite;
}

// ============ FUNCIONES DE CONVERSIÓN ============
unsigned int strToUInt(unsigned char len) {
    unsigned int resultado = 0;
    unsigned char i;
    
    for(i = 0; i < len && tempBuffer[i] != '\0'; i++) {
        resultado = (resultado << 3) + (resultado << 1) + (tempBuffer[i] - '0');
    }
    
    return resultado;
}

unsigned char leerDigitos(void) {
    unsigned char c, idx = 0;
    
    do {
        c = UART_LeeBuffer();
    } while(c == ' ' || c == ',');
    
    while(c >= '0' && c <= '9' && idx < TEMP_BUFFER_SIZE - 1) {
        tempBuffer[idx++] = c;
        c = UART_LeeBuffer();
    }
    
    tempBuffer[idx] = '\0';
    return idx;
}

// ============ PARSEO JSON ============
void JSON_Parse(void) {
    unsigned char c, i, len;
    unsigned int intentos = 0;
    
    nivel.characterLoaded = 0;
    nivel.obstacleLoaded = 0;
    nivel.goalLoaded = 0;
    
    while(!buscaChar('{') && intentos < 1000) {
        __delay_ms(10);
        intentos++;
    }
    
    if(intentos >= 1000) return;
    
    while(UART_LeeBuffer() != '{');
    
    // Parseo CHARACTER
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'c')
            break;
    }
    while(UART_LeeBuffer() != '[');
    
    for(i = 0; i < 8; i++) {
        len = leerDigitos();
        nivel.character[i] = (unsigned char)strToUInt(len);
    }
    nivel.characterLoaded = 1;
    
    // Parseo OBSTACLE
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'o')
            break;
    }
    while(UART_LeeBuffer() != '[');
    
    for(i = 0; i < 8; i++) {
        len = leerDigitos();
        nivel.obstacle[i] = (unsigned char)strToUInt(len);
    }
    nivel.obstacleLoaded = 1;
    
    // Parseo GOALTYPE
    while(1) {
        c = UART_LeeBuffer();
        if(c == 'g') {
            c = UART_LeeBuffer();
            if(c == 'o')
                break;
        }
    }
    
    while(UART_LeeBuffer() != ':');
    while(UART_LeeBuffer() != '"');
    
    c = UART_LeeBuffer();
    nivel.goalType = (c == 't') ? 0 : 1;
    
    // Parseo GOALVALUE
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
    
    len = leerDigitos();
    nivel.goalValue = strToUInt(len);
    nivel.goalLoaded = 1;
}

// ============ CONFIRMACIÓN DE CARGA ============
void enviarConfirmacion(void) {
    UART_Escr_String("{\"status\":\"loaded\"");
    
    UART_Escr_String(",\"character\":\"");
    UART_Escr_String(nivel.characterLoaded ? "ok" : "error");
    UART_Escr_String("\"");
    
    UART_Escr_String(",\"obstacle\":\"");
    UART_Escr_String(nivel.obstacleLoaded ? "ok" : "error");
    UART_Escr_String("\"");
    
    UART_Escr_String(",\"goal\":\"");
    UART_Escr_String(nivel.goalLoaded ? "ok" : "error");
    UART_Escr_String("\"");
    
    UART_Escr_String(",\"goalType\":");
    UART_Escr((nivel.goalType == 0) ? '0' : '1');
    
    UART_Escr_String(",\"goalValue\":");
    {
        unsigned char buffer[6];
        unsigned char idx = 0;
        unsigned int val = nivel.goalValue;
        unsigned char temp;
        
        if(val == 0) {
            buffer[idx++] = '0';
        } else {
            unsigned char start = idx;
            while(val > 0) {
                buffer[idx++] = (val % 10) + '0';
                val /= 10;
            }
            unsigned char end = idx - 1;
            while(start < end) {
                temp = buffer[start];
                buffer[start] = buffer[end];
                buffer[end] = temp;
                start++;
                end--;
            }
        }
        buffer[idx] = '\0';
        
        for(temp = 0; temp < idx; temp++) {
            UART_Escr(buffer[temp]);
        }
    }
    
    UART_Escr_String("}\r\n");
}

// ============ TELEMETRÍA DEL JUEGO ============
void inicializar_telemetria(void) {
    telemetria.obstaclesEsquivados = 0;
    telemetria.tiempoTranscurrido = 0;
    telemetria.resultado = 0;  // 0 = lose por defecto
    telemetria.partidaActiva = 1;
    
    // Reiniciar tiempo
    segundosJuego = 0;
    timerTicks = 0;
    TMR1 = 0;
    T1CONbits.TMR1ON = 1;  // Activar Timer1
}

void enviar_telemetria(void) {
    unsigned char buffer[6];
    unsigned char idx, temp;
    unsigned int val;
    
    // Construir JSON de telemetría
    UART_Escr_String("{\"obstacles\":");
    
    // Enviar obstáculos esquivados
    val = telemetria.obstaclesEsquivados;
    idx = 0;
    if(val == 0) {
        buffer[idx++] = '0';
    } else {
        unsigned char start = idx;
        while(val > 0) {
            buffer[idx++] = (val % 10) + '0';
            val /= 10;
        }
        unsigned char end = idx - 1;
        while(start < end) {
            temp = buffer[start];
            buffer[start] = buffer[end];
            buffer[end] = temp;
            start++;
            end--;
        }
    }
    buffer[idx] = '\0';
    for(temp = 0; temp < idx; temp++) {
        UART_Escr(buffer[temp]);
    }
    
    UART_Escr_String(",\"time\":");
    
    // Enviar tiempo transcurrido
    val = telemetria.tiempoTranscurrido;
    idx = 0;
    if(val == 0) {
        buffer[idx++] = '0';
    } else {
        unsigned char start = idx;
        while(val > 0) {
            buffer[idx++] = (val % 10) + '0';
            val /= 10;
        }
        unsigned char end = idx - 1;
        while(start < end) {
            temp = buffer[start];
            buffer[start] = buffer[end];
            buffer[end] = temp;
            start++;
            end--;
        }
    }
    buffer[idx] = '\0';
    for(temp = 0; temp < idx; temp++) {
        UART_Escr(buffer[temp]);
    }
    
    UART_Escr_String(",\"result\":\"");
    
    // Enviar resultado
    if(telemetria.resultado == 1) {
        UART_Escr_String("win");
    } else {
        UART_Escr_String("lose");
    }
    
    UART_Escr_String("\"}\r\n");
    
    // Detener Timer1
    T1CONbits.TMR1ON = 0;
    telemetria.partidaActiva = 0;
}

void limpiarRecursos(void) {
    unsigned char i;
    
    UART_LimpiaBuffer();
    
    for(i = 0; i < TEMP_BUFFER_SIZE; i++) {
        tempBuffer[i] = 0;
    }
    
    if(RCSTAbits.OERR) {
        RCSTAbits.CREN = 0;
        RCSTAbits.CREN = 1;
    }
    
    while(PIR1bits.RCIF) {
        i = RCREG;
    }
}

unsigned char validarConfiguracion(void) {
    if(!nivel.characterLoaded || !nivel.obstacleLoaded || !nivel.goalLoaded) {
        return 0;
    }
    
    if(nivel.goalValue == 0 || nivel.goalValue > 9999) {
        return 0;
    }
    
    if(nivel.goalType > 1) {
        return 0;
    }
    
    return 1;
}

void inicializarNivel(void) {
    unsigned char i;
    
    for(i = 0; i < 8; i++) {
        nivel.character[i] = 0;
        nivel.obstacle[i] = 0;
    }
    
    nivel.goalType = 1;
    nivel.goalValue = 10;
    nivel.characterLoaded = 0;
    nivel.obstacleLoaded = 0;
    nivel.goalLoaded = 0;
}

// ============ FUNCIONES DEL JUEGO ============

void inicializar_juego(void) {
    unsigned char fil, col;
    
    COMANDO(0x01);
    __delay_ms(2);
    
    LCD_CargarTodosSprites();
    
    for(fil = 0; fil < FILAS; fil++) {
        for(col = 0; col < COLUMNAS; col++) {
            displayBuffer[fil][col] = ' ';
        }
    }
    
    Fila_Personaje = 1;
    Ult_Fila_Personaje = 1;
    Activo = 1;
    Cont_Obstaculo = 0;
    puntuacion = 0;
    
    displayBuffer[Fila_Personaje][Columna_Personaje] = PERSONAJE;
    
    actualizar_pantalla();
    actualizar_score();
    
    // Inicializar telemetría
    inicializar_telemetria();
    
    gameInitialized = 1;
    
    UART_Escr_String("Juego inicializado\r\n");
}

void reiniciar_juego(void) {
    unsigned char fil, col;
    
    for(fil = 0; fil < FILAS; fil++) {
        for(col = 0; col < COLUMNAS; col++) {
            displayBuffer[fil][col] = ' ';
        }
    }
    
    Fila_Personaje = 1;
    Ult_Fila_Personaje = 1;
    Activo = 1;
    Cont_Obstaculo = 0;
    puntuacion = 0;
    
    displayBuffer[Fila_Personaje][Columna_Personaje] = PERSONAJE;
    
    actualizar_pantalla();
    actualizar_score();
    
    // Reiniciar telemetría
    inicializar_telemetria();
}

void leer_botones(void) {
    if(SALTA == 1 && Fila_Personaje != 0) {
        displayBuffer[Ult_Fila_Personaje][Columna_Personaje] = ' ';
        Ult_Fila_Personaje = Fila_Personaje;
        Fila_Personaje = 0;
        displayBuffer[Fila_Personaje][Columna_Personaje] = PERSONAJE;
        __delay_ms(20);
    }
    else if(AGACHA == 1 && Fila_Personaje != 1) {
        displayBuffer[Ult_Fila_Personaje][Columna_Personaje] = ' ';
        Ult_Fila_Personaje = Fila_Personaje;
        Fila_Personaje = 1;
        displayBuffer[Fila_Personaje][Columna_Personaje] = PERSONAJE;
        __delay_ms(20);
    }
}

void generar_obstaculo(void) {
    if(displayBuffer[0][COLUMNAS - 4] == ' ' && displayBuffer[1][COLUMNAS - 4] == ' ') {
        unsigned char fila_obstaculo = random_number(2);
        displayBuffer[fila_obstaculo][COLUMNAS - 4] = OBSTACULO;
    }
}

void desplazar_mundo(void) {
    unsigned char fil, col;
    
    for(fil = 0; fil < FILAS; fil++) {
        for(col = 0; col < COLUMNAS - 4; col++) {
            displayBuffer[fil][col] = displayBuffer[fil][col + 1];
        }
        displayBuffer[fil][COLUMNAS - 4] = ' ';
    }
}

void actualizar_pantalla(void) {
    unsigned char fil, col;
    
    for(fil = 0; fil < FILAS; fil++) {
        LCD_Posicion(0, fil);
        for(col = 0; col < COLUMNAS - 3; col++) {
            DIGITO(displayBuffer[fil][col]);
        }
    }
}

void actualizar_score(void) {
    unsigned char decenas = '0' + (puntuacion / 10);
    unsigned char unidades = '0' + (puntuacion % 10);
    
    LCD_Posicion(SCORE_COL, 0);
    DIGITO(decenas);
    DIGITO(unidades);
}

unsigned char random_number(unsigned char max) {
    semilla += TMR0;
    semilla = (semilla * 13 + 17) % 251;
    return semilla % max;
}

// ============ DETECCIÓN DE COLISIONES ============
unsigned char detectar_colision(void) {
    // Verifica si el personaje está en la misma posición que un obstáculo
    if(displayBuffer[Fila_Personaje][Columna_Personaje] == OBSTACULO) {
        return 1;  // Colisión detectada
    }
    return 0;  // Sin colisión
}

// ============ PANTALLA DE VICTORIA ============
void mostrar_victoria(void) {
    unsigned char i;
    
    // Limpiar LCD
    COMANDO(0x01);
    __delay_ms(2);
    
    // Línea 1: "YOU WIN!"
    LCD_Posicion(4, 0);  // Centrado
    LCD_Escr_String("YOU WIN!");
    
    // Línea 2: Estadísticas según tipo de meta
    LCD_Posicion(0, 1);
    if(nivel.goalType == 1) {
        // Victoria por obstáculos
        LCD_Escr_String("Obst:");
        
        // Convertir y mostrar obstáculos (hasta 999)
        if(telemetria.obstaclesEsquivados >= 100) {
            DIGITO('0' + (telemetria.obstaclesEsquivados / 100));
        }
        if(telemetria.obstaclesEsquivados >= 10) {
            DIGITO('0' + ((telemetria.obstaclesEsquivados / 10) % 10));
        }
        DIGITO('0' + (telemetria.obstaclesEsquivados % 10));
        
        // Mostrar tiempo también
        LCD_Escr_String(" T:");
        if(telemetria.tiempoTranscurrido >= 10) {
            DIGITO('0' + (telemetria.tiempoTranscurrido / 10));
        }
        DIGITO('0' + (telemetria.tiempoTranscurrido % 10));
        DIGITO('s');
    } else {
        // Victoria por tiempo
        LCD_Escr_String("Tiempo:");
        
        if(telemetria.tiempoTranscurrido >= 100) {
            DIGITO('0' + (telemetria.tiempoTranscurrido / 100));
        }
        if(telemetria.tiempoTranscurrido >= 10) {
            DIGITO('0' + ((telemetria.tiempoTranscurrido / 10) % 10));
        }
        DIGITO('0' + (telemetria.tiempoTranscurrido % 10));
        DIGITO('s');
    }
    
    // Efecto de parpadeo (3 veces)
    for(i = 0; i < 3; i++) {
        __delay_ms(500);
        COMANDO(0x08);  // Display OFF
        __delay_ms(300);
        COMANDO(0x0C);  // Display ON
    }
    
    __delay_ms(2000);
}

// ============ PANTALLA DE DERROTA ============
void mostrar_derrota(void) {
    unsigned char i;
    
    // Limpiar LCD
    COMANDO(0x01);
    __delay_ms(2);
    
    // Línea 1: "GAME OVER"
    LCD_Posicion(3, 0);  // Centrado
    LCD_Escr_String("GAME OVER");
    
    // Línea 2: Estadísticas
    LCD_Posicion(0, 1);
    LCD_Escr_String("O:");
    
    // Mostrar obstáculos esquivados
    if(telemetria.obstaclesEsquivados >= 10) {
        DIGITO('0' + (telemetria.obstaclesEsquivados / 10));
    }
    DIGITO('0' + (telemetria.obstaclesEsquivados % 10));
    
    // Mostrar tiempo
    LCD_Escr_String(" T:");
    if(telemetria.tiempoTranscurrido >= 10) {
        DIGITO('0' + (telemetria.tiempoTranscurrido / 10));
    }
    DIGITO('0' + (telemetria.tiempoTranscurrido % 10));
    DIGITO('s');
    
    // Mostrar meta si es por obstáculos
    if(nivel.goalType == 1) {
        LCD_Escr_String(" /");
        if(nivel.goalValue >= 10) {
            DIGITO('0' + (nivel.goalValue / 10));
        }
        DIGITO('0' + (nivel.goalValue % 10));
    }
    
    // Efecto de parpadeo rápido (5 veces)
    for(i = 0; i < 5; i++) {
        __delay_ms(200);
        COMANDO(0x08);  // Display OFF
        __delay_ms(150);
        COMANDO(0x0C);  // Display ON
    }
    
    __delay_ms(2000);
}

// ============ EVALUACIÓN DE METAS ============
void evaluar_metas(void) {
    // Meta tipo 1: Obstáculos esquivados
    if(nivel.goalType == 1) {
        if(telemetria.obstaclesEsquivados >= nivel.goalValue) {
            Activo = 0;
            telemetria.resultado = 1;  // Win
            
            // Mostrar pantalla de victoria
            mostrar_victoria();
            
            // Enviar telemetría
            enviar_telemetria();
            
            gameInitialized = 0;
            
            LCD_Posicion(0, 0);
            LCD_Escr_String("Esperando");
            LCD_Posicion(0, 1);
            LCD_Escr_String("config...");
        }
    }
    // Meta tipo 0: Tiempo sobrevivido
    else if(nivel.goalType == 0) {
        if(telemetria.tiempoTranscurrido >= nivel.goalValue) {
            Activo = 0;
            telemetria.resultado = 1;  // Win
            
            // Mostrar pantalla de victoria
            mostrar_victoria();
            
            // Enviar telemetría
            enviar_telemetria();
            
            gameInitialized = 0;
            
            LCD_Posicion(0, 0);
            LCD_Escr_String("Esperando");
            LCD_Posicion(0, 1);
            LCD_Escr_String("config...");
        }
    }
}

// ============ FUNCIÓN PRINCIPAL ============
void main(void) {
    unsigned char i;
    
    // Configuración de puertos
    TRISB = 0x00;
    TRISC = 0x00;
    TRISD = 0b00000011;
    
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
    
    // Configurar Timer0 para aleatorios
    OPTION_REGbits.T0CS = 1;
    OPTION_REGbits.PSA = 0;
    OPTION_REGbits.PS = 0b111;
    
    ADCON1 = 0x07;
    CMCON = 0x07;
    
    // Inicializar subsistemas
    UART_Init();
    LCD_Init();
    Timer1_Init();
    inicializarNivel();
    limpiarRecursos();
    
    TMR0 = 0;
    semilla = TMR0;
    
    UART_Escr_String("Sistema listo\r\n");
    
    LCD_Posicion(0, 0);
    LCD_Escr_String("Esperando");
    LCD_Posicion(0, 1);
    LCD_Escr_String("config...");
    
    // ===== LOOP PRINCIPAL =====
    while(1) {
        // ===== FASE 1: ESPERAR CONFIGURACIÓN =====
        if(buscaChar('}') && !gameInitialized) {
            __delay_ms(100);
            
            JSON_Parse();
            
            if(validarConfiguracion()) {
                LCD_CargarTodosSprites();
                __delay_ms(100);
                enviarConfirmacion();
                inicializar_juego();
                limpiarRecursos();
            } else {
                UART_Escr_String("{\"status\":\"error\",\"message\":\"invalid config\"}\r\n");
            }
            
            __delay_ms(100);
        }
        
        // ===== FASE 2: LOOP DEL JUEGO =====
        if(gameInitialized && Activo) {
            // Leer botones
            leer_botones();
            
            // Actualizar pantalla
            actualizar_pantalla();
            
            // Generar obstáculos
            if(++Cont_Obstaculo >= 4) {
                Cont_Obstaculo = 0;
                generar_obstaculo();
            }
            
            // Desplazar mundo
            desplazar_mundo();
            
            // Actualizar pantalla
            actualizar_pantalla();
            
            // Actualizar score si cambia
            static unsigned char Ult_Puntuacion = 0;
            if(puntuacion != Ult_Puntuacion) {
                actualizar_score();
                Ult_Puntuacion = puntuacion;
            }
            
            // ===== DETECCIÓN DE COLISIONES =====
            if(detectar_colision()) {
                Activo = 0;
                telemetria.resultado = 0;  // Lose
                
                // Mostrar pantalla de derrota
                mostrar_derrota();
                
                // Enviar telemetría de pérdida
                enviar_telemetria();
                
                gameInitialized = 0;
                
                LCD_Posicion(0, 0);
                LCD_Escr_String("Esperando");
                LCD_Posicion(0, 1);
                LCD_Escr_String("config...");
            }
            
            // ===== INCREMENTAR OBSTÁCULOS ESQUIVADOS =====
            // Cuando un obstáculo pasa por la columna 0 (donde está el personaje)
            // y no hubo colisión, se cuenta como esquivado
            if(displayBuffer[1][0] == OBSTACULO || displayBuffer[0][0] == OBSTACULO) {
                puntuacion++;
                telemetria.obstaclesEsquivados++;
            }
            
            // ===== EVALUACIÓN DE METAS =====
            evaluar_metas();
            
            // Velocidad del juego
            __delay_ms(150);
        }
        
        // Pausa pequeña
        __delay_ms(10);
        
        // Mantenimiento preventivo
        static unsigned char contador = 0;
        if(++contador > 100) {
            contador = 0;
            if(UART_Disp() && !buscaChar('{') && !gameInitialized) {
                limpiarRecursos();
            }
        }
    }
}
