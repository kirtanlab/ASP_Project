#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

int PORT;                        // port on which socket is created
int CUSTOM_COMMANDS_LENGTH = 7;  // number of custom commands
int VALID_EXTENSIONS_LENGTH = 4; // number of custom commands
char IP[] = "127.0.0.1";         // ip address
// char IP[] = "127.0.1.1";

// commands client24s will support
char *custom_commands[7] = {"ufile", "dfile", "rmfile", "dtar", "display", "exit", "clear"};
char *valid_extensions[4] = {".c", ".pdf", ".txt", ".zip"}; // add ".zip" here

int selected_custom_command; // will map to custom_commands
int selected_extension;      // will map to valid_extensions

struct sockaddr_in server; // server for adding ip address and port number
int socket_sd;             // socket fd

// CONFIG FUNCTIONS
// shows manual page
void show_docs()
{
    char *buffer = malloc(sizeof(char) * 2500);

    int man_fd = open("client24_man_page.txt", O_RDONLY);

    if (man_fd == -1)
        exit(-1);

    read(man_fd, buffer, 2500);

    printf("%s\n", buffer);

    close(man_fd);
    free(buffer);
    exit(0);
}

void sigpipe_handler(int signum)
{
    printf("Caught SIGPIPE - client likely disconnected unexpectedly\n");
    // Don't exit, just handle the signal and continue
}

// connect to smain
void connect_to_smain()
{
    // listening socket
    socket_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_sd == -1)
    {
        printf("Socket failure!\n");
        exit(-1);
    }

    // initialize server
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(IP); // add IP address of server

    // connect to server
    if (connect(socket_sd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("Connection failure!\n");
        exit(-1);
    }
}

// UTILITY FUNCTIONS
// this function will find selected option and verify that it exists
void find_custom_command(char *token)
{
    // reset
    selected_custom_command = -1;

    // find selected command
    for (int i = 0; i < CUSTOM_COMMANDS_LENGTH; i++)
    {
        if (token && strcmp(custom_commands[i], token) == 0)
        {
            selected_custom_command = i;
        }
    }
}

// finds extension of file
int find_file_extension(char *file_name)
{
    selected_extension = -1; // reset
    // loop to find extension
    for (int i = 0; i < VALID_EXTENSIONS_LENGTH; i++)
    {
        if (file_name && strstr(file_name, valid_extensions[i]) != NULL)
        {
            selected_extension = i;
        }
    }
}

// finds extension, used in tar file
int find_extension(char *extension)
{
    selected_extension = -1; // reset
    // loop to find extension
    for (int i = 0; i < VALID_EXTENSIONS_LENGTH; i++)
    {
        if (extension && strcmp(extension, valid_extensions[i]) == 0)
        {
            selected_extension = i;
        }
    }
}

// this function will remove consecutive forwardslashes from filepath from behind
void remove_consecutive_forwardslashes(char *string)
{
    int length = strlen(string);
    for (int i = length - 1; i > 0; i--)
    {
        if (string[i] == '/')
        {
            // end string
            string[i] = '\0';
        }
        else
        {
            break;
        }
    }
}

// check input of a command
// returns number of tokens on success, -1 for error
// and populates arguments by seperating command
int check_input(char *command, char *arguments[])
{
    // Make a copy of the original command to avoid modifying it
    char *command_copy = strdup(command);
    if (!command_copy)
    {
        printf("Memory allocation failed\n");
        return -1;
    }

    char *delimiters = "\n\t\r\v\f ";
    int token_cnt = 0;
    char *token_context = NULL;

    // Use strtok_r which is thread-safe and more predictable
    for (char *token = strtok_r(command_copy, delimiters, &token_context);
         token && token_cnt <= 3;
         token = strtok_r(NULL, delimiters, &token_context))
    {

        if (token_cnt > 3)
        {
            printf("Maximum 3 Arguments allowed\n");
            free(command_copy);
            return -1;
        }

        // Allocate and copy the token
        arguments[token_cnt] = strdup(token);
        if (!arguments[token_cnt])
        {
            printf("Memory allocation failed for argument %d\n", token_cnt);
            // Free previously allocated memory
            for (int i = 0; i < token_cnt; i++)
            {
                free(arguments[i]);
            }
            free(command_copy);
            return -1;
        }

        token_cnt++;
    }

    free(command_copy);
    // find which command is being used
    find_custom_command(arguments[0]);
    printf("Selected Command:%d\n", selected_custom_command);
    if (selected_custom_command == -1)
    {
        printf("%s is not a valid command\n", arguments[0]);
        return -1;
    }
    else if (selected_custom_command != 3 && selected_custom_command < 4 && token_cnt < 2)
    {
        printf("Atleast 2 arguments required\n");
        return -1;
    }
    return token_cnt;
}

