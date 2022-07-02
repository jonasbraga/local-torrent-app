#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close() */
#include <string.h> /* memset() */
#include <stdlib.h>

#define PORTA_SERVIDOR 1500
#define IP_LOCAL "127.0.0.1"
#define SIZE_BUFFER 1024

typedef struct segmento
{
	int porta;
	char arquivo[50];
} segmento;


// Função para configurar o servidor
int configura_socket()
{
	struct sockaddr_in endereco_serv;
	int socket_serv;

	socket_serv = socket(AF_INET, SOCK_DGRAM, 0);

	if (socket_serv < 0)
	{
		perror("Error");
		exit(1);
	}

	endereco_serv.sin_family = AF_INET;
	endereco_serv.sin_addr.s_addr = inet_addr(IP_LOCAL);
	endereco_serv.sin_port = htons(PORTA_SERVIDOR);

	if (bind(socket_serv, (struct sockaddr *)&endereco_serv, sizeof(endereco_serv)) < 0)
	{
		perror("Error");
		exit(1);
	}

	return socket_serv;
}

//Função para verificar se algum cliente na base de dados
//possui o arquivo, retornando a porta de tal cliente.
int busca_no_banco(FILE *BD, char *buffer, char *portaC)
{

	char porta[5];
	char arquivo[50];
	
	fseek(BD, 0, SEEK_SET);
	while (fscanf(BD, "%s %s", arquivo, porta) != EOF)
	{
		if (strcmp(arquivo, buffer) == 0)
		{
			strcpy(portaC, porta);
			return 1;
		}
	}

	return 0;
}

//Função para atualizar o banco, primeiro verifica 
//primeiro verifica se a informação já está presente
//caso não esteja, o banco é atualizado
int atualiza_banco(FILE *BD, struct segmento blk)
{
	char porta[5];
	char arquivo[50];

	while (fscanf(BD, "%s %s", arquivo, porta) != EOF)
	{
		if (strcmp(arquivo, blk.arquivo) == 0 && blk.porta == atoi(porta))
			return 0;
	}

	fprintf(BD, "%s %d\n", blk.arquivo, blk.porta);
	fflush(BD);

	return 0;
}

int verifica_buffer(char *buffer)			
{
	if (buffer[0] != '\0')
		return 1;
	
	return 0;
}

int main(int argc, char *argv[])
{

	int socket_serv;

	struct sockaddr_in endereco_clienteA;
	socklen_t tam_clienteA;

	char cliente_com_arquivo[50];
	char *buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

	FILE *BD;
	BD = fopen("database.txt", "r+b");
	if (BD == NULL)
    {
        printf("Error - Arquivo não pôde ser aberto\n");
        exit(1);
    }

	//configura o socket do server
	socket_serv = configura_socket();
	printf("Servidor online\n\n");

	//Comunicação com o cliente A
	while (1)
	{

		memset(buffer, '\0', SIZE_BUFFER);

		tam_clienteA = sizeof(endereco_clienteA);

		recvfrom(socket_serv, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clienteA, &tam_clienteA);
		printf("Requisição recebida!\n");

		//caso o buffer tenha conteúdo
		if (verifica_buffer(buffer))
		{

			//Caso o arquivo requisitado esteja ligado a algum
			//cliente na base de dados, envia a porta desse cliente
			//e espera um pedido do cliente A para atualizar o banco
			if (busca_no_banco(BD, buffer, cliente_com_arquivo))
			{
				
				memset(buffer, '\0', SIZE_BUFFER);
				buffer[0] = '1';
				strcat(buffer, cliente_com_arquivo);
				printf("Enviando resposta ao cliente\n");
				sendto(socket_serv, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clienteA, tam_clienteA);

				//segmento que possui a porta do cliente A, e o arquivo
				//que possui agora
				printf("esperando resposta\n");
				segmento blk;
				while (1)
				{
					memset(&blk, 0, sizeof(segmento));
					recvfrom(socket_serv, &blk, sizeof(blk), 0, (struct sockaddr *)&endereco_clienteA, &tam_clienteA);
                    atualiza_banco(BD, blk);
					sendto(socket_serv, buffer, sizeof(buffer), 0, (struct sockaddr *)&endereco_clienteA, tam_clienteA);
					printf("Banco de dados atualizado\n");
					break;
				}
			}
			//Caso ninguem tenha o arquivo, retorna um nak
			else
			{
				memset(buffer, '\0', SIZE_BUFFER);
				buffer[0] = '0';
				sendto(socket_serv, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&endereco_clienteA, tam_clienteA);
			}
		}
	}
}
	
