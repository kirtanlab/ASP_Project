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
#include <limits.h> // For PATH_MAX
#include <signal.h>

int PORT;                        // port on which socket is created
int CUSTOM_COMMANDS_LENGTH = 7;  // number of custom commands
int VALID_EXTENSIONS_LENGTH = 4; // number of custom commands
char IP[] = "127.0.0.1";         // ip address

// commands client24s will support
char *custom_commands[7] = {"ufile", "dfile", "rmfile", "dtar", "display", "exit", "clear"};
char *valid_extensions[4] = {".c", ".pdf", ".txt", ".zip"}; // add ".zip" here

int selected_custom_command; // will map to custom_commands
int selected_extension;      // will map to valid_extensions

struct sockaddr_in server; // server for adding ip address and port number
int socket_sd;             // socket fd

// --- Robust I/O Helper Functions ---

// Reads exactly 'n' bytes into 'buf' from 'fd'.
// Returns: n on success, 0 on EOF before reading n bytes, -1 on error.
ssize_t read_exact(int fd, void *buf, size_t n)
{
    size_t bytes_read = 0;
    ssize_t result;
    char *ptr = (char *)buf;

    if (n == 0)
        return 0; // Nothing to read

    while (bytes_read < n)
    {
        result = read(fd, ptr + bytes_read, n - bytes_read);

        if (result < 0)
        {
            if (errno == EINTR)
                continue; // Interrupted by signal, try again
            perror("read_exact error");
            return -1; // Other read error
        }
        else if (result == 0)
        {
            // EOF reached before reading n bytes
            fprintf(stderr, "read_exact: EOF reached after %zu bytes, expected %zu\n", bytes_read, n);
            return 0; // Indicate EOF occurred
        }
        bytes_read += result;
    }
    return bytes_read; // Should be equal to n if successful
}

// Writes exactly 'n' bytes from 'buf' to 'fd'.
// Returns: n on success, -1 on error.
ssize_t write_exact(int fd, const void *buf, size_t n)
{
    size_t bytes_written = 0;
    ssize_t result;
    const char *ptr = (const char *)buf;

    if (n == 0)
        return 0; // Nothing to write

    while (bytes_written < n)
    {
        result = write(fd, ptr + bytes_written, n - bytes_written);

        if (result < 0)
        {
            if (errno == EINTR)
                continue; // Interrupted by signal, try again
            perror("write_exact error");
            return -1; // Other write error
        }
        // result == 0 is unlikely for blocking sockets but technically possible
        bytes_written += result;
    }
    return bytes_written; // Should be equal to n if successful
}

// --- CONFIG FUNCTIONS ---

