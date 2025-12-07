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
void detectar_colision(void);

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

// ============ ESCRITURA EN CGRAM - PERSONAJE (0x00) ============
unsigned char LCD_CargarPersonaje(void) {
    unsigned char i;
    unsigned char addr = 0x40;  // CGRAM posición 0
    
    COMANDO(addr);
    __delay_us(100);
    
    for(i = 0; i < 8; i++) {
        DIGITO(nivel.character[i]);
        __delay_us(50);
    }
    
    COMANDO(0x80);  // Regresar a DDRAM
    __delay_us(100);
    
    nivel.characterLoaded = 1;
    return 1;
}

// ============ ESCRITURA EN CGRAM - OBSTÁCULO (0x01) ============
unsigned char LCD_CargarObstaculo(void) {
    unsigned char i;
    unsigned char addr = 0x40 | (1 << 3);  // CGRAM posición 1
    
    COMANDO(addr);
    __delay_us(100);
    
    for(i = 0; i < 8; i++) {
        DIGITO(nivel.obstacle[i]);
        __delay_us(50);
    }
    
    COMANDO(0x80);  // Regresar a DDRAM
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

void UART_LimpiaBuffer(void) {
    bufferRead = bufferWrite;
}

// ============ FUNCIONES DE CONVERSIÓN OPTIMIZADAS ============
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

// ============ PARSEO JSON OPTIMIZADO ============
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

// ============ CONFIRMACIÓN DE CARGA EXITOSA ============
void enviarConfirmacion(void) {
    UART_Escr_String("{\"status\":\"loaded\"");
    
    UART_Escr_String(",\"character\":\"");
    if(nivel.characterLoaded) {
        UART_Escr_String("ok");
    } else {
        UART_Escr_String("error");
    }
    UART_Escr_String("\"");
    
    UART_Escr_String(",\"obstacle\":\"");
    if(nivel.obstacleLoaded) {
        UART_Escr_String("ok");
    } else {
        UART_Escr_String("error");
    }
    UART_Escr_String("\"");
    
    UART_Escr_String(",\"goal\":\"");
    if(nivel.goalLoaded) {
        UART_Escr_String("ok");
    } else {
        UART_Escr_String("error");
    }
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

// Inicializa el juego con sprites custom cargados
void inicializar_juego(void) {
    unsigned char fil, col;
    
    // Limpiar LCD
    COMANDO(0x01);
    __delay_ms(2);
    
    // Cargar sprites custom en CGRAM
    LCD_CargarTodosSprites();
    
    // Limpiar buffer de pantalla
    for(fil = 0; fil < FILAS; fil++) {
        for(col = 0; col < COLUMNAS; col++) {
            displayBuffer[fil][col] = ' ';
        }
    }
    
    // Inicializar variables del juego
    Fila_Personaje = 1;
    Ult_Fila_Personaje = 1;
    Activo = 1;
    Cont_Obstaculo = 0;
    puntuacion = 0;
    
    // Colocar personaje en posición inicial
    displayBuffer[Fila_Personaje][Columna_Personaje] = PERSONAJE;
    
    // Actualizar pantalla
    actualizar_pantalla();
    actualizar_score();
    
    gameInitialized = 1;
    
    UART_Escr_String("Juego inicializado\r\n");
}

// Reinicia el juego manteniendo los sprites
void reiniciar_juego(void) {
    unsigned char fil, col;
    
    // Limpiar buffer de pantalla
    for(fil = 0; fil < FILAS; fil++) {
        for(col = 0; col < COLUMNAS; col++) {
            displayBuffer[fil][col] = ' ';
        }
    }
    
    // Reiniciar variables
    Fila_Personaje = 1;
    Ult_Fila_Personaje = 1;
    Activo = 1;
    Cont_Obstaculo = 0;
    puntuacion = 0;
    
    // Colocar personaje
    displayBuffer[Fila_Personaje][Columna_Personaje] = PERSONAJE;
    
    actualizar_pantalla();
    actualizar_score();
}

// Lee los botones y actualiza posición del personaje
void leer_botones(void) {
    // Botón SALTA - mueve a fila superior
    if(SALTA == 1 && Fila_Personaje != 0) {
        // Borrar posición anterior
        displayBuffer[Ult_Fila_Personaje][Columna_Personaje] = ' ';
        
        // Actualizar fila
        Ult_Fila_Personaje = Fila_Personaje;
        Fila_Personaje = 0;
        
        // Colocar en nueva posición
        displayBuffer[Fila_Personaje][Columna_Personaje] = PERSONAJE;
        
        // Antirebote
        __delay_ms(20);
    }
    
    // Botón AGACHA - mueve a fila inferior
    else if(AGACHA == 1 && Fila_Personaje != 1) {
        // Borrar posición anterior
        displayBuffer[Ult_Fila_Personaje][Columna_Personaje] = ' ';
        
        // Actualizar fila
        Ult_Fila_Personaje = Fila_Personaje;
        Fila_Personaje = 1;
        
        // Colocar en nueva posición
        displayBuffer[Fila_Personaje][Columna_Personaje] = PERSONAJE;
        
        // Antirebote
        __delay_ms(20);
    }
}

// Genera obstáculo aleatorio
void generar_obstaculo(void) {
    // Solo genera si no hay obstáculos cerca del borde derecho
    if(displayBuffer[0][COLUMNAS - 4] == ' ' && displayBuffer[1][COLUMNAS - 4] == ' ') {
        unsigned char fila_obstaculo = random_number(2);
        displayBuffer[fila_obstaculo][COLUMNAS - 4] = OBSTACULO;
    }
}

// Desplaza el mundo a la izquierda
void desplazar_mundo(void) {
    unsigned char fil, col;
    
    for(fil = 0; fil < FILAS; fil++) {
        // Desplaza hasta columna 12 (reserva espacio para score)
        for(col = 0; col < COLUMNAS - 4; col++) {
            displayBuffer[fil][col] = displayBuffer[fil][col + 1];
        }
        
        // Limpia la última columna
        displayBuffer[fil][COLUMNAS - 4] = ' ';
    }
}

// Actualiza la pantalla desde el buffer
void actualizar_pantalla(void) {
    unsigned char fil, col;
    
    for(fil = 0; fil < FILAS; fil++) {
        LCD_Posicion(0, fil);
        
        // Actualiza hasta columna 13 (deja espacio para score)
        for(col = 0; col < COLUMNAS - 3; col++) {
            DIGITO(displayBuffer[fil][col]);
        }
    }
}

// Actualiza el score en pantalla
void actualizar_score(void) {
    unsigned char decenas = '0' + (puntuacion / 10);
    unsigned char unidades = '0' + (puntuacion % 10);
    
    LCD_Posicion(SCORE_COL, 0);
    DIGITO(decenas);
    DIGITO(unidades);
}

// Genera número aleatorio
unsigned char random_number(unsigned char max) {
    semilla += TMR0;
    semilla = (semilla * 13 + 17) % 251;
    return semilla % max;
}

// Detecta colisión
void detectar_colision(void) {
    if(displayBuffer[Fila_Personaje][Columna_Personaje] == OBSTACULO) {
        Activo = 0;
        
        COMANDO(0x01);
        __delay_ms(2);
        
        LCD_Posicion(0, 0);
        LCD_Escr_String("GAME OVER!");
        LCD_Posicion(0, 1);
        LCD_Escr_String("Score: ");
        
        unsigned char decenas = '0' + (puntuacion / 10);
        unsigned char unidades = '0' + (puntuacion % 10);
        DIGITO(decenas);
        DIGITO(unidades);
        
        __delay_ms(3000);
        
        // Reiniciar
        reiniciar_juego();
    }
}

// ============ FUNCIÓN PRINCIPAL ============
void main(void) {
    unsigned char i;
    
    // Configuración de puertos
    TRISB = 0x00;
    TRISC = 0x00;
    TRISD = 0b00000011;  // RD0 y RD1 como entradas (botones)
    
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
    
    // Configurar Timer0 para números aleatorios
    OPTION_REGbits.T0CS = 1;
    OPTION_REGbits.PSA = 0;
    OPTION_REGbits.PS = 0b111;
    
    ADCON1 = 0x07;
    CMCON = 0x07;
    
    // Inicializar subsistemas
    UART_Init();
    LCD_Init();
    inicializarNivel();
    limpiarRecursos();
    
    // Inicializar semilla
    TMR0 = 0;
    semilla = TMR0;
    
    UART_Escr_String("Sistema listo\r\n");
    
    // Mensaje de espera
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
                // Cargar sprites y preparar juego
                LCD_CargarTodosSprites();
                
                __delay_ms(100);
                enviarConfirmacion();
                
                // Inicializar juego
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
            
            // Generar obstáculos periódicamente
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
            
            // Detectar colisión
            detectar_colision();
            
            // Incrementar puntuación cuando pasa obstáculo
            if(displayBuffer[1][0] == OBSTACULO || displayBuffer[0][0] == OBSTACULO) {
                puntuacion++;
            }
            
            // Verificar si alcanzó la meta
            if(nivel.goalType == 1 && puntuacion >= nivel.goalValue) {
                Activo = 0;
                
                COMANDO(0x01);
                __delay_ms(2);
                
                LCD_Posicion(0, 0);
                LCD_Escr_String("GANASTE!");
                LCD_Posicion(0, 1);
                LCD_Escr_String("Score: ");
                
                unsigned char decenas = '0' + (puntuacion / 10);
                unsigned char unidades = '0' + (puntuacion % 10);
                DIGITO(decenas);
                DIGITO(unidades);
                
                __delay_ms(3000);
                
                gameInitialized = 0;
                
                LCD_Posicion(0, 0);
                LCD_Escr_String("Esperando");
                LCD_Posicion(0, 1);
                LCD_Escr_String("config...");
            }
            
            // Velocidad del juego
            __delay_ms(150);
        }
        
        // Pausa pequeña
        __delay_ms(10);
        
        // Mantenimiento preventivo del buffer
        static unsigned char contador = 0;
        if(++contador > 100) {
            contador = 0;
            if(UART_Disp() && !buscaChar('{') && !gameInitialized) {
                limpiarRecursos();
            }
        }
    }
}
