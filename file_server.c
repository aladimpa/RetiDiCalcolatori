#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct user_data_s {
  char *data;
  unsigned int length;
  unsigned int size;
} user_data_t;

typedef struct new_connection_s {
  int fd;
} new_connection_t;

void check_pointer(void *ptr) {
  if (ptr == NULL) {
    fprintf(stderr, "Failed a memory allocation\n");
    exit(1);
  }
}

user_data_t *get_line(int fd) {
  // Allocate initial structure
  user_data_t *line = (user_data_t *)malloc(sizeof(user_data_t));
  check_pointer(line);
  line->data = (char *)calloc(1, sizeof(char));
  check_pointer(line->data);
  line->length = 0;
  line->size = 1;
  // Receive a line
  for (;;) {
    // Receive a single character
    recv(fd, line->data + line->length, sizeof(char), 0);
    line->length++;
    // Exit on newline character
    if (line->data[line->length - 1] == '\n')
      break;
    // Relocate array if needed
    if (line->length == line->size) {
      char *old_data = line->data;
      unsigned int old_size = line->size;

      line->size *= 2;
      line->data = (char *)calloc(line->size, sizeof(char));
      check_pointer(line->data);
      memcpy(line->data, old_data, old_size);

      free(old_data);
    }
  }
  // Print comand
  char *debug_print = strdup(line->data);
  for (unsigned int i = 0; i < strlen(debug_print); i++) {
    if (debug_print[i] == '\n') {
      debug_print[i] = '\0';
    }
  }
  printf("Got a command of %d bytes (%s)\n", line->length, debug_print);
  free(debug_print);
  // Return
  return line;
}

DIR *open_files_dir() {
  DIR *files_dir = opendir("files");
  // Create the directory if it does not exist
  if (files_dir == NULL && errno == ENOENT) {
    if (mkdir("files", S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP |
                           S_IXGRP) == -1) {
      perror("Could not create files directory");
      exit(EXIT_FAILURE);
    }
    files_dir = opendir("files");
  }
  // Handle an error
  if (files_dir == NULL) {
    perror("Could not open files directory");
    exit(EXIT_FAILURE);
  }
  // Return the dir
  return files_dir;
}

void command_upload(int fd, user_data_t *command) {
    strtok(command->data, " ");
    char *filename = strtok(NULL, " ");

    if (filename == NULL) {
        const char msg[] = "ERR Filename not specified\n";
        send(fd, &msg, sizeof(msg), 0);
        return;
    }
    strtok(NULL, " ");
    char *filesize_string = strtok(NULL, " ");

    if (filesize_string == NULL) {
        const char msg[] = "ERR Size not provided\n";
        send(fd, &msg, sizeof(msg), 0);
        return;
    }
    unsigned int filesize = atoi(filesize_string);

    if (filesize < 1) {
        const char msg[] = "ERR Size invalid\n";
        send(fd, &msg, sizeof(msg), 0);
        return;
    }
    printf("Upload: %s\n", filename);

    // Get path
    char *path = (char *)calloc(1 + 6 + strlen(filename), sizeof(char));
    strcat(path, "files");
    strcat(path, "/");
    strcat(path, filename);

    // Return the file
    FILE *file_to_receive = fopen(path, "w");
    free(path);

    if (file_to_receive == NULL) {
        perror("Could not open file");
        const char err[] = "ERR Server error\n";
        send(fd, err, sizeof(err), 0);
        return;
    }
    ssize_t read_size;

    for (;;) {
        unsigned int max_buffer_size = 512;
        if (filesize < max_buffer_size)
        max_buffer_size = filesize;

        char *buf = (char *)calloc(max_buffer_size, sizeof(char));

        read_size = recv(fd, buf, max_buffer_size, MSG_WAITALL);
        if (fwrite(buf, sizeof(char), read_size, file_to_receive) < (long unsigned int)read_size) {
            printf("Warning: short write, file will end up corrupted");
        }

        free(buf);

        printf("Received %ld bytes, waiting for %d bytes before closing\n", read_size, filesize);

        filesize -= read_size;
        if (filesize == 0)
            break;
    }

    fclose(file_to_receive);

    char unused;
    for (;;) {
        recv(fd, &unused, 1, MSG_WAITALL);
        if (unused == '\n')
            break;
    }

    printf("Received OK\n");
    dprintf(fd, "RECEIVED OK\n");
}

