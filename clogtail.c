#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>

#define OFFSETEXT ".offset"

#define FASSERT(x, y) if ((x) == -1) { perror((y)); return 1; }

typedef struct {
    off_t offset;
    ino_t inode;
} offset_t;

int main (int argc, char *argv[]) {

    char *buf;
    struct stat input_stat;
    int res;
    offset_t offset_data;

    long page_size = sysconf(_SC_PAGESIZE);
    long page_offset = page_size - 1;
    long page_align  = ~ page_offset;

    if (argc < 2) {
        printf("arguments required:\n\t- path to file (required)\n\t- glob to search for rotated file (optional)\n");
        return 1;
    }

    char * input_fn = argv[1];
    int input_fd = open(input_fn, O_RDONLY);
    FASSERT(input_fd, "file open")

    res = fstat(input_fd, &input_stat);
    FASSERT(res, "file stat")

    char offset_fn[strlen(input_fn) + sizeof(OFFSETEXT)];
    strcpy(offset_fn, input_fn);
    strcat(offset_fn, OFFSETEXT);

    res = access(offset_fn, F_OK | R_OK | W_OK );

    int offset_fd = open(offset_fn, O_RDWR | O_CREAT);
    FASSERT(offset_fd, "offset file open")

    if (res == 0) { // we found the ".offset" file

        res = read(offset_fd, &offset_data, sizeof(offset_t));
        FASSERT(res, "offset file read")

        if (offset_data.offset == input_stat.st_size &&
            offset_data.inode == input_stat.st_ino) // no changes
                return 0;

        if (offset_data.inode != input_stat.st_ino) { // file rotated

            if (argc == 3) { // we have glob to search for rotated file

                int i, found = 0;
                glob_t glob_data;
                struct stat search_stat;

                if (!glob(argv[2], GLOB_NOSORT, NULL, &glob_data))
                    for (i = 0; i < glob_data.gl_pathc && !found; i++)
                        if(!stat(glob_data.gl_pathv[i], &search_stat))
                            found = (search_stat.st_ino == offset_data.inode);

                if (found) {

                    fprintf(stderr, "file rotated, found at %s\n", glob_data.gl_pathv[i - 1]);

                    size_t tail_len = search_stat.st_size - offset_data.offset;
                    size_t buf_offset = offset_data.offset & page_offset;
                    size_t buf_size = (tail_len + buf_offset + page_offset) & page_align;

                    int globfd = open(glob_data.gl_pathv[i - 1], O_RDONLY);
                    FASSERT(globfd, "found file open")

                    buf = mmap(NULL, buf_size, PROT_READ, MAP_SHARED, globfd, offset_data.offset & page_align);
                    if (buf == MAP_FAILED) {
                        perror("mmap file");
                        return 1;
                    }

                    write(STDOUT_FILENO, buf + buf_offset, tail_len);

                    res = munmap(buf, buf_size);
                    FASSERT(res, "unmap file")

                    close(globfd);

                } else
                    fputs("warning, file rotated and was not found\n", stderr);

                globfree(&glob_data);

            } else
                fputs("warning, file rotated and no glob specified\n", stderr);

            offset_data.inode = input_stat.st_ino;
            offset_data.offset = 0;

        } else { // file was not rotated

            if (offset_data.offset > input_stat.st_size ) {
                fputs("warning, truncated file\n", stderr);
                offset_data.offset = 0;
            }
        }

        size_t buf_offset = offset_data.offset & page_offset;
        size_t buf_size = (input_stat.st_size - offset_data.offset + buf_offset + page_offset) & page_align;
        buf = mmap(NULL, buf_size, PROT_READ, MAP_SHARED, input_fd, offset_data.offset & page_align);

        if (buf == MAP_FAILED) {
            perror("cannot mmap file");
            return 1;
        }

        char *line_end, *line_start = buf + buf_offset;

        while (
                (offset_data.offset < input_stat.st_size) && 
                (line_end = memchr(line_start, '\n', input_stat.st_size - offset_data.offset))
        ) {

            size_t line_len = line_end - line_start + 1;
            offset_data.offset += line_len;

            write(STDOUT_FILENO, line_start, line_len);
            line_start = line_end + 1;
        }

        res = munmap(buf, buf_size);
        FASSERT(res, "unmap file")

    } else { // no offset file

        offset_data.offset = input_stat.st_size;
        offset_data.inode = input_stat.st_ino;
    }

    close(input_fd);

    lseek(offset_fd, 0, SEEK_SET);
    write(offset_fd, &offset_data, sizeof(offset_t));
    close(offset_fd);

    return 0;
}
