#!/bin/bash

# Define color codes for better readability
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Looking for distributed file system processes...${NC}"

# Find all processes
SPDF_PROCESSES=$(pgrep -f "./spdf [0-9]*")
STEXT_PROCESSES=$(pgrep -f "./stext [0-9]*")
SMAIN_PROCESSES=$(pgrep -f "./smain [0-9]* [0-9]* [0-9]*")

# Create a combined list of all PIDs
ALL_PIDS=""
if [ ! -z "$SPDF_PROCESSES" ]; then
    ALL_PIDS="$ALL_PIDS $SPDF_PROCESSES"
fi
if [ ! -z "$STEXT_PROCESSES" ]; then
    ALL_PIDS="$ALL_PIDS $STEXT_PROCESSES"
fi
if [ ! -z "$SMAIN_PROCESSES" ]; then
    ALL_PIDS="$ALL_PIDS $SMAIN_PROCESSES"
fi

# Count the total processes
TOTAL_COUNT=$(echo $ALL_PIDS | wc -w)

if [ $TOTAL_COUNT -eq 0 ]; then
    echo -e "${GREEN}No distributed file system processes found.${NC}"
    exit 0
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

echo -e "${YELLOW}SMAIN Processes:${NC}"
if [ ! -z "$SMAIN_PROCESSES" ]; then
    for pid in $SMAIN_PROCESSES; do
        ps -p $pid -o pid,cmd
    done
else
    echo "None"
fi
echo "---------------------------------------------------------"

# Ask for confirmation before killing
echo -e "${YELLOW}Do you want to kill all $TOTAL_COUNT processes? (y/n)${NC}"
read -r answer
if [[ "$answer" =~ ^[Yy] ]]; then
    # Kill all processes
    for pid in $ALL_PIDS; do
        echo -e "Killing process $pid: $(ps -p $pid -o cmd=)"
        kill $pid 2>/dev/null
    done
    
    # Verify all processes were killed
    sleep 2
    REMAINING=$(pgrep -f "./s(pdf|text|main) [0-9]*" | wc -l)
    
    if [ $REMAINING -eq 0 ]; then
        echo -e "${GREEN}All processes successfully terminated.${NC}"
    else
        echo -e "${RED}Some processes are still running. Attempting to force kill...${NC}"
        for pid in $(pgrep -f "./s(pdf|text|main) [0-9]*"); do
            echo -e "Force killing process $pid: $(ps -p $pid -o cmd=)"
            kill -9 $pid 2>/dev/null
        done
        
        sleep 1
        REMAINING=$(pgrep -f "./s(pdf|text|main) [0-9]*" | wc -l)
        if [ $REMAINING -eq 0 ]; then
            echo -e "${GREEN}All processes successfully terminated.${NC}"
        else
            echo -e "${RED}Failed to kill all processes. Please check manually with 'ps aux | grep \"./s\"'${NC}"
        fi
    fi
else
    echo -e "${YELLOW}Operation cancelled. No processes were killed.${NC}"
fi

# Also clean up any leftover files
if [ -f .server_pids ]; then
    rm -f .server_pids
    echo "Removed .server_pids file"
fi

if [ -f .server_ports ]; then
    rm -f .server_ports
    echo "Removed .server_ports file"
fi

echo -e "${GREEN}Done!${NC}"