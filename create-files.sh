#!/bin/bash

# create rootdir if it doesn't exist
if [ ! -d "rootdir" ]; then
  mkdir rootdir
fi

# create 100 files inside rootdir with random text size
for i in {1..100}; do
  # generate random text size between 10 bytes and 10 kB
  size=$(($RANDOM % 10000 + 10))
  # generate random text of the given size
  text=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w $size | head -n 1)
  # write text to file
  echo $text > "rootdir/file$i.txt"
done