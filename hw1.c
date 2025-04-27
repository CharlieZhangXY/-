#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <aio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>

#define BUF_SIZE 4096

void clear_page_cache() {
    system("sync; echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null");
}

void print_time(struct timeval start, struct timeval end) {
    double ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                (end.tv_usec - start.tv_usec) / 1000.0;
    printf("\t耗時：%.3f 毫秒\n", ms);
}

void blocking_io(const char* in_file, const char* out_file) {
    printf("\n🔸 Blocking I/O\n");
    int in_fd = open(in_file, O_RDONLY);
    int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (in_fd < 0 || out_fd < 0) { perror("open"); exit(1); }

    char buffer[BUF_SIZE];
    ssize_t n;
    struct timeval start, end;
    gettimeofday(&start, NULL);

    while ((n = read(in_fd, buffer, BUF_SIZE)) > 0) {
        write(out_fd, buffer, n);
    }

    gettimeofday(&end, NULL);
    print_time(start, end);
    close(in_fd); close(out_fd);
}

void nonblocking_io(const char* in_file, const char* out_file) {
    printf("\n🔸 Non-Blocking I/O\n");
    int in_fd = open(in_file, O_RDONLY | O_NONBLOCK);
    int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (in_fd < 0 || out_fd < 0) { perror("open"); exit(1); }

    char buffer[BUF_SIZE];
    ssize_t n;
    struct timeval start, end;
    gettimeofday(&start, NULL);

    while (1) {
        n = read(in_fd, buffer, BUF_SIZE);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("read"); break;
        } else if (n == 0) break;
        write(out_fd, buffer, n);
    }

    gettimeofday(&end, NULL);
    print_time(start, end);
    close(in_fd); close(out_fd);
}

void aio_split_blocks(const char* in_file, const char* out_file, int aio_parts) {
    printf("\n🔸 Asynchronous I/O（Polling 模式，等分 %d 塊）\n", aio_parts);
    int in_fd = open(in_file, O_RDONLY);
    int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (in_fd < 0 || out_fd < 0) { perror("open"); exit(1); }

    struct stat st;
    fstat(in_fd, &st);
    off_t file_size = st.st_size;

    off_t chunk_size = file_size / aio_parts;
    off_t remainder = file_size % aio_parts;

    struct aiocb read_cbs[aio_parts];
    struct aiocb write_cbs[aio_parts];
    char* buffers[aio_parts];

    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (int i = 0; i < aio_parts; i++) {
        off_t current_size = chunk_size + (i == aio_parts - 1 ? remainder : 0);
        buffers[i] = malloc(current_size);
        memset(&read_cbs[i], 0, sizeof(struct aiocb));
        read_cbs[i].aio_fildes = in_fd;
        read_cbs[i].aio_buf = buffers[i];
        read_cbs[i].aio_nbytes = current_size;
        read_cbs[i].aio_offset = i * chunk_size;
        aio_read(&read_cbs[i]);
    }

    for (int i = 0; i < aio_parts; i++) {
        while (aio_error(&read_cbs[i]) == EINPROGRESS);
        ssize_t r = aio_return(&read_cbs[i]);

        memset(&write_cbs[i], 0, sizeof(struct aiocb));
        write_cbs[i].aio_fildes = out_fd;
        write_cbs[i].aio_buf = buffers[i];
        write_cbs[i].aio_nbytes = r;
        write_cbs[i].aio_offset = read_cbs[i].aio_offset;
        aio_write(&write_cbs[i]);
    }

    for (int i = 0; i < aio_parts; i++) {
        while (aio_error(&write_cbs[i]) == EINPROGRESS);
        aio_return(&write_cbs[i]);
        free(buffers[i]);
    }

    gettimeofday(&end, NULL);
    print_time(start, end);
    close(in_fd); close(out_fd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("使用方式：%s <AIO併發區塊數>\n", argv[0]);
        printf("範例：    %s 4\n", argv[0]);
        return 1;
    }

    int aio_parts = atoi(argv[1]);

    if (access("input.txt", F_OK) != 0) {
        printf("❌ 請先建立 input.txt，例如：\n");
        printf("dd if=/dev/urandom of=input.txt bs=1M count=500\n");
        return 1;
    }

    printf("📂 開始測試，AIO 併發區塊數：%d\n", aio_parts);

    clear_page_cache();
    blocking_io("input.txt", "output_block.txt");

    clear_page_cache();
    nonblocking_io("input.txt", "output_nonblock.txt");
    
    clear_page_cache();
    aio_split_blocks("input.txt", "output_aio.txt", aio_parts);

    return 0;
}
