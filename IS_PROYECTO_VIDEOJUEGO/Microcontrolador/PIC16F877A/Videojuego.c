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


#define BUFFER_SIZE 32
#define BUFFER_MASK 0x1F
volatile unsigned char uartBuffer[BUFFER_SIZE];
volatile unsigned char bufferWrite = 0;
volatile unsigned char bufferRead = 0;

// Estructura completa que contiene toda la configuración del juego
typedef struct {
    // Sprites personalizados (8 bytes cada uno)
    unsigned char character[8];     // Sprite del personaje
    unsigned char obstacle[8];      // Sprite del obstáculo
    
    // Configuración de meta del juego
    unsigned char goalType;         // 0=tiempo, 1=obstáculos
    unsigned int goalValue;         // Valor objetivo (0-9999)
    
    // Flags de validación
    unsigned char characterLoaded;  // 1 si character está cargado
    unsigned char obstacleLoaded;   // 1 si obstacle está cargado
    unsigned char goalLoaded;       // 1 si goal está cargado
} LevelConfig;

// Variable global de configuración del nivel
LevelConfig nivel;

// Buffer temporal multiuso - SE REUTILIZA en diferentes funciones
#define TEMP_BUFFER_SIZE 5
unsigned char tempBuffer[TEMP_BUFFER_SIZE];

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

unsigned char LCD_CargarPersonaje(void) {
    unsigned char i;
    unsigned char addr;
    
    // Dirección CGRAM para posición 0 (0x40 = base CGRAM)
    // Posición 0: 0x40 | (0 << 3) = 0x40
    addr = 0x40;
    COMANDO(addr);
    
    // Pequeña pausa para asegurar que el LCD procese el comando
    __delay_us(100);
    
    // Escribir 8 bytes del personaje en CGRAM
    for(i = 0; i < 8; i++) {
        DIGITO(nivel.character[i]);
        __delay_us(50);  // Pausa entre bytes
    }
    
    // Verificar que se escribieron los datos (lectura simple)
    // En un LCD real, podríamos leer de vuelta, pero aquí confiamos
    // en que si llegamos aquí, la escritura fue exitosa
    
    // Regresar a DDRAM (dirección 0x80)
    COMANDO(0x80);
    __delay_us(100);
    
    // Marcar como cargado
    nivel.characterLoaded = 1;
    
    return 1;  // Éxito
}

unsigned char LCD_CargarObstaculo(void) {
    unsigned char i;
    unsigned char addr;
    
    // Dirección CGRAM para posición 1
    // Posición 1: 0x40 | (1 << 3) = 0x48
    addr = 0x40 | (1 << 3);
    COMANDO(addr);
    
    // Pequeña pausa para asegurar que el LCD procese el comando
    __delay_us(100);
    
    // Escribir 8 bytes del obstáculo en CGRAM
    for(i = 0; i < 8; i++) {
        DIGITO(nivel.obstacle[i]);
        __delay_us(50);  // Pausa entre bytes
    }
    
    // Regresar a DDRAM
    COMANDO(0x80);
    __delay_us(100);
    
    // Marcar como cargado
    nivel.obstacleLoaded = 1;
    
    return 1;  // Éxito
}

unsigned char LCD_CargarTodosSprites(void) {
    unsigned char exitosos = 0;
    
    // Cargar personaje en CGRAM 0x00
    if(LCD_CargarPersonaje()) {
        exitosos++;
    }
    
    // Cargar obstáculo en CGRAM 0x01
    if(LCD_CargarObstaculo()) {
        exitosos++;
    }
    
    return exitosos;
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

void UART_BuscaInicioJSON(void) {
    unsigned char c;
    unsigned int timeout = 0;
    
    while(timeout < 5000) {
        if(UART_Disp()) {
            c = UART_LeeBuffer();
            if(c == '{') {
                bufferRead = (bufferRead == 0) ? (BUFFER_SIZE - 1) : (bufferRead - 1);
                return;
            }
        }
        __delay_us(100);
        timeout++;
    }
}

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

void JSON_Parse(void) {
    unsigned char c, i, len;
    unsigned int intentos = 0;
    
    // Reiniciar flags de carga
    nivel.characterLoaded = 0;
    nivel.obstacleLoaded = 0;
    nivel.goalLoaded = 0;
    
    // Espera inicio de JSON
    while(!buscaChar('{') && intentos < 1000) {
        __delay_ms(10);
        intentos++;
    }
    
    if(intentos >= 1000) {
        return;
    }
    
    while(UART_LeeBuffer() != '{');
    
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'c')
            break;
    }
    while(UART_LeeBuffer() != '[');
    
    // Lee 8 valores para character
    for(i = 0; i < 8; i++) {
        len = leerDigitos();
        nivel.character[i] = (unsigned char)strToUInt(len);
    }
    nivel.characterLoaded = 1;  // Marcar como cargado
    
    while(1) {
        c = UART_LeeBuffer();
        if(c == '"' && UART_LeeBuffer() == 'o')
            break;
    }
    while(UART_LeeBuffer() != '[');
    
    // Lee 8 valores para obstacle
    for(i = 0; i < 8; i++) {
        len = leerDigitos();
        nivel.obstacle[i] = (unsigned char)strToUInt(len);
    }
    nivel.obstacleLoaded = 1;  // Marcar como cargado
    
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
    nivel.goalLoaded = 1;  // Marcar como cargado
}