// checks if file path begins with ~/smain/
// return -1 for error, 1 for success
int check_file_path(char *arguments[])
{
    // filepaths in all commands must begin with ~/smain except when dtar is entered
    if (selected_custom_command == 0)
    {
        // file path needs to start from ~smain/ if not show error
        if (arguments[2] && arguments[1] && strncmp("~/smain/", arguments[2], 8) != 0)
        {
            printf("Destination Path must Begin with ~/smain/ \n");
            return -1;
        }
        arguments[2] = arguments[2] + 7; // skip server root folder name as we don't know what server it'll be saved under

        // accept valid file extensions
        find_file_extension(arguments[1]);

        if (selected_extension == -1)
        {
            printf("Invalid Extension of file:%s\n", arguments[1]);
            return -1;
        }
    }
    else if (selected_custom_command == 1 || selected_custom_command == 2 || selected_custom_command == 4)
    {
        // file path needs to start from ~smain/ if not show error
        if (arguments[1] && strncmp("~/smain/", arguments[1], 8) != 0)
        {
            printf("Destination Path must Begin with ~/smain/ \n");
            return -1;
        }
        arguments[1] = arguments[1] + 7; // skip server root folder name as we don't know what server it'll be saved under

        // accept valid file extensions
        find_file_extension(arguments[1]);

        if (selected_extension == -1 && selected_custom_command != 4)
        {
            printf("Invalid Extension of file:%s\n", arguments[1]);
            return -1;
        }
    }
    return 1;
}

// makes file path by adding / in between string 1 and string 2 and storing it in result
char *make_file_path(char *string1, char *string2)
{
    char *result;
    int file_size = strlen(string1) + strlen(string2) + 2; // +2 because one for '/', one for file name
    result = malloc(sizeof(char) * file_size);
    strcpy(result, string1);
    strcat(result, "/");
    strcat(result, string2);
    result[file_size] = '\0';
    return result;
}

// user may pass a path in argument 1, extract just the file name from it
char *extract_file_name(char *file_name)
{
    int file_size = strlen(file_name);
    int skip_index = 0;

    // find / from behind and skip until that index
    for (int i = file_size - 1; i >= 0; i--)
    {
        if (file_name[i] == '/')
        {
            skip_index = i;
            break;
        }
    }
    file_name = file_name + skip_index;

    // +1 to skip the actual '/', but only if '/' are found
    if (skip_index != 0)
    {
        file_name += 1;
    }
    return file_name;
}

