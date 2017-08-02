#include "ice.h"

int main() {
    ice::Server server(NULL);
    server.disable_request_logging();

    server.add_endpoint("/hello_world", [](ice::Request req) {
        auto resp = req.create_response();
        resp.set_body("Hello world!");
        resp.send();
    });
    server.run("127.0.0.1:4121");
    return 0;
}
