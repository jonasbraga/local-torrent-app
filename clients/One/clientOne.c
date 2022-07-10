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

#include "../utils/objetos.c"
#include "../utils/constantes.c"


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
int checksum(pacote *pacote)
{
    if (pacote == NULL)
        return 0; /* no input string */

    int binary[8];

    int Sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    //percorre todas as 1204 posições
    for (int i = 0; i < pacote->tam; ++i)
    {
        //tranforma cada posição em um palavra de 8 bits
        char ch = pacote->dados[i];
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
    addBinary(Sum, pacote->checksum);

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
        rc = recvfrom(sd, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&remoteAddr, &addrlen);

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
        rc = sendto(sd, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&remoteAddr, addrlen);

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

void receivePackage(int sd, struct sockaddr_in remoteAddr, char *nomearq)
{

    pacote pkt;
    FILE *arq;
    int rc, cont = 0;
    char ack = '1';
    char nak = '0';

    //Lê e escreve no arquivo em modo binário
    arq = fopen(nomearq, "wb");
    if (arq == NULL)
    {
        printf("Error - Arquivo não pôde ser criado\n");
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
        if (checksum(&pkt) == 1 && pkt.numseq == cont + 1)
        {

            printf("Pacote %d recebido com sucesso\n", pkt.numseq);
            usleep(4000);
            system("tput cuu1");
            system("tput dl1");

            fwrite(pkt.dados, 1, pkt.tam, arq);
            sendto(sd, &ack, sizeof(ack), 0, (struct sockaddr *)&remoteAddr, addrlen);
            cont++;

            //se o tamanho é menor que 1024, é o ultimo pacote
            if (pkt.tam < 1024)
                break;
        }
        //arquivo corrompeu no caminho
        else
        {
            printf("Pacote %d corrompido no campinho, aguardando reenvio\n", pkt.numseq);
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
    struct sockaddr_in remoteClientTwo;

    //buffer para guardar data temporários
    char *buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

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
    memset(buffer, '\0', SIZE_BUFFER);

    //Recebe a resposta do servidor, contendo a porta do cliente
    //que possui o file
    receiveMessage(sd, remoteServAddr, buffer);
    if (buffer[0] == '1')
    {
        PORT_CLIENT_TWO = atoi(&buffer[1]);
        printf("O cliente na porta %d possui o file.\n", PORT_CLIENT_TWO);
        memset(buffer, '\0', SIZE_BUFFER);
    }
    else
    {
        printf("Error - file não encontrado na base de data.\n");
        exit(1);
    }

    //Comunicação com o cliente Two
    //zerando a struct
    memset(&remoteClientTwo, 0, sizeof(remoteClientTwo));

    //struct com data do cliente Two
    remoteClientTwo.sin_family = AF_INET;
    remoteClientTwo.sin_port = htons(PORT_CLIENT_TWO);
    remoteClientTwo.sin_addr.s_addr = inet_addr("127.0.0.1");

    strcpy(buffer, argv[1]);
    //envia uma requisição com o nome do file para o cliente Two
    sendMessage(sd, remoteClientTwo, buffer, 1);
    sleep(1);
    printf("Requisição enviada ao cliente que possui o file\n");
    memset(buffer, '\0', SIZE_BUFFER);

    receiveMessage(sd, remoteClientTwo, buffer);
    //se o cliente Two confirmar que tem o file, começa a transferência
    if (buffer[0] == '1')
    {
        sleep(1);
        printf("Iniciando transferência\n");
        sleep(1);
        strcpy(buffer, argv[1]);
        receivePackage(sd, remoteClientTwo, buffer);

        //Avisando o servidor que o cliente One tambem possui o arquivo
        sendMessage(sd, remoteServAddr, buffer, 2);

        memset(buffer, '\0', SIZE_BUFFER);
        receiveMessage(sd, remoteServAddr, buffer);
        if (buffer[0] == '1')
            printf("\nCliente One agora presente na base de dados\n");
    }
    else
        printf("Error, cliente Two não conseguiu enviar o file!\n");

    free(buffer);
    return 0;
}