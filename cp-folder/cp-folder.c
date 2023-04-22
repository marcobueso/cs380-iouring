#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <liburing.h>

#define QD  1
#define BS (16 * 1024) //16kB

static int infd, outfd;
struct io_data {
    int read;
    off_t first_offset, offset;
    size_t first_len;
    struct iovec iov;
};

static int setup_context(unsigned entries, struct io_uring *ring) {
    int ret;

    ret = io_uring_queue_init(entries, ring, 0);
    if( ret < 0) {
        //fprintf(stderr, "queue_init: %s\n", strerror(-ret));
        return -1;
    }

    return 0;
}

off_t get_file_size(int fd) {
    struct stat st;
    off_t size = 0;
    if (fstat(fd, &st) < 0 )
        return -1;
    if(S_ISREG(st.st_mode)) {
        size = st.st_size;
        return size;
    } else if (S_ISBLK(st.st_mode)) {
        unsigned long long bytes;

        if (ioctl(fd, BLKGETSIZE64, &bytes) != 0)
            return -1;

        size = bytes;
        return size;
    }
    return -1;
}

static void queue_prepped(struct io_uring *ring, struct io_data *data) {
    struct io_uring_sqe *sqe;

    sqe = io_uring_get_sqe(ring);
    assert(sqe);

    if (data->read)
        io_uring_prep_readv(sqe, infd, &data->iov, 1, data->offset);
    else
        io_uring_prep_writev(sqe, outfd, &data->iov, 1, data->offset);

    io_uring_sqe_set_data(sqe, data);
}

static int queue_read(struct io_uring *ring, off_t size, off_t offset) {
    struct io_uring_sqe *sqe;
    struct io_data *data;

    data = malloc(size + sizeof(*data));
    if (!data)
        return 1;
    //fprintf(stderr,"io_uring_get_sqe\n");
    sqe = io_uring_get_sqe(ring);

    data->read = 1;
    data->offset = data->first_offset = offset;

    data->iov.iov_base = data + 1;
    data->iov.iov_len = size;
    data->first_len = size;
    //fprintf(stderr,"io_uring_prep_readv\n");
    io_uring_prep_readv(sqe, infd, &data->iov, 1, offset);
    io_uring_sqe_set_data(sqe, data);
    return 0;
}

static void queue_write(struct io_uring *ring, struct io_data *data) {
    data->read = 0;
    data->offset = data->first_offset;

    data->iov.iov_base = data + 1;
    data->iov.iov_len = data->first_len;

    queue_prepped(ring, data);
    io_uring_submit(ring);
}

int copy_file(char* src, char* dst, struct io_uring *ring) {
    infd = open(src, O_RDONLY);
    outfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    off_t size = get_file_size(infd);
    //fprintf(stderr, "file size: %ld\n", size);
    unsigned long reads, writes;
    struct io_uring_cqe *cqe;
    off_t write_left, offset;
    int ret;

    write_left = size;
    writes = reads = offset = 0;
    while (size || write_left) {
        int had_reads, got_comp;

        /* Queue up as many reads as we can */
        had_reads = reads;
        while (size) {
            off_t this_size = size;
            // fprintf(stderr, "reads + writes= %ld + %ld\n", reads, writes);
            if (reads + writes >= QD)
                break;
            if (this_size > BS)
                this_size = BS;
            else if (!this_size)
                break;
            //fprintf(stderr,"queue_read called...\n");
            if (queue_read(ring, this_size, offset))
                break;

            size -= this_size;
            offset += this_size;
            reads++;
        }

        if (had_reads != reads) {
            ret = io_uring_submit(ring);
            if (ret < 0) {
                //fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
                break;
            }
        }

        /* Queue is full at this point. Let's find at least one completion */
        got_comp = 0;
        while (write_left) {
            struct io_data *data;

            if (!got_comp) {
                ret = io_uring_wait_cqe(ring, &cqe);
                got_comp = 1;
            } else {
                ret = io_uring_peek_cqe(ring, &cqe);
                if (ret == -EAGAIN) {
                    cqe = NULL;
                    ret = 0;
                }
            }
            if (ret < 0) {
                fprintf(stderr, "io_uring_peek_cqe: %s\n",
                        strerror(-ret));
                return 1;
            }
            if (!cqe)
                break;

            data = io_uring_cqe_get_data(cqe);
            if (cqe->res < 0) {
                if (cqe->res == -EAGAIN) {
                    queue_prepped(ring, data);
                    io_uring_cqe_seen(ring, cqe);
                    continue;
                }
                fprintf(stderr, "cqe failed: %s\n",
                        strerror(-cqe->res));
                return 1;
            } else if (cqe->res != data->iov.iov_len) {
                /* short read/write; adjust and requeue */
                data->iov.iov_base += cqe->res;
                data->iov.iov_len -= cqe->res;
                queue_prepped(ring, data);
                io_uring_cqe_seen(ring, cqe);
                continue;
            }

            /*
             * All done. If write, nothing else to do. If read,
             * queue up corresponding write.
             * */

            if (data->read) {
                queue_write(ring, data);
                write_left -= data->first_len;
                reads--;
                writes++;
            } else {
                writes--;
            }
            io_uring_cqe_seen(ring, cqe);
        }
    }
    close(infd);
    close(outfd);
    return 0;
}

void copy_directory(char* source_path, char* dest_path, struct io_uring *ring) {
    DIR* dir = opendir(source_path);

    if (!dir) {
        fprintf(stderr, "Error: could not open directory\n");
        exit(1);
    }


    struct dirent* entry;
    while ((entry = readdir(dir))) {
        char source_item_path[4096];
        snprintf(source_item_path, sizeof(source_item_path), "%s/%s", source_path, entry->d_name);

        char dest_item_path[4096];
        snprintf(dest_item_path, sizeof(dest_item_path), "%s/%s", dest_path, entry->d_name);

        struct stat statbuf;
        if (stat(source_item_path, &statbuf) == -1) {
            fprintf(stderr, "Error: could not get file/directory information\n");
            exit(1);
        }

        if (S_ISREG(statbuf.st_mode)) {
            copy_file(source_item_path, dest_item_path, ring);
        } else if (S_ISDIR(statbuf.st_mode)) {
            mkdir(dest_item_path, statbuf.st_mode);
            copy_directory(source_item_path, dest_item_path, ring);
        }
    }

    closedir(dir);
}

int main(int argc, char* argv[]) {
    struct io_uring ring;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        exit(1);
    }

    setup_context(QD, &ring);

    copy_directory(argv[1], argv[2], &ring);

    return 0;
}