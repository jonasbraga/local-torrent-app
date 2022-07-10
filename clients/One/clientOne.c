#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define SERVER_PORT 3000
#define PORT_CLIENT_ONE 1501
#define SIZE 1024

typedef struct message
{
    int clientPort;
    char file[20];
} message;

typedef struct pacote
{
    int nextnum;
    int checksum[8];
    int siz;
    char data[1024];
} pacote;

//função para somar dois valores binários
void addBinary(int result[], int binary[])
{
    int i, c = 0;

    int k; // assist
    for (i = 7; i >= 0; i--)
    {
        k = result[i];
        result[i] = ((k ^ binary[i]) ^ c);                    //a xor b xor c
        c = ((k & binary[i]) | (k & c)) | (binary[i] & c); //ab+bc+ca
    }
    if (c == 1)
    {
        int k;
        for (i = 7; i >= 0; i--)
        {
            k = result[i];
            result[i] = ((k ^ 0) ^ c);           //a xor 0 xor c
            c = ((k & 0) | (k & c)) | (0 & c); //a0+bc+0a
        }
    }
}

//Função para calcular o cheksum do pacote
int checksum(pacote *pkt)
{
    if (pkt == NULL)
        return 0; /* no input string */

    int binary[8];

    int Sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    //percorre todas as 1204 posições
    for (int i = 0; i < pkt->siz; ++i)
    {
        //tranforma cada posição em um palavra de 8 bits
        char ch = pkt->data[i];
        for (int j = 7; j >= 0; --j)
        {
            if (ch & (1 << j))
                binary[7 - j] = 1;

            else
                binary[7 - j] = 0;
        }

        //vai somando cada palavra
        addBinary(Sum, binary);
    }

    //Soma com o cheksum
    addBinary(Sum, pkt->checksum);

    //verifica se o pacote está corrompído
    //precisar estar assim: (1,1,1,1,1,1,1,1)
    int validate = 1;
    for (int i = 0; i < 8; i++)
        if (Sum[i] != 1)
            validate = 0;

    return validate;
}

// Função para receber as mensagens
void receiveMessage(int sd, struct sockaddr_in remoteAddr, char *buffer)
{
    int rc;
    socklen_t addrlen = sizeof(remoteAddr);

    while (1)
    {
        rc = recvfrom(sd, buffer, SIZE, 0, (struct sockaddr *)&remoteAddr, &addrlen);

        //checagem de erro
        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }
        else
            break;
    }
}

//função para enviar as mensagens
void sendMessage(int sd, struct sockaddr_in remoteAddr, char *buffer, int type)
{
    int rc;
    socklen_t addrlen = sizeof(remoteAddr);

    //Se for do type 1, envia apenas o nome do file que deseja
    if (type == 1)
    {
        //envia a requisição do file desejado
        rc = sendto(sd, buffer, SIZE, 0, (struct sockaddr *)&remoteAddr, addrlen);

        //checagem de erro
        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }
    }

    //Se for do type 2, é a resposta contendo uma struct ao servidor.
    else if (type == 2)
    {
        message message;
        message.clientPort = PORT_CLIENT_ONE;
        strcpy(message.file, buffer);

        //envia a message
        rc = sendto(sd, &message, sizeof(message), 0, (struct sockaddr *)&remoteAddr, addrlen);

        //checagem de erro
        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }
    }
}

