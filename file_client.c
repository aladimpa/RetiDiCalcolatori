#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>

void socketskip(int fd, int count) {
  char buf;
  for (int i = 0; i < count; i++)
    recv(fd, &buf, 1, MSG_WAITALL);
    /*int recv (int descrittore_socket, const void* buffer, int dimensione_buffer, unsigned int opzioni)
    Serve a ricevere messaggi da un socket e puo' essere usato solamente 
    in presenza di una connessione. Il risultato della chiamata a questa 
    funzione, in caso di errore, e' "-1", altrimenti e' il numero di 
    caratteri ricevuti. Il messaggio ottenuto e' contenuto nella memoria 
    puntata da "buffer". Il parametro "len" non e' altro che la dimensione 
    del buffer. "opzioni" puo' essere posto a "0". */
}

void _socket_newline(int fd, int skip) {
  char resp;
  for (;;) {
    recv(fd, &resp, 1, 0);
    if (resp == '\n')
      break;
    if (skip)
      continue;
    printf("%c", resp);
  }
  printf("\n");
}

void skip_until_newline(int fd) { _socket_newline(fd, 1); }

void echo_until_newline(int fd) { _socket_newline(fd, 0); }


int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "USAGE: %s SERVER PORT <command>\n", argv[0]);
        exit(1);
    }

    //Check port number
    if (atoi(argv[2]) < 1 || atoi(argv[2]) > (1 << 16)) {
        fprintf(stderr, "Invalid port number %d\n", atoi(argv[2]));
        exit(1);
    }
    
    // Get the address
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo)); // Zero out the hints structure

    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = 0; // No flags
    hints.ai_protocol = 0; // Automatic protocol selection

    struct addrinfo *result; // Structure to store the result in

    // DNS query
    if (getaddrinfo(argv[1], argv[2], &hints, &result) == -1) {
        perror("Could not find address");
        exit(1);
    }

    int sock;
    struct addrinfo* rp;

    /*
    getaddrinfo returns a linked list of possible addresses
    Try each address and return if successful
    */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        // Create a socket on the found address
        if ((sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
         continue;
        }
        // Check if a connection is possible
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == -1) {
          close(sock); // The connection failed, close the socket
        } else {
          break; // The connection was successful
        }
    }
    if (rp == NULL) {
        // If we tried all addresses and none connected, fail
        fprintf(stderr, "No address was found\n");
        exit(1);
    }

    //handle commands (gestione dei comandi)
    int command_ok = 0;
    // se il comando è ls allora argv[3] deve essere lungo 2 e deve essere ovviamente ls
    // strncmp(s1, s2, n) = confronta s1 e s2 le quali devono essere al massimo di n caratteri
    

    if (strlen(argv[3])==2 && strncmp(argv[3], "LS", 2) == 0) {
        printf("ENTRA LS");
        dprintf(sock, "LS\n");
        socketskip(sock, 11);

        char file_size_buf[256];
        memset(&file_size_buf, '\0', 256);
        char charbuf;

        for (int file_size_length = 0;; file_size_length++) {
            recv(sock, &charbuf, 1, 0);
            if (charbuf == '\n') {
                break;
            }
            file_size_buf[file_size_length] = charbuf;
        }

        int direntries = atoi(file_size_buf);
        
        // Directory listing
        char recvb;
        int received_entries = 0;
        for (;;) {
            unsigned int recv_len = recv(sock, &recvb, 1, MSG_WAITALL);
            printf("%c", recvb);
            if (recvb == '\n') {
                received_entries++;
                if (received_entries == direntries)
                    break;
            }
            if (recv_len < 1) {
                break;
            }
        }

        command_ok = 1;
    }
    if (strlen(argv[3]) == 6 && strncmp(argv[3], "UPLOAD", 6) == 0) {
        printf("entra UPLOAD");
        if (argc < 5) {
            fprintf(stderr, "USAGE: %s SERVER PORT upload FILENAME\n", argv[0]);
            exit(1);
        }
        
        // Get file data
        struct stat file_info;
        if (stat(argv[4], &file_info) == -1) {
        perror("Could not stat file");
        exit(EXIT_FAILURE);
        }
        // Open file
        FILE *file_to_upload = fopen(argv[4], "r");
        if (file_to_upload == NULL) {
            perror("Could not open file");
            exit(EXIT_FAILURE);
        }
        // Upload file
        dprintf(sock, "UPLOAD %s SIZE %ld\n", basename(argv[4]), file_info.st_size);
        while (!feof(file_to_upload)) {
            char buf[128];
            unsigned int read_bytes = fread(&buf, sizeof(char), 128, file_to_upload);
            send(sock, &buf, read_bytes, 0);
        }
        dprintf(sock, "\n");
        
        // Get result
        echo_until_newline(sock);
        
        // Cleanup
        fclose(file_to_upload);
        command_ok = 1;
    }
    if (strlen(argv[3]) == 8 && strncmp(argv[3], "DOWNLOAD", 8) == 0) {
        printf("entra download");
        if (argc < 5) {
            fprintf(stderr, "USAGE: %s SERVER PORT download FILENAME\n", argv[0]);
            exit(1);
        }
        // Perform request
        dprintf(sock, "DOWNLOAD %s\n", basename(argv[4]));
        // Check for error
        char errbuf[3];
        recv(sock, &errbuf, 3, MSG_WAITALL);
        if (memcmp(&errbuf, "ERR", 3) == 0) {
            fprintf(stderr, "ERROR");
            echo_until_newline(sock);
            exit(1);
        }
        // Consume until "E SIZE "
        socketskip(sock, 7);
        // Get file size
        char file_size_buf[256];
        memset(&file_size_buf, '\0', 256);
        char charbuf;

        for (int file_size_length = 0;; file_size_length++) {
            recv(sock, &charbuf, 1, 0);
            if (charbuf == '\n') {
                break;
            }
            file_size_buf[file_size_length] = charbuf;
        }
        unsigned int file_size = atoi(file_size_buf);
        // Copy file
        FILE *file = fopen(basename(argv[4]), "w");
        if (file == NULL) {
            perror("Could not open file");
            exit(1);
        }
        while (file_size > 0) {
            int buf_size = 128;
            if (buf_size > (int)file_size) {
                buf_size = file_size;
            }
            char buf[buf_size];
            ssize_t received_bytes = recv(sock, &buf, buf_size, MSG_WAITALL);
            fwrite(&buf, sizeof(char), received_bytes, file);
            file_size -= received_bytes;
            printf("Received %ld bytes, remaining %d bytes\n", received_bytes, file_size);
        }
        fclose(file);
        command_ok = 1;
    }
    if (!command_ok) {
        fprintf(stderr, "Unknown command\n");
        exit(EXIT_FAILURE);
    } else {
        dprintf(sock, "EXIT\n");
    }

    // Close socket
    freeaddrinfo(result);
    if (sock != -1)
    shutdown(sock, SHUT_RDWR);
}