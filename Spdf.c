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
#include <errno.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>

int PORT;          // port on which socket is created
#define BACKLOG 10 // maximum 10 concurrent connections

// socket variables
int socket_sd, client_sd;
struct sockaddr_in server;

#define FTW_PHYS 1 // this is already defined in ftw.h but for some reaseon intellisense doesn't detect it.
// for some reason FTW struct in ftw.h is not being imported so making this same struct from ftw.h
struct FTW
{
    int base;
    int level;
};
char *file_paths[10000];    // to hold file paths of files to tar
int no_of_files = 0;        // to determine how many files to tar
int tar_file_size;          // to hold tar file size
long int file_bytes_size__; // to hold file size while downloading

char *file_names[10000]; // to hold file names of files during display
int no_of_files_display; // no of files in file_name

// CONFIG FUNCTIONS
// shows manual page
void show_docs()
{
    char *buffer = malloc(sizeof(char) * 2500);

    int man_fd = open("spdf_man_page.txt", O_RDONLY);

    if (man_fd == -1)
        exit(-1);

    read(man_fd, buffer, 2500);

    printf("%s\n", buffer);

    close(man_fd);
    free(buffer);
    exit(0);
}

// setup spdf socket configurations
void spdf_socket_setup()
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

// UTILITY FUNCTIONS
// signal handler for SIGCHLD
void sigchld_signal_handler()
{
    wait(NULL);
}

// gives index of file name
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

int write_logs(char send_ack, char *destination_path, char *err_msg, char *success_msg)
{
    // Replace entire function with a simplified version
    printf("Log: %s%s\n", send_ack == '1' ? success_msg : err_msg, destination_path);
    return 1; // Always return success
}
// FOR upload_file
// the path might not always be existing so call mkdir on each folder that needs to be created

