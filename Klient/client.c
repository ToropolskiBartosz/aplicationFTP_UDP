#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#define SIZE 32768

void timeout(int sockfd, int sec, int m_sec){
  struct timeval tv;
  tv.tv_sec = sec;  /* 10 Sek Timeout */
  tv.tv_usec = m_sec;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv,sizeof(tv))) {        
    perror("setsockopt: rcvtimeo");
    exit(1);
  }
}

void send_file_data(FILE *fp, int sockfd, struct sockaddr_in addr){
  int n;
  int step_read = 0;
  char buffer[SIZE];
  char receiving[100];
  socklen_t addr_size;

  //Operacje na plikach
  fseek(fp, 0, SEEK_END);  
  int file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  size_t read_bite;
  printf("Wielkosc pliku: %d",file_size);

  printf("[Wysylanie] ");

  // Wysylanie danych 
  while((step_read * SIZE)<file_size){
    read_bite= fread( buffer, sizeof( char ), SIZE, fp );
    printf("\n#");

    sleep(1);
    n = sendto(sockfd, buffer, read_bite, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (n == -1){
      perror("[ERROR] sending data to the server.");
      exit(1);
    }

    //======================
    //Jesli nie otrzyma sie potwierdzniea otrzymania datagramu
    // wysyla sie ponownie go, az do mementu uzyskania odp 
    while((n=(recvfrom(sockfd, receiving, 100, 0, NULL, NULL))) == -1){

      n = sendto(sockfd, buffer, read_bite, 0, (struct sockaddr*)&addr, sizeof(addr));
      if (n == -1){
        perror("[ERROR] sending data to the server.");
        exit(1);
      }
    }

    bzero(buffer, SIZE);
    step_read++;
  }

  // Wysylanie 'END'
  strcpy(buffer, "END");
  sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&addr, sizeof(addr));
  printf(" ILOSC DATAGRAMOW WYSLANYCH: %d",step_read);
  fclose(fp);
  return;
}

void write_file(int sockfd, struct sockaddr_in addr, char* buffer_filename){
  FILE *fp;
  char *filename = buffer_filename;
  int n;
  char* confi = "CONFIRMATION";
  int step_read = 0;
  char buffer[SIZE];
  char exp[1];
  socklen_t addr_size;
  size_t receiving_bite;

  // Tworzenie pliku
  fp = fopen(filename, "w");
  printf("[POBIERANIE]: ");
  // Odbieranie pliku 
  while(1){
    addr_size = sizeof(addr);
    n = recvfrom(sockfd, buffer, SIZE, 0, (struct sockaddr*)&addr, &addr_size);
    printf("\n#");

    if (strcmp(buffer, "END") == 0){
      printf(" \n ILOSC DATAGRAMOW ODEBRANYCH: %d",step_read);
      break;
      return;
    }

    //Wczytywanie datagramu do pliku
    receiving_bite = fwrite(buffer,sizeof( char ),n,fp);

    bzero(buffer, SIZE);
    step_read++;
    sendto(sockfd, confi, strlen(confi), 0, (const struct sockaddr *)&addr, addr_size); // Wysłanie danych do klienta
  }

  fclose(fp);
  return;
}


int main(){

  // Defining the IP and Port
  char *ip = "127.0.0.1";
  int port = 8080;

  // Defining variables
  int server_sockfd;
  socklen_t addr_size;
  char buffer[SIZE];
  struct sockaddr_in server_addr;
  char chose[2], filename[30],file[30];
  FILE *fp;
  // Creating a UDP socket
  server_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (server_sockfd < 0){
    perror("[ERROR] socket error");
    exit(1);
  }
  //Okreslenie czasu oczekiwania na otrzymanie wiadomosci 
  timeout(server_sockfd,15,0);

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = port;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  int i;
  
  bzero(chose,sizeof(chose));
  bzero(filename,sizeof(filename));
  printf("Wybierz operacje \n");
  scanf("%s",chose);
  sendto(server_sockfd, chose, strlen(chose), 0, 
        (struct sockaddr*)&server_addr, sizeof(server_addr));
  addr_size = sizeof(server_addr);
  recvfrom(server_sockfd, buffer, SIZE, 0, (struct sockaddr*)&server_addr, &addr_size);
  printf("\n[SERWER]: %s",buffer);

   if(chose[0] == 'd'){
      printf("\nPodaj plik do pobrania: \n");
      scanf("%s",filename);
      memcpy(file, filename, strlen(filename)+1);
      printf("\nPobierany plik to : %s ",filename); 

      sendto(server_sockfd, filename, strlen(filename), 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));

     write_file(server_sockfd,server_addr,file);
    
  }else if(chose[0] == 'u'){
    printf("\nPodaj plik do przeslania: \n");
    scanf("%s",filename);
    //memcpy(file, filename, strlen(filename)+1);
    printf("\nPrzesylany plik : %s ",filename); 

    fp = fopen(filename, "r");
    if (fp == NULL){
      perror("[ERROR] reading the file");
      exit(1);
    }else{
    sendto(server_sockfd, filename, strlen(filename), 0, 
          (struct sockaddr*)&server_addr, sizeof(server_addr));

    send_file_data(fp, server_sockfd, server_addr);
    }
  }

  bzero(chose,sizeof(chose));
  bzero(filename,sizeof(filename));
  printf("\n[SUKCES] Transfer plikow zakonczony.\n");
  printf("[ZAMYKANIE]\n");
  close(server_sockfd);
  return 0;
}