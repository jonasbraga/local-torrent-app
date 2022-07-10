#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../utils/objetos.c"
#include "../utils/constantes.c"


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
int checksum(pacote *pacote)
{
    if (pacote == NULL)
        return 0; /* no input string */

    int binario[8];

    int Sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    //percorre todas as 1204 posições
    for (int i = 0; i < pacote->tam; ++i)
    {
        //tranforma cada posição em um palavra de 8 bits
        char ch = pacote->dados[i];
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
            pacote->checksum[i] = 0;
        else
            pacote->checksum[i] = 1;
    }

    return 0;
}

//função responsável por transferir o arquivo para o cliente One
int enviaPacote(FILE *arquivo, int socket_clienteB, struct sockaddr_in endereco_clienteA, socklen_t tam_struct_clienteA, char *buffer)
{

    int num_seq = 0;
    pacote pacote;

    memset(&pacote, 0, sizeof(pacote));

    //enquanto o arquivo não acabar
    while (!feof(arquivo))
    {

        num_seq++;

        //preenche o pacote
        pacote.tam = fread(pacote.dados, 1, 1024, arquivo);
        pacote.numseq = num_seq;
        checksum(&pacote);

        //envia o pacote
        while (1)
        {
            memset(buffer, '\0', SIZE_BUFFER);

            if (sendto(socket_clienteB, &pacote, sizeof(pacote), 0, (struct sockaddr *)&endereco_clienteA, tam_struct_clienteA) < 0)
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
                printf("Pacote %d enviado com sucesso!\n", pacote.numseq);
                break;
            }
            else
                printf("Falha ao enviar pacote %d, iniciando reenvio\n", pacote.numseq);
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
    endereco_clienteB.sin_port = htons(PORTA_CLIENTE_TWO);

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

    printf("Cliente Two online\n\n");

    //Comunicação com o cliente One
    while (1)
    {

        printf("Aguardando solicitações...\n");
        memset(buffer, '\0', SIZE_BUFFER);

        tam_struct_clienteA = sizeof(endereco_clienteA);

        recvfrom(socket_clienteB, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clienteA, &tam_struct_clienteA);

        printf("Mensagem recebida do cliente One: %s\n", buffer);

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