// Replace the make_dirs_save_file function in Spdf.c
int make_dirs_save_file(char *destination_path, char *file_data, int file_bytes_size)
{
    char *path = "/home/";
    char *user = getenv("USER");
    if (!user)
    {
        printf("Failed to get USER environment variable\n");
        return -1;
    }

    int user_dir_len = strlen(user) + strlen(path) + 1;
    char *user_dir = malloc(sizeof(char) * user_dir_len);
    if (!user_dir)
    {
        printf("Memory allocation failed for user_dir\n");
        return -1;
    }

    strcpy(user_dir, path);
    strcat(user_dir, user);

    // Keep track of cwd
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd)))
    {
        printf("Failed to get current working directory\n");
        free(user_dir);
        return -1;
    }

    // Change to user's home directory
    if (chdir(user_dir) == -1)
    {
        printf("Change to user directory failed: %s\n", strerror(errno));
        free(user_dir);
        return -1;
    }

    // Create spdf directory if it doesn't exist
    mkdir("spdf", 0777);
    if (chdir("spdf") == -1)
    {
        printf("Change to spdf directory failed: %s\n", strerror(errno));
        free(user_dir);
        if (chdir(cwd) == -1)
        {
            printf("Failed to return to original directory\n");
        }
        return -1;
    }

    // Make a copy of destination_path since strtok modifies its input
    char *dest_copy = strdup(destination_path);
    if (!dest_copy)
    {
        printf("Memory allocation failed for dest_copy\n");
        free(user_dir);
        if (chdir(cwd) == -1)
        {
            printf("Failed to return to original directory\n");
        }
        return -1;
    }

    // Extract filename and directory path
    int skip_index = extract_file_name(dest_copy);

    // Allocate memory for file name
    char *file_name = malloc(strlen(dest_copy + skip_index) + 1);
    if (!file_name)
    {
        printf("Memory allocation failed for file_name\n");
        free(dest_copy);
        free(user_dir);
        if (chdir(cwd) == -1)
        {
            printf("Failed to return to original directory\n");
        }
        return -1;
    }

    strcpy(file_name, dest_copy + skip_index);
    dest_copy[skip_index - 1] = '\0'; // Separate directory path

    // Create directories
    for (char *token = strtok(dest_copy, "/"); token; token = strtok(NULL, "/"))
    {
        mkdir(token, 0777);
        if (chdir(token) == -1)
        {
            printf("Failed to change to directory %s: %s\n", token, strerror(errno));
            free(file_name);
            free(dest_copy);
            free(user_dir);
            if (chdir(cwd) == -1)
            {
                printf("Failed to return to original directory\n");
            }
            return -1;
        }
    }

    // Remove file if it exists
    remove(file_name);

    // Create and write to file
    int fd = open(file_name, O_CREAT | O_WRONLY, 0777);
    if (fd == -1)
    {
        printf("Failed to create file %s: %s\n", file_name, strerror(errno));
        free(file_name);
        free(dest_copy);
        free(user_dir);
        if (chdir(cwd) == -1)
        {
            printf("Failed to return to original directory\n");
        }
        return -1;
    }

    // Write in chunks for large files
    ssize_t bytes_written = 0;
    ssize_t total_written = 0;
    int chunk_size = 4096;

    while (total_written < file_bytes_size)
    {
        int to_write = (file_bytes_size - total_written < chunk_size) ? (file_bytes_size - total_written) : chunk_size;

        bytes_written = write(fd, file_data + total_written, to_write);
        if (bytes_written <= 0)
        {
            printf("Write error at byte %ld: %s\n", total_written, strerror(errno));
            close(fd); // Always close fd before returning
            free(file_name);
            free(dest_copy);
            free(user_dir);
            if (chdir(cwd) == -1)
            {
                printf("Failed to return to original directory\n");
            }
            return -1;
        }

        total_written += bytes_written;
    }

    // Ensure we flush and close the file properly
    fsync(fd);
    close(fd);

    printf("Successfully wrote %ld bytes to %s\n", total_written, file_name);

    // Return to original directory
    if (chdir(cwd) == -1)
    {
        printf("Failed to return to original directory: %s\n", strerror(errno));
    }

    // Free allocated memory
    free(file_name);
    free(dest_copy);
    free(user_dir);

    return 1;
}
// upload file recieves data and sends to appropriate server to save file
// Replace the upload_file function in spdf (paste-2.txt)

