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


// Somando valores binários
void addBinary(int result[], int binary[])
{
    int i, c = 0;

    int k; 
    for (i = 7; i >= 0; i--)
    {
        k = result[i];
        result[i] = ((k ^ binary[i]) ^ c);                  
        c = ((k & binary[i]) | (k & c)) | (binary[i] & c);
    }
    if (c == 1)
    {
        int k;
        for (i = 7; i >= 0; i--)
        {
            k = result[i];
            result[i] = ((k ^ 0) ^ c);          
            c = ((k & 0) | (k & c)) | (0 & c); 
        }
    }
}

// Calculando o checksum de cada pacote
int checksum(pacote *pacote)
{
    if (pacote == NULL)
        return 0; 

    int binary[8];

    int Sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    // Percorrendo todas as posições do pacote (1024)
    for (int i = 0; i < pacote->tam; ++i)
    {
        // Transformando cada pacote em uma palavra de 8 bits
        char ch = pacote->dados[i];
        for (int j = 7; j >= 0; --j)
        {
            if (ch & (1 << j))
                binary[7 - j] = 1;

            else
                binary[7 - j] = 0;
        }

        // Soma cada palavra
        addBinary(Sum, binary);
    }

    // Somando com o checksum
    addBinary(Sum, pacote->checksum);

    //verifica se o pacote está corrompído
    //precisar estar assim: (1,1,1,1,1,1,1,1)
    int validate = 1;
    for (int i = 0; i < 8; i++)
        if (Sum[i] != 1)
            validate = 0;

    return validate;
}

// Recebendo as mensagens e tratando em caso de erro
void receiveMessage(int sd, struct sockaddr_in remoteAddr, char *buffer)
{
    int rc;
    socklen_t addrlen = sizeof(remoteAddr);

    while (1)
    {
        rc = recvfrom(sd, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&remoteAddr, &addrlen);

        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }
        else
            break;
    }
}

// Enviando as mensagens
void sendMessage(int sd, struct sockaddr_in remoteAddr, char *buffer, int type)
{
    int rc;
    socklen_t addrlen = sizeof(remoteAddr);
    //Se type == 1, então envia-se para o servidor o nome do arquivo desejado
    if (type == 1)
    {
        //envia a requisição do file desejado
        rc = sendto(sd, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&remoteAddr, addrlen);

        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }
    }
    //Se type == 2, então envia-se a resposta contendo uma struct ao servidor
    else if (type == 2)
    {
        message message;
        message.clientPort = PORT_CLIENT_ONE;
        strcpy(message.file, buffer);

        rc = sendto(sd, &message, sizeof(message), 0, (struct sockaddr *)&remoteAddr, addrlen);

        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }
    }
}

// Tratando o pacote recebido
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

    //recebe pacote do cliente 
    while (1)
    {
        memset(&pkt, 0, sizeof(pacote));

        rc = recvfrom(sd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&remoteAddr, &addrlen);
        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }

        //Verifica o checksum correspondente
        if (checksum(&pkt) == 1 && pkt.numseq == cont + 1)
        {

            printf("Pacote %d recebido com sucesso\n", pkt.numseq);
            usleep(4000);
            system("tput cuu1");
            system("tput dl1");

            fwrite(pkt.dados, 1, pkt.tam, arq);
            sendto(sd, &ack, sizeof(ack), 0, (struct sockaddr *)&remoteAddr, addrlen);
            cont++;

            //Verifica se é o último pacote
            if (pkt.tam < 1024)
                break;
        }
        // Arquivo corrompido
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
    struct sockaddr_in clientTwo;

    // Buffer para guardar dados temporários
    char *buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

    if (argc == 1)
    {
        printf("Client One online\n");
        printf("Aguardando solicitações\n");
        exit(1);
    }

    // Passando nome do arquivo desejado para o buffer
    strcpy(buffer, argv[1]);

    // Criando o socket
    sd = socket(AF_INET, SOCK_DGRAM, 0);

    // Tratando erro
    if (sd == -1)
    {
        perror("Error");
        exit(1);
    }

    // Zerando a struct
    memset(&remoteServAddr, 0, sizeof(remoteServAddr));

    // Struct com dados do servidor
    remoteServAddr.sin_family = AF_INET;
    remoteServAddr.sin_port = htons(SERVER_PORT);
    remoteServAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /*
        Enviando a requisição para o servidor com o nome do arquivo,
        porta do server e buffer.
    */
    sendMessage(sd, remoteServAddr, buffer, 1);

    printf("Requisição enviada ao servidor\n");
    sleep(1);

    // Zerando buffer para receber o response do server
    memset(buffer, '\0', SIZE_BUFFER);

    /*
        Recebendo a resposta do servidor, com a porta
        do cliente que possui o arquivo
    */
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

    //Comunicação com o clientTwo

    // Zerando a struct
    memset(&clientTwo, 0, sizeof(clientTwo));

    // Struct com dados do clientTwo
    clientTwo.sin_family = AF_INET;
    clientTwo.sin_port = htons(PORT_CLIENT_TWO);
    clientTwo.sin_addr.s_addr = inet_addr("127.0.0.1");

    strcpy(buffer, argv[1]);
    //Envia uma requisição com o nome do file para o clientTwo
    sendMessage(sd, clientTwo, buffer, 1);
    sleep(1);
    printf("Requisição enviada ao cliente que possui o file\n");
    memset(buffer, '\0', SIZE_BUFFER);

    receiveMessage(sd, clientTwo, buffer);

    //Se o clientTwo confirmar que tem o file, começa a transferência
    if (buffer[0] == '1')
    {
        sleep(1);
        printf("Iniciando transferência\n");
        sleep(1);
        strcpy(buffer, argv[1]);
        receivePackage(sd, clientTwo, buffer);

        //Avisando o servidor que o clientOne tambem possui o arquivo
        sendMessage(sd, remoteServAddr, buffer, 2);

        memset(buffer, '\0', SIZE_BUFFER);
        receiveMessage(sd, remoteServAddr, buffer);
        if (buffer[0] == '1')
            printf("\nClientOne agora presente na base de dados\n");
    }
    else
        printf("Error, o cliente não conseguiu enviar o file!\n");

    free(buffer);
    return 0;
}