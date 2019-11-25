#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
// splice 在两个文件描述符中移动数据, 0 拷贝操作
#define PORT 9999
#define file_name "test.txt"
#define ip "127.0.0.1"
#define BUFSIZE 1024
void error_handling(const std::string &s) {
    perror(s.data());
    exit(-1);
}
int main() {
    int pipe_out[2], pipe_file[2], ret, file_fd;
    file_fd = open(file_name, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    assert(file_fd > 0);
    ret = pipe(pipe_out);
    assert(ret != -1);
    ret = pipe(pipe_file);
    assert(ret != -1);
    // 把标准输入, 定向到管道输入
    ret = splice(STDIN_FILENO, NULL, pipe_out[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret != -1);
    // 把 pipe_out[0] 复制到 pipe_file[1] 的输入
    ret = tee(pipe_out[0], pipe_file[1], 32768, SPLICE_F_NONBLOCK);
    assert(ret != -1);
    // 把pipe_out[0], 重定向到标准输出
    ret = splice(pipe_out[0], NULL, STDOUT_FILENO, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
    assert(ret != -1);
    ret = splice(pipe_file[0], NULL, file_fd, NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
    assert(ret != -1);
    close(file_fd);
    close(pipe_file[0]);
    close(pipe_file[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    return 0;
}