// Replace the upload_file function in SPDF.c
void upload_file()
{
    // Add timeout configuration
    struct timeval timeout;
    timeout.tv_sec = 60; // Increase timeout for large files
    timeout.tv_usec = 0;
    setsockopt(client_sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(client_sd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));

    char send_ack = '1';
    int bytes_read;
    char *dp_size = NULL;
    char *file_size = NULL;
    char *destination_path = NULL;
    char *file_data = NULL;
    char *full_path = NULL;

    // Allocate memory safely
    dp_size = malloc(sizeof(char) * 15);
    if (!dp_size)
    {
        printf("Memory allocation failed for dp_size\n");
        send_ack = '0';
        write(client_sd, &send_ack, 1);
        return;
    }

    file_size = malloc(sizeof(char) * 15);
    if (!file_size)
    {
        printf("Memory allocation failed for file_size\n");
        free(dp_size);
        send_ack = '0';
        write(client_sd, &send_ack, 1);
        return;
    }

    // Read destination path size with error handling
    if ((bytes_read = read(client_sd, dp_size, 15)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed for destination path size: %s\n", strerror(errno));
        goto cleanup;
    }
    dp_size[bytes_read] = '\0';
    printf("Received destination path size: %s\n", dp_size);

    // Send ACK
    if (write(client_sd, &send_ack, 1) != 1)
    {
        printf("Failed to send ACK after dp_size: %s\n", strerror(errno));
        goto cleanup;
    }

    // Allocate memory for destination path
    int dp_size_int = atoi(dp_size);
    destination_path = malloc(sizeof(char) * (dp_size_int + 1));
    if (!destination_path)
    {
        printf("Memory allocation failed for destination_path\n");
        send_ack = '0';
        write(client_sd, &send_ack, 1);
        goto cleanup;
    }

    // Read destination path
    memset(destination_path, 0, dp_size_int + 1); // Initialize with zeros
    if ((bytes_read = read(client_sd, destination_path, dp_size_int)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed for destination path: %s\n", strerror(errno));
        goto cleanup;
    }
    destination_path[bytes_read] = '\0';
    printf("Received destination path: %s\n", destination_path);

    // Send ACK
    if (write(client_sd, &send_ack, 1) != 1)
    {
        printf("Failed to send ACK after destination path: %s\n", strerror(errno));
        goto cleanup;
    }

    // Read file size
    memset(file_size, 0, 15); // Initialize with zeros
    if ((bytes_read = read(client_sd, file_size, 15)) <= 0)
    {
        send_ack = '0';
        printf("Read Failed for file size: %s\n", strerror(errno));
        goto cleanup;
    }
    file_size[bytes_read] = '\0';
    printf("Received file size: %s\n", file_size);

    // Send ACK
    if (write(client_sd, &send_ack, 1) != 1)
    {
        printf("Failed to send ACK after file size: %s\n", strerror(errno));
        goto cleanup;
    }

    // Allocate memory for file data - IMPORTANT: no +1 for binary data
    int file_bytes_size = atoi(file_size);
    printf("Allocating %d bytes for file data\n", file_bytes_size);
    file_data = malloc(file_bytes_size);
    if (!file_data)
    {
        send_ack = '0';
        printf("Memory allocation failed for file data (%d bytes)\n", file_bytes_size);
        goto cleanup;
    }

    // Read file data in chunks
    int total_read = 0;
    int remaining = file_bytes_size;
    int chunk_size = 4096;

    printf("Starting to read file data (%d bytes)...\n", file_bytes_size);
    while (remaining > 0)
    {
        int to_read = (remaining < chunk_size) ? remaining : chunk_size;

        bytes_read = read(client_sd, file_data + total_read, to_read);
        if (bytes_read <= 0)
        {
            send_ack = '0';
            printf("Read Failed for file data at byte %d: %s\n", total_read, strerror(errno));
            goto cleanup;
        }

        total_read += bytes_read;
        remaining -= bytes_read;

        // Log progress for large files
        if (file_bytes_size > 1024 * 1024 && (total_read % (1024 * 1024) == 0 || total_read == file_bytes_size))
        {
            printf("Received %d/%d bytes (%.1f%%)\n",
                   total_read, file_bytes_size,
                   ((float)total_read / file_bytes_size) * 100);
        }
    }
    printf("Finished reading file data: %d bytes\n", total_read);

    // Save backup of destination path
    full_path = strdup(destination_path);
    if (!full_path)
    {
        send_ack = '0';
        printf("Memory allocation failed for full_path\n");
        goto cleanup;
    }

    // Save file - using explicit file size, not relying on null termination
    printf("Saving %d bytes to %s\n", file_bytes_size, full_path);
    if (make_dirs_save_file(destination_path, file_data, file_bytes_size) == -1)
    {
        send_ack = '0';
        printf("Save Failed\n");
    }
    else
    {
        printf("File saved successfully\n");
    }

    // Write logs
    write_logs(send_ack, full_path ? full_path : "unknown",
               "Error in Saving ", "File Uploaded Successfully at: ");

    // Send final ACK
    printf("Sending final ACK: %c\n", send_ack);
    if (write(client_sd, &send_ack, 1) != 1)
    {
        printf("Failed to send final ACK: %s\n", strerror(errno));
    }

cleanup:
    printf("Cleanup: Freeing resources\n");
    // Free all allocated memory
    if (dp_size)
    {
        free(dp_size);
        dp_size = NULL;
    }
    if (file_size)
    {
        free(file_size);
        file_size = NULL;
    }
    if (destination_path)
    {
        free(destination_path);
        destination_path = NULL;
    }
    if (file_data)
    {
        free(file_data);
        file_data = NULL;
    }
    if (full_path)
    {
        free(full_path);
        full_path = NULL;
    }

    // If we're here because of an error, make sure we send a NAK
    if (send_ack == '0')
    {
        printf("Sending NAK due to error\n");
        write(client_sd, &send_ack, 1);
    }

    printf("Upload operation complete\n");
    fflush(stdout);
}

// FOR download_file
// get_pdf_file transfers further flow to either of the servers or gets the c file
char *get_pdf_file(char *destination_path)
{
    struct stat file_info; // to hold file info
    int file_bytes_size;
    char *file_data; // to hold file data
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
    int file_path_size = strlen(destination_path) + 5 + user_dir_len;
    char *file_path = malloc(sizeof(char) * file_path_size);
    strcpy(file_path, user_dir);
    strcat(file_path, "/spdf");
    strcat(file_path, destination_path);
    file_path[file_path_size] = '\0';

    // get file size using stat
    stat(file_path, &file_info);
    file_bytes_size = file_info.st_size;
    file_data = malloc(sizeof(char) * file_bytes_size);

    if ((fd = open(file_path, O_RDONLY)) == -1)
    {
        return NULL;
    }
    if (read(fd, file_data, file_bytes_size) == -1)
    {
        return NULL;
    }
    file_bytes_size__ = file_bytes_size;
    close(fd);
    free(user_dir);
    free(file_path);

    return file_data;
}

// socket communication for downloading a file
void download_file()
{
    // send ack
    char send_ack = '1';

    // write that file option is taken
    char *dp_size = malloc(sizeof(char) * 15);   // destination path size in string
    char *file_size = malloc(sizeof(char) * 30); // file size in string
    char *destination_path, *file_data;
    int bytes_read;

    // read destination path size
    if ((bytes_read = read(client_sd, dp_size, 15)) == -1)
    {
        send_ack = '0';
        printf("Read Failed - download files 1\n");
    }
    dp_size[bytes_read] = '\0';
    write(client_sd, &send_ack, 1); // send ack

    // read destination path
    destination_path = malloc(sizeof(char) * atoi(dp_size)); // make destination path string dynamically
    if ((bytes_read = read(client_sd, destination_path, atoi(dp_size))) == -1)
    {
        send_ack = '0';
        printf("Read Failed - download files 2\n");
    }
    destination_path[bytes_read] = '\0';

    file_data = get_pdf_file(destination_path); // get file data from /stext
    if (file_data == NULL)
    {
        // send negative ack to signal to stop communication
        send_ack = '0';
        write(client_sd, &send_ack, 1);
        return;
    }

    sprintf(file_size, "%ld", file_bytes_size__); // get file size

    write(client_sd, file_size, strlen(file_size)); // send file size

    // read ack
    if ((read(client_sd, &send_ack, 1) == -1) || send_ack == '0')
    {
        write(client_sd, &send_ack, 1);
        return;
    }

    // write logs
    if (write_logs(send_ack, destination_path, "Error in Downloading File:", "File Downloaded:") == -1)
    {
        send_ack = '0';
    }

    write(client_sd, file_data, file_bytes_size__); // send file data

    // read ack
    if ((read(client_sd, &send_ack, 1) == -1) || send_ack == '0')
    {
    }

    free(file_size);
    free(dp_size);
    free(file_data);
    free(destination_path);
}

// FOR removing a file
// removes pdf file from stext server
int remove_pdf_file(char *destination_path)
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
    int file_path_size = strlen(destination_path) + 5 + user_dir_len;
    char *file_path = malloc(sizeof(char) * file_path_size);
    strcpy(file_path, user_dir);
    strcat(file_path, "/spdf");
    strcat(file_path, destination_path);
    file_path[file_path_size] = '\0';

    return_value = remove(file_path);

    free(file_path);
    free(user_dir);
    return return_value;
}

// remove file, removes file from any servers
void remove_file()
{
    // send ack
    char send_ack = '1';

    // write that file option is taken
    char *dp_size = malloc(sizeof(char) * 15); // destination path size in string
    char *destination_path;
    int bytes_read, file_removal_status;

    // read destination path size
    if ((bytes_read = read(client_sd, dp_size, 15)) == -1)
    {
        send_ack = '0';
        printf("Read Failed - remove files 1\n");
    }
    dp_size[bytes_read] = '\0';
    write(client_sd, &send_ack, 1); // send ack

    // read destination path
    destination_path = malloc(sizeof(char) * atoi(dp_size)); // make destination path string dynamically
    if (read(client_sd, destination_path, atoi(dp_size)) == -1)
    {
        send_ack = '0';
        printf("Read Failed - remove files 2\n");
    }
    destination_path[atoi(dp_size)] = '\0';

    file_removal_status = remove_pdf_file(destination_path);

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

// FOR tar_file

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
char *get_pdf_tar_data(char *file_name)
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
    int server_path_size = user_dir_len + 5;
    char *server_path = malloc(sizeof(char) * server_path_size);
    strcpy(server_path, user_dir);
    strcat(server_path, "/spdf");

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

// socket communication for sending tar file to smain
void tar_file()
{
    // send ack
    char send_ack = '1';

    char *tar_file_size_char = malloc(sizeof(char) * 15); // file size in string
    char *tar_file_data, *file_name = "pdf.tar";

    tar_file_data = get_pdf_tar_data(file_name); // make tar file and get tar file data

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

    // tar_file_size is populated by get_pdf_tar_data when it reads the data
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
    }

    free(tar_file_size_char);
    free(tar_file_data);
}

// for displaying files

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
    int server_path_size = user_dir_len + 5 + strlen(destination_path);
    char *server_path = malloc(sizeof(char) * server_path_size);
    strcpy(server_path, user_dir);
    strcat(server_path, "/spdf");
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
        printf("Read Failed - dis 1\n");
    }
    dp_size[bytes_read] = '\0';
    write(client_sd, &send_ack, 1); // send ack

    // read destination path
    destination_path = malloc(sizeof(char) * atoi(dp_size)); // make destination path string dynamically
    if ((bytes_read = read(client_sd, destination_path, atoi(dp_size))) <= 0)
    {
        send_ack = '0';
        printf("Read Failed - dis 2\n");
    }
    destination_path[bytes_read] = '\0';

    result = display_file_spdf(destination_path);

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
        printf("Read Failed - dis 3\n");
        send_ack = '0';
        write(client_sd, &send_ack, 1); // send nak
        return;
    }

    // send result
    write(client_sd, result, result_size_bytes);

    // read ack
    if (read(client_sd, &send_ack, 1) <= 0)
    {
    }

    free(destination_path);
    free(dp_size);
}

// process smain's requests
void prcsmain()
{
    while (1)
    {
        printf("\nWaiting for Input\n");
        char ack;
        char send_ack = '1';
        // read what command is being performed
        if (read(client_sd, &ack, 1) <= 0)
        {
            printf("Read Failed - prcs1\n");
            break;
        }

        // no need to send ack in case of tarring file
        if (ack != '3')
        {
            write(client_sd, &send_ack, 1); // send ack
        }
        printf("%d ack in Spdf prcsmain: ", ack);
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

// Driver Function
int main(int argc, char *argv[])
{

    // show documentation
    if (argc == 2 && (strcmp(argv[1], "--help") == 0))
    {
        show_docs();
        exit(0);
    }
    else if (argc == 2)
    {
        PORT = atoi(argv[1]);
        spdf_socket_setup();
        signal(SIGCHLD, sigchld_signal_handler);

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
                // process smain's request
                prcsmain();
            }
            else if (child_pid == -1)
            {
                printf("Fork Failed, Please Retry to connect to the server\n");
            }
        }
    }
    else
    {
        printf("Usage:spdf port\n");
    }
}