// reads data from file and sends file data to smain and path where it needs to be saved
// returns 1 on success, -1 on failure
// Enhanced upload_file function for client24s.c
int upload_file(char *arguments[])
{
    // Add timeout code
    struct timeval timeout;
    timeout.tv_sec = 30; // 30 second timeout
    timeout.tv_usec = 0;

    // Set timeouts
    if (setsockopt(socket_sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        printf("setsockopt failed for receive timeout\n");
    }
    if (setsockopt(socket_sd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        printf("setsockopt failed for send timeout\n");
    }

    char ack = '0';                     // character for acknowledgment
    struct stat file_info;              // to hold file info
    int fd, dp_bytes_size;              // file descriptor
    int file_bytes_size;                // file bytes of file
    char *file_data = NULL;             // to hold file data
    char *file_name = NULL;             // to hold file name
    char *destination_file_path = NULL; // will hold arguments[2]/arguments[1] as file_path

    char *size_destination_path = NULL; // maximum size of file size can be transferred is 15 digits
    char *size_file = NULL;             // maximum size of file size can be transferred is 15 digits

    char *cwd = NULL;
    int result = -1; // Default to failure

    // Allocate memory
    size_destination_path = malloc(sizeof(char) * 15);
    if (!size_destination_path)
    {
        printf("Memory allocation failed for size_destination_path\n");
        return -1;
    }

    size_file = malloc(sizeof(char) * 15);
    if (!size_file)
    {
        printf("Memory allocation failed for size_file\n");
        free(size_destination_path);
        return -1;
    }

    cwd = malloc(sizeof(char) * 1000);
    if (!cwd)
    {
        printf("Memory allocation failed for cwd\n");
        free(size_destination_path);
        free(size_file);
        return -1;
    }

    if (!getcwd(cwd, 1000))
    {
        printf("Failed to get current working directory\n");
        goto cleanup;
    }

    // Make cwd + arguments[1] into file_name
    file_name = make_file_path(cwd, arguments[1]);
    if (!file_name)
    {
        printf("Failed to create file path\n");
        goto cleanup;
    }

    printf("CLIENT: Processing file: %s\n", file_name);

    // Get file size using stat
    if (stat(file_name, &file_info) != 0)
    {
        printf("Failed to stat file: %s\n", strerror(errno));
        goto cleanup;
    }

    file_bytes_size = file_info.st_size;
    printf("CLIENT: File size: %d bytes\n", file_bytes_size);

    fd = open(file_name, O_RDONLY); // open file in read only mode

    // Check if file exists
    if (fd == -1)
    {
        printf("File %s Does not exist: %s\n", file_name, strerror(errno));
        goto cleanup;
    }

    // Make destination path
    remove_consecutive_forwardslashes(arguments[2]);

    arguments[1] = extract_file_name(arguments[1]);                     // remove folders if any
    destination_file_path = make_file_path(arguments[2], arguments[1]); // make arguments[2]/arguments[1]
    if (!destination_file_path)
    {
        printf("Failed to create destination file path\n");
        close(fd);
        goto cleanup;
    }

    printf("CLIENT: Destination path: %s\n", destination_file_path);

    // Read file data - critical for binary files
    file_data = malloc(sizeof(char) * file_bytes_size);
    if (!file_data)
    {
        printf("Memory allocation failed for file_data (%d bytes)\n", file_bytes_size);
        close(fd);
        goto cleanup;
    }

    int total_read = 0;
    int current_read = 0;

    // Read in chunks to handle large files
    printf("CLIENT: Reading file data...\n");
    while (total_read < file_bytes_size &&
           (current_read = read(fd, file_data + total_read, file_bytes_size - total_read)) > 0)
    {
        total_read += current_read;

        // Log progress for large files
        if (file_bytes_size > 1024 * 1024 && (total_read % (1024 * 1024) == 0 || total_read == file_bytes_size))
        {
            printf("CLIENT: Read %d/%d bytes (%.1f%%)\n",
                   total_read, file_bytes_size,
                   ((float)total_read / file_bytes_size) * 100);
        }
    }

    close(fd);

    if (total_read != file_bytes_size)
    {
        printf("CLIENT: Unable to read complete file data (%d/%d bytes): %s\n",
               total_read, file_bytes_size, strerror(errno));
        goto cleanup;
    }

    printf("CLIENT: Successfully read %d bytes\n", total_read);

    // Socket communication
    printf("CLIENT: Starting socket communication\n");

    // Send command as '0' indicating file being uploaded
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("CLIENT: Write Failed - initial command: %s\n", strerror(errno));
        goto cleanup;
    }

    // Read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("CLIENT: Read Failed - initial acknowledgment: %s\n", strerror(errno));
        goto cleanup;
    }

    // Send destination path size
    dp_bytes_size = strlen(destination_file_path);
    memset(size_destination_path, 0, 15);
    sprintf(size_destination_path, "%d", dp_bytes_size);
    printf("CLIENT: Sending destination path size: %s\n", size_destination_path);

    if (write(socket_sd, size_destination_path, strlen(size_destination_path)) <= 0)
    {
        printf("CLIENT: Write Failed - destination path size: %s\n", strerror(errno));
        goto cleanup;
    }

    // Read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("CLIENT: Read Failed - dp_size acknowledgment: %s\n", strerror(errno));
        goto cleanup;
    }

    // Send destination path
    printf("CLIENT: Sending destination path: %s\n", destination_file_path);
    if (write(socket_sd, destination_file_path, dp_bytes_size) <= 0)
    {
        printf("CLIENT: Write Failed - destination path: %s\n", strerror(errno));
        goto cleanup;
    }

    // Read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("CLIENT: Read Failed - destination path acknowledgment: %s\n", strerror(errno));
        goto cleanup;
    }

    // Send file data size
    memset(size_file, 0, 15);
    sprintf(size_file, "%d", file_bytes_size);
    printf("CLIENT: Sending file size: %s\n", size_file);

    if (write(socket_sd, size_file, strlen(size_file)) <= 0)
    {
        printf("CLIENT: Write Failed - file size: %s\n", strerror(errno));
        goto cleanup;
    }

    // Read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("CLIENT: Read Failed - file size acknowledgment: %s\n", strerror(errno));
        goto cleanup;
    }

    // Write file data to socket in chunks
    int chunk_size = 4096;
    int total_sent = 0;
    int bytes_sent = 0;

    printf("CLIENT: Starting to send file data (%d bytes)...\n", file_bytes_size);

    while (total_sent < file_bytes_size)
    {
        int to_write = (file_bytes_size - total_sent < chunk_size) ? (file_bytes_size - total_sent) : chunk_size;

        bytes_sent = write(socket_sd, file_data + total_sent, to_write);

        if (bytes_sent <= 0)
        {
            printf("CLIENT: Write Failed at byte %d: %s\n", total_sent, strerror(errno));
            goto cleanup;
        }

        total_sent += bytes_sent;

        // Log progress
        if (file_bytes_size > 1024 * 1024 && (total_sent % (1024 * 1024) == 0 || total_sent == file_bytes_size))
        {
            printf("CLIENT: Sent %d/%d bytes (%.1f%%)\n",
                   total_sent, file_bytes_size,
                   ((float)total_sent / file_bytes_size) * 100);
        }
    }

    printf("CLIENT: Finished sending file data (%d bytes)\n", total_sent);

    // Read final ack
    printf("CLIENT: Waiting for final acknowledgment %d\n", ack);
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("CLIENT: Final acknowledgment failed or negative: %s\n",
               (ack == '0') ? "NAK received" : strerror(errno));
        goto cleanup;
    }

    printf("File Saved Successfully at path: ~/smain%s\n", destination_file_path);
    result = 1; // Success

