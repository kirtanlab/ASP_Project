#include <ftw.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <ctype.h>
#include <regex.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>

#define FTW_PHYS 1 // this is already defined in ftw.h but for some reaseon intellisense doesn't detect it.
// for some reason FTW struct in ftw.h is not being imported so making this same struct from ftw.h
struct FTW
{
    int base;
    int level;
};
char *file_paths[10000]; // to hold file paths of files to tar
int no_of_files = 0;     // to determine how many files to tar
int tar_file_size;       // to hold tar file size

int file_size_bytes; // to hold file size while downloading

char *file_names[10000]; // to hold file names of files during display
int no_of_files_display; // no of files in file_name

// Smain
#define BACKLOG 10 // maximum 10 concurrent connections
int socket_sd, client_sd, PORT;
struct sockaddr_in server; // smain server

// port & ip on which spdf socket is created

// char SPDF_IP[] = "127.0.1.1"; // ip address localhost
char SPDF_IP[] = "127.0.0.1"; // ip address

int spdf_sd, SPDF_PORT;
struct sockaddr_in spdf_server;

// port & ip on which stext socket is created
// char STEXT_IP[] = "127.0.1.1"; // ip address localhost
char STEXT_IP[] = "127.0.0.1"; // ip address

int stext_sd, STEXT_PORT;
struct sockaddr_in stext_server;

char SZIP_IP[] = "127.0.0.1"; // ip address

int szip_sd, SZIP_PORT;
struct sockaddr_in szip_server;

int VALID_EXTENSIONS_LENGTH = 4;                            // number of custom commands
int selected_extension;                                     // will map to valid_extensions
char *valid_extensions[4] = {".c", ".pdf", ".txt", ".zip"}; // add ".zip" here

// CONFIG FUNCTIONS

void connect_to_szip()
{
    // listening socket
    szip_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (szip_sd == -1)
    {
        printf("Socket failure!\n");
        exit(-1);
    }

    // initialize server
    szip_server.sin_family = AF_INET;
    szip_server.sin_port = htons(SZIP_PORT);
    szip_server.sin_addr.s_addr = inet_addr(SZIP_IP); // add IP address of server

    // connect to server
    if (connect(szip_sd, (struct sockaddr *)&szip_server, sizeof(szip_server)) < 0)
    {
        printf("Connection failure!\n");
        exit(-1);
    }
}

// shows manual page
void show_docs()
{
    char *buffer = malloc(sizeof(char) * 2500);

    int man_fd = open("smain_man_page.txt", O_RDONLY);

    if (man_fd == -1)
        exit(-1);

    read(man_fd, buffer, 2500);

    printf("%s\n", buffer);

    close(man_fd);
    free(buffer);
    exit(0);
}

// smain server socket setup
void smain_socket_setup()
{
    // if socket fails show error and exit
    // AF_INET for internet & SOCK_STREAM for TCP connection
    // socket_fd is the listening socket
    socket_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_sd == -1)
    {
        printf("Socket failure!\n");
        exit(-1);
    }

    // make a server via socketaddr_in struct
    // add port, internet option and in ip address as any ip address
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY); // ip address of system

    // bind socket to the server
    if ((bind(socket_sd, (struct sockaddr *)&server, sizeof(server))) == -1)
    {
        printf("Socket Binding Failed!\n");
        exit(-1);
    }

    // listen to requests on socket
    if ((listen(socket_sd, BACKLOG)) == -1)
    {
        printf("Listening on Socket Failed!\n");
        exit(-1);
    }
}

// connect to spdf
void connect_to_spdf()
{
    // listening socket
    spdf_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (spdf_sd == -1)
    {
        printf("Socket failure!\n");
        exit(-1);
    }

    // initialize server
    spdf_server.sin_family = AF_INET;
    spdf_server.sin_port = htons(SPDF_PORT);
    spdf_server.sin_addr.s_addr = inet_addr(SPDF_IP); // add IP address of server

    // connect to server
    if (connect(spdf_sd, (struct sockaddr *)&spdf_server, sizeof(spdf_server)) < 0)
    {
        printf("Connection failure!\n");
        exit(-1);
    }
}

// connect to stext
void connect_to_stext()
{
    // listening socket
    stext_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (stext_sd == -1)
    {
        printf("Socket failure!\n");
        exit(-1);
    }

    // initialize server
    stext_server.sin_family = AF_INET;
    stext_server.sin_port = htons(STEXT_PORT);
    stext_server.sin_addr.s_addr = inet_addr(STEXT_IP); // add IP address of server

    // connect to server
    if (connect(stext_sd, (struct sockaddr *)&stext_server, sizeof(stext_server)) < 0)
    {
        printf("Connection failure!\n");
        exit(-1);
    }
}

// UTILITY FUNCTIONS

// signal handler for SIGCHLD
void sigchld_signal_handler()
{
    wait(NULL);
}

// finds extension of file
void find_file_extension(char *file_name)
{
    selected_extension = -1; // reset
    // loop to find extension
    for (int i = 0; i < VALID_EXTENSIONS_LENGTH; i++)
    {
        if (strstr(file_name, valid_extensions[i]) != NULL)
        {
            selected_extension = i;
        }
    }
}

