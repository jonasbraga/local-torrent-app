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


// Função para somar dois valores binários
void addBinary(int result[], int binario[])
{
    int i, c = 0;

    int aux;
    for (i = 7; i >= 0; i--)
    {
        aux = result[i];
        result[i] = ((aux ^ binario[i]) ^ c);
        c = ((aux & binario[i]) | (aux & c)) | (binario[i] & c);
    }
    if (c == 1)
    {
        int aux;
        for (i = 7; i >= 0; i--)
        {
            aux = result[i];
            result[i] = ((aux ^ 0) ^ c);
            c = ((aux & 0) | (aux & c)) | (0 & c);
        }
    }
}

//Função para calcular o cheksum do pacote
int checksum(pacote *pacote)
{
    if (pacote == NULL)
        return 0;

    int binario[8];

    int Sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    for (int i = 0; i < pacote->tam; ++i)
    {
        
        char ch = pacote->dados[i];
        for (int j = 7; j >= 0; --j)
        {
            if (ch & (1 << j))
                binario[7 - j] = 1;

            else
                binario[7 - j] = 0;
        }

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

//função responsável por transferir o arquivo para o clientOne
void sendPackage(FILE *arquivo, int socket_clientTwo, struct sockaddr_in endereco_clientOne, socklen_t tam_struct_clientOne, char *buffer)
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

            if (sendto(socket_clientTwo, &pacote, sizeof(pacote), 0, (struct sockaddr *)&endereco_clientOne, tam_struct_clientOne) < 0)
            {
                perror("Error");
                exit(1);
            }

            //recebe resposta do cliente destinatário
            if (recvfrom(socket_clientTwo, buffer, sizeof(buffer), 0, (struct sockaddr *)&endereco_clientOne, &tam_struct_clientOne) < 0)
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
    int socket_clientTwo;
    struct sockaddr_in endereco_clientTwo;

    socket_clientTwo = socket(AF_INET, SOCK_DGRAM, 0);

    if (socket_clientTwo < 0)
    {
        printf("Criação do socket falhou!\n");
        exit(1);
    }

    memset(&endereco_clientTwo, 0, sizeof(endereco_clientTwo));
    endereco_clientTwo.sin_family = AF_INET;
    endereco_clientTwo.sin_port = htons(PORTA_CLIENTE_TWO);

    if (bind(socket_clientTwo, (struct sockaddr *)&endereco_clientTwo, sizeof(endereco_clientTwo)) < 0)
    {
        perror("bind");
        printf("Bind no socket falhou!\n");
        exit(1);
    }

    return socket_clientTwo;
}

int bufferVerify(char *buffer)
{
    if (buffer[0] != '\0')
        return 1;
    return 0;
}

int main()
{

    int socket_clientTwo;
    char *buffer;
    struct sockaddr_in endereco_clientOne;
    socklen_t tam_struct_clientOne;
    FILE *arquivo_clientTwo;

    buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

    socket_clientTwo = configura_socket();

    printf("clientTwo online\n\n");

    //Comunicação com o clientOne
    while (1)
    {

        printf("Aguardando solicitações...\n");
        memset(buffer, '\0', SIZE_BUFFER);

        tam_struct_clientOne = sizeof(endereco_clientOne);

        recvfrom(socket_clientTwo, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clientOne, &tam_struct_clientOne);

        printf("Mensagem recebida do clientOne: %s\n", buffer);

        if (bufferVerify(buffer))
        {

            arquivo_clientTwo = fopen(buffer, "rb");
            memset(buffer, '\0', SIZE_BUFFER);

            if (arquivo_clientTwo == NULL)
            {
                buffer[0] = '0';
                sendto(socket_clientTwo, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clientOne, tam_struct_clientOne);
            }
            else
            {
                buffer[0] = '1';
                sendto(socket_clientTwo, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clientOne, tam_struct_clientOne);
                sendPackage(arquivo_clientTwo, socket_clientTwo, endereco_clientOne, tam_struct_clientOne, buffer);
            }

            fclose(arquivo_clientTwo);
        }
    }
}
