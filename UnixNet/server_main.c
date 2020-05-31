#include "global.h"
#include "echo/echo_server.h"
#include "mul_process/mul_server.h"
int main() {
    return multi_process_server_run();
}