void receivePackage(int sd, struct sockaddr_in remoteAddr, char *filename)
{

    pacote pkt;
    FILE *arq;
    int rc, cont = 0;
    char ack = '1';
    char nak = '0';

    //Lê e escreve no file em modo binário
    arq = fopen(filename, "wb");
    if (arq == NULL)
    {
        printf("Error - file não pôde ser criado\n");
        exit(1);
    }

    socklen_t addrlen = sizeof(remoteAddr);

    //recebe pacote do cliente B
    while (1)
    {
        memset(&pkt, 0, sizeof(pacote));

        rc = recvfrom(sd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&remoteAddr, &addrlen);
        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }

        //cheksum corresponde
        if (checksum(&pkt) == 1 && pkt.nextnum == cont + 1)
        {

            printf("Pacote %d recebido com sucesso\n", pkt.nextnum);
            usleep(4000);
            system("tput cuu1");
            system("tput dl1");

            fwrite(pkt.data, 1, pkt.siz, arq);
            sendto(sd, &ack, sizeof(ack), 0, (struct sockaddr *)&remoteAddr, addrlen);
            cont++;

            //se o sizanho é menor que 1024, é o ultimo pacote
            if (pkt.siz < 1024)
                break;
        }
        //file corrompeu no caminho
        else
        {
            printf("Pacote %d corrompido no campinho, aguardando reenvio\n", pkt.nextnum);
            sendto(sd, &nak, sizeof(nak), 0, (struct sockaddr *)&remoteAddr, addrlen);
        }
    }

    fclose(arq);
}

int main(int argc, char *argv[])
{

    int sd;
    int PORT_CLIENT_TWO;
    struct sockaddr_in remoteServAddr;
    struct sockaddr_in remoteClientB;

    //buffer para guardar data temporários
    char *buffer = (char *)malloc(SIZE * sizeof(char));

    // validateção do parâmetro passado
    if (argc == 1)
    {
        printf("Client One online\n");
        printf("Aguardando solicitações\n");
        exit(1);
    }

    //passa o nome do file desejado para o buffer
    strcpy(buffer, argv[1]);

    //Criação do socket
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    //Checagem de erro
    if (sd == -1)
    {
        perror("Error");
        exit(1);
    }

    //zerando a struct
    memset(&remoteServAddr, 0, sizeof(remoteServAddr));

    //struct com data do servidor
    remoteServAddr.sin_family = AF_INET;
    remoteServAddr.sin_port = htons(SERVER_PORT);
    remoteServAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //enviando requisição ao servidor, em busca de algum cliente
    //que tenha tal file
    sendMessage(sd, remoteServAddr, buffer, 1);

    printf("Requisição enviada ao servidor\n");
    sleep(1);
    //zerando o buffer para receber a resposta do servidor
    memset(buffer, '\0', SIZE);

    //Recebe a resposta do servidor, contendo a porta do cliente
    //que possui o file
    receiveMessage(sd, remoteServAddr, buffer);
    if (buffer[0] == '1')
    {
        PORT_CLIENT_TWO = atoi(&buffer[1]);
        printf("O cliente na porta %d possui o file.\n", PORT_CLIENT_TWO);
        memset(buffer, '\0', SIZE);
    }
    else
    {
        printf("Error - file não encontrado na base de data.\n");
        exit(1);
    }

    //Comunicação com o cliente B
    //zerando a struct
    memset(&remoteClientB, 0, sizeof(remoteClientB));

    //struct com data do cliente B
    remoteClientB.sin_family = AF_INET;
    remoteClientB.sin_port = htons(PORT_CLIENT_TWO);
    remoteClientB.sin_addr.s_addr = inet_addr("127.0.0.1");

    strcpy(buffer, argv[1]);
    //envia uma requisição com o nome do file para o cliente B
    sendMessage(sd, remoteClientB, buffer, 1);
    sleep(1);
    printf("Requisição enviada ao cliente que possui o file\n");
    memset(buffer, '\0', SIZE);

    receiveMessage(sd, remoteClientB, buffer);
    //se o cliente B confirmar que tem o file, começa a transferência
    if (buffer[0] == '1')
    {
        sleep(1);
        printf("Iniciando transferência\n");
        sleep(1);
        strcpy(buffer, argv[1]);
        receivePackage(sd, remoteClientB, buffer);

        //Avisando o servidor que o cliente A sizbém possui o file
        sendMessage(sd, remoteServAddr, buffer, 2);

        memset(buffer, '\0', SIZE);
        receiveMessage(sd, remoteServAddr, buffer);
        if (buffer[0] == '1')
            printf("\nCliente A agora presente na base de data\n");
    }
    else
        printf("Error, cliente B não conseguiu enviar o file!\n");

    free(buffer);
    return 0;
}