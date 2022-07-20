// -------------------------------------------------------------|
// Code developed for Toradex hardware
//   +  Computer on Module COM - Colibri VF50
//   +  Base Board  - Viola
// -------------------------------------------------------------|
// State Machine Code adapted from: 
//           https://www.embarcados.com.br/maquina-de-estado/
//              Original   - Pedro Bertoleti
// -------------------------------------------------------------|

///Links úteis para revisão de termos utilizados durante o programa:
/// https://www.kennethkuhn.com/electronics/debounce.c
/// https://stackoverflow.com/questions/5256599/what-are-file-descriptors-explained-in-simple-terms
/// https://pubs.opengroup.org/onlinepubs/9699919799/functions/poll.html
/// https://man7.org/linux/man-pages/man2/poll.2.html
/// https://pubs.opengroup.org/onlinepubs/009604599/functions/read.html
/// https://pubs.opengroup.org/onlinepubs/007904975/functions/write.html
/// https://pubs.opengroup.org/onlinepubs/007904875/functions/open.html

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

//Debounce parameters 
//Numbers can be tweaked according to each hardware specifications
#define DEBOUNCE_TIME       0.3
#define SAMPLE_FREQUENCY    10
#define MAXIMUM         (DEBOUNCE_TIME * SAMPLE_FREQUENCY)

//GPIO Led/Botões
//Defina os Leds conforme as portas a serem utilizadas
#define Led1 
#define Led2
#define Led_Start 89
#define Botao1
#define Botao2
#define Botao_Start 88

//Global variables:
void (*PointerToFunction)(); // Pointer to the functions (states) of the state machine. 
                             // It points to the function runs in the given time
int Player1_Points;          // Collects points from Player 1 
int Player2_Points;          // Collects points from Player 2 

// debounce integrator
unsigned int integrator;     // Will range from 0 to the specified MAXIMUM
//https://www.kennethkuhn.com/electronics/debounce.c for more information on the debouncer used in this program

struct pollfd poll_gpio;    //Poll para o Botao_Start

//Function prototypes:
void Starting_State(void);       //function representing the initial state of the state machine
void Game_Running_State(void);   //function representing the state after Play Push-Button is pressed
void Player_1_Score_State(void); //function representing the state after Push-Button 1 is pressed
void Player_2_Score_State(void); //function representing the state after Push-Button 2 is pressed

//Auxiliary functions 
const char* get_path(char tipo[], int porta)
{
    ///Recebe o "tipo" do get_path e a "porta" do I/O
    ///Tipos: "export","direction","edge","value"
    ///Returns possíveis:
    ///  "/sys/class/gpio/export"
    ///  "/sys/class/gpio/gpioXX/tipo",sendo os tipos: "direction","edge","value"

    static char path[50];
    char aux[3];
    char gpio_porta[10];

    strcpy(path, "/sys/class/gpio/");
    if (tipo == "export" || tipo =="import")
    {
        strncat(path, tipo, 7);
    }
    else
    {
        sprintf(aux, "%d", porta);
        strcpy(gpio_porta, "gpio");
        strncat(gpio_porta, aux, 2);
        strncat(gpio_porta, "/", 2);
        strncat(path, gpio_porta, 10);
        strncat(path, tipo, 10);
    }

    return path;
}

void configura_led(int porta)
{
    //Configura o led (verifica o Export) conforme a porta de I/O
    //Verifica o Export e coloca a Direction para "out"

    int fd;
    char aux_int[3]; //porta com maximo de 3 digitos
    fd = open(get_path("export", porta), O_WRONLY);
    sprintf(aux_int, "%d", porta);
    write(fd, aux_int, 2);
    close(fd);
    fd = open(get_path("direction", porta), O_WRONLY);
    write(fd, "out", 3);
    close(fd);

    return;
}

void desliga_led(int desligado, int porta)
{
    ///Desliga/Liga o led conforme o int desligado e porta especificados:
    ///Liga(desligado = 0) / Desliga(desligado = 1)

    int fd;
    char aux_int[2];

    fd = open(get_path("value", porta), O_WRONLY);
    sprintf(aux_int, "%d", desligado);
    write(fd, aux_int, 1);
    close(fd);

    return;
}

void configura_botao(int porta)
{
    ///Configura o botao (verifica o Export) conforme a porta de I/O
    ///Verifica o Export, direction para "in" e edge para "rising"

    int fd;
    char aux_int[3];
    fd = open(get_path("export", porta), O_WRONLY);
    sprintf(aux_int, "%d", porta);
    write(fd, aux_int, 2);
    close(fd);
    fd = open(get_path("direction", porta), O_WRONLY);
    write(fd, "in", 2);
    close(fd);
    //fd = open("/sys/class/gpio/gpio88/edge", O_WRONLY);
    fd = open(get_path("edge", porta), O_WRONLY);
    write(fd, "rising", 6); // configure as rising edge
    close(fd);

    return;
}