// shows manual page
void show_docs()
{
    char *buffer = malloc(sizeof(char) * 2500);
    int man_fd = -1;

    if (!buffer)
    {
        perror("Failed to allocate buffer for docs");
        exit(EXIT_FAILURE);
    }

    man_fd = open("client24_man_page.txt", O_RDONLY);
    if (man_fd == -1)
    {
        perror("Failed to open client24_man_page.txt");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read = read(man_fd, buffer, 2499); // Leave space for null terminator
    if (bytes_read < 0)
    {
        perror("Failed to read man page");
    }
    else
    {
        buffer[bytes_read] = '\0'; // Null terminate the buffer
        printf("%s\n", buffer);
    }

    close(man_fd);
    free(buffer);
    exit(0); // Exit after showing docs
}

void sigpipe_handler(int signum)
{
    fprintf(stderr, "\nCaught SIGPIPE - server likely disconnected unexpectedly. Please try reconnecting or exit.\n");
    // Don't exit here, let the I/O function return an error
}

// connect to smain
void connect_to_smain()
{
    socket_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_sd == -1)
    {
        perror("Socket creation failure");
        exit(EXIT_FAILURE);
    }

    // Set timeouts
    struct timeval timeout;
    timeout.tv_sec = 60; // 60 second timeout (adjust as needed)
    timeout.tv_usec = 0;
    if (setsockopt(socket_sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt failed for receive timeout");
    }
    if (setsockopt(socket_sd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt failed for send timeout");
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(IP);

    printf("Connecting to smain server %s:%d...\n", IP, PORT);
    if (connect(socket_sd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Connection failure");
        close(socket_sd); // Close socket on failure
        exit(EXIT_FAILURE);
    }
    printf("Connected to smain server.\n");
}

// --- UTILITY FUNCTIONS ---

// this function will find selected option and verify that it exists
void find_custom_command(char *token)
{
    selected_custom_command = -1; // Reset
    if (!token)
        return;

    for (int i = 0; i < CUSTOM_COMMANDS_LENGTH; i++)
    {
        if (strcmp(custom_commands[i], token) == 0)
        {
            selected_custom_command = i;
            return; // Found
        }
    }
}

// finds extension of file
void find_file_extension(char *file_name)
{
    selected_extension = -1; // Reset
    if (!file_name)
        return;

    for (int i = 0; i < VALID_EXTENSIONS_LENGTH; i++)
    {
        // Use strrchr to find the last '.' for the extension
        char *dot = strrchr(file_name, '.');
        if (dot && strcmp(dot, valid_extensions[i]) == 0)
        {
            selected_extension = i;
            return; // Found based on suffix
        }
        // Original strstr logic (less precise but kept for compatibility if needed)
        // if (strstr(file_name, valid_extensions[i]) != NULL) {
        //     selected_extension = i;
        // }
    }
    // If suffix match failed, fall back to original strstr (less reliable)
    if (selected_extension == -1)
    {
        for (int i = 0; i < VALID_EXTENSIONS_LENGTH; i++)
        {
            if (strstr(file_name, valid_extensions[i]) != NULL)
            {
                selected_extension = i;
                return;
            }
        }
    }
}

// finds extension, used in tar file
void find_extension(char *extension)
{
    selected_extension = -1; // Reset
    if (!extension)
        return;

    for (int i = 0; i < VALID_EXTENSIONS_LENGTH; i++)
    {
        if (strcmp(extension, valid_extensions[i]) == 0)
        {
            selected_extension = i;
            return; // Found
        }
    }
}

// this function will remove consecutive forwardslashes from filepath from behind
void remove_consecutive_forwardslashes(char *string)
{
    if (!string)
        return;
    int length = strlen(string);
    // Start from the end, go back until a non-'/' is found or beginning is reached
    while (length > 0 && string[length - 1] == '/')
    {
        string[length - 1] = '\0';
        length--;
    }
}

// check input of a command
// returns number of tokens on success, -1 for error
// populates arguments array (caller must free elements if needed)
int check_input(char *command, char *arguments[], int max_args)
{
    if (!command)
        return -1;

    char *command_copy = strdup(command);
    if (!command_copy)
    {
        perror("strdup failed in check_input");
        return -1;
    }

    char *delimiters = "\n\t\r\v\f ";
    int token_cnt = 0;
    char *token_context = NULL; // For strtok_r

    for (char *token = strtok_r(command_copy, delimiters, &token_context);
         token && token_cnt < max_args;
         token = strtok_r(NULL, delimiters, &token_context))
    {
        arguments[token_cnt] = strdup(token);
        if (!arguments[token_cnt])
        {
            perror("strdup failed for argument");
            // Free previously allocated arguments
            for (int i = 0; i < token_cnt; i++)
            {
                free(arguments[i]);
                arguments[i] = NULL;
            }
            free(command_copy);
            return -1;
        }
        token_cnt++;
    }

    // Check if there were more tokens than allowed
    if (token_cnt < max_args && strtok_r(NULL, delimiters, &token_context) != NULL)
    {
        printf("Too many arguments provided.\n");
        for (int i = 0; i < token_cnt; i++)
        {
            free(arguments[i]);
            arguments[i] = NULL;
        }
        token_cnt = -1; // Indicate error
    }

    free(command_copy);

    if (token_cnt == 0)
        return -1; // No command entered

    // find which command is being used
    find_custom_command(arguments[0]);

    // Validate command and basic argument counts
    if (selected_custom_command == -1)
    {
        printf("'%s' is not a valid command. Type '--help' for usage.\n", arguments[0]);
        for (int i = 0; i < token_cnt; i++)
            free(arguments[i]);
        return -1;
    }
    else if (selected_custom_command == 5 || selected_custom_command == 6)
    { // exit, clear
        if (token_cnt > 1)
        {
            printf("Command '%s' does not take arguments.\n", arguments[0]);
            for (int i = 0; i < token_cnt; i++)
                free(arguments[i]);
            return -1;
        }
    }
    else if (selected_custom_command == 0)
    { // ufile needs 3
        if (token_cnt != 3)
        {
            printf("Usage: ufile <local_filename> <remote_destination_path>\n");
            for (int i = 0; i < token_cnt; i++)
                free(arguments[i]);
            return -1;
        }
    }
    else if (selected_custom_command < 5)
    { // dfile, rmfile, dtar, display need 2
        if (token_cnt != 2)
        {
            printf("Command '%s' requires exactly one argument.\n", arguments[0]);
            // Add specific usage messages here if needed
            for (int i = 0; i < token_cnt; i++)
                free(arguments[i]);
            return -1;
        }
    }

    return token_cnt;
}

// checks if file path begins with ~/smain/
// Modifies arguments array in place to remove prefix
// return -1 for error, 1 for success
int check_file_path(char *arguments[])
{
    int path_arg_index = -1;

    // Determine which argument holds the remote path
    if (selected_custom_command == 0)
    { // ufile remote_dest
        path_arg_index = 2;
    }
    else if (selected_custom_command == 1 || // dfile remote_path
             selected_custom_command == 2 || // rmfile remote_path
             selected_custom_command == 4)
    { // display remote_dir
        path_arg_index = 1;
    }
    else
    {
        return 1; // Command doesn't have a remote path to check (like dtar, exit, clear)
    }

    // Check prefix
    const char *prefix = "~/smain/";
    size_t prefix_len = strlen(prefix);
    if (!arguments[path_arg_index] || strncmp(prefix, arguments[path_arg_index], prefix_len) != 0)
    {
        printf("Error: Remote path must begin with %s\n", prefix);
        return -1;
    }

    // Check extension (only for file operations, not display)
    int file_arg_index = -1;
    if (selected_custom_command == 0)
    { // ufile local_filename
        file_arg_index = 1;
    }
    else if (selected_custom_command == 1 || selected_custom_command == 2)
    {                       // dfile/rmfile remote_path
        file_arg_index = 1; // Use the remote path itself to determine extension
    }

    if (file_arg_index != -1)
    {
        find_file_extension(arguments[file_arg_index]);
        if (selected_extension == -1)
        {
            printf("Error: Invalid or unsupported file extension in '%s'\n", arguments[file_arg_index]);
            printf("Supported extensions: .c, .pdf, .txt, .zip\n");
            return -1;
        }
    }

    // Modify argument to remove prefix (only if it's not the display command, where the path is the arg itself)
    if (selected_custom_command != 4 && selected_custom_command != 0)
    {
        // For ufile (index 2), dfile (index 1), rmfile (index 1)
        // Shift pointer past prefix. Note: This assumes arguments[path_arg_index]
        // was allocated with enough space or is part of a larger buffer where
        // this pointer shift is safe. Since we use strdup, it's safe.
        arguments[path_arg_index] += prefix_len;
        // Ensure the path doesn't become empty after stripping prefix
        if (strlen(arguments[path_arg_index]) == 0 && selected_custom_command != 0)
        { // Allow empty dest for ufile initially
            printf("Error: Remote path cannot be empty after removing prefix.\n");
            // Restore pointer for freeing? No, let check_input_and_perform_command handle freeing original pointers.
            return -1;
        }
    }
    else if (selected_custom_command == 0)
    {                                            // ufile destination directory path
        arguments[path_arg_index] += prefix_len; // Shift destination path pointer
                                                 // Allow empty destination path for ufile (means save in root ~/smain/...)
    }
    else if (selected_custom_command == 4)
    {                                            // display directory path
        arguments[path_arg_index] += prefix_len; // Shift directory path pointer
    }

    return 1;
}

// makes file path by adding / in between string 1 and string 2 and storing it in result
// Returns allocated string (caller must free), or NULL on error.
char *make_file_path(const char *dir, const char *file)
{
    if (!dir || !file)
        return NULL;

    // Calculate required size
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    // Size = dir + '/' + file + '\0'
    // Handle case where dir already ends with '/' or is empty
    bool needs_slash = (dir_len > 0 && dir[dir_len - 1] != '/');
    size_t total_size = dir_len + (needs_slash ? 1 : 0) + file_len + 1;

    char *result = malloc(total_size);
    if (!result)
    {
        perror("malloc failed in make_file_path");
        return NULL;
    }

    // Construct path
    strcpy(result, dir);
    if (needs_slash)
    {
        strcat(result, "/");
    }
    strcat(result, file);

    return result;
}

// user may pass a path in argument 1, extract just the file name from it
// Returns a pointer into the original string, or the original string if no '/' found.
char *extract_file_name(char *file_path)
{
    if (!file_path)
        return ""; // Return empty string for safety

    char *last_slash = strrchr(file_path, '/');
    if (last_slash)
    {
        return last_slash + 1; // Return pointer to character after the last slash
    }
    else
    {
        return file_path; // No slash found, the whole string is the filename
    }
}

// --- COMMAND FUNCTIONS ---

// reads data from file and sends file data to smain and path where it needs to be saved
// returns 1 on success, -1 on failure
int upload_file(char *arguments[])
{
    // arguments[0] = "ufile"
    // arguments[1] = local_filename (potentially with path components)
    // arguments[2] = remote_destination_path (relative to ~/smain/)

    char ack_cmd = '0'; // command code for upload
    struct stat file_info;
    int fd = -1;
    size_t file_bytes_size = 0;
    char *file_data = NULL;
    char *local_file_path = arguments[1]; // Use argument directly
    char *remote_dest_dir = arguments[2]; // Relative dir path
    char *remote_filename = extract_file_name(local_file_path);
    char *full_remote_path = NULL;
    size_t dp_bytes_size = 0;

    char size_destination_path_str[16];
    char size_file_str[16];
    char server_ack;
    int result = -1; // Default to failure

    printf("CLIENT Upload: Local file: %s\n", local_file_path);
    printf("CLIENT Upload: Remote filename: %s\n", remote_filename);
    printf("CLIENT Upload: Remote dest dir (relative): %s\n", remote_dest_dir);

    // 1. Stat local file
    if (stat(local_file_path, &file_info) != 0)
    {
        perror("CLIENT Upload: Failed to stat local file");
        fprintf(stderr, "CLIENT: Error stating file %s\n", local_file_path);
        goto cleanup_upload;
    }
    file_bytes_size = file_info.st_size;
    if (file_bytes_size == 0)
    {
        printf("CLIENT Upload: Warning: File %s is empty.\n", local_file_path);
        // Allow empty file uploads, server should handle
    }
    printf("CLIENT Upload: Local file size: %zu bytes\n", file_bytes_size);

    // 2. Open local file
    fd = open(local_file_path, O_RDONLY);
    if (fd == -1)
    {
        perror("CLIENT Upload: Failed to open local file");
        fprintf(stderr, "CLIENT: Error opening file %s\n", local_file_path);
        goto cleanup_upload;
    }

    // 3. Construct the full remote path (relative path + filename)
    full_remote_path = make_file_path(remote_dest_dir, remote_filename);
    if (!full_remote_path)
    {
        fprintf(stderr, "CLIENT Upload: Failed to construct full remote path\n");
        goto cleanup_upload;
    }
    dp_bytes_size = strlen(full_remote_path);
    printf("CLIENT Upload: Full remote path: %s (%zu bytes)\n", full_remote_path, dp_bytes_size);

    // 4. Allocate memory and Read file data
    if (file_bytes_size > 0)
    {
        file_data = malloc(file_bytes_size);
        if (!file_data)
        {
            perror("CLIENT Upload: Memory allocation failed for file_data");
            goto cleanup_upload;
        }
        printf("CLIENT Upload: Reading file data...\n");
        if (read_exact(fd, file_data, file_bytes_size) != file_bytes_size)
        {
            fprintf(stderr, "CLIENT Upload: Failed to read full file data from %s\n", local_file_path);
            goto cleanup_upload;
        }
        printf("CLIENT Upload: Successfully read %zu bytes\n", file_bytes_size);
    }
    else
    {
        file_data = NULL; // Ensure null if size is 0
    }
    close(fd);
    fd = -1;

    // --- Socket Communication ---
    printf("CLIENT Upload: Starting socket communication\n");

    // 5. Send command code '0'
    if (write_exact(socket_sd, &ack_cmd, 1) != 1)
    {
        fprintf(stderr, "CLIENT Upload: Write Failed - initial command\n");
        goto cleanup_upload;
    }

    // 6. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Upload: Read Failed or NAK received - initial acknowledgment\n");
        goto cleanup_upload;
    }

    // 7. Send destination path size string (fixed 15 bytes)
    snprintf(size_destination_path_str, sizeof(size_destination_path_str), "%-15zu", dp_bytes_size);
    printf("CLIENT Upload: Sending destination path size: '%s'\n", size_destination_path_str);
    if (write_exact(socket_sd, size_destination_path_str, 15) != 15)
    {
        fprintf(stderr, "CLIENT Upload: Write Failed - destination path size\n");
        goto cleanup_upload;
    }

    // 8. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Upload: Read Failed or NAK received - dp_size acknowledgment\n");
        goto cleanup_upload;
    }

    // 9. Send destination path
    printf("CLIENT Upload: Sending destination path: %s\n", full_remote_path);
    if (write_exact(socket_sd, full_remote_path, dp_bytes_size) != dp_bytes_size)
    {
        fprintf(stderr, "CLIENT Upload: Write Failed - destination path\n");
        goto cleanup_upload;
    }

    // 10. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Upload: Read Failed or NAK received - destination path acknowledgment\n");
        goto cleanup_upload;
    }

    // 11. Send file data size string (fixed 15 bytes)
    snprintf(size_file_str, sizeof(size_file_str), "%-15zu", file_bytes_size);
    printf("CLIENT Upload: Sending file size: '%s'\n", size_file_str);
    if (write_exact(socket_sd, size_file_str, 15) != 15)
    {
        fprintf(stderr, "CLIENT Upload: Write Failed - file size\n");
        goto cleanup_upload;
    }

    // 12. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Upload: Read Failed or NAK received - file size acknowledgment\n");
        goto cleanup_upload;
    }

    // 13. Send file data
    printf("CLIENT Upload: Sending file data (%zu bytes)...\n", file_bytes_size);
    if (file_bytes_size > 0)
    {
        if (write_exact(socket_sd, file_data, file_bytes_size) != file_bytes_size)
        {
            fprintf(stderr, "CLIENT Upload: Write Failed - file data\n");
            goto cleanup_upload;
        }
    }
    printf("CLIENT Upload: Finished sending file data.\n");

    // 14. Read final ack
    printf("CLIENT Upload: Waiting for final acknowledgment\n");
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Upload: Final acknowledgment failed or negative.\n");
        if (server_ack == '0')
            fprintf(stderr, "CLIENT: Server indicated save failure (NAK received).\n");
        goto cleanup_upload;
    }

    // Adjust path for user message to include the prefix again
    printf("File Uploaded Successfully to path: ~/smain/%s\n", full_remote_path);
    result = 1; // Success

