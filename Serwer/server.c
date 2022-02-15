#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SIZE 32768

//Struktura klientow
struct server_udp{
    struct sockaddr_in cli;
    socklen_t cliLen;
    int pipeFd[2];
    char chose[1];
    char filename[50];
};

void write_file(int sockfd, struct sockaddr_in addr,struct server_udp client){
  FILE *fp;
  char *filename = client.filename;
  int n;
  char buffer[SIZE];
  char* confi = "CONFIRMATION";
  int step_read = 0;
  size_t receiving_bite;
  fp = fopen(filename, "w");
  // Receiving the data and writing it into the file.
  printf("\n==============FORK () PRZESYLANIE KLIENT %s===============\n",filename);
  while(1){
    n = read(client.pipeFd[0],buffer,SIZE);
    if (strcmp(buffer, "END") == 0){
      //printf(" \n ILOSC DATAGRAMOW ODEBRANYCH: %d\n",step_read);
      break;
      return;
    }

    //Wczytywanie datagramu do pliku
    // ewentualnie a 
    receiving_bite = fwrite(buffer,sizeof( char ),n,fp);

    bzero(buffer, SIZE);
    step_read++;

    sendto(sockfd, confi, strlen(confi), 0, (const struct sockaddr *)&client.cli, client.cliLen); // Wysłanie danych do klienta
  }

  fclose(fp);
  printf("\n==============FORK () PRZESYLANIE KONIEC %s===============\n",filename);
  return;
}

void send_file_data(int sockfd, struct server_udp client){
  int n;
  FILE *fp;
  char *filename = client.filename;
  int step_read = 0;
  char buffer[SIZE];
  char receiving[100];
  socklen_t addr_size;

  fp = fopen(filename, "r");
  if (fp == NULL){
    perror("[ERROR] reading the file");
    exit(1);}

  //Operacje na plikach
  fseek(fp, 0, SEEK_END);  
  int file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  size_t read_bite;

  printf("\n==============FORK () POBIERANIE KLIENT %s ===============\n",filename);
  // Sending the data
  while((step_read * SIZE)<file_size){
    read_bite= fread( buffer, sizeof( char ), SIZE, fp );
    sleep(1);
    n = sendto(sockfd, buffer, read_bite, 0, (struct sockaddr*)&client.cli, client.cliLen);
    if (n == -1){
      perror("[ERROR] sending data to the server.");
      exit(1);
    }

    //======================
    //Jesli nie otrzyma sie potwierdzniea otrzymania datagramu
    // wysyla sie ponownie go, az do mementu uzyskania odp 
    while((n = read(client.pipeFd[0],receiving,100)) == -1){
      printf("\nWYSLANIE DATAGRAMU JESZCZE RAZ BO NIE ORZYMANO POTWIERDZENIA");
      n = sendto(sockfd, buffer, n, 0, (struct sockaddr*)&client.cli, client.cliLen);
      if (n == -1){
        perror("[ERROR] sending data to the server.");
        exit(1);
      }
    }

    bzero(buffer, SIZE);
    step_read++;
  }

  // Sending the 'END'
  strcpy(buffer, "END");
  sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&client.cli, client.cliLen);

  fclose(fp);
  printf("\n==============FORK () POBIERANIE KONIEC %s ===============\n", filename);
  return;
}

typedef struct server_udp server_udp;

int main(){

  // Defining the IP and Port
  char *ip = "127.0.0.1";
  int port = 8080;

  // Defining variables
  int server_sockfd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_size;
  char buffer[SIZE];
  int e;
  int n;

  //Pomoc przy fork()
  server_udp clients[256];
  server_udp* client;
  int i;
  int exists;
  int nextFreeSlot = 0;

  //====== TWORZENIE GNIAZDA UDP =======
  server_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (server_sockfd < 0){
    perror("[ERROR] socket error");
    exit(1);
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = port;
  server_addr.sin_addr.s_addr = inet_addr(ip);

  e = bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (e < 0){
    perror("[ERROR] bind error");
    exit(1);
  }


  printf("\n[START] Serwer UDP zostal uruchomiony. \n");

for(;;){

  client_addr_size = sizeof(client_addr);
  n = recvfrom(server_sockfd, buffer, SIZE, 0, 
          (struct sockaddr*)&client_addr, &client_addr_size);

  exists = 0;
  for (i = 0; i < 256; i++)
  {
    if (clients[i].cliLen == client_addr_size 
            && memcmp(&clients[i].cli, &client_addr, client_addr_size) == 0)
    {
      if(strlen(clients[i].filename)==0){
        memcpy(&clients[i].filename, &buffer, n);
        pipe(clients[i].pipeFd);

        if(clients[i].chose[0] == 'd'){
          if(fork()==0){
          send_file_data(server_sockfd, clients[i]);
          printf("\nKlient %d zakonczyl dzialanie na %s",i,clients[i].filename);
          exit(0);
          }
        }else if (clients[i].chose[0]  == 'u'){
          if(fork()==0){
          write_file(server_sockfd, client_addr,clients[i]);
          printf("\nKlient %d zakonczyl dzialanie na %s",i,clients[i].filename);
          exit(0);
          }
        }else{
          printf("ERROR");
        }
        exists = 2;
        break;

      }else{

      exists = 1;
      break;
      }

    }
  }
  //Wyslanie danych do procesu
  if(exists == 1){
    
    write(clients[i].pipeFd[1], buffer, n);
    bzero(buffer,SIZE);
    continue;
  }
  //Dodanie obslugiwanego klienta
  if(exists == 0){
    if (nextFreeSlot > 255){
      nextFreeSlot = 0;
    }
    //Dodanie danych do struktury
    memcpy(&clients[nextFreeSlot].cli, &client_addr, client_addr_size);
    memcpy(&clients[nextFreeSlot].chose, &buffer, n);
    clients[nextFreeSlot].cliLen = client_addr_size;
    nextFreeSlot = nextFreeSlot + 1;
    printf("\n[DODANIE] Dodanie Klienta do pamieci\n");

    if(clients[nextFreeSlot-1].chose[0] == 'd'){
      FILE* dir;
      char output[512];
      dir  = popen("ls", "r");  
      bzero(output, 100);
      fread(output,sizeof( char ), 512, dir); 

      sendto(server_sockfd, output, 512, 0, 
            (const struct sockaddr *)&clients[nextFreeSlot-1].cli, clients[nextFreeSlot-1].cliLen);
    
    }else if(clients[nextFreeSlot-1].chose[0] == 'u'){
      char* confi = "[Przesylanie] Serwer jest gotowy ";
    sendto(server_sockfd, confi, strlen(confi), 0, 
          (const struct sockaddr *)&clients[nextFreeSlot-1].cli, clients[nextFreeSlot-1].cliLen);
    }

  }

  bzero(buffer,SIZE);
} // for 


  printf("\n[SUKCES] Dane zostaly odebrane.\n");
  printf("[WYLACZENIE ]\n");

  close(server_sockfd);

  return 0;
}