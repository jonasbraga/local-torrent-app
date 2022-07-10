# TRABALHO 02 
### COM240 - Redes de Computadores
### Grupo 02
1. Matheus Robusti Henriques Marqui - 2019007055
2. Gabriel Mussolini de Moraes - 2020007960
3. Jonas Henrique Santos Braga - 2020000057
4. Antonio Bittencourt Pagotto - 2020032785

## 1. Sobre o projeto
O projeto é uma entrega do Trabalho 02 da matéria COM240 - Redes de Computadores da Universidade Federal de Itajubá. O projeto é uma simulação de uma aplicação P2P para a transferência local de arquivos, assim como, ocorre em aplicações como Torrent.

O projeto foi desenvolvido em linguagem C, e executa a transferência de arquivos entre dois clientes, conectados em um mesmo servidor, o qual possui as informaçõe sobre os nós que possuem o arquivo desejado.
## 2. Execução

Para a execução do codigo você deverá seguir os passos abaixo:

### 2.1. Clone do repositorio

Execute o comando a seguir para clonar o repositorio:

```terminal
    git clone https://github.com/jonasbraga/local-torrent-app.git
```

### 2.2 Remover Git Remote

Execute o comando abaixo para remove o link com o repositorio
```terminal
    git remote remove origin
```

### 2.3 Terminal como servidor

Execute o camando abaixo para ligar o servidor da aplicação:

```terminal
    cd server

    gcc server.c -o server && ./server
```

### 2.4 Terminal como cliente

Execute o comando abaixo para abrir o cliente dois

```terminal
    cd clients/Two

    cc clientTwo.c -o clientTwo
```

Execute o comando abaixo para abrir o cliente um

```terminal
    cd clients/One

    cc clientOne.c -o clientOne
```

### 2.5 Transferindo os arquivos

Para fazer a transferência de um arquivo presente no cliente dois para o cliente um, execute o comando abaixo no terminal do cliente um:

```terminal
    ./clientOne nomeDoArquivoPresenteNoClienteDois
```

Para fazer a transferência de um arquivo presente no cliente um para o cliente dois, execute o comando abaixo no terminal do cliente dois:

```terminal
    ./clientTwo nomeDoArquivoPresenteNoClienteUm
```

### 3. Verificando a transação do arquivo

Para finalizar, o jeito mais rapido para saber se o arquivo foi transferido, é olhando a pasta do cliente, porém podemos olhar o arquivo db.txt na raiz do projeto, ela apresenta o nome do arquivo transferido e o numero da porta, ele apresentará o exmplo abaixo: 

```txt
NOME DO ARQUIVO | PORTA
senna.jpg 3002
Trabalho_2.pdf 3002
vai-dar-merda.mp4 3002
metro.jpeg 3002
```