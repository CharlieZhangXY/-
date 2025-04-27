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
#define MATRIX_SIZE 10  // 可調整大小來改變負載

void clear_page_cache() {
    system("sync; echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null");
}

void print_time(struct timeval start, struct timeval end) {
    double ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                (end.tv_usec - start.tv_usec) / 1000.0;
    printf("\t耗時：%.3f 毫秒\n", ms);
}

void matrix_multiply() {
    int A[MATRIX_SIZE][MATRIX_SIZE], B[MATRIX_SIZE][MATRIX_SIZE], C[MATRIX_SIZE][MATRIX_SIZE];
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            A[i][j] = rand() % 10;
            B[i][j] = rand() % 10;
            C[i][j] = 0;
        }
    }
    for (int i = 0; i < MATRIX_SIZE; i++) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            for (int k = 0; k < MATRIX_SIZE; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

void blocking_io_with_matrix(const char* in_file, const char* out_file) {
    printf("\n🔸 Blocking I/O + 矩陣運算\n");
    int in_fd = open(in_file, O_RDONLY);
    int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (in_fd < 0 || out_fd < 0) { perror("open"); exit(1); }

    char buffer[BUF_SIZE];
    ssize_t n;
    struct timeval start, end;
    gettimeofday(&start, NULL);

    while ((n = read(in_fd, buffer, BUF_SIZE)) > 0) {
        matrix_multiply();  // 做計算工作
        write(out_fd, buffer, n);
    }

    gettimeofday(&end, NULL);
    print_time(start, end);
    close(in_fd); close(out_fd);
}

void aio_with_matrix(const char* in_file, const char* out_file, int aio_parts) {
    printf("\n🔸 AIO + 矩陣運算（Polling 模式，%d 塊）\n", aio_parts);

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

    // Step 1: 發出所有 aio_read
    for (int i = 0; i < aio_parts; i++) {
        off_t this_chunk = chunk_size + (i == aio_parts - 1 ? remainder : 0);
        buffers[i] = malloc(this_chunk);
        memset(&read_cbs[i], 0, sizeof(struct aiocb));
        read_cbs[i].aio_fildes = in_fd;
        read_cbs[i].aio_buf = buffers[i];
        read_cbs[i].aio_nbytes = this_chunk;
        read_cbs[i].aio_offset = i * chunk_size;
        aio_read(&read_cbs[i]);
    }

    // Step 2: 主執行緒做計算工作同時輪詢是否完成
    for (int i = 0; i < aio_parts; i++) {
        matrix_multiply();  // 做計算工作
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
        printf("使用方式：%s <AIO併發數量>\n", argv[0]);
        return 1;
    }
    int aio_parts = atoi(argv[1]);

    if (access("input.txt", F_OK) != 0) {
        printf("請先建立 input.txt，例如：dd if=/dev/urandom of=input.txt bs=1M count=100\n");
        return 1;
    }

    clear_page_cache();
    blocking_io_with_matrix("input.txt", "output_block_matrix.txt");
    clear_page_cache();
    aio_with_matrix("input.txt", "output_aio_matrix.txt", aio_parts);

    return 0;
}
