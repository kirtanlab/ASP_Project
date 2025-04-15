#!/bin/bash

# Define color codes for better readability
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Define log file paths
LOG_DIR="$HOME/dfs_logs"
SPDF_LOG="$LOG_DIR/spdf.log"
STEXT_LOG="$LOG_DIR/stext.log"
SZIP_LOG="$LOG_DIR/szip.log"
SMAIN_LOG="$LOG_DIR/smain.log"
CLIENT_LOG="$LOG_DIR/client.log"

# Create log directory if it doesn't exist
create_log_dir() {
    if [ ! -d "$LOG_DIR" ]; then
        echo -e "${YELLOW}Creating log directory at $LOG_DIR...${NC}"
        mkdir -p "$LOG_DIR"
        check_status
    fi
    
    # Clear previous log files if they exist
    > "$SPDF_LOG"
    > "$STEXT_LOG"
    > "$SZIP_LOG"
    > "$SMAIN_LOG"
    > "$CLIENT_LOG"
    
    echo -e "${GREEN}Log files initialized in $LOG_DIR${NC}"
}

# Function to check if a command was successful
check_status() {
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Success!${NC}"
    else
        echo -e "${RED}Failed!${NC}"
        exit 1
    fi
}

# Function to check if a port is available
check_port() {
    local port=$1
    if netstat -tuln | grep -q ":$port "; then
        return 1
    else
        return 0
    fi
}

# Find available ports
find_available_ports() {
    local base_port=8080
    local max_attempts=20
    
    # Find SMAIN port
    local port=$base_port
    local attempts=0
    while ! check_port $port && [ $attempts -lt $max_attempts ]; do
        port=$((port + 1))
        attempts=$((attempts + 1))
    done
    if [ $attempts -lt $max_attempts ]; then
        SMAIN_PORT=$port
    fi
    
    # Find SPDF port
    port=$((SMAIN_PORT + 1))
    attempts=0
    while ! check_port $port && [ $attempts -lt $max_attempts ]; do
        port=$((port + 1))
        attempts=$((attempts + 1))
    done
    if [ $attempts -lt $max_attempts ]; then
        SPDF_PORT=$port
    fi
    
    # Find STEXT port
    port=$((SPDF_PORT + 1))
    attempts=0
    while ! check_port $port && [ $attempts -lt $max_attempts ]; do
        port=$((port + 1))
        attempts=$((attempts + 1))
    done
    if [ $attempts -lt $max_attempts ]; then
        STEXT_PORT=$port
    fi
    
    # Find SZIP port
    port=$((STEXT_PORT + 1))
    attempts=0
    while ! check_port $port && [ $attempts -lt $max_attempts ]; do
        port=$((port + 1))
        attempts=$((attempts + 1))
    done
    if [ $attempts -lt $max_attempts ]; then
        SZIP_PORT=$port
    fi
    
    echo -e "${GREEN}Found available ports: SMAIN=$SMAIN_PORT, SPDF=$SPDF_PORT, STEXT=$STEXT_PORT, SZIP=$SZIP_PORT${NC}"
}

# Default port numbers - will be updated if auto-port-selection is enabled
SPDF_PORT=8081
STEXT_PORT=8082
SMAIN_PORT=8080
SZIP_PORT=8083
AUTO_PORT=true