cleanup_upload:
    if (file_data)
        free(file_data);
    if (full_remote_path)
        free(full_remote_path);
    if (fd != -1)
        close(fd);
    fflush(stdout); // Ensure logs are out
    return result;
}

// writes data to file, used in download_file
// returns -1 on error, 1 on success
int write_data_to_file(const char *file_name, const char *file_data, size_t file_data_size)
{
    int fd = -1;
    int result = -1;
    const char *clean_file_name = file_name;

    if (!file_name || strlen(file_name) == 0)
    {
        fprintf(stderr, "Error: Invalid file name for saving.\n");
        return -1;
    }

    // Prevent writing to absolute paths or paths with ".."
    if (strstr(file_name, "..") != NULL || file_name[0] == '/')
    {
        fprintf(stderr, "Error: Invalid characters or absolute path in local filename '%s'. Saving in current directory only.\n", file_name);
        clean_file_name = extract_file_name((char *)file_name); // Get only the filename part
        if (strlen(clean_file_name) == 0)
        {
            fprintf(stderr, "Error: Invalid filename after cleaning.\n");
            return -1;
        }
    }

    printf("Attempting to save to file: %s\n", clean_file_name);
    remove(clean_file_name); // remove if file exist, ignore error

    fd = open(clean_file_name, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd == -1)
    {
        perror("Failed to open file for writing");
        fprintf(stderr, "Error opening file %s\n", clean_file_name);
        return -1;
    }

    if (file_data_size > 0)
    {
        if (write_exact(fd, file_data, file_data_size) != file_data_size)
        {
            fprintf(stderr, "Writing to File %s Failed\n", clean_file_name);
            goto cleanup_write;
        }
        printf("File %s Downloaded Successfully (%zu bytes)\n", clean_file_name, file_data_size);
    }
    else
    {
        printf("File %s created (empty file downloaded).\n", clean_file_name);
    }

    result = 1; // Success

cleanup_write:
    if (fd != -1)
        close(fd);
    return result;
}

