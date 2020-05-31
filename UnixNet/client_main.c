#include "global.h"
#include "echo/echo_client.h"
#include "mul_process/mul_client.h"
int main() {
    return multi_process_client_run();
}