//State functions
void Starting_State(void)
{
    ///Espera a entrada do Botao_Start, com uso de debounce code
    ///Concluindo a leitura do Botão_Start, vai para o próximo estado:
    ///  PointerToFunction = Game_Running_State;

    char value;
    int fd;
    // int n_value; //descomentar caso precise inverter os valores de input; o default é de input = 1 caso o botão esteja pressionado
    unsigned int input;       //
    unsigned int output;      

    fd = open(get_path("value", Botao_Start), O_RDONLY);

    poll(&poll_gpio, 1, -1); // discard first IRQ
    read(fd, &value, 1);
    
    // wait for interrupt
    while(1){
        poll(&poll_gpio, 1, -1);
        if((poll_gpio.revents & POLLPRI) == POLLPRI){
            lseek(fd, 0, SEEK_SET);
            read(fd, &value, 1);
            break;
        }
    }

    //Aqui é opcional, depende do output
    // Invert input 0 -> 1 and 1 -> 0
    //n_value = (int)value;
    // if(n_value == 0)
    //   input = 1;
    // else
    //   input = 0;

    //Debounce code
    input = (int)value;

    if (input == 0)
    {
        if (integrator > 0)
        integrator--;
    }
    else if (integrator < MAXIMUM)
        integrator++;

    if (integrator == 0)
        output = 0;
    else if (integrator >= MAXIMUM)
    {
        output = 1;
        integrator = MAXIMUM;  /* defensive code if integrator got corrupted */
    }
 
    if (output == 1)
    {
        PointerToFunction = Game_Running_State;
    }
}

void Game_Running_State(void)
{
    ///Liga o Led de Play e espera pela entrada dos Botao1 e Botao2
    ///Com o Botao acionado, leva ao proximo estado:
    ///  PointerToFunction = Player_1_Score_State;

    int fd;
    int pressed; //verifica se o botao foi pressionado pelo pressed = poll();
    struct pollfd poll_players[2]; //considerando 2 times
    poll_players[0].fd = open(get_path("value", Botao1), O_RDONLY);
    poll_players[1].fd = open(get_path("value", Botao2), O_RDONLY);
    poll_players[0].events = POLLIN;
    poll_players[1].events = POLLIN;
  
    //pressed = poll(poll_players, 2, 1);
    //if (pressed)
    //{
    //    if(poll[0].revents &POLLIN)
    //    {
    //        //código de queimou a largada
     //    }
    //    else if (poll[1].revents & POLLIN)
    //    {
     //        //código de queimou a largada
    //    }
    //}

    desliga_led(0, Led_Start);

    pressed = poll(poll_players, 2, -1); //-1 mostra a espera até que algum dos botões sejam pressionados
    if (pressed)
     {
        if(poll_players[0].revents & POLLIN)
        {
            PointerToFunction = Player_1_Score_State;
         }
        else if (poll_players[1].revents & POLLIN)
        {
             PointerToFunction = Player_2_Score_State;

        }
    }
}

void Player_1_Score_State(void)
{
    ///Adiciona ponto ao jogador X, liga o led do jogador X, apaga o led do Play e volta para o estado inicial:
    ///  PointerToFunction = Starting_State;

    Player1_Points++;
    desliga_led(0, Led1);
    desliga_led(1, Led_Start);
    PointerToFunction = Starting_State;
}

void Player_2_Score_State(void)
{
    ///Adiciona ponto ao jogador X, liga o led do jogador X, apaga o led do Play e volta para o estado inicial:
    /// PointerToFunction = Starting_State;

    Player2_Points++;
    desliga_led(0, Led2);
    desliga_led(1, Led_Start);
    PointerToFunction = Starting_State;
}


int main(int argc, char *argv[])
{
    /// Inicializa variáveis globais em 0 e o PointerToFunction no estado inicial.
    /// Desliga o Led do Start
    /// Configura o poll_gpio(poll para o botão do Start), usado no estado seguinte
    
    //Inicialização de variáveis globais e do PointerToFunction
    Player1_Points = 0;
    Player2_Points = 0;
    integrator = 0;
    PointerToFunction = Starting_State; //points to the initial state. 
                                        //Never forget to inform the initial state 
                                        //(otherwise, in this specific case, fatal error may occur/ hard fault).

    int fd;
    //https://stackoverflow.com/questions/5256599/what-are-file-descriptors-explained-in-simple-terms
    char aux_int[3]; //Auxiliar que armazena valores de int das portas gpio para string


    //Desligar o Led do Start
    configura_led(Led_Start);
    desliga_led(1, Led_Start);


    //Configura o Botao_Start: Checa export,direction=in,edge=rising,fd = value(o fd é usado no poll_gpio.fd);
    configura_botao(Botao_Start);

    //Set no poll_gpio.fd para a entrada do Botao_Start
    fd = open(get_path("value", Botao_Start), O_RDONLY);

    poll_gpio.events = POLLPRI;
    poll_gpio.fd = fd;

    //Configurações de botões e leds

    configura_led(Led1);
    configura_led(Led2);

    configura_botao(Botao1);
    configura_botao(Botao2);

    while(1)
    {
        (*PointerToFunction)();  //calls a function pointed out by the pointer to function (thus, calls the current state)
    }
    system("PAUSE"); 
    return 0;
}