// returns index of file name beginning
int extract_file_name(char *file_path)
{
    int file_size = strlen(file_path);
    int skip_index = 0;

    // find / from behind and skip until that index
    for (int i = file_size - 1; i >= 0; i--)
    {
        if (file_path[i] == '/')
        {
            skip_index = i;
            break;
        }
    }
    return skip_index + 1;
}

// logs errors or sucess depending upon send_ack to /home/damlet/Desktop/asp_project/smain_log.txt
// returns -1 on error, 1 on success
int write_logs(char send_ack, char *file_name, char *err_msg, char *success_msg)
{
    // get time
    int ret_value = 1;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char time_now[64];
    size_t ret = strftime(time_now, sizeof(time_now), "%c", tm);
    assert(ret);

    char log_path[1024];
    char *user = getenv("USER");
    sprintf(log_path, "/home/%s/spdf_log.txt", user);
    int log_fd = open(log_path, O_CREAT | O_WRONLY | O_APPEND, 0666);
    lseek(log_fd, 0, SEEK_END); // go to end
    if (log_fd == -1)
    {
        printf("No File Access\n");
        return -1;
    }
    char *message = malloc(sizeof(char) * 1000);
    strcpy(message, time_now);
    strcat(message, ":");

    if (send_ack == '0')
    {
        strcat(message, err_msg);
        strcat(message, file_name);
        strcat(message, "\n");
        message[strlen(message)] = '\0';
        ret_value = -1;
    }
    else
    {
        strcat(message, success_msg);
        strcat(message, file_name);
        strcat(message, "\n");
        message[strlen(message)] = '\0';
        ret_value = 1;
    }

    // write to file and print to screen
    write(log_fd, message, strlen(message));
    printf("%s", message);
    close(log_fd);
    free(message);
    return ret_value;
}

// FOR uploading file
// the path might not always be existing so call mkdir on each folder that needs to be created
// returns -1 on error, 1 on success
int make_dirs_save_file(char *destination_path, char *file_data)
{
    char *path = "/home/";
    char *user = getenv("USER");
    int user_dir_len = strlen(user) + strlen(path) + 1;

    char *user_dir = malloc(sizeof(char) * user_dir_len);
    strcpy(user_dir, path);
    strcat(user_dir, user);
    user_dir[user_dir_len] = '\0';

    // keep track of cwd
    char *cwd = malloc(sizeof(char) * 1000);
    getcwd(cwd, 1000); // current working directory

    if (chdir(user_dir) == -1)
    {
        printf("Change Directory Failed\n");
        return -1;
    }
    mkdir("smain", 0777); // make smain first if does not exist
    if (chdir("smain") == -1)
    {
        printf("Change Directory Failed\n");
        return -1;
    }

    // seperate filename and destination path
    char *dp = strdup(destination_path);
    int skip_index = extract_file_name(dp);
    int file_name_size = strlen(dp + skip_index);
    char *file_name = malloc(sizeof(char) * file_name_size);
    strncpy(file_name, dp + skip_index, file_name_size);
    file_name[file_name_size] = '\0';
    dp[skip_index - 1] = '\0';

    // tokenize destination_path and make directories
    for (char *token = strtok(dp, "/"); token; token = strtok(NULL, "/"))
    {
        mkdir(token, 0777);
        if (chdir(token) == -1)
        {
            return -1;
        }
    }

    // make file
    remove(file_name); // remove file if present
    int fd = open(file_name, O_CREAT | O_WRONLY, 0777);
    write(fd, file_data, strlen(file_data));
    close(fd);
    free(user_dir);
    free(file_name);
    chdir(cwd);
    free(cwd);
    return 1;
}

// send data to stxt/ spdf
// returns -1 on error, 1 on success
int send_data_to_stext_spdf(char *file_data, char *file_size, char *destination_path, char *dp_size, int sd)
{
    char ack = '0';

    // Send command
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // Read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed\n");
        return -1;
    }

    // Send destination path size
    if (write(sd, dp_size, strlen(dp_size)) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // Read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed\n");
        return -1;
    }

    // Send destination path
    if (write(sd, destination_path, strlen(destination_path)) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // Read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed\n");
        return -1;
    }

    // Send file data size
    if (write(sd, file_size, strlen(file_size)) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // Read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed\n");
        return -1;
    }

    // Send file data in chunks
    int file_bytes_size = atoi(file_size);
    int total_sent = 0;
    int chunk_size = 4096;

    while (total_sent < file_bytes_size)
    {
        int to_write = (file_bytes_size - total_sent < chunk_size) ? (file_bytes_size - total_sent) : chunk_size;

        int bytes_sent = write(sd, file_data + total_sent, to_write);

        if (bytes_sent <= 0)
        {
            printf("Write Failed at byte %d\n", total_sent);
            return -1;
        }

        total_sent += bytes_sent;
    }

    // Read final ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - final acknowledgment\n");
        return -1;
    }

    return 1;
}

