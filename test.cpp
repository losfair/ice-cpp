#include "ice.h"

int main() {
    ice::Server server(NULL);
    server.run("127.0.0.1:4121");
    return 0;
}

