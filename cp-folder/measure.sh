#!/bin/bash

source_dir="rootdir"
destination_dir="dst"

# Measure performance of regular cp -r
echo "Measuring performance of regular cp -r"
# Clear cache
sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
# Use cp -rT to copy everything inside of source directory, rather than copying source directory
# Use the 2>&1 to capture the output and print to console
time_output=$( { time cp -rT "$source_dir" "$destination_dir"; } 2>&1 )
echo "Regular cp -r time: $time_output"
echo ""

# Assert that destination directory matches source directory
diff -r "$source_dir" "$destination_dir" && echo "Destination directory matches source directory"

# Clean up destination directory
rm -rf "$destination_dir"/*
# Wait for directory removal to complete
sleep 4

# Measure performance of io_uring cp -r
echo "Measuring performance of io_uring cp -r"
# Clear cache
sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
time_output=$( { time ./cp-folder "$source_dir" "$destination_dir"; } 2>&1 )
echo "io_uring cp -r time: $time_output"
echo ""

# Assert that destination directory matches source directory
diff -r "$source_dir" "$destination_dir" && echo "Destination directory matches source directory"

#clean up
rm -rf "$destination_dir"/*