# Function to kill lingering server processes
cleanup_processes() {
    echo -e "${YELLOW}Cleaning up lingering server processes...${NC}"

    # Find all processes
    SPDF_PROCESSES=$(pgrep -f "./spdf [0-9]*")
    STEXT_PROCESSES=$(pgrep -f "./stext [0-9]*")
    SZIP_PROCESSES=$(pgrep -f "./szip [0-9]*")
    SMAIN_PROCESSES=$(pgrep -f "./smain [0-9]* [0-9]* [0-9]* [0-9]*")

    # Create a combined list of all PIDs
    ALL_PIDS=""
    if [ ! -z "$SPDF_PROCESSES" ]; then
        ALL_PIDS="$ALL_PIDS $SPDF_PROCESSES"
    fi
    if [ ! -z "$STEXT_PROCESSES" ]; then
        ALL_PIDS="$ALL_PIDS $STEXT_PROCESSES"
    fi
    if [ ! -z "$SZIP_PROCESSES" ]; then
        ALL_PIDS="$ALL_PIDS $SZIP_PROCESSES"
    fi
    if [ ! -z "$SMAIN_PROCESSES" ]; then
        ALL_PIDS="$ALL_PIDS $SMAIN_PROCESSES"
    fi

    # Count the total processes
    TOTAL_COUNT=$(echo $ALL_PIDS | wc -w)

    if [ $TOTAL_COUNT -eq 0 ]; then
        echo -e "${GREEN}No lingering processes found.${NC}"
        return 0
    fi

    # Show the processes that will be killed
    echo -e "${YELLOW}Found $TOTAL_COUNT distributed file system processes:${NC}"
    echo "---------------------------------------------------------"
    echo -e "${YELLOW}SPDF Processes:${NC}"
    if [ ! -z "$SPDF_PROCESSES" ]; then
        for pid in $SPDF_PROCESSES; do
            ps -p $pid -o pid,cmd
        done
    else
        echo "None"
    fi

    echo -e "${YELLOW}STEXT Processes:${NC}"
    if [ ! -z "$STEXT_PROCESSES" ]; then
        for pid in $STEXT_PROCESSES; do
            ps -p $pid -o pid,cmd
        done
    else
        echo "None"
    fi

    echo -e "${YELLOW}SZIP Processes:${NC}"
    if [ ! -z "$SZIP_PROCESSES" ]; then
        for pid in $SZIP_PROCESSES; do
            ps -p $pid -o pid,cmd
        done
    else
        echo "None"
    fi

    echo -e "${YELLOW}SMAIN Processes:${NC}"
    if [ ! -z "$SMAIN_PROCESSES" ]; then
        for pid in $SMAIN_PROCESSES; do
            ps -p $pid -o pid,cmd
        done
    else
        echo "None"
    fi
    echo "---------------------------------------------------------"

    # Kill all processes
    for pid in $ALL_PIDS; do
        echo -e "Killing process $pid: $(ps -p $pid -o cmd=)"
        kill $pid 2>/dev/null
    done
    
    # Verify all processes were killed
    sleep 2
    REMAINING=$(pgrep -f "./s(pdf|text|zip|main) [0-9]*" | wc -l)
    
    if [ $REMAINING -eq 0 ]; then
        echo -e "${GREEN}All processes successfully terminated.${NC}"
    else
        echo -e "${RED}Some processes are still running. Attempting to force kill...${NC}"
        for pid in $(pgrep -f "./s(pdf|text|zip|main) [0-9]*"); do
            echo -e "Force killing process $pid: $(ps -p $pid -o cmd=)"
            kill -9 $pid 2>/dev/null
        done
        
        sleep 1
        REMAINING=$(pgrep -f "./s(pdf|text|zip|main) [0-9]*" | wc -l)
        if [ $REMAINING -eq 0 ]; then
            echo -e "${GREEN}All processes successfully terminated.${NC}"
        else
            echo -e "${RED}Failed to kill all processes. Please check manually with 'ps aux | grep \"./s\"'${NC}"
        fi
    fi
    
    # Clean up any leftover files
    if [ -f .server_pids ]; then
        rm -f .server_pids
    fi

    if [ -f .server_ports ]; then
        rm -f .server_ports
    fi
}

# Create a function to print usage information
print_usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  --cleanup               Kill all lingering server processes and exit"
    echo "  --spdf-port PORT        Set SPDF server port (default: auto-detect)"
    echo "  --stext-port PORT       Set STEXT server port (default: auto-detect)"
    echo "  --szip-port PORT        Set SZIP server port (default: auto-detect)"
    echo "  --smain-port PORT       Set SMAIN server port (default: auto-detect)"
    echo "  --fixed-ports           Use default ports instead of auto-detecting"
    echo "  --build-only            Only build, don't run"
    echo "  --client-only           Start only client (servers must be running)"
    echo "  --log-dir PATH          Specify a custom log directory (default: ~/dfs_logs)"
    exit 0
}

# Parse command line arguments
BUILD_ONLY=false
CLIENT_ONLY=false
CLEANUP_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_usage
            ;;
        --cleanup)
            CLEANUP_ONLY=true
            shift
            ;;
        --spdf-port)
            SPDF_PORT="$2"
            AUTO_PORT=false
            shift 2
            ;;
        --stext-port)
            STEXT_PORT="$2"
            AUTO_PORT=false
            shift 2
            ;;
        --szip-port)
            SZIP_PORT="$2"
            AUTO_PORT=false
            shift 2
            ;;
        --smain-port)
            SMAIN_PORT="$2"
            AUTO_PORT=false
            shift 2
            ;;
        --build-only)
            BUILD_ONLY=true
            shift
            ;;
        --client-only)
            CLIENT_ONLY=true
            shift
            ;;
        --fixed-ports)
            AUTO_PORT=false
            shift
            ;;
        --log-dir)
            LOG_DIR="$2"
            SPDF_LOG="$LOG_DIR/spdf.log"
            STEXT_LOG="$LOG_DIR/stext.log"
            SZIP_LOG="$LOG_DIR/szip.log"
            SMAIN_LOG="$LOG_DIR/smain.log"
            CLIENT_LOG="$LOG_DIR/client.log"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            ;;
    esac