void command_download(int fd, user_data_t *command) {

  strtok(command->data, " ");
  char *filename = strtok(NULL, " ");
  filename[strlen(filename) - 1] = '\0'; // Terminate the command string

  printf("Download: %s\n", filename);

  // Check if filename has been provided
  if (filename == NULL) {
    const char msg[] = "ERR Filename not specified\n";
    send(fd, &msg, sizeof(msg), 0);
    return;
  }

  // Check if file exists
  struct stat file_info;

  char *path = (char *)calloc(1 + 6 + strlen(filename), sizeof(char));
  strcat(path, "files");
  strcat(path, "/");
  strcat(path, filename);

  if (stat(path, &file_info) == -1) {
    perror("Failed to stat file");
    if (errno == ENOENT) {
      const char err[] = "ERR File not found\n";
      send(fd, err, sizeof(err), 0);
    } else {
      const char err[] = "ERR Server error\n";
      send(fd, err, sizeof(err), 0);
    }
    return;
  }

  // Return the file
  FILE *file_to_send = fopen(path, "r");
  free(path);

  if (file_to_send == NULL) {
    perror("Could not open file");
    const char err[] = "ERR Server error\n";
    send(fd, err, sizeof(err), 0);
    return;
  }

  dprintf(fd, "FILE SIZE %ld\n", file_info.st_size);

  ssize_t read_size;
  char buf[64];
  while ((read_size = fread(&buf, sizeof(char), 64, file_to_send)) > 0) {
    send(fd, &buf, read_size, 0);
  }
  fclose(file_to_send);

  dprintf(fd, "\n");
}

void *handle_connection(void *arg) {
    new_connection_t *conn = (new_connection_t *)arg;
    for(;;){
        user_data_t *command = get_line(conn->fd);
        if (command->length > 2 && (memcmp(command->data, "LS", 2) == 0)) {
            DIR *files_dir = open_files_dir();
            //La funzione telldir() ritorna la locazione corrente associata con il directory stream dir.
            long initial_pos = telldir(files_dir);
            struct dirent *direntry;
            // Count the files
            unsigned int files_count = 0;
            for (;;) {
                // Get the next direntry
                direntry = readdir(files_dir);
                if (direntry == NULL)
                    break;
                // Check if file is ok
                if (strlen(direntry->d_name) == 1 && direntry->d_name[0] == '.')
                    continue;
                if (strlen(direntry->d_name) == 2 && direntry->d_name[0] == '.' && direntry->d_name[1] == '.')
                    continue;
                files_count++;
            }
            dprintf(conn->fd , "FILES COUNT %d\n", files_count);
            // Restore cursor
            seekdir(files_dir, initial_pos);
            // List the contents
            for (;;) {
                // Get the next direntry
                direntry = readdir(files_dir);
                if (direntry == NULL)
                break;
                // Check if file is ok
                if (strlen(direntry->d_name) == 1 && direntry->d_name[0] == '.')
                continue;
                if (strlen(direntry->d_name) == 2 && direntry->d_name[0] == '.' &&
                    direntry->d_name[1] == '.')
                continue;
                // Send file name
                dprintf(conn->fd, "%s\n", direntry->d_name);
            }
            // Close the directory
            closedir(files_dir);
        } else if (command->length > 6 && (memcmp(command->data, "UPLOAD", 6) == 0)) {

            command_upload(conn->fd, command);

        } else if (command->length > 8 && (memcmp(command->data, "DOWNLOAD", 8) == 0)) {
            
            command_download(conn->fd, command);

        } else if (command->length > 4 && (memcmp(command->data, "EXIT", 4) == 0)) {
            const char bye[] = "BYE\n";
            send(conn->fd, bye, sizeof(bye), 0);
            break;
        } else {
            const char error[] = "UNKNOWN COMMAND\n";
            send(conn->fd, &error, sizeof(error), 0);
        }
        free(command);
    }
    free(conn);
    printf("Thread ending\n");
    return NULL;
}

int main(int argc, char *argv[]) {
    //Check arguments
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s PORT\n", argv[0]);
        exit(1);
    }

    // Check port number
    if (atoi(argv[1]) < 1 || atoi(argv[1]) > (1 << 16)) {
        fprintf(stderr, "Invalid port number %d\n", atoi(argv[1]));
        exit(EXIT_FAILURE);
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    // Bind socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(argv[1]));
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&server_address, sizeof(struct sockaddr_in)) == -1) {
        perror("Could not bind");
        exit(EXIT_FAILURE);
    }

    // Start listening
    if (listen(sock, 1 << 8) == -1) {
        perror("Could not listen");
        exit(EXIT_FAILURE);
    }

    //Gestione dei parametri
    for (;;) {
        new_connection_t *new_conn = (new_connection_t *)malloc(sizeof(new_connection_t));
        check_pointer(new_conn);
        // Accept connection
        new_conn->fd = accept(sock, NULL, NULL);
        if (new_conn->fd == -1) {
            perror("Could not accept connection");
            free(new_conn);
            continue;
        }
        // Handle connection
        pthread_t thread;
        pthread_create(&thread, NULL, &handle_connection, new_conn);
    }
}