// downloads data from smain server and saves it PWD
// returns 1 on success, -1 on failure
int download_file(char *arguments[])
{
    // arguments[0] = "dfile"
    // arguments[1] = remote_filepath (relative to ~/smain/)

    char ack_cmd = '1'; // Command code
    char dp_size_str[16];
    char file_size_str[16];
    char *remote_path = arguments[1];
    char *local_file_name = extract_file_name(remote_path);
    char *file_data = NULL;
    int ret_value = -1;
    size_t file_data_size = 0;
    char server_ack;
    char client_ack = '1';

    printf("CLIENT Download: Remote path: %s\n", remote_path);
    printf("CLIENT Download: Local filename target: %s\n", local_file_name);

    size_t dp_size = strlen(remote_path);
    snprintf(dp_size_str, sizeof(dp_size_str), "%-15zu", dp_size);

    // 1. Send command code '1'
    if (write_exact(socket_sd, &ack_cmd, 1) != 1)
    {
        fprintf(stderr, "CLIENT Download: Write Failed - command code\n");
        goto cleanup_download;
    }

    // 2. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Download: Read Failed or NAK - command ack\n");
        goto cleanup_download;
    }

    // 3. Send dp size string (fixed size)
    if (write_exact(socket_sd, dp_size_str, 15) != 15)
    {
        fprintf(stderr, "CLIENT Download: Write Failed - dp size\n");
        goto cleanup_download;
    }

    // 4. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Download: Read Failed or NAK - dp size ack\n");
        goto cleanup_download;
    }

    // 5. Send destination path
    if (write_exact(socket_sd, remote_path, dp_size) != dp_size)
    {
        fprintf(stderr, "CLIENT Download: Write Failed - destination path\n");
        goto cleanup_download;
    }

    // 6. Read file size string (fixed size)
    memset(file_size_str, 0, sizeof(file_size_str));
    if (read_exact(socket_sd, file_size_str, 15) != 15)
    {
        fprintf(stderr, "CLIENT Download: Read Failed - file size string\n");
        goto cleanup_download;
    }
    // Check for server-side file not found (indicated by '0')
    if (strspn(file_size_str, " 0") == 15 || atoll(file_size_str) == 0)
    { // Check if it's "0" padded or actual 0
        // Need to distinguish "Not Found" vs "Empty File"
        // Let's assume a padded "0" string means Not Found
        if (strstr(file_size_str, "0") == file_size_str && strspn(file_size_str, " 0") >= 1)
        {
            printf("File ~/smain/%s Not Found on server.\n", remote_path);
            // Consume potential final NAK? Protocol is ambiguous here.
            goto cleanup_download; // Indicate failure locally
        }
        else
        {
            // It's likely an empty file
            printf("CLIENT Download: Server indicates file is empty.\n");
            file_data_size = 0;
        }
    }
    else
    {
        file_data_size = (size_t)atoll(file_size_str);
    }

    printf("CLIENT Download: Expecting file size: %zu bytes\n", file_data_size);

    // 7. Send ack
    if (write_exact(socket_sd, &client_ack, 1) != 1)
    {
        fprintf(stderr, "CLIENT Download: Write Failed - file size ack\n");
        goto cleanup_download;
    }

    // 8. Read file data
    if (file_data_size > 0)
    {
        file_data = malloc(file_data_size);
        if (!file_data)
        {
            perror("CLIENT Download: Memory allocation failed for file data");
            goto cleanup_download;
        }
        printf("CLIENT Download: Receiving file data (%zu bytes)...\n", file_data_size);
        if (read_exact(socket_sd, file_data, file_data_size) != file_data_size)
        {
            fprintf(stderr, "CLIENT Download: Read Failed - file data reception\n");
            goto cleanup_download;
        }
        printf("CLIENT Download: Received file data.\n");
    }
    else
    {
        printf("CLIENT Download: Receiving empty file.\n");
        file_data = NULL; // Explicitly NULL for empty file case
    }

    // 9. Send final ack
    if (write_exact(socket_sd, &client_ack, 1) != 1)
    {
        fprintf(stderr, "CLIENT Download: Write Failed - final ack\n");
        // Server might still be okay, but our state is uncertain
        goto cleanup_download;
    }

    // 10. Write data to local file
    if (write_data_to_file(local_file_name, file_data, file_data_size) == 1)
    {
        ret_value = 1; // Success
    }