done

# If cleanup-only is set, kill all lingering processes and exit
if [ "$CLEANUP_ONLY" = true ]; then
    cleanup_processes
    exit 0
fi

# Create and initialize log directory
create_log_dir

# Always run cleanup first to ensure no lingering processes
cleanup_processes

# Auto-select ports if enabled and not in client-only mode
if [ "$AUTO_PORT" = true ] && [ "$CLIENT_ONLY" = false ]; then
    find_available_ports
fi

# Step 1: Compile all source files
echo -e "${YELLOW}Building all components...${NC}"

if [ "$CLIENT_ONLY" = false ]; then
    echo -n "Compiling SPDF server... "
    gcc -o spdf Spdf.c
    check_status

    echo -n "Compiling STEXT server... "
    gcc -o stext Stext.c
    check_status

    echo -n "Compiling SZIP server... "
    gcc -o szip Szip.c
    check_status

    echo -n "Compiling SMAIN server... "
    gcc -o smain Smain.c
    check_status
fi

echo -n "Compiling client... "
gcc -o client24s client24s.c
check_status

echo -e "${GREEN}All components built successfully!${NC}"

# Create required directories
echo -e "${YELLOW}Setting up directories...${NC}"
mkdir -p ~/smain
mkdir -p ~/spdf
mkdir -p ~/stext
mkdir -p ~/szip
echo -e "${GREEN}Directories created!${NC}"

# If build-only flag is set, exit here
if [ "$BUILD_ONLY" = true ]; then
    echo -e "${GREEN}Build completed. Not starting servers as --build-only was specified.${NC}"
    echo "To start the servers with log redirection, run:"
    echo "./spdf $SPDF_PORT > $SPDF_LOG 2>&1 &"
    echo "./stext $STEXT_PORT > $STEXT_LOG 2>&1 &"
    echo "./szip $SZIP_PORT > $SZIP_LOG 2>&1 &"
    echo "./smain $SMAIN_PORT $SPDF_PORT $STEXT_PORT $SZIP_PORT > $SMAIN_LOG 2>&1 &"
    echo "And to connect with the client:"
    echo "./client24s $SMAIN_PORT > $CLIENT_LOG 2>&1"
    exit 0
fi

# Modify smain.c to use localhost as IP address
echo -e "${YELLOW}Adjusting IP addresses in source code...${NC}"

# Backup original files
cp Smain.c Smain.c.bak

# Update IP addresses in Smain.c to use localhost
sed -i 's/char SPDF_IP\[\] = "10.60.8.51";/char SPDF_IP\[\] = "127.0.0.1";/' Smain.c 2>>$LOG_DIR/script.log || sed -i 's/char SPDF_IP\[\] = "127.0.1.1";/char SPDF_IP\[\] = "127.0.0.1";/' Smain.c 2>>$LOG_DIR/script.log
sed -i 's/char STEXT_IP\[\] = "10.60.8.51";/char STEXT_IP\[\] = "127.0.0.1";/' Smain.c 2>>$LOG_DIR/script.log || sed -i 's/char STEXT_IP\[\] = "127.0.1.1";/char STEXT_IP\[\] = "127.0.0.1";/' Smain.c 2>>$LOG_DIR/script.log
sed -i 's/char SZIP_IP\[\] = "10.60.8.51";/char SZIP_IP\[\] = "127.0.0.1";/' Smain.c 2>>$LOG_DIR/script.log || sed -i 's/char SZIP_IP\[\] = "127.0.1.1";/char SZIP_IP\[\] = "127.0.0.1";/' Smain.c 2>>$LOG_DIR/script.log

# Recompile Smain with new IP addresses
echo -n "Recompiling SMAIN server with localhost IP... "
gcc -o smain Smain.c
check_status

# Update IP address in client24s.c to use localhost for easier testing
cp client24s.c client24s.c.bak
sed -i 's/char IP\[\] = "10.60.8.51";/char IP\[\] = "127.0.0.1";/' client24s.c 2>>$LOG_DIR/script.log || sed -i 's/char IP\[\] = "127.0.1.1";/char IP\[\] = "127.0.0.1";/' client24s.c 2>>$LOG_DIR/script.log