cleanup:
    // Free pointers
    if (cwd)
        free(cwd);
    if (file_data)
        free(file_data);
    if (size_file)
        free(size_file);
    if (size_destination_path)
        free(size_destination_path);
    if (destination_file_path)
        free(destination_file_path);
    if (file_name)
        free(file_name);

    return result;
}
// writes data to file, used in download_file
// returns -1 on error, 1 on success
int write_data_to_file(char *file_name, char *file_data, int file_data_size)
{
    remove(file_name); // remove if file exist

    if (file_name[0] == '/')
    {
        file_name = file_name + 1;
    }

    int fd = open(file_name, O_CREAT | O_RDWR, 0777);
    if (write(fd, file_data, file_data_size) <= 0)
    {
        printf("Writing to File %s Failed\n", file_name);
        return -1;
    }
    else
    {
        printf("File %s Downloaded Successfully\n", file_name);
    }
    return 1;
    close(fd);
}

// downloads data from smain server and saves it PWD
// returns 1 on success, -1 on failure
int download_file(char *arguments[])
{
    char ack = '1';
    char *dp_size = malloc(sizeof(char) * 15);         // destination path size in characters
    char *file_size = malloc(sizeof(char) * 15);       // file size which will be recieved from smain
    char *file_name = extract_file_name(arguments[1]); // arguments[1] holds destination path, filename will be extracted from it
    char *file_data;                                   // file data
    int ret_value, bytes_read;

    remove_consecutive_forwardslashes(arguments[1]);
    sprintf(dp_size, "%d", strlen(arguments[1])); // get dp size

    // socket communication
    // send command as '1' indicating that file being uploaded
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - download filter\n");
        return -1;
    }

    // send dp size
    if (write(socket_sd, dp_size, 15) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - download too 2nd\n");
        return -1;
    }

    // send destination path
    if (write(socket_sd, arguments[1], strlen(arguments[1])) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read file_size, if file_size is null that means that, file is not found
    if ((bytes_read = read(socket_sd, file_size, 15)) <= 0 || file_size[0] == '0')
    {
        printf("File %s Not Found\n", arguments[1]);
        return -1;
    }
    file_size[bytes_read] = '\0';

    // send ack
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read file_data
    int file_data_size = atoi(file_size);
    file_data = malloc(sizeof(char) * file_data_size);
    if ((bytes_read = read(socket_sd, file_data, file_data_size)) <= 0 || file_data[0] == '0')
    {
        printf("Error in getting file data\n", arguments[1]);
        return -1;
    }
    file_data[bytes_read] = '\0';

    // send ack
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // write data to file
    ret_value = write_data_to_file(file_name, file_data, file_data_size);

    free(dp_size);
    free(file_size);
    free(file_data);
    return ret_value;
}