cleanup_download:
    if (file_data)
        free(file_data);
    fflush(stdout);
    return ret_value;
}

// removes file from smain server
// returns 1 on success, -1 on failure
int remove_file(char *arguments[])
{
    // arguments[0] = "rmfile"
    // arguments[1] = remote_filepath (relative to ~/smain/)

    char ack_cmd = '2'; // command code
    char dp_size_str[16];
    char *remote_path = arguments[1];
    char *local_file_name_hint = extract_file_name(remote_path); // For messages
    char server_ack;
    int ret_value = -1; // Default to failure

    printf("CLIENT Remove: Remote path: %s\n", remote_path);

    size_t dp_size = strlen(remote_path);
    snprintf(dp_size_str, sizeof(dp_size_str), "%-15zu", dp_size);

    // 1. Send command '2'
    if (write_exact(socket_sd, &ack_cmd, 1) != 1)
    {
        fprintf(stderr, "CLIENT Remove: Write Failed - command code\n");
        return -1;
    }

    // 2. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Remove: Read Failed or NAK - command ack\n");
        return -1;
    }

    // 3. Send dp size string (fixed size)
    if (write_exact(socket_sd, dp_size_str, 15) != 15)
    {
        fprintf(stderr, "CLIENT Remove: Write Failed - dp size\n");
        return -1;
    }

    // 4. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Remove: Read Failed or NAK - dp size ack\n");
        return -1;
    }

    // 5. Send destination path
    if (write_exact(socket_sd, remote_path, dp_size) != dp_size)
    {
        fprintf(stderr, "CLIENT Remove: Write Failed - destination path\n");
        return -1;
    }

    // 6. Read final ack/status from server
    if (read_exact(socket_sd, &server_ack, 1) != 1)
    {
        fprintf(stderr, "CLIENT Remove: Read Failed - final status ack\n");
        return -1;
    }

    if (server_ack == '0')
    {
        printf("File ~/smain/%s removal failed: Not found or permission denied on server.\n", remote_path);
        // Return -1 as it failed server-side
    }
    else if (server_ack == '1')
    {
        printf("File ~/smain/%s Removed Successfully on server.\n", remote_path);
        ret_value = 1; // Success
    }
    else
    {
        fprintf(stderr, "CLIENT Remove: Received unexpected final status from server: %c\n", server_ack);
        // Treat as failure
    }
    fflush(stdout);
    return ret_value;
}

