#!/bin/bash

output_file="measure_output_10000files.txt"

# create output file
echo -n "" > "$output_file"

# run 100 times, timeout of 8 seconds enough for a successful completion
for i in {1..100}; do
  echo "Running iteration $i..."
  timeout 9s ./measure.sh >> "$output_file"
  echo "Iteration $i done." >> "$output_file"
  echo "-------------------------" >> "$output_file"
done

echo "Done"