// removes file from smain server
// returns 1 on success, -1 on failure
int remove_file(char *arguments[])
{
    char ack = '2';
    char *dp_size = malloc(sizeof(char) * 15);         // destination path size in characters
    char *file_name = extract_file_name(arguments[1]); // arguments[1] holds destination path, filename will be extracted from it

    remove_consecutive_forwardslashes(arguments[1]);
    sprintf(dp_size, "%d", strlen(arguments[1])); // get dp size

    // socket communication
    // send command as '2' indicating that file needs to be removed
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - remove file\n");
        return -1;
    }

    // send dp size
    if (write(socket_sd, dp_size, 15) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - remove file\n");
        return -1;
    }

    // send destination path
    if (write(socket_sd, arguments[1], strlen(arguments[1])) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack, if ack == 0 then file is not removed
    if (read(socket_sd, &ack, 1) <= 0)
    {
        printf("Server unable to get hold of\n");
        return -1;
    }

    if (ack == '0')
    {
        printf("File %s either does not exist or you don't have permission to remove it\n", file_name);
    }
    else if (ack == '1')
    {
        printf("File %s Removed\n", file_name);
    }

    free(dp_size);
    return 1;
}

// downloads tar file from the extension in arguments[1]
// returns 1 on success, -1 on failure
int tar_file(char *arguments[])
{
    find_extension(arguments[1]);                  // find extension of arguments[1]
    char extension;                                // extension to hold which extension
    sprintf(&extension, "%d", selected_extension); // get extension in character
    char *tar_file_size, *tar_file_data;           // tar file size in character and data
    int ret_value, tar_file_bytes, bytes_read;
    char *file_name;
    char ack = '3';

    // file name
    switch (selected_extension)
    {
    case 0:
        file_name = "cfiles.tar";
        break;
    case 1:
        file_name = "pdf.tar";
        break;
    case 2:
        file_name = "text.tar";
        break;
    case 3:
        file_name = "zip.tar";
        break;
    case -1:
        printf("%s is not a valid extension\n", arguments[1]);
        return -1;
        break;
    }

    // socket communication
    // send command as '3' indicating that tar file needs to be downloaded
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack,ack will be replaced by '1'
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - tar file\n");
        return -1;
    }

    // send extension
    if (write(socket_sd, &extension, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read tar file size
    tar_file_size = malloc(sizeof(char) * 15);
    if ((bytes_read = read(socket_sd, tar_file_size, 15)) <= 0 || tar_file_size[0] == '0')
    {
        printf("No %s files in ~/smain/\n", valid_extensions[selected_extension]);
        if (bytes_read <= 0)
        {
            ack = '0';
            write(socket_sd, &ack, 1);
        }
        return -1;
    }
    tar_file_size[bytes_read] = '\0';

    // send ack
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read tar file data
    tar_file_bytes = atoi(tar_file_size);
    tar_file_data = malloc(sizeof(char) * tar_file_bytes);
    if ((bytes_read = read(socket_sd, tar_file_data, tar_file_bytes)) <= 0 || tar_file_data[0] == '0')
    {
        if (bytes_read <= 0)
        {
            ack = '0';
            write(socket_sd, &ack, 1);
        }
        return -1;
    }
    tar_file_data[bytes_read] = '\0';

    // send ack
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    remove(file_name); // remove file if exists
    ret_value = write_data_to_file(file_name, tar_file_data, tar_file_bytes);

    free(tar_file_size);
    free(tar_file_data);
    return ret_value;
}

// displays all files from the path in arguments[1]
// returns 1 on success, -1 on failure
int display_file(char *arguments[])
{
    char ack = '4';
    char *dp_size = malloc(sizeof(char) * 15); // destination path size in characters
    char *result;
    char *result_size_char = malloc(sizeof(char) * 15);
    int bytes_read, result_size_bytes;

    remove_consecutive_forwardslashes(arguments[1]);
    sprintf(dp_size, "%d", strlen(arguments[1])); // get dp size

    // socket communication
    // send command as '4' indicating that file needs to be removed
    if (write(socket_sd, &ack, 1) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack, ack will be replaced by '1'
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - display file\n");
        return -1;
    }

    // send dp size
    if (write(socket_sd, dp_size, 15) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }

    // read ack
    if (read(socket_sd, &ack, 1) <= 0 || ack == '0')
    {
        printf("Read Failed - display 2d\n");
        return -1;
    }

    // send destination path
    if (write(socket_sd, arguments[1], strlen(arguments[1])) <= 0)
    {
        printf("Write Failed\n");
        return -1;
    }
    // read nak / result size
    if ((bytes_read = read(socket_sd, result_size_char, 15)) <= 0 || result_size_char[0] == '0')
    {
        ack = '0';
        // write(socket_sd, &ack, 1); // send nak
        printf("No files at %s\n", arguments[1]);
        return -1;
    }
    result_size_char[bytes_read] = '\0';

    // send ack
    write(socket_sd, &ack, 1);

    // read result
    result_size_bytes = atoi(result_size_char);
    result = malloc(sizeof(char) * result_size_bytes);
    if ((bytes_read = read(socket_sd, result, result_size_bytes)) <= 0 || result[0] == '0')
    {
        ack = '0';
        // write(socket_sd, &ack, 1); // send nak
        printf("Error in listing files at path %s\n", arguments[1]);
        return -1;
    }
    result[bytes_read] = '\0';

    // send ack
    write(socket_sd, &ack, 1);

    printf("Files at path ~/smain%s:\n", arguments[1]);
    printf("%s\n", result);

    free(dp_size);
    free(result_size_char);
    free(result);
    return 1;
}

// performs command, returns -1 on error, 1 on success
int perform_command(int token_cnt, char *arguments[])
{
    switch (selected_custom_command)
    {
    case 0:
        // for ufile
        if (token_cnt < 3)
        {
            printf("Usage:ufile filename destination\n");
            return -1;
        }
        if (upload_file(arguments) == -1)
        {
            return -1;
        }

        break;
    case 1:
        // for dfile
        if (token_cnt != 2)
        {
            printf("Usage:dfile filepath\n");
            return -1;
        }

        if (download_file(arguments) == -1)
        {
            return -1;
        }
        break;
    case 2:
        // for rmfile
        if (token_cnt != 2)
        {
            printf("Usage:rmfile filepath\n");
            return -1;
        }
        if (remove_file(arguments) == -1)
        {
            return -1;
        }
        break;
    case 3:
        // for dtar
        if (token_cnt != 2)
        {
            printf("Usage:dtar filetype\n");
            return -1;
        }
        if (tar_file(arguments) == -1)
        {
            return -1;
        }
        break;
    case 4:
        // for display
        if (token_cnt != 2)
        {
            printf("Usage:display directory_pathname\n");
            return -1;
        }
        if (display_file(arguments) == -1)
        {
            return -1;
        }
        break;
    case 5:
        // for exit
        close(socket_sd); // close the socket
        exit(0);
        break;
    case 6:
        // for clear
        printf("\e[1;1H\e[2J"); // clear screen ascii
        break;
    default:
        break;
    }
    return 1;
}

// check input syntax
int check_input_and_perform_command(char *command)
{
    // PART 1: Check Input
    char *arguments[3];
    int token_cnt = check_input(command, arguments);
    if (token_cnt == -1)
    {
        return -1;
    }

    if (token_cnt <= 3)
    {

        // check file path
        if (check_file_path(arguments) == -1)
        {
            return -1;
        }

        // PART 2: Perform Command

        // perform command
        if (perform_command(token_cnt, arguments) == -1)
        {
            return -1;
        }
    }
}

// Driver Function
int main(int argc, char *argv[])
{
    signal(SIGPIPE, sigpipe_handler);
    // show documentation if args > 1
    if (argc > 2)
    {
        show_docs();
    }
    else if (argc == 2)
    {
        PORT = atoi(argv[1]);
        connect_to_smain();

        // infinite loop
        while (1)
        {
            long unsigned int command_size = 1000;
            char *command = malloc(sizeof(char) * command_size);
            printf("Enter a command:");
            int size = getline(&command, &command_size, stdin);
            command[size - 1] = '\0';
            if (strlen(command) <= 0)
            {
                free(command);
                continue;
            }

            // check input syntax and perform command
            check_input_and_perform_command(command);

            free(command);
        }
    }
    else
    {
        printf("Usage:client24s port\n");
    }
    return 0;
}
