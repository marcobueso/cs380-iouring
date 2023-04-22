import csv

# Open the output file in append mode
with open('results-10000-files.csv', 'a') as csvfile:

    # Create a CSV writer object
    csvwriter = csv.writer(csvfile)

    # Find the successful iterations and extract the times for each experiment
    with open('measure_output_10000files.txt', 'r') as f:
        lines = f.readlines()
        iteration = 1
        io_uring_real = io_uring_sys = io_uring_user = 0

        for i in range(len(lines)):
            if lines[i].startswith('Iteration'):
                if i > 0:
                    csvwriter.writerow([iteration, regular_real, regular_user, regular_sys, io_uring_real, io_uring_user, io_uring_sys])
                    iteration += 1
                regular_real = regular_user = regular_sys = io_uring_real = io_uring_user = io_uring_sys = ''
            elif lines[i].startswith('Regular cp -r time'):
                regular_real = lines[i+1].split()[1]
                regular_user = lines[i+2].split()[1]
                regular_sys = lines[i+3].split()[1]
            elif lines[i].startswith('io_uring cp -r time'):
                io_uring_real = lines[i+1].split()[1]
                io_uring_user = lines[i+2].split()[1]
                io_uring_sys = lines[i+3].split()[1]

        # Write the last iteration
        csvwriter.writerow([iteration, regular_real, regular_user, regular_sys, io_uring_real, io_uring_user, io_uring_sys])
