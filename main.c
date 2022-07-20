// -------------------------------------------------------------|
// Code developed for Toradex hardware
//   +  Computer on Module COM - Colibri VF50
//   +  Base Board  - Viola
// -------------------------------------------------------------|
// State Machine Code adapted from: 
//              https://www.embarcados.com.br/maquina-de-estado/
//              Original   - Pedro Bertoleti

// -------------------------------------------------------------|

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

//Starting State OKish
// Game_Running_State, trocar input para botão físico e adicionar outras funcionalidades
// Player_State, até o momento supérfluo, ver o que se pode fazer, dada a máquina de estados do git
// TurnGOSignON/OFF OK, só trocar os valores do buffer de entrada de 0->1 ou 1->0


//Debounce parameters 
#define DEBOUNCE_TIME       0.3
#define SAMPLE_FREQUENCY    10
#define MAXIMUM         (DEBOUNCE_TIME * SAMPLE_FREQUENCY)

//GPIO Led/Botões
#define Led1 
#define Led2
#define Led_Start 89
#define Botao1
#define Botao2
#define Botao_Start 88

//Global variables:
void (*PointerToFunction)(); // Pointer to the functions (states) of the state machine. 
                             // It points to the function that must run on the state machine
int Player1_Points;          // Collects the points from Player 1 for display
int Player2_Points;          // Collects the points from Player 2 for display

// debounce integrator
unsigned int integrator;     // Will range from 0 to the specified MAXIMUM

struct pollfd poll_gpio;

//prototypes:
void Starting_State(void);       //function representing the initial state of the state machine
void Game_Running_State(void);   //function representing the state after Play Push-Button is pressed
void Player_1_Score_State(void); //function representing the state after Push-Button 1 is pressed
void Player_2_Score_State(void); //function representing the state after Push-Button 2 is pressed

//Auxiliary functions (19/07/2022)
const char* get_path(char tipo[], int porta)
{
    //Tipos: "export","direction","edge"

    static char path[50];
    char aux[3];
    char gpio_porta[8];

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
        strncat(gpio_porta, "/", 1);
        strncat(path, gpio_porta, 10);
        strncat(path, tipo, 10);
    }

    return path;
}

void configura_led(int porta)
{
    int fd;
    char aux_int[3]; //porta com maximo de 3 digitos
    fd = open(get_path("export", porta), O_WRONLY);
    sprintf(aux_int, "%d", porta);
    write(fd, aux_int, 2);
    close(fd);
    fd = open(get_path("direction", porta), O_WRONLY);
    write(fd, "out", 3);
    close(fd);
}

void desliga_led(int desligado, int porta)
{
    int fd;
    char aux_int[1];
    // Turn GO-Sign ON
    fd = open(get_path("value", porta), O_WRONLY | O_SYNC);
    //fd = open("/sys/class/gpio/gpio89/value", O_WRONLY | O_SYNC);
    sprintf(aux_int, "%d", desligado);
    write(fd, aux_int, 1);
    close(fd);
    // ----------------------------------------------------------|
}

void configura_botao(int porta)
{
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
}


//States 
void Starting_State(void)
{
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
    int fd;
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
    pressed = poll(poll_players, 2, -1);
    if (pressed)
     {
        if(poll[0].revents &POLLIN)
        {
            PointerToFunction = Player_1_Score_State;
         }
        else if (poll[1].revents & POLLIN)
        {
             PointerToFunction = Player_2_Score_State;

        }
    }
}

//Switch to the next state if key pressed is 'b'. Otherwise, stay in Game_Running_State (waits for letters 'b' or 'c')
//Player_1_Score_State e Player_2_Score_State não sei se não necessários. Essa de armazenar pontos pode ser colocada no Game_Running_State 
//(Perguntar para o Guilherme)
void Player_1_Score_State(void)
{
    Player1_Points++;
    desliga_led(0, Led1);
    desliga_led(1, Led_Start);
    PointerToFunction = Starting_State;
}

//Validate the state if key pressed is 'd'. Otherwise, return to initial state (waits for letter 'p')
void Player_2_Score_State(void)
{
    Player2_Points++;
    desliga_led(0, Led2);
    desliga_led(1, Led_Start);
    PointerToFunction = Starting_State;
}

int main(int argc, char *argv[])
{
    Player1_Points = 0;
    Player2_Points = 0;
    integrator = 0;
    PointerToFunction = Starting_State; //points to the initial state. 
                                      //Never forget to inform the initial state 
                                      //(otherwise, in this specific case, fatal error may occur/ hard fault).

    int fd;
    //https://stackoverflow.com/questions/5256599/what-are-file-descriptors-explained-in-simple-terms
    char aux_int[3]; //Armazena valores de int das portas gpio para string


    //Desligar o Led do Start
    configura_led(Led_Start);
    desliga_led(1, Led_Start);


    //Configura o Botao_Start: Checa export,direction=in,edge=rising,fd = value(o fd é usado no poll_gpio.fd);
    configura_botao(Botao_Start);

    //Set no poll_gpio.fd para a entrada do Botao_Start
    //fd = open("/sys/class/gpio/gpio88/value", O_RDONLY);
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