#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define SIZE_BUFFER 1024
#define PORTA_CLIENTEB 1502

//estrutura do pacote
typedef struct pacote
{
    int numseq;       //número de sequência do pacote
    int checksum[8];  //valor do checksum do pacote
    int tam;          //tamanho do pacote
    char dados[1024]; //segmento do pacote
} pacote;

//função para somar dois valores binários
void addBinary(int result[], int binario[])
{
    int i, c = 0;

    int aux;
    for (i = 7; i >= 0; i--)
    {
        aux = result[i];
        result[i] = ((aux ^ binario[i]) ^ c); //a xor b xor c
        c = ((aux & binario[i]) | (aux & c)) | (binario[i] & c); //ab+bc+ca
    }
    if (c == 1)
    {
        int aux;
        for (i = 7; i >= 0; i--)
        {
            aux = result[i];
            result[i] = ((aux ^ 0) ^ c); //a xor 0 xor c
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

    //Complemento de 1 dessa soma passa a ser o checksum
    for (int i = 0; i < 8; i++)
    {
        if (Sum[i] == 1)
            pkt->checksum[i] = 0;
        else
            pkt->checksum[i] = 1;
    }

    return 0;
}

//função responsável por transferir o arquivo para o cliente A
int enviaPacote(FILE *arquivo, int socket_clienteB, struct sockaddr_in endereco_clienteA, socklen_t tam_struct_clienteA, char *buffer)
{

    int num_seq = 0;
    pacote pkt;

    memset(&pkt, 0, sizeof(pkt));

    //enquanto o arquivo não acabar
    while (!feof(arquivo))
    {

        num_seq++;

        //preenche o pacote
        pkt.tam = fread(pkt.dados, 1, 1024, arquivo);
        pkt.numseq = num_seq;
        checksum(&pkt);

        //envia o pacote
        while (1)
        {
            memset(buffer, '\0', SIZE_BUFFER);

            if (sendto(socket_clienteB, &pkt, sizeof(pkt), 0, (struct sockaddr *)&endereco_clienteA, tam_struct_clienteA) < 0)
            {
                perror("Error");
                exit(1);
            }

            //recebe resposta do cliente destinatário
            if (recvfrom(socket_clienteB, buffer, sizeof(buffer), 0, (struct sockaddr *)&endereco_clienteA, &tam_struct_clienteA) < 0)
            {
                perror("Error");
                exit(1);
            }

            //só passa para o próximo pacote quando chegar um ACK simbolizado pelo valor "1"
            if (buffer[0] == '1')
            {
                printf("Pacote %d enviado com sucesso!\n", pkt.numseq);
                break;
            }
            else
                printf("Falha ao enviar pacote %d, iniciando reenvio\n", pkt.numseq);
        }
    }
}

int configura_socket()
{
    int socket_clienteB;
    struct sockaddr_in endereco_clienteB;

    socket_clienteB = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_clienteB < 0)
    {
        printf("Criação do socket falhou!\n");
        exit(1);
    }

    memset(&endereco_clienteB, 0, sizeof(endereco_clienteB));
    endereco_clienteB.sin_family = AF_INET;
    endereco_clienteB.sin_port = htons(PORTA_CLIENTEB);

    if (bind(socket_clienteB, (struct sockaddr *)&endereco_clienteB, sizeof(endereco_clienteB)) < 0)
    {
        perror("bind");
        printf("Bind no socket falhou!\n");
        exit(1);
    }

    return socket_clienteB;
}

int verifica_buffer(char *buffer)
{
    if (buffer[0] != '\0')
        return 1;
    return 0;
}

int main()
{

    int socket_clienteB;
    char *buffer;
    struct sockaddr_in endereco_clienteA;
    socklen_t tam_struct_clienteA;
    FILE *arquivo_clienteB;

    buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

    socket_clienteB = configura_socket();

    printf("Cliente B online\n\n");

    //Comunicação com o cliente A
    while (1)
    {

        printf("Aguardando solicitações...\n");
        memset(buffer, '\0', SIZE_BUFFER);

        tam_struct_clienteA = sizeof(endereco_clienteA);

        recvfrom(socket_clienteB, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clienteA, &tam_struct_clienteA);

        printf("Mensagem recebida do cliente A: %s\n", buffer);

        if (verifica_buffer(buffer))
        {

            arquivo_clienteB = fopen(buffer, "rb");
            memset(buffer, '\0', SIZE_BUFFER);

            if (arquivo_clienteB == NULL)
            {
                buffer[0] = '0';
                sendto(socket_clienteB, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clienteA, tam_struct_clienteA);
            }
            else
            {
                buffer[0] = '1';
                sendto(socket_clienteB, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clienteA, tam_struct_clienteA);
                enviaPacote(arquivo_clienteB, socket_clienteB, endereco_clienteA, tam_struct_clienteA, buffer);
            }

            fclose(arquivo_clienteB);
        }
    }
}
