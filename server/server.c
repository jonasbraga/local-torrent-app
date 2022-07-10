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
#define SIZE_BUFFER 2048

typedef struct segment
{
	int port;
	char file[50];
} segment;

// Função para configurar o servidor
int setupSocket() {
	struct sockaddr_in serverAddress;
	int socketServer;

	socketServer = socket(AF_INET, SOCK_DGRAM, 0);

	if (socketServer < 0)
	{
		perror("Could not connect to server.");
		exit(1);
	}

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddress.sin_port = htons(PORT);

	if (bind(socketServer, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
	{
		perror("Error");
		exit(1);
	}

	return socketServer;
}

// Função para verificar se algum cliente na base de dados
// possui o file, retornando a port de tal cliente.
int searchDatabase(FILE *DB, char *buffer, char *portC)
{
	char port[5];
	char file[60];

	fseek(DB, 0, SEEK_SET);
	while (fscanf(DB, "%s %s", file, port) != EOF)
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
int updateDatabase(FILE *DB, struct segment blk)
{
	char port[5];
	char file[50];

	while (fscanf(DB, "%s %s", file, port) != EOF)
	{
		if (strcmp(file, blk.file) == 0 && blk.port == atoi(port))
			return 0;
	}

	fprintf(DB, "%s %d\n", blk.file, blk.port);
	fflush(DB);

	return 0;
}

int checkBuffer(char *buffer)
{
	if (buffer[0] != '\0')
		return 1;

	return 0;
}

int main(int argc, char *argv[]) {

	int socketServer;

	struct sockaddr_in clientA;
	socklen_t sizeClientA;

	char clientFile[50];
	char *buffer = (char *)malloc(SIZE_BUFFER * sizeof(char));

	FILE *DB;
	DB = fopen("db.txt", "r+b");
	if (DB == NULL) {
		printf("Failed open file db\n");
		exit(1);
	}

	// configura o socket do server
	socketServer = setupSocket();
	printf("Server is running\n\n");

	// Comunicação com o cliente One
	while (1) {

		memset(buffer, '\0', SIZE_BUFFER);

		sizeClientA = sizeof(clientA);

		recvfrom(socketServer, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&clientA, &sizeClientA);
		printf("Receive!\n");

		// caso o buffer tenha conteúdo
		if (checkBuffer(buffer)) {

			// Caso o file requisitado esteja ligado a algum
			// cliente na base de dados, envia a port desse cliente
			// e espera um pedido do cliente One para atualizar o banco
			if (searchDatabase(DB, buffer, clientFile)) {

				memset(buffer, '\0', SIZE_BUFFER);
				buffer[0] = '1';
				strcat(buffer, clientFile);
				printf("Sending\n");
				sendto(socketServer, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&clientA, sizeClientA);

				// segment que possui a port do cliente One, e o file
				// que possui agora
				printf("Waiting\n");
				segment blk;
				while (1) {
					memset(&blk, 0, sizeof(segment));
					recvfrom(socketServer, &blk, sizeof(blk), 0, (struct sockaddr *)&clientA, &sizeClientA);
					updateDatabase(DB, blk);
					sendto(socketServer, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientA, sizeClientA);
					printf("Database updated\n");
					break;
				}
			}
			// Caso ninguem tenha o file, retorna um nak
			else {
				memset(buffer, '\0', SIZE_BUFFER);
				buffer[0] = '0';
				sendto(socketServer, buffer, SIZE_BUFFER, 0, (struct sockaddr *)&clientA, sizeClientA);
			}
		}
	}
}
