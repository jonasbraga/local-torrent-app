#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>   /* memset() */
#include <sys/time.h> /* select() */

#define REMOTE_SERVER_PORT 1500
#define PORTA_CLIENTE_A 1501
#define IP_LOCAL "127.0.0.1"
#define SIZE 1024

typedef struct mensagem
{
    int porta_cliente;
    char arquivo[20];
} mensagem;

typedef struct pacote
{
    int numseq;
    int checksum[8];
    int tam;
    char dados[1024];
} pacote;

//função para somar dois valores binários
void addBinary(int result[], int binario[])
{
    int i, c = 0;

    int aux;
    for (i = 7; i >= 0; i--)
    {
        aux = result[i];
        result[i] = ((aux ^ binario[i]) ^ c);                    //a xor b xor c
        c = ((aux & binario[i]) | (aux & c)) | (binario[i] & c); //ab+bc+ca
    }
    if (c == 1)
    {
        int aux;
        for (i = 7; i >= 0; i--)
        {
            aux = result[i];
            result[i] = ((aux ^ 0) ^ c);           //a xor 0 xor c
            c = ((aux & 0) | (aux & c)) | (0 & c); //a0+bc+0a
        }
    }
}

//Função para calcular o cheksum do pacote
int checksum(pacote *pkt)
{
    if (pkt == NULL)
        return 0; /* no input string */

    int binario[8];

    int Sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    //percorre todas as 1204 posições
    for (int i = 0; i < pkt->tam; ++i)
    {
        //tranforma cada posição em um palavra de 8 bits
        char ch = pkt->dados[i];
        for (int j = 7; j >= 0; --j)
        {
            if (ch & (1 << j))
                binario[7 - j] = 1;

            else
                binario[7 - j] = 0;
        }

        //vai somando cada palavra
        addBinary(Sum, binario);
    }

    //Soma com o cheksum
    addBinary(Sum, pkt->checksum);

    //verifica se o pacote está corrompído
    //precisar estar assim: (1,1,1,1,1,1,1,1)
    int valida = 1;
    for (int i = 0; i < 8; i++)
        if (Sum[i] != 1)
            valida = 0;

    return valida;
}

// Função para receber as mensagens
void recebeMensagem(int sd, struct sockaddr_in remoteAddr, char *buffer)
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
void enviaMensagem(int sd, struct sockaddr_in remoteAddr, char *buffer, int tipo)
{
    int rc;
    socklen_t addrlen = sizeof(remoteAddr);

    //Se for do tipo 1, envia apenas o nome do arquivo que deseja
    if (tipo == 1)
    {
        //envia a requisição do arquivo desejado
        rc = sendto(sd, buffer, SIZE, 0, (struct sockaddr *)&remoteAddr, addrlen);

        //checagem de erro
        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }
    }

    //Se for do tipo 2, é a resposta contendo uma struct ao servidor.
    else if (tipo == 2)
    {
        mensagem mensagem;
        mensagem.porta_cliente = PORTA_CLIENTE_A;
        strcpy(mensagem.arquivo, buffer);

        //envia a mensagem
        rc = sendto(sd, &mensagem, sizeof(mensagem), 0, (struct sockaddr *)&remoteAddr, addrlen);

        //checagem de erro
        if (rc == -1)
        {
            perror("Error");
            exit(1);
        }
    }
}

void recebePacote(int sd, struct sockaddr_in remoteAddr, char *nomearq)
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
    int PORTA_CLIENTE_B;
    struct sockaddr_in remoteServAddr;
    struct sockaddr_in remoteClientB;

    //buffer para guardar dados temporários
    char *buffer = (char *)malloc(SIZE * sizeof(char));

    // Validação do parâmetro passado
    if (argc == 1)
    {
        printf("Error -- Inserir nome do arquivo desejado\n");
        exit(1);
    }

    //passa o nome do arquivo desejado para o buffer
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

    //struct com dados do servidor
    remoteServAddr.sin_family = AF_INET;
    remoteServAddr.sin_port = htons(REMOTE_SERVER_PORT);
    remoteServAddr.sin_addr.s_addr = inet_addr(IP_LOCAL);

    //enviando requisição ao servidor, em busca de algum cliente
    //que tenha tal arquivo
    enviaMensagem(sd, remoteServAddr, buffer, 1);

    printf("Requisição enviada ao servidor\n");
    sleep(1);
    //zerando o buffer para receber a resposta do servidor
    memset(buffer, '\0', SIZE);

    //Recebe a resposta do servidor, contendo a porta do cliente
    //que possui o arquivo
    recebeMensagem(sd, remoteServAddr, buffer);
    if (buffer[0] == '1')
    {
        PORTA_CLIENTE_B = atoi(&buffer[1]);
        printf("O cliente na porta %d possui o arquivo.\n", PORTA_CLIENTE_B);
        memset(buffer, '\0', SIZE);
    }
    else
    {
        printf("Error - Arquivo não encontrado na base de dados.\n");
        exit(1);
    }

    //Comunicação com o cliente B
    //zerando a struct
    memset(&remoteClientB, 0, sizeof(remoteClientB));

    //struct com dados do cliente B
    remoteClientB.sin_family = AF_INET;
    remoteClientB.sin_port = htons(PORTA_CLIENTE_B);
    remoteClientB.sin_addr.s_addr = inet_addr(IP_LOCAL);

    strcpy(buffer, argv[1]);
    //envia uma requisição com o nome do arquivo para o cliente B
    enviaMensagem(sd, remoteClientB, buffer, 1);
    sleep(1);
    printf("Requisição enviada ao cliente que possui o arquivo\n");
    memset(buffer, '\0', SIZE);

    recebeMensagem(sd, remoteClientB, buffer);
    //se o cliente B confirmar que tem o arquivo, começa a transferência
    if (buffer[0] == '1')
    {
        sleep(1);
        printf("Iniciando transferência\n");
        sleep(1);
        strcpy(buffer, argv[1]);
        recebePacote(sd, remoteClientB, buffer);

        //Avisando o servidor que o cliente A também possui o arquivo
        enviaMensagem(sd, remoteServAddr, buffer, 2);

        memset(buffer, '\0', SIZE);
        recebeMensagem(sd, remoteServAddr, buffer);
        if (buffer[0] == '1')
            printf("\nCliente A agora presente na base de dados\n");
    }
    else
        printf("Error, cliente B não conseguiu enviar o arquivo!\n");

    free(buffer);
    return 0;
}