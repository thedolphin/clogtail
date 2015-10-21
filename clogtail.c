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

long page_size, page_offset, page_align;

off_t logtail(int fd, off_t read_from, off_t read_to) {

    if (read_to > 0) {

        char *buf;

        size_t buf_offset = read_from & page_offset;
        size_t buf_size = (read_to - read_from + buf_offset + page_offset) & page_align;
        buf = mmap(NULL, buf_size, PROT_READ, MAP_SHARED, fd, read_from & page_align);

        if (buf == MAP_FAILED) {
            perror("cannot mmap file");
            return 1;
        }

        char *line_end, *line_start = buf + buf_offset;

        while (
                (read_from < read_to) && 
                (line_end = memchr(line_start, '\n', read_to - read_from))
        ) {

            size_t line_len = line_end - line_start + 1;
            read_from += line_len;

            write(STDOUT_FILENO, line_start, line_len);
            line_start = line_end + 1;
        }

        int res = munmap(buf, buf_size);
        FASSERT(res, "unmap file")

        return read_from;

    } else

        return 0;
}

int main (int argc, char *argv[]) {

    offset_t offset_data;
    int res;

    page_size = sysconf(_SC_PAGESIZE);
    page_offset = page_size - 1;
    page_align  = ~ page_offset;

    if (argc < 2) {
        printf("arguments required:\n\t- path to file (required)\n\t- glob to search for rotated file (optional)\n");
        return 1;
    }

    char * input_fn = argv[1];
    int input_fd = open(input_fn, O_RDONLY);
    FASSERT(input_fd, "file open")

    struct stat input_stat;
    res = fstat(input_fd, &input_stat);
    FASSERT(res, "file stat")

    char offset_fn[strlen(input_fn) + sizeof(OFFSETEXT)];
    strcpy(offset_fn, input_fn);
    strcat(offset_fn, OFFSETEXT);

    res = access(offset_fn, F_OK | R_OK | W_OK );

    int offset_fd = open(offset_fn, O_RDWR | O_CREAT, 0664);
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

                    int globfd = open(glob_data.gl_pathv[i - 1], O_RDONLY);
                    FASSERT(globfd, "found file open")

                    logtail(globfd, offset_data.offset, search_stat.st_size);

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

        offset_data.offset = logtail(input_fd, offset_data.offset, input_stat.st_size);

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
