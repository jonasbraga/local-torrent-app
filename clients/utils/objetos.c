typedef struct message
{
    int clientPort;
    char file[20];
} message;

//estrutura do pacote
typedef struct pacote
{
    int numseq;       //número de sequência do pacote
    int checksum[8];  //valor do checksum do pacote
    int tam;          //tamanho do pacote
    char dados[1024]; //segmento do pacote
} pacote;