// downloads tar file from the extension in arguments[1]
// returns 1 on success, -1 on failure
int tar_file(char *arguments[])
{
    // arguments[0] = "dtar"
    // arguments[1] = filetype extension (e.g., ".pdf")

    find_extension(arguments[1]); // find extension index
    if (selected_extension == -1)
    {
        printf("'%s' is not a valid filetype for dtar.\n", arguments[1]);
        printf("Supported types: .c, .pdf, .txt, .zip\n");
        return -1;
    }

    char extension_code = selected_extension + '0'; // Convert int 0..3 to char '0'..'3'
    char tar_file_size_str[16];
    char *tar_file_data = NULL;
    int ret_value = -1; // Default failure
    size_t tar_file_bytes = 0;
    char *local_file_name = NULL;
    char ack_cmd = '3'; // Command code
    char server_ack;
    char client_ack = '1';

    printf("CLIENT dtar: Requesting tar for type %s (code %c)\n", arguments[1], extension_code);

    // Determine local filename based on extension
    switch (selected_extension)
    {
    case 0:
        local_file_name = "cfiles.tar";
        break;
    case 1:
        local_file_name = "pdf.tar";
        break;
    case 2:
        local_file_name = "text.tar";
        break;
    case 3:
        local_file_name = "zip.tar";
        break;
    default: // Should not happen
        fprintf(stderr, "CLIENT dtar: Internal error - invalid selected extension.\n");
        return -1;
    }
    printf("CLIENT dtar: Target local filename: %s\n", local_file_name);

    // 1. Send command '3'
    if (write_exact(socket_sd, &ack_cmd, 1) != 1)
    {
        fprintf(stderr, "CLIENT dtar: Write Failed - command code\n");
        goto cleanup_dtar;
    }

    // 2. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT dtar: Read Failed or NAK - command ack\n");
        goto cleanup_dtar;
    }

    // 3. Send extension code
    if (write_exact(socket_sd, &extension_code, 1) != 1)
    {
        fprintf(stderr, "CLIENT dtar: Write Failed - extension code\n");
        goto cleanup_dtar;
    }

    // 4. Read tar file size string (fixed size)
    memset(tar_file_size_str, 0, sizeof(tar_file_size_str));
    if (read_exact(socket_sd, tar_file_size_str, 15) != 15)
    {
        fprintf(stderr, "CLIENT dtar: Read Failed - tar size string\n");
        goto cleanup_dtar;
    }
    // Check for server-side indication of no files/error ('0' padded)
    if (strspn(tar_file_size_str, " 0") == 15 || atoll(tar_file_size_str) == 0)
    {
        if (strstr(tar_file_size_str, "0") == tar_file_size_str && strspn(tar_file_size_str, " 0") >= 1)
        {
            printf("No %s files found on server to tar.\n", valid_extensions[selected_extension]);
            goto cleanup_dtar; // Local failure indication
        }
        else
        {
            printf("CLIENT dtar: Server indicates empty tar file.\n");
            tar_file_bytes = 0;
        }
    }
    else
    {
        tar_file_bytes = (size_t)atoll(tar_file_size_str);
    }

    printf("CLIENT dtar: Expecting tar file size: %zu bytes\n", tar_file_bytes);

    // 5. Send ack
    if (write_exact(socket_sd, &client_ack, 1) != 1)
    {
        fprintf(stderr, "CLIENT dtar: Write Failed - tar size ack\n");
        goto cleanup_dtar;
    }

    // 6. Read tar file data
    if (tar_file_bytes > 0)
    {
        tar_file_data = malloc(tar_file_bytes);
        if (!tar_file_data)
        {
            perror("CLIENT dtar: Memory allocation failed for tar data");
            goto cleanup_dtar;
        }
        printf("CLIENT dtar: Receiving tar data (%zu bytes)...\n", tar_file_bytes);
        if (read_exact(socket_sd, tar_file_data, tar_file_bytes) != tar_file_bytes)
        {
            fprintf(stderr, "CLIENT dtar: Read Failed - tar data reception\n");
            goto cleanup_dtar;
        }
        printf("CLIENT dtar: Received tar data.\n");
    }
    else
    {
        tar_file_data = NULL;
    }

    // 7. Send final ack
    if (write_exact(socket_sd, &client_ack, 1) != 1)
    {
        fprintf(stderr, "CLIENT dtar: Write Failed - final ack\n");
        goto cleanup_dtar;
    }

    // 8. Write data to local file
    if (write_data_to_file(local_file_name, tar_file_data, tar_file_bytes) == 1)
    {
        ret_value = 1; // Success
    }

