#!/bin/bash


# REGULAR CP
echo "Clearing file cache..."
sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'

echo "Starting program... regular cp"
start_time=$(date +%s.%N)   # Record start time

cp "$1" "$2"              # Run the program with the given arguments

end_time=$(date +%s.%N)     # Record end time

elapsed_time=$(echo "$end_time - $start_time" | bc)   # Calculate elapsed time

echo "Elapsed time: $elapsed_time seconds"

diff "$1" "$2" && echo "Destination file matches source file"

# IO_URING CP
echo "Clearing file cache..."
sync && echo 3 > /proc/sys/vm/drop_caches   # Clear file cache

echo "Starting program... io_uring cp"
start_time=$(date +%s.%N)   # Record start time

./cp "$1" "$2"              # Run the program with the given arguments

end_time=$(date +%s.%N)     # Record end time

elapsed_time=$(echo "$end_time - $start_time" | bc)   # Calculate elapsed time

echo "Elapsed time: $elapsed_time seconds"

diff "$1" "$2" && echo "Destination file matches source file"