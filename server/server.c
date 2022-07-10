#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define PORT 3000
#define SIZE_BUFFER 1024

typedef struct segment
{
	int port;
	char file[50];
} segment;

// Função para configurar o servidor
int configura_socket()
{
	struct sockaddr_in server_address;
	int socket_serv;

	socket_serv = socket(AF_INET, SOCK_DGRAM, 0);

	if (socket_serv < 0)
	{
		perror("Error");
		exit(1);
	}

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_address.sin_port = htons(PORT);

	if (bind(socket_serv, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
	{
		perror("Error");
		exit(1);
	}

	return socket_serv;
}

// Função para verificar se algum cliente na base de dados
// possui o file, retornando a port de tal cliente.
int searchInDb(FILE *BD, char *buffer, char *portC)
{

	char port[5];
	char file[50];

	fseek(BD, 0, SEEK_SET);
	while (fscanf(BD, "%s %s", file, port) != EOF)
	{
		if (strcmp(file, buffer) == 0)
		{
			strcpy(portC, port);
			return 1;
		}
	}

	return 0;
}

// Função para atualizar o banco, primeiro verifica
// primeiro verifica se a informação já está presente
// caso não esteja, o banco é atualizado
int saveInDatabase(FILE *BD, struct segment blk)
{
	char port[5];
	char file[50];

	while (fscanf(BD, "%s %s", file, port) != EOF)
	{
		if (strcmp(file, blk.file) == 0 && blk.port == atoi(port))
			return 0;
	}

	fprintf(BD, "%s %d\n", blk.file, blk.port);
	fflush(BD);

	return 0;
}

int verifyBuffer(char *buffer)
{
	if (buffer[0] != '\0')
		return 1;

	return 0;
}

int main(int argc, char *argv[])
{

	int socket_serv;

	struct sockaddr_in clientA;
	socklen_t sizeClientA;

	char clientFile[50];
	char *buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

	FILE *BD;
	BD = fopen("db.txt", "r+b");
	if (BD == NULL)
	{
		printf("Failed open file db\n");
		exit(1);
	}

	// configura o socket do server
	socket_serv = configura_socket();
	printf("Server is running\n\n");

	// Comunicação com o cliente A
	while (1)
	{

		memset(buffer, '\0', SIZE_BUFFER);

		sizeClientA = sizeof(clientA);

		recvfrom(socket_serv, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&clientA, &sizeClientA);
		printf("Receive!\n");

		// caso o buffer tenha conteúdo
		if (verifyBuffer(buffer))
		{

			// Caso o file requisitado esteja ligado a algum
			// cliente na base de dados, envia a port desse cliente
			// e espera um pedido do cliente A para atualizar o banco
			if (searchInDb(BD, buffer, clientFile))
			{

				memset(buffer, '\0', SIZE_BUFFER);
				buffer[0] = '1';
				strcat(buffer, clientFile);
				printf("Sending\n");
				sendto(socket_serv, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&clientA, sizeClientA);

				// segment que possui a port do cliente A, e o file
				// que possui agora
				printf("Waiting\n");
				segment blk;
				while (1)
				{
					memset(&blk, 0, sizeof(segment));
					recvfrom(socket_serv, &blk, sizeof(blk), 0, (struct sockaddr *)&clientA, &sizeClientA);
					saveInDatabase(BD, blk);
					sendto(socket_serv, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientA, sizeClientA);
					printf("Database updated\n");
					break;
				}
			}
			// Caso ninguem tenha o file, retorna um nak
			else
			{
				memset(buffer, '\0', SIZE_BUFFER);
				buffer[0] = '0';
				sendto(socket_serv, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&clientA, sizeClientA);
			}
		}
	}
}