// Add this function to Smain.c
void clean_socket_state(int socket_sd)
{
    int flags = fcntl(socket_sd, F_GETFL, 0);
    char drain_buf[4096];

    // Set to non-blocking temporarily
    fcntl(socket_sd, F_SETFL, flags | O_NONBLOCK);

    // Drain any remaining data
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(socket_sd, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0; // Don't wait, just check

    // Check if there's data available
    while (select(socket_sd + 1, &readfds, NULL, NULL, &tv) > 0)
    {
        ssize_t drain_size = read(socket_sd, drain_buf, sizeof(drain_buf));
        if (drain_size <= 0)
        {
            break; // Either error or no more data
        }
        FD_ZERO(&readfds);
        FD_SET(socket_sd, &readfds);
    }

    // Reset to original blocking mode
    fcntl(socket_sd, F_SETFL, flags);
}

// In Smain.c, modify the save_file function to clean the socket after an operation
// Add this at the end of the function before returning:

int save_file(char *file_data, char *file_size, char *destination_path, char *dp_size)
{
    int result;

    find_file_extension(destination_path);

    if (selected_extension == -1)
    {
        return -1;
    }

    switch (selected_extension)
    {
    case 0:
        // for .c
        result = make_dirs_save_file(destination_path, file_data);
        break;

    case 1:
        // for .pdf
        result = send_data_to_stext_spdf(file_data, file_size, destination_path, dp_size, spdf_sd);
        clean_socket_state(spdf_sd); // Clean socket state after operation
        break;

    case 2:
        // for .txt
        result = send_data_to_stext_spdf(file_data, file_size, destination_path, dp_size, stext_sd);
        clean_socket_state(stext_sd); // Clean socket state after operation
        break;

    case 3:
        // for .zip
        result = send_data_to_stext_spdf(file_data, file_size, destination_path, dp_size, szip_sd);
        clean_socket_state(szip_sd); // Clean socket state after operation
        break;

    default:
        result = -1;
        break;
    }

    return result;
}
// In Smain.c, modify the save_file function to clean the socket after an operation
// Add this at the end of the function before returning:

// upload file, recieves and sends data to appropriate server to save file using sockets
void upload_file()
{
    char send_ack = '1';

    // write that file option is taken
    char *dp_size = malloc(sizeof(char) * 15);   // destination path size in string
    char *file_size = malloc(sizeof(char) * 15); // file size in string
    char *destination_path, *file_data;
    int bytes_read;

    // read destination path size
    if ((bytes_read = read(client_sd, dp_size, 15)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed - 1\n");
    }
    dp_size[bytes_read] = '\0';

    write(client_sd, &send_ack, 1); // send ack

    // read destination path
    destination_path = malloc(sizeof(char) * atoi(dp_size)); // make destination path string dynamically
    if (read(client_sd, destination_path, atoi(dp_size)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed - 2\n");
    }
    destination_path[atoi(dp_size)] = '\0';
    write(client_sd, &send_ack, 1); // send ack

    // read file data size
    if ((bytes_read = read(client_sd, file_size, 15)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed - 3\n");
    }
    file_size[bytes_read] = '\0';

    write(client_sd, &send_ack, 1); // send ack

    // read file data
    file_data = malloc(sizeof(char) * atoi(file_size)); // make destination path string dynamically
    if ((bytes_read = read(client_sd, file_data, atoi(file_size))) <= 0)
    {
        send_ack = '0';
        printf("Read Failed - 4\n");
    }
    file_data[atoi(file_size)] = '\0';

    // here we have file data, filename path
    // now send this to file_data to upload in either of the servers depending upon what extension file has
    if (save_file(file_data, file_size, destination_path, dp_size) == -1)
    {
        send_ack = '0';
        printf("Save Failed\n");
    }
    if (write_logs(send_ack, destination_path, "Error in Saving ", "File Uploaded Sucessfully at: ") == -1)
    {
        send_ack = '0';
    }

    printf("Final ack:: %d", send_ack);
    write(client_sd, &send_ack, 1); // send ack

    free(dp_size);
    free(file_data);
    free(file_size);
    free(destination_path);
}

// FOR downloading file
// get data from either of the servers
char *get_data_from_stext_spdf(char *dp_size, char *destination_path, int sd)
{
    char ack = '1';
    char *file_size = malloc(sizeof(char) * 15); // file size which will be recieved from smain
    char *file_data;                             // file_name

    // socket communication
    // send command as '1' indicating that file being uploaded
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return NULL;
    }

    // read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - Get data from pdf 1\n");
        return NULL;
    }

    // send dp size
    if (write(sd, dp_size, 15) <= 0)
    {
        printf("Write Failed Get data from pdf 1\n");
        return NULL;
    }

    // read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed Get data from pdf 2\n");
        return NULL;
    }

    // send destination path
    if (write(sd, destination_path, strlen(destination_path)) <= 0)
    {
        printf("Write Failed Get data from pdf 2\n");
        return NULL;
    }

    // read file_size, if file_size is null that means that, file is not found
    if (read(sd, file_size, 15) <= 0 || file_size[0] == '0')
    {
        printf("File %s Not Found\n", destination_path);
        return NULL;
    }
    file_size[strlen(file_size)] = '\0';

    // send ack
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed Get data from pdf 3\n");
        return NULL;
    }

    // read file_data
    file_size_bytes = atoi(file_size);
    file_data = malloc(sizeof(char) * file_size_bytes);
    if (read(sd, file_data, file_size_bytes) <= 0 || file_data[0] == '0')
    {
        printf("Error in getting file data\n", destination_path);
        return NULL;
    }
    file_data[file_size_bytes] = '\0';

    // send ack
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed Get data from pdf 4\n");
        return NULL;
    }
    free(file_size);
    return file_data;
}