cleanup_dtar:
    if (tar_file_data)
        free(tar_file_data);
    fflush(stdout);
    return ret_value;
}

// displays all files from the path in arguments[1]
// returns 1 on success, -1 on failure
int display_file(char *arguments[])
{
    // arguments[0] = "display"
    // arguments[1] = remote_directory_path (relative to ~/smain/)

    char ack_cmd = '4'; // Command code
    char dp_size_str[16];
    char *remote_path = arguments[1];
    char *result = NULL;
    char result_size_str[16];
    size_t result_size_bytes = 0;
    char server_ack;
    char client_ack = '1';
    int ret_value = -1; // Default failure

    printf("CLIENT Display: Remote path: %s\n", remote_path);

    size_t dp_size = strlen(remote_path);
    snprintf(dp_size_str, sizeof(dp_size_str), "%-15zu", dp_size);

    // 1. Send command '4'
    if (write_exact(socket_sd, &ack_cmd, 1) != 1)
    {
        fprintf(stderr, "CLIENT Display: Write Failed - command code\n");
        goto cleanup_display;
    }

    // 2. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Display: Read Failed or NAK - command ack\n");
        goto cleanup_display;
    }

    // 3. Send dp size string (fixed size)
    if (write_exact(socket_sd, dp_size_str, 15) != 15)
    {
        fprintf(stderr, "CLIENT Display: Write Failed - dp size\n");
        goto cleanup_display;
    }

    // 4. Read ack
    if (read_exact(socket_sd, &server_ack, 1) != 1 || server_ack == '0')
    {
        fprintf(stderr, "CLIENT Display: Read Failed or NAK - dp size ack\n");
        goto cleanup_display;
    }

    // 5. Send destination path
    if (write_exact(socket_sd, remote_path, dp_size) != dp_size)
    {
        fprintf(stderr, "CLIENT Display: Write Failed - destination path\n");
        goto cleanup_display;
    }

    // 6. Read result size string (fixed size)
    memset(result_size_str, 0, sizeof(result_size_str));
    if (read_exact(socket_sd, result_size_str, 15) != 15)
    {
        fprintf(stderr, "CLIENT Display: Read Failed - result size string\n");
        goto cleanup_display;
    }
    // Check for server-side indication of no files/error ('0' padded)
    if (strspn(result_size_str, " 0") == 15 || atoll(result_size_str) == 0)
    {
        if (strstr(result_size_str, "0") == result_size_str && strspn(result_size_str, " 0") >= 1)
        {
            printf("No files found at path ~/smain/%s on server.\n", remote_path);
            goto cleanup_display; // Local failure indication (or treat as success with no output)
        }
        else
        {
            printf("CLIENT Display: Server indicates empty result.\n");
            result_size_bytes = 0;
        }
    }
    else
    {
        result_size_bytes = (size_t)atoll(result_size_str);
    }

    printf("CLIENT Display: Expecting display result size: %zu bytes\n", result_size_bytes);

    // 7. Send ack
    if (write_exact(socket_sd, &client_ack, 1) != 1)
    {
        fprintf(stderr, "CLIENT Display: Write Failed - result size ack\n");
        goto cleanup_display;
    }

    // 8. Read result data
    if (result_size_bytes > 0)
    {
        result = malloc(result_size_bytes + 1); // +1 for null terminator
        if (!result)
        {
            perror("CLIENT Display: Memory allocation failed for result data");
            goto cleanup_display;
        }
        printf("CLIENT Display: Receiving display data (%zu bytes)...\n", result_size_bytes);
        if (read_exact(socket_sd, result, result_size_bytes) != result_size_bytes)
        {
            fprintf(stderr, "CLIENT Display: Read Failed - result data reception\n");
            goto cleanup_display;
        }
        result[result_size_bytes] = '\0'; // Null terminate the received string data
        printf("CLIENT Display: Received display data.\n");

        // Print the result
        printf("\n--- Files at path ~/smain/%s ---\n%s\n---------------------------------\n", remote_path, result);
    }
    else
    {
        printf("\n--- No files found at path ~/smain/%s ---\n", remote_path);
        result = NULL;
    }

    // 9. Send final ack
    if (write_exact(socket_sd, &client_ack, 1) != 1)
    {
        fprintf(stderr, "CLIENT Display: Write Failed - final ack\n");
        // Data was displayed, but final state is uncertain
        goto cleanup_display;
    }

    ret_value = 1; // Success (even if no files were found, the operation completed)

