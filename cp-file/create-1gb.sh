#!/bin/bash

size=1000000000 # 1GB
# generate random text of the given size
text=$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w $size | head -n 1)
# write text to file
echo $text > "src/file$i.txt"