// get c file
char *get_c_file(char *destination_path)
{
    struct stat file_info; // to hold file info
    char *file_data;       // to hold file data
    int fd;

    // get user dir
    char *path = "/home/";
    char *user = getenv("USER");
    int user_dir_len = strlen(user) + strlen(path) + 1;
    char *user_dir = malloc(sizeof(char) * user_dir_len);
    strcpy(user_dir, path);
    strcat(user_dir, user);
    user_dir[user_dir_len] = '\0';

    // make file path
    int file_path_size = strlen(destination_path) + 6 + user_dir_len;
    char *file_path = malloc(sizeof(char) * file_path_size);
    strcpy(file_path, user_dir);
    strcat(file_path, "/smain");
    strcat(file_path, destination_path);
    file_path[file_path_size] = '\0';

    // get file size using stat
    stat(file_path, &file_info);
    file_size_bytes = file_info.st_size;
    file_data = malloc(sizeof(char) * file_size_bytes);

    if ((fd = open(file_path, O_RDONLY)) == -1)
    {
        return NULL;
    }
    if (read(fd, file_data, file_size_bytes) <= 0)
    {
        return NULL;
    }
    close(fd);
    free(user_dir);
    free(file_path);

    return file_data;
}

// get_file_data transfers further flow to either of the servers or gets the c file
char *get_file_data(char *dp_size, char *destination_path)
{
    find_file_extension(destination_path);
    if (selected_extension == -1)
    {
        return NULL;
    }
    switch (selected_extension)
    {
    case 0:
        // for .c
        return get_c_file(destination_path);
        break;
    case 1:
        // for .pdf
        return get_data_from_stext_spdf(dp_size, destination_path, spdf_sd);
        break;
    case 2:
        // for .txt
        return get_data_from_stext_spdf(dp_size, destination_path, stext_sd);
        break;
    case 3:
        // for .zip
        return get_data_from_stext_spdf(dp_size, destination_path, szip_sd);
        break;
    default:
        break;
    }
    return NULL;
}
// downloads file, recieves data / error if file not found
void download_file()
{
    // send ack
    char send_ack = '1';

    // write that file option is taken
    char *dp_size = malloc(sizeof(char) * 15);   // destination path size in string
    char *file_size = malloc(sizeof(char) * 15); // file size in string
    char *destination_path, *file_data;
    int bytes_read;

    // read destination path size
    if ((bytes_read = read(client_sd, dp_size, 15)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed\n");
    }
    dp_size[bytes_read] = '\0';
    write(client_sd, &send_ack, 1); // send ack

    // read destination path
    destination_path = malloc(sizeof(char) * atoi(dp_size)); // make destination path string dynamically
    if (read(client_sd, destination_path, atoi(dp_size)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed\n");
    }
    destination_path[atoi(dp_size)] = '\0';

    file_data = get_file_data(dp_size, destination_path);
    if (file_data == NULL)
    {
        // send negative ack to signal to stop communication
        send_ack = '0';
    }

    // // write logs
    // if (write_logs(send_ack, destination_path, "Error in Downloading File:", "File Downloaded:") == -1)
    // {
    //     send_ack = '0';
    // }

    // if file data is null send
    if (!file_data)
    {
        send_ack = '0';
        write(client_sd, &send_ack, 1);
        return;
    }

    sprintf(file_size, "%d", file_size_bytes); // get file size

    write(client_sd, file_size, strlen(file_size)); // send file size

    // read ack
    if ((read(client_sd, &send_ack, 1) <= 0) || send_ack == '0')
    {
        return;
    }

    write(client_sd, file_data, file_size_bytes); // send file data

    // read ack
    if ((read(client_sd, &send_ack, 1) <= 0) || send_ack == '0')
    {
        return;
    }

    free(file_size);
    free(dp_size);
    free(destination_path);
    free(file_data);
}

// FOR removing a file
// socket communication to indicate to remove file from spdf / stext servers
int remove_file_from_stext_spdf(char *dp_size, char *destination_path, int sd)
{

    char ack = '2';

    // socket communication
    // send command as '2' indicating that file needs to be removed
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed\n");
        return -1;
    }

    // send dp size
    if (write(sd, dp_size, strlen(dp_size)) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed\n");
        return -1;
    }

    // send destination path
    if (write(sd, destination_path, strlen(destination_path)) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack, if ack == 0 then file is not removed
    if (read(sd, &ack, 1) <= 0)
    {
        printf("Server unable to get hold of\n");
        return -1;
    }

    if (ack == '0')
    {
        return -1;
    }
    else if (ack == '1')
    {
        return 1;
    }
}

// removes c file
int remove_c_file(char *destination_path)
{

    // get user dir
    int return_value;
    char *path = "/home/";
    char *user = getenv("USER");
    int user_dir_len = strlen(user) + strlen(path) + 1;
    char *user_dir = malloc(sizeof(char) * user_dir_len);
    strcpy(user_dir, path);
    strcat(user_dir, user);
    user_dir[user_dir_len] = '\0';

    // make file path
    int file_path_size = strlen(destination_path) + 6 + user_dir_len;
    char *file_path = malloc(sizeof(char) * file_path_size);
    strcpy(file_path, user_dir);
    strcat(file_path, "/smain");
    strcat(file_path, destination_path);
    file_path[file_path_size] = '\0';

    return_value = remove(file_path);

    free(file_path);
    free(user_dir);
    return return_value;
}

// removes files transfers further flow to either of the servers or removes the c file
int remove_file_from_servers(char *dp_size, char *destination_path)
{
    find_file_extension(destination_path);
    if (selected_extension == -1)
    {
        return -1;
    }

    switch (selected_extension)
    {
    case 0:
        // for .c
        return remove_c_file(destination_path);
        break;
    case 1:
        // for .pdf
        return remove_file_from_stext_spdf(dp_size, destination_path, spdf_sd);
        break;
    case 2:
        // for .txt
        return remove_file_from_stext_spdf(dp_size, destination_path, stext_sd);
        break;
    case 3:
        // for .zip
        return remove_file_from_stext_spdf(dp_size, destination_path, szip_sd);
        break;
    default:
        break;
    }
    return -1;
}
// remove file, removes file from any servers
void remove_file()
{
    // send ack
    char send_ack = '1';

    // write that file option is taken
    char *dp_size = malloc(sizeof(char) * 15); // destination path size in string
    char *destination_path, *file_data;
    int bytes_read, file_removal_status;

    // read destination path size
    if ((bytes_read = read(client_sd, dp_size, 15)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed\n");
    }
    dp_size[bytes_read] = '\0';
    write(client_sd, &send_ack, 1); // send ack

    // read destination path
    destination_path = malloc(sizeof(char) * atoi(dp_size)); // make destination path string dynamically
    if (read(client_sd, destination_path, atoi(dp_size)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed\n");
    }
    destination_path[atoi(dp_size)] = '\0';

    file_removal_status = remove_file_from_servers(dp_size, destination_path);

    if (file_removal_status == -1)
    {
        send_ack = '0';
    }

    // write logs
    if (write_logs(send_ack, destination_path, "Error in Removing file ", "File Removed Sucessfully from: ") == -1)
    {
        send_ack = '0';
    }

    write(client_sd, &send_ack, 1); // send ack

    free(destination_path);
    free(dp_size);
}

// FOR zipping all files present in a server

// returns index of file_path after server path
int skip_server_path(const char *file_path)
{
    int file_size = strlen(file_path);
    int skip_index = 0;
    int count = 0;
    // find / from ahead and skip 4 times till server path is removed
    for (int i = 0; i <= file_size - 1; i++)
    {
        if (file_path[i] == '/')
        {
            skip_index = i;
            count++;
            if (count == 4)
            {
                break;
            }
        }
    }
    return skip_index + 1;
}

// this function will be recursively called to traverse all files in ~/smain
int callback_function(const char *file_path, const struct stat *file_info, int flag, struct FTW *ftw_info)
{
    // for all files, add file path after severing server_path from ahead
    if (flag == FTW_F)
    {
        int skip_index = skip_server_path(file_path);
        char *fp = strdup(file_path);
        fp = fp + skip_index;
        file_paths[no_of_files] = fp;
        no_of_files++;
    }
    return 0;
}

// makes the actual tar file
// returns 1 on success, -1 on error
int make_tar_file(char *file_name)
{
    int child_pid = fork();
    if (child_pid == 0)
    {
        // for child
        // +3 to hold tar,-cvf and NULL in the end
        char *command[4 + no_of_files];
        command[0] = "tar";
        command[1] = "-cf";
        command[2] = file_name;

        // add file paths to command
        int i;
        for (i = 3; i < no_of_files + 3; i++)
        {
            command[i] = file_paths[i - 3];
        }
        command[i] = NULL;           // add last arg as NULL
        execvp(command[0], command); // replace with tar command
    }
    else if (child_pid > 0)
    {
        // for parent
        int status;
        wait(&status);

        // if properly exited, return 1, else return -1
        if (WEXITSTATUS(status) == 0)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
    else if (child_pid == -1)
    {
        printf("Fork Failed!\n");
        return -1;
    }
}

// makes tar file of all exisiting files
// returns tar file data and stores its size in tar_file_size on success, else returns NULL
char *get_cfiles_tar_data(char *file_name)
{
    struct stat file_info; // to hold file info
    char *tar_file_data;

    // get user dir path
    char *path = "/home/";
    char *user = getenv("USER");
    int user_dir_len = strlen(user) + strlen(path) + 1;

    char *user_dir = malloc(sizeof(char) * user_dir_len);
    strcpy(user_dir, path);
    strcat(user_dir, user);
    user_dir[user_dir_len] = '\0';

    // get server path
    int server_path_size = user_dir_len + 6;
    char *server_path = malloc(sizeof(char) * server_path_size);
    strcpy(server_path, user_dir);
    strcat(server_path, "/smain");

    // keep track of current cwd
    char *cwd = malloc(sizeof(char) * 1000);
    getcwd(cwd, 1000); // current working directory

    if (chdir(server_path) == -1)
    {
        printf("Change Directory Failed\n");
        return NULL;
    }

    no_of_files = 0; // reset the number of files

    // nftw to get file_paths of all existing files in the server
    // getdtablesize returns maximum number of processes a file can access
    nftw(server_path, callback_function, getdtablesize(), FTW_PHYS); // recursively call the callback function

    // make tar file
    if (make_tar_file(file_name) == -1)
    {
        return NULL;
    }

    no_of_files = 0; // reset the number of files

    // read data from  the file

    // get file size using stat
    stat(file_name, &file_info);
    tar_file_size = file_info.st_size;
    tar_file_data = malloc(sizeof(char) * tar_file_size);

    // open file in read only mode
    int fd = open(file_name, O_RDONLY);
    read(fd, tar_file_data, tar_file_size);
    close(fd);

    // remove file
    remove(file_name);

    // change to previous path
    if (chdir(cwd) == -1)
    {
        printf("Change Directory Failed\n");
        return NULL;
    }

    free(cwd);
    free(user_dir);
    free(server_path);
    return tar_file_data;
}

// returns tar file data from spdf/stext servers and stores its size in tar_file_size on success, else returns NULL
char *get_tar_data_from_spdf_stext(int sd)
{
    char ack = '3';
    char *tar_file_size_char, *tar_file_data; // tar file size in character and data
    int bytes_read;

    // socket communication
    // send command as '3' indicating that tar file is requested
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return NULL;
    }

    // read tar file size
    tar_file_size_char = malloc(sizeof(char) * 15);
    if ((bytes_read = read(sd, tar_file_size_char, 15)) <= 0 || tar_file_size_char[0] == '0')
    {
        if (bytes_read <= 0)
        {
            ack = '0';
            write(sd, &ack, 1);
        }
        return NULL;
    }
    tar_file_size_char[bytes_read] = '\0';

    // send ack
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return NULL;
    }

    // read tar file data
    tar_file_size = atoi(tar_file_size_char);
    tar_file_data = malloc(sizeof(char) * tar_file_size);
    if ((bytes_read = read(sd, tar_file_data, tar_file_size)) <= 0 || tar_file_data[0] == '0')
    {
        if (bytes_read <= 0)
        {
            ack = '0';
            write(sd, &ack, 1);
        }
        return NULL;
    }
    tar_file_data[bytes_read] = '\0';

    // send ack
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return NULL;
    }
    free(tar_file_size_char);
    return tar_file_data;
}

// gets tar file_data
char *get_tar_file_data(char extension, char *file_name)
{
    // file name
    switch (extension)
    {
    case '0':
        // for cfiles.tar
        return get_cfiles_tar_data(file_name);
        break;
    case '1':
        // for pdf.tar
        return get_tar_data_from_spdf_stext(spdf_sd);
        break;
    case '2':
        // for text.tar
        return get_tar_data_from_spdf_stext(stext_sd);
        break;
    case '3':
        // for zip.tar
        return get_tar_data_from_spdf_stext(szip_sd);
        break;
    default:
        break;
    }

    return NULL;
}

// tars file, recieves data / error if no files found
void tar_file()
{
    // send ack
    char send_ack = '1';

    char extension;                                       // destination path size in string
    char *tar_file_size_char = malloc(sizeof(char) * 15); // file size in string
    char *tar_file_data;
    char *file_name; // file name

    // read extension
    if (read(client_sd, &extension, 1) <= 0)
    {
        send_ack = '0';
        printf("Read Failed\n");
    }

    // file name
    switch (extension)
    {
    case '0':
        file_name = "cfiles.tar";
        break;
    case '1':
        file_name = "pdf.tar";
        break;
    case '2':
        file_name = "text.tar";
        break;
    case '3':
        file_name = "zip.tar";
        break;
    default:
        break;
    }

    tar_file_data = get_tar_file_data(extension, file_name);
    if (tar_file_data == NULL)
    {
        // send negative ack to signal to stop communication
        send_ack = '0';
    }

    // write logs
    if (write_logs(send_ack, file_name, "Error in Tarring File:", "File Downloaded:") == -1)
    {
        send_ack = '0';
    }

    // if file data is null send -ve ack
    if (!tar_file_data)
    {
        send_ack = '0';
        write(client_sd, &send_ack, 1);
        return;
    }

    sprintf(tar_file_size_char, "%d", tar_file_size); // get tar file size

    write(client_sd, tar_file_size_char, strlen(tar_file_size_char)); // send tar file size

    // read ack
    if ((read(client_sd, &send_ack, 1) <= 0) || send_ack == '0')
    {
        return;
    }

    write(client_sd, tar_file_data, tar_file_size); // send file data

    // read ack
    if ((read(client_sd, &send_ack, 1) <= 0) || send_ack == '0')
    {
        return;
    }

    free(tar_file_size_char);
    free(tar_file_data);
}
// for displaying all files in a path

// this function will be recursively called to traverse all files in ~/smain
int callback_function_1(const char *file_path, const struct stat *file_info, int flag, struct FTW *ftw_info)
{
    // traverse in current folder's files
    if (flag == FTW_F && ftw_info->level == 1)
    {
        const char *entity_name = file_path + ftw_info->base;
        char *dp = strdup(entity_name);
        file_names[no_of_files_display] = dp;
        no_of_files_display++;
    }
    return 0;
}

// display files from smain server
char *display_file_spdf(char *destination_path)
{
    char *result;
    int result_size = 0;

    // get user dir path
    char *path = "/home/";
    char *user = getenv("USER");
    int user_dir_len = strlen(user) + strlen(path) + 1;

    char *user_dir = malloc(sizeof(char) * user_dir_len);
    strcpy(user_dir, path);
    strcat(user_dir, user);
    user_dir[user_dir_len] = '\0';

    // get server path
    int server_path_size = user_dir_len + 6 + strlen(destination_path);
    char *server_path = malloc(sizeof(char) * server_path_size);
    strcpy(server_path, user_dir);
    strcat(server_path, "/smain");
    strcat(server_path, destination_path);
    server_path[server_path_size] = '\0';

    no_of_files_display = 0; // reset the number of files

    // nftw to get file_names of all existing files in given path
    // getdtablesize returns maximum number of processes a file can access
    nftw(server_path, callback_function_1, getdtablesize(), FTW_PHYS); // recursively call the callback function

    // get result_size
    for (int i = 0; i < no_of_files_display; i++)
    {
        result_size += strlen(file_names[i]) + 1; // +1 because after every file name I want a \n
    }

    // copy results
    result = malloc(sizeof(char) * result_size);
    result[0] = '\0';
    for (int i = 0; i < no_of_files_display; i++)
    {
        strcat(result, strdup(file_names[i]));
        strcat(result, "\n");
    }
    result[result_size] = '\0';
    no_of_files_display = 0; // reset the number of files
    free(user_dir);
    free(server_path);
    if (result_size == 0)
    {
        return NULL;
    }
    return result;
}

// handles socket communication for spdf and stext servers
// returns display data from either of the servers
char *display_file_spdf_stext(char *dp_size, char *destination_path, int sd)
{
    char ack = '4';
    char *result;
    char *result_size_char = malloc(sizeof(char) * 15);
    int bytes_read, result_size_bytes;

    // socket communication
    // send command as '4' indicating that file needs to be removed
    if (write(sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return NULL;
    }

    // read ack, ack will be replaced by '1'
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed\n");
        return NULL;
    }

    // send dp size
    if (write(sd, dp_size, 15) <= 0)
    {
        printf("Write Failed\n");
        return NULL;
    }

    // read ack
    if (read(sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed\n");
        return NULL;
    }

    // send destination path
    if (write(sd, destination_path, strlen(destination_path)) <= 0)
    {
        printf("Write Failed\n");
        return NULL;
    }
    // read nak / result size
    if ((bytes_read = read(sd, result_size_char, 15)) <= 0 || result_size_char[0] == '0')
    {
        ack = '0';
        // write(sd, &ack, 1); // send nak
        printf("No files at %s\n", destination_path);
        return NULL;
    }
    result_size_char[bytes_read] = '\0';

    // send ack
    write(sd, &ack, 1);

    // read result
    result_size_bytes = atoi(result_size_char);
    result = malloc(sizeof(char) * result_size_bytes);
    if ((bytes_read = read(sd, result, result_size_bytes)) <= 0 || result[0] == '0')
    {
        ack = '0';
        // write(sd, &ack, 1); // send nak
        printf("Error in listing files at path %s\n", destination_path);
        return NULL;
    }
    result[bytes_read] = '\0';

    // send ack
    write(sd, &ack, 1);
    free(result_size_char);

    if (result_size_bytes == 0)
    {
        return NULL;
    }
    return result;
}

// aggregates data from all 3 servers for given destination path
char *agg_data(char *dp_size, char *destination_path)
{
    // hold all data
    char *result_all;
    char *results[4]; // Updated from 3 to 4
    char *c_data, *pdf_data, *text_data, *zip_data;
    int c_size = 0, pdf_size = 0, text_size = 0, zip_size = 0;
    int result_size = 0;

    results[0] = display_file_spdf(destination_path);
    results[1] = display_file_spdf_stext(dp_size, destination_path, spdf_sd);
    results[2] = display_file_spdf_stext(dp_size, destination_path, stext_sd);
    results[3] = display_file_spdf_stext(dp_size, destination_path, szip_sd); // Add zip results

    // get result_size
    for (int i = 0; i < 4; i++) // Changed from 3 to 4
    {
        if (results[i] != NULL)
        {
            result_size += strlen(results[i]); // +1 because after every file name I want a \n
        }
    }

    // get result_size
    result_size += 1; // +1 for \n
    if (result_size == 1)
    {
        return NULL;
    }
    result_all = malloc(sizeof(char) * result_size);
    result_all[0] = '\0';
    for (int i = 0; i < 4; i++) // Changed from 3 to 4
    {
        if (results[i] != NULL)
        {
            strcat(result_all, strdup(results[i]));
            strcat(result_all, "\n");
        }
    }
    result_all[result_size] = '\0';
    return result_all;
}
// socket communication for displaying data
void display_file()
{
    // send ack
    char send_ack = '1';

    // write that file option is taken
    char *dp_size = malloc(sizeof(char) * 15); // destination path size in string
    char *destination_path, *result;
    char *result_size_char = malloc(sizeof(char) * 15);
    int bytes_read, result_size_bytes; // to hold result size bytes

    // read destination path size
    if ((bytes_read = read(client_sd, dp_size, 15)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed\n");
    }
    dp_size[bytes_read] = '\0';
    write(client_sd, &send_ack, 1); // send ack

    // read destination path
    destination_path = malloc(sizeof(char) * atoi(dp_size)); // make destination path string dynamically
    if ((bytes_read = read(client_sd, destination_path, atoi(dp_size))) <= 0)
    {
        send_ack = '0';
        printf("Read Failed\n");
    }
    destination_path[bytes_read] = '\0';

    result = agg_data(dp_size, destination_path);

    if (result == NULL)
    {
        send_ack = '0';
    }

    // write logs
    if (write_logs(send_ack, destination_path, "Error in Displaying files at:", "Display Performed Successfuly for path: ") == -1)
    {
        send_ack = '0';
    }

    if (result == NULL || send_ack == '0')
    {
        send_ack = '0';
        write(client_sd, &send_ack, 1); // send nak
        return;
    }

    // send result size
    result_size_bytes = strlen(result);
    sprintf(result_size_char, "%d", result_size_bytes);
    write(client_sd, result_size_char, strlen(result_size_char));

    // read ack
    if (read(client_sd, &send_ack, 1) <= 0)
    {
        printf("Read Failed\n");
        // send_ack = '0';
        // write(client_sd, &send_ack, 1); // send nak
        return;
    }

    // send result
    write(client_sd, result, result_size_bytes);

    // read ack
    if (read(client_sd, &send_ack, 1) <= 0)
    {
    }

    free(result);
    free(destination_path);
    free(dp_size);
}

// process client requests
void prcclient(int client_fd)
{
    while (1)
    {
        printf("\nWaiting for Input\n");
        char ack;
        char send_ack = '1';
        int bytes_read;

        // read what command is being performed
        if ((bytes_read = read(client_fd, &ack, 1)) <= 0)
        {
            // if this reads fails, it means that connection by client closed or client has exited
            printf("Read Failed11\n");
            break;
        }

        // send ack
        if (write(client_fd, &send_ack, 1) <= 0)
        {
            break;
        }

        switch (ack)
        {
        case '0':
            // for ufile
            upload_file();
            break;
        case '1':
            // for dfile
            download_file();
            break;
        case '2':
            // for rmfile
            remove_file();
            break;
        case '3':
            // for dtar
            tar_file();
            break;
        case '4':
            // for display
            display_file();
            break;
        default:
            break;
        }
    }
    exit(0);
}

void sigpipe_handler(int signum)
{
    printf("Caught SIGPIPE - client likely disconnected unexpectedly\n");
    // Don't exit, just handle the signal and continue
}

// Driver Function
int main(int argc, char *argv[])
{
    signal(SIGPIPE, sigpipe_handler);
    // show documentation
    if (argc == 2 && (strcmp(argv[1], "--help") == 0))
    {
        show_docs();
        exit(0);
    }
    else if (argc == 5) // Changed from 4 to 5 to accommodate the new SZIP_PORT
    {
        PORT = atoi(argv[1]);
        SPDF_PORT = atoi(argv[2]);
        STEXT_PORT = atoi(argv[3]);
        SZIP_PORT = atoi(argv[4]); // New parameter for SZIP_PORT

        smain_socket_setup();
        connect_to_spdf();
        connect_to_stext();
        connect_to_szip(); // Connect to the new SZIP server
        signal(SIGCHLD, sigchld_signal_handler);

        // listen to requests
        while (1)
        {
            client_sd = accept(socket_sd, (struct sockaddr *)NULL, NULL);
            if (client_sd == -1)
            {
                printf("Connection not accepted!\n");
                exit(-1);
            }

            int child_pid = fork();

            if (child_pid == 0)
            {
                // process client request
                prcclient(client_sd);
            }
            else if (child_pid == -1)
            {
                printf("Fork Failed, Please Retry to connect to the server\n");
            }
        }
    }
    else
    {
        printf("Usage: smain smain_port spdf_port stext_port szip_port\n"); // Update usage message
    }
}