echo -n "Recompiling client with localhost IP... "
gcc -o client24s client24s.c
check_status

# Start the servers in the correct order with additional error checking, but only if not in client-only mode
if [ "$CLIENT_ONLY" = false ]; then
    echo -e "${YELLOW}Starting servers with log redirection...${NC}"
    
    # Start SPDF server
    echo -e "${YELLOW}Starting SPDF server on port $SPDF_PORT (logs: $SPDF_LOG)...${NC}"
    ./spdf $SPDF_PORT > "$SPDF_LOG" 2>&1 &
    SPDF_PID=$!
    sleep 2
    
    # Check if SPDF is running
    if ! ps -p $SPDF_PID > /dev/null; then
        echo -e "${RED}SPDF server failed to start. Trying another port...${NC}"
        SPDF_PORT=$((SPDF_PORT + 10))
        echo -e "${YELLOW}Retrying with port $SPDF_PORT...${NC}"
        ./spdf $SPDF_PORT > "$SPDF_LOG" 2>&1 &
        SPDF_PID=$!
        sleep 2
        
        if ! ps -p $SPDF_PID > /dev/null; then
            echo -e "${RED}SPDF server failed to start again. Check $SPDF_LOG for details. Aborting.${NC}"
            exit 1
        fi
    fi
    
    # Start STEXT server
    echo -e "${YELLOW}Starting STEXT server on port $STEXT_PORT (logs: $STEXT_LOG)...${NC}"
    ./stext $STEXT_PORT > "$STEXT_LOG" 2>&1 &
    STEXT_PID=$!
    sleep 2
    
    # Check if STEXT is running
    if ! ps -p $STEXT_PID > /dev/null; then
        echo -e "${RED}STEXT server failed to start. Trying another port...${NC}"
        STEXT_PORT=$((STEXT_PORT + 10))
        echo -e "${YELLOW}Retrying with port $STEXT_PORT...${NC}"
        ./stext $STEXT_PORT > "$STEXT_LOG" 2>&1 &
        STEXT_PID=$!
        sleep 2
        
        if ! ps -p $STEXT_PID > /dev/null; then
            echo -e "${RED}STEXT server failed to start again. Check $STEXT_LOG for details. Aborting.${NC}"
            kill $SPDF_PID 2>/dev/null
            exit 1
        fi
    fi
    
    # Start SZIP server
    echo -e "${YELLOW}Starting SZIP server on port $SZIP_PORT (logs: $SZIP_LOG)...${NC}"
    ./szip $SZIP_PORT > "$SZIP_LOG" 2>&1 &
    SZIP_PID=$!
    sleep 2
    
    # Check if SZIP is running
    if ! ps -p $SZIP_PID > /dev/null; then
        echo -e "${RED}SZIP server failed to start. Trying another port...${NC}"
        SZIP_PORT=$((SZIP_PORT + 10))
        echo -e "${YELLOW}Retrying with port $SZIP_PORT...${NC}"
        ./szip $SZIP_PORT > "$SZIP_LOG" 2>&1 &
        SZIP_PID=$!
        sleep 2
        
        if ! ps -p $SZIP_PID > /dev/null; then
            echo -e "${RED}SZIP server failed to start again. Check $SZIP_LOG for details. Aborting.${NC}"
            kill $SPDF_PID $STEXT_PID 2>/dev/null
            exit 1
        fi
    fi
    
    # Start SMAIN server
    echo -e "${YELLOW}Starting SMAIN server on port $SMAIN_PORT (connecting to SPDF:$SPDF_PORT, STEXT:$STEXT_PORT, and SZIP:$SZIP_PORT) (logs: $SMAIN_LOG)...${NC}"
    ./smain $SMAIN_PORT $SPDF_PORT $STEXT_PORT $SZIP_PORT > "$SMAIN_LOG" 2>&1 &
    SMAIN_PID=$!
    sleep 2
    
    # Check if SMAIN is running
    if ! ps -p $SMAIN_PID > /dev/null; then
        echo -e "${RED}SMAIN server failed to start. Trying another port...${NC}"
        SMAIN_PORT=$((SMAIN_PORT + 10))
        echo -e "${YELLOW}Retrying with port $SMAIN_PORT...${NC}"
        ./smain $SMAIN_PORT $SPDF_PORT $STEXT_PORT $SZIP_PORT > "$SMAIN_LOG" 2>&1 &
        SMAIN_PID=$!
        sleep 2
        
        if ! ps -p $SMAIN_PID > /dev/null; then
            echo -e "${RED}SMAIN server failed to start again. Check $SMAIN_LOG for details. Aborting.${NC}"
            kill $SPDF_PID $STEXT_PID $SZIP_PID 2>/dev/null
            exit 1
        fi
    fi

    echo -e "${GREEN}All servers started successfully!${NC}"
    echo "SPDF server PID: $SPDF_PID (port $SPDF_PORT, logs: $SPDF_LOG)"
    echo "STEXT server PID: $STEXT_PID (port $STEXT_PORT, logs: $STEXT_LOG)"
    echo "SZIP server PID: $SZIP_PID (port $SZIP_PORT, logs: $SZIP_LOG)"
    echo "SMAIN server PID: $SMAIN_PID (port $SMAIN_PORT, logs: $SMAIN_LOG)"
    
    # Save PIDs to a file for easier cleanup later
    echo "$SPDF_PID $STEXT_PID $SZIP_PID $SMAIN_PID" > .server_pids
    # Also save ports for client-only mode
    echo "$SMAIN_PORT $SPDF_PORT $STEXT_PORT $SZIP_PORT" > .server_ports
