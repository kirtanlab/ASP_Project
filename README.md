# Distributed File System

Distributed File System is a project designed to manage and distribute files across multiple servers, providing a seamless user experience. The system consists of three different servers (`smain.c`, `spdf.c`, and `stext.c`) and one client program (`client24s.c`).

## Key Features:
* Client-Server Architecture: The client connects to the primary server (smain.c) to interact with the system.
* File Operations: Users can upload, download, display, remove files, and create tarballs of the files stored in the system.
* Automatic File Distribution: The main server (smain.c) intelligently distributes files across different servers based on their type (e.g., PDF files to spdf.c, text files to stext.c), abstracting the distributed nature of the system from the user.
* Server Communication: The smain.c server communicates with spdf.c and stext.c to distribute and retrieve files, ensuring efficient file management across the network.
* Transparent to Users: From the user’s perspective, the system behaves as if it is interacting with a single server, while in reality, files are stored and managed across multiple servers.
This project demonstrates the principles of distributed systems, networking, and file management, ensuring scalability and efficient handling of different file types across multiple servers.


## System Design

Below figures shows how the program functions are structured

### Servers

![Servers](https://github.com/damletanmay/distributed_file_system/blob/main/0%20Servers.png)

### Upload File

![upload_file](https://github.com/damletanmay/distributed_file_system/blob/main/1%20Upload%20File.png)

### Download File

![download_file](https://github.com/damletanmay/distributed_file_system/blob/main/2%20Download%20File.png)

### Remove File

![remove_file](https://github.com/damletanmay/distributed_file_system/blob/main/3%20Remove%20File.png)

### Tar File

![tar_file](https://github.com/damletanmay/distributed_file_system/blob/main/4%20Tar%20File.png)

### Display File

![display_file](https://github.com/damletanmay/distributed_file_system/blob/main/5%20Display%20File.png)

## Usage
       ./spdf <port number for spdf>
       ./stext <port number for stext>
       ./smain <port number for smain> <spdf port> <stext port>
       ./client24s


Take a look at [test_cases] to learn more about all the client commands (https://github.com/damletanmay/distributed_file_system/blob/main/test_cases.txt)