cleanup_display:
    if (result)
        free(result);
    fflush(stdout);
    return ret_value;
}

// --- Command Execution Logic ---

// performs command, returns -1 on error, 1 on success
int perform_command(int token_cnt, char *arguments[])
{
    int status = -1; // Default to error

    // Path check is done *before* this in check_input_and_perform_command
    // and arguments are modified in place

    switch (selected_custom_command)
    {
    case 0: // ufile
        status = upload_file(arguments);
        break;
    case 1: // dfile
        status = download_file(arguments);
        break;
    case 2: // rmfile
        status = remove_file(arguments);
        break;
    case 3: // dtar (path check skipped earlier)
        status = tar_file(arguments);
        break;
    case 4: // display
        status = display_file(arguments);
        break;
    case 5: // exit
        printf("Exiting.\n");
        close(socket_sd);
        exit(0);
    case 6:                     // clear
        printf("\033[H\033[J"); // ANSI clear screen
        status = 1;             // Success
        break;
    default:
        fprintf(stderr, "Internal Error: Invalid selected_custom_command %d\n", selected_custom_command);
        break;
    }
    return status;
}

// check input syntax and perform command
int check_input_and_perform_command(char *command)
{
    char *arguments[4] = {NULL, NULL, NULL, NULL};     // Max 3 args + command name
    char *original_ptrs[4] = {NULL, NULL, NULL, NULL}; // To free original strdup'd args
    int token_cnt = -1;
    int status = -1; // Default failure

    token_cnt = check_input(command, arguments, 4);

    if (token_cnt == -1)
    {
        // Error message already printed
        goto check_cleanup; // arguments are freed by check_input on error
    }

    // Keep original pointers for freeing later, as check_file_path modifies the pointers
    for (int i = 0; i < token_cnt; ++i)
    {
        original_ptrs[i] = arguments[i];
    }

    // PART 1.5: Check File Path (for relevant commands)
    // check_file_path modifies arguments array pointers (removes ~/smain/)
    if (selected_custom_command != 3 && // dtar has different args
        selected_custom_command != 5 && // exit
        selected_custom_command != 6)   // clear
    {
        if (check_file_path(arguments) == -1)
        {
            // Error message printed by check_file_path
            status = -1;
            goto check_cleanup; // Go cleanup original args
        }
    }

    // PART 2: Perform Command
    status = perform_command(token_cnt, arguments);
    if (status == -1)
    {
        // Error message should have been printed by perform_command or sub-functions
        fprintf(stderr, "Command execution failed.\n");
    }

check_cleanup:
    // Free memory allocated by strdup in check_input using original pointers
    for (int i = 0; i < 4; ++i)
    { // Check all 4 potential slots
        if (original_ptrs[i])
        {
            free(original_ptrs[i]);
            original_ptrs[i] = NULL; // Avoid double free if somehow used again
        }
    }
    fflush(stdout);
    fflush(stderr);
    return status;
}

// --- Driver Function ---
int main(int argc, char *argv[])
{
    // Setup SIGPIPE handler
    struct sigaction sa;
    sa.sa_handler = sigpipe_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Or SA_RESTART if desired
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
    {
        perror("Failed to set SIGPIPE handler");
        exit(EXIT_FAILURE);
    }

    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--help") == 0))
    {
        show_docs(); // Exits
    }
    else if (argc == 2)
    {
        PORT = atoi(argv[1]);
        if (PORT <= 0 || PORT > 65535)
        {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }
        connect_to_smain();

        // Infinite loop for commands
        while (1)
        {
            long unsigned int command_buf_size = 0;
            char *command = NULL;

            printf("client24> "); // Prompt
            fflush(stdout);

            ssize_t line_len = getline(&command, &command_buf_size, stdin);

            if (line_len == -1)
            {
                if (feof(stdin))
                {
                    printf("\nEOF detected. Exiting.\n");
                }
                else
                {
                    perror("getline failed");
                }
                free(command);
                break; // Exit loop
            }

            // Remove trailing newline
            if (line_len > 0 && command[line_len - 1] == '\n')
            {
                command[line_len - 1] = '\0';
            }

            // Skip empty lines or lines with only whitespace
            if (strspn(command, " \t\n\r\v\f") == strlen(command))
            {
                free(command);
                continue;
            }

            // check input syntax and perform command
            check_input_and_perform_command(command);

            free(command); // Free memory from getline
        }

        printf("Closing connection.\n");
        if (socket_sd >= 0)
            close(socket_sd);
    }
    else
    {
        printf("Usage: client24 <port>\n");
        printf("   or: client24 --help\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
