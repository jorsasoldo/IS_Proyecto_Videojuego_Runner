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

//Buffer circular de 64 bytes
#define BUFFER_SIZE 64
#define BUFFER_MASK 0x3F  //63 para operacion AND rapida
volatile unsigned char uartBuffer[BUFFER_SIZE];
volatile unsigned char bufferWrite = 0;
volatile unsigned char bufferRead = 0;

//Arrays para almacenar datos parseados
unsigned char character[8];
unsigned char obstacle[8];

void UART_Init(void) 
{
    //Configura pines
    TRISCbits.TRISC6 = 0;       //RC6/TX como salida
    TRISCbits.TRISC7 = 1;       //RC7/RX como entrada
    
    //Configura todos los pines analogicos como digitales
    ADCON1 = 0x07;
    
    //Configurar UART para 9600 baudios con 4MHz
    SPBRG = 25;                 //Valor para 9600 con 4MHz
    TXSTAbits.BRGH = 1;         //Deteccion a alta velocidad
    TXSTAbits.SYNC = 0;         //Modo asincrono
    TXSTAbits.TXEN = 1;         //Habilita transmision
    
    RCSTAbits.SPEN = 1;         //Habilita el puerto serial
    RCSTAbits.CREN = 1;         //Habilita la recepcion continua
    
    //Habilita interrupcion de recepcion
    PIE1bits.RCIE = 1;          //Habilita interrupcion RX
    INTCONbits.PEIE = 1;        //Habilita interrupciones perifericas
    INTCONbits.GIE = 1;         //Habilita interrupciones globales
    
    //Inicializa indices del buffer
    bufferWrite = 0;
    bufferRead = 0;
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

//Interrupcion para recibir datos y almacenar en buffer
void __interrupt() ISR(void) 
{
    if(PIR1bits.RCIF) 
    {
        //Verifica error de overrun
        if(RCSTAbits.OERR) 
        {
            RCSTAbits.CREN = 0;
            RCSTAbits.CREN = 1;
        }
        
        //Lee el dato y lo guarda en el buffer
        uartBuffer[bufferWrite & BUFFER_MASK] = RCREG;
        bufferWrite++;
    }
}

//Verifica si hay datos disponibles en el buffer
unsigned char UART_Disp(void) 
{
    return (bufferWrite != bufferRead);
}

//Lee un byte del buffer
unsigned char UART_LeeBuffer(void) 
{
    unsigned char dato;
    
    while(!UART_Disp());   //Espera datos
    
    dato = uartBuffer[bufferRead & BUFFER_MASK];
    bufferRead++;
    
    return dato;
}

//Busca un caracter en el buffer (retorna 1 si lo encuentra)
unsigned char buscaChar(unsigned char c) 
{
    unsigned char temp = bufferRead;
    
    while(temp != bufferWrite) 
    {
        if(uartBuffer[temp & BUFFER_MASK] == c) 
            return 1;
        temp++;
    }
    return 0;
}

//Parseo JSON ligero - extrae arrays de 8 bytes
void JSON_Parse(void) 
{
    unsigned char c, i;
    unsigned char temp[4];  //Buffer temporal para numeros (max 3 digitos + null)
    unsigned char tempIdx;
    
    //Espera inicio de JSON
    while(UART_LeeBuffer() != '{');
    
    //Busca "character"
    while(1) 
    {
        c = UART_LeeBuffer();
        if(c == '"') 
        {
            if(UART_LeeBuffer() == 'c') 
                break;
        }
    }
    
    //Salta hasta '['
    while(UART_LeeBuffer() != '[');
    
    //Lee 8 valores para character
    for(i = 0; i < 8; i++) 
    {
        tempIdx = 0;
        
        //Salta espacios y comas
        do {
            c = UART_LeeBuffer();
        } while(c == ' ' || c == ',');
        
        //Lee digitos del numero
        while(c >= '0' && c <= '9') 
        {
            temp[tempIdx++] = c;
            c = UART_LeeBuffer();
        }
        temp[tempIdx] = '\0';
        
        //Convierte string a numero
        character[i] = 0;
        for(tempIdx = 0; temp[tempIdx] != '\0'; tempIdx++) 
        {
            character[i] = character[i] * 10 + (temp[tempIdx] - '0');
        }
    }
    
    //Busca "obstacle"
    while(1) 
    {
        c = UART_LeeBuffer();
        if(c == '"') 
        {
            if(UART_LeeBuffer() == 'o') 
                break;
        }
    }
    
    //Salta hasta '['
    while(UART_LeeBuffer() != '[');
    
    //Lee 8 valores para obstacle
    for(i = 0; i < 8; i++) 
    {
        tempIdx = 0;
        
        //Salta espacios y comas
        do {
            c = UART_LeeBuffer();
        } while(c == ' ' || c == ',');
        
        //Lee digitos del numero
        while(c >= '0' && c <= '9') 
        {
            temp[tempIdx++] = c;
            c = UART_LeeBuffer();
        }
        temp[tempIdx] = '\0';
        
        //Convierte string a numero
        obstacle[i] = 0;
        for(tempIdx = 0; temp[tempIdx] != '\0'; tempIdx++) 
        {
            obstacle[i] = obstacle[i] * 10 + (temp[tempIdx] - '0');
        }
    }
}

// Funcion principal de ejemplo
void main(void) 
{
    unsigned char i;
    
    UART_Init();
    
    UART_Escr_String("Sistema listo\r\n");
    
    while(1) 
    {
        //Espera a recibir JSON completo
        if(buscaChar('}')) 
        {
            JSON_Parse();
            
            //Envia confirmacion con los datos recibidos
            UART_Escr_String("Character: ");
            for(i = 0; i < 8; i++) 
            {
                UART_Escr(character[i] + '0');
                UART_Escr(' ');
            }
            UART_Escr_String("\r\nObstacle: ");
            for(i = 0; i < 8; i++) 
            {
                UART_Escr(obstacle[i] + '0');
                UART_Escr(' ');
            }
            UART_Escr_String("\r\n");
        }
    }
}