fi

# Start the client - FIXED: Direct execution instead of tee to avoid issues
if [ "$CLIENT_ONLY" = true ] && [ -f .server_ports ]; then
    # Read the saved SMAIN port if in client-only mode
    read -r SAVED_SMAIN_PORT SAVED_SPDF_PORT SAVED_STEXT_PORT SAVED_SZIP_PORT < .server_ports
    echo -e "${YELLOW}Starting client (connecting to previously started SMAIN server on port $SAVED_SMAIN_PORT) (logs: $CLIENT_LOG)...${NC}"
    echo -e "${GREEN}Client started. Output will be displayed in terminal and saved to $CLIENT_LOG${NC}"
    # Run client directly - log via script command if available
    if command -v script >/dev/null 2>&1; then
        script -c "./client24s $SAVED_SMAIN_PORT" -a "$CLIENT_LOG"
    else
        # Fallback if script is not available
        ./client24s $SAVED_SMAIN_PORT 2>&1 | tee "$CLIENT_LOG"
    fi
else
    echo -e "${YELLOW}Starting client (connecting to SMAIN server on port $SMAIN_PORT) (logs: $CLIENT_LOG)...${NC}"
    echo -e "${GREEN}Client started. Output will be displayed in terminal and saved to $CLIENT_LOG${NC}"
    # Run client directly - log via script command if available
    if command -v script >/dev/null 2>&1; then
        script -c "./client24s $SMAIN_PORT" -a "$CLIENT_LOG"
    else
        # Fallback if script is not available
        ./client24s $SMAIN_PORT 2>&1 | tee "$CLIENT_LOG"
    fi
fi

# When client exits, ask if servers should be stopped (if we started them)
if [ "$CLIENT_ONLY" = false ] && [ -f .server_pids ]; then
    echo -e "${YELLOW}Client exited. Do you want to stop the servers? (y/n)${NC}"
    read -r answer
    if [[ "$answer" =~ ^[Yy] ]]; then
        read -r SPDF_PID STEXT_PID SZIP_PID SMAIN_PID < .server_pids
        echo -e "${YELLOW}Stopping servers...${NC}"
        kill $SMAIN_PID $SPDF_PID $STEXT_PID $SZIP_PID 2>/dev/null
        rm -f .server_pids
        rm -f .server_ports
        echo -e "${GREEN}Servers stopped.${NC}"
        echo -e "${YELLOW}Log files are available at:${NC}"
        echo "SPDF: $SPDF_LOG"
        echo "STEXT: $STEXT_LOG"
        echo "SZIP: $SZIP_LOG"
        echo "SMAIN: $SMAIN_LOG"
        echo "Client: $CLIENT_LOG"
    else
        echo -e "${YELLOW}Servers are still running. To stop them later, run:${NC}"
        echo "$0 --cleanup"
        echo -e "${YELLOW}Log files are available at:${NC}"
        echo "SPDF: $SPDF_LOG"
        echo "STEXT: $STEXT_LOG"
        echo "SZIP: $SZIP_LOG"
        echo "SMAIN: $SMAIN_LOG"
        echo "Client: $CLIENT_LOG"
    fi
fi

# Restore original files if they were modified
if [ -f Smain.c.bak ]; then
    mv Smain.c.bak Smain.c
    echo -e "${YELLOW}Restored original Smain.c${NC}"
fi

if [ -f client24s.c.bak ]; then
    mv client24s.c.bak client24s.c
    echo -e "${YELLOW}Restored original client24s.c${NC}"
fi

echo -e "${GREEN}Done!${NC}"