void enviarConfirmacion(void) {
    // Inicio del JSON
    UART_Escr_String("{\"status\":\"loaded\"");
    
    // Estado del personaje
    UART_Escr_String(",\"character\":\"");
    if(nivel.characterLoaded) {
        UART_Escr_String("ok");
    } else {
        UART_Escr_String("error");
    }
    UART_Escr_String("\"");
    
    // Estado del obstáculo
    UART_Escr_String(",\"obstacle\":\"");
    if(nivel.obstacleLoaded) {
        UART_Escr_String("ok");
    } else {
        UART_Escr_String("error");
    }
    UART_Escr_String("\"");
    
    // Estado de la meta
    UART_Escr_String(",\"goal\":\"");
    if(nivel.goalLoaded) {
        UART_Escr_String("ok");
    } else {
        UART_Escr_String("error");
    }
    UART_Escr_String("\"");
    
    // Información adicional de depuración (opcional)
    UART_Escr_String(",\"goalType\":");
    UART_Escr((nivel.goalType == 0) ? '0' : '1');
    
    UART_Escr_String(",\"goalValue\":");
    // Enviar goalValue como número
    {
        unsigned char buffer[6];
        unsigned char idx = 0;
        unsigned int val = nivel.goalValue;
        unsigned char temp;
        
        // Convertir número a string
        if(val == 0) {
            buffer[idx++] = '0';
        } else {
            // Extraer dígitos en orden inverso
            unsigned char start = idx;
            while(val > 0) {
                buffer[idx++] = (val % 10) + '0';
                val /= 10;
            }
            // Invertir los dígitos
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
        
        // Enviar el número
        for(temp = 0; temp < idx; temp++) {
            UART_Escr(buffer[temp]);
        }
    }
    
    // Fin del JSON
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

void mostrarConfiguracion(void) {
    unsigned char digito;
    
    // Limpiar LCD
    COMANDO(0x01);
    __delay_ms(2);
    
    // Cargar sprites en CGRAM usando las funciones específicas
    LCD_CargarPersonaje();   // Carga en CGRAM 0x00
    LCD_CargarObstaculo();   // Carga en CGRAM 0x01
    
    // Línea 1: Mostrar sprites
    LCD_Posicion(0, 0);
    LCD_Escr_String("Char:");
    DIGITO(0);  // Muestra character desde CGRAM 0x00
    LCD_Escr_String(" Obst:");
    DIGITO(1);  // Muestra obstacle desde CGRAM 0x01
    
    // Línea 2: Mostrar meta
    LCD_Posicion(0, 1);
    LCD_Escr_String("Meta:");
    
    DIGITO((nivel.goalType == 0) ? 'T' : 'O');
    DIGITO('=');
    
    // Valor de meta (4 dígitos)
    digito = nivel.goalValue / 1000;
    DIGITO(digito + '0');
    
    digito = (nivel.goalValue / 100) % 10;
    DIGITO(digito + '0');
    
    digito = (nivel.goalValue / 10) % 10;
    DIGITO(digito + '0');
    
    digito = nivel.goalValue % 10;
    DIGITO(digito + '0');
}

unsigned char validarConfiguracion(void) {
    // Verificar que todos los componentes estén cargados
    if(!nivel.characterLoaded || !nivel.obstacleLoaded || !nivel.goalLoaded) {
        return 0;
    }
    
    // Verificar que goalValue esté en rango válido
    if(nivel.goalValue == 0 || nivel.goalValue > 9999) {
        return 0;
    }
    
    // Verificar que goalType sea válido
    if(nivel.goalType > 1) {
        return 0;
    }
    
    return 1;  // Configuración válida
}

void inicializarNivel(void) {
    unsigned char i;
    
    // Inicializar sprites vacíos
    for(i = 0; i < 8; i++) {
        nivel.character[i] = 0;
        nivel.obstacle[i] = 0;
    }
    
    // Valores por defecto
    nivel.goalType = 1;      // Obstáculos
    nivel.goalValue = 10;    // 10 obstáculos
    
    // Flags de carga
    nivel.characterLoaded = 0;
    nivel.obstacleLoaded = 0;
    nivel.goalLoaded = 0;
}

void main(void) {
    UART_Init();
    LCD_Init();
    
    // Inicializar estructura de nivel
    inicializarNivel();
    
    // Limpia todos los recursos
    limpiarRecursos();
    
    UART_Escr_String("Sistema listo\r\n");
    
    // Mensaje de bienvenida
    LCD_Posicion(0, 0);
    LCD_Escr_String("Esperando");
    LCD_Posicion(0, 1);
    LCD_Escr_String("config...");
    
    while(1) {
        // Verifica si hay JSON completo
        if(buscaChar('}')) {
            __delay_ms(100);
            
            // Parsea configuración en la estructura
            JSON_Parse();
            
            // Valida la configuración recibida
            if(validarConfiguracion()) {
                // Muestra en LCD (esto carga sprites en CGRAM)
                mostrarConfiguracion();
                
                // Pequeña pausa para asegurar que LCD terminó
                __delay_ms(100);
                
                // Envía confirmación detallada
                enviarConfirmacion();
            } else {
                // Envía error si la validación falló
                UART_Escr_String("{\"status\":\"error\",\"message\":\"invalid config\"}\r\n");
            }
            
            // Limpia recursos
            limpiarRecursos();
            
            __delay_ms(100);
        }
        
        __delay_ms(10);
        
        // Mantenimiento preventivo
        static unsigned char contador = 0;
        if(++contador > 100) {
            contador = 0;
            if(UART_Disp() && !buscaChar('{')) {
                limpiarRecursos();
            }
        }
    }
}