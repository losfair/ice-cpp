#include "ice.h"

int main() {
    ice::Server server(NULL);
    server.disable_request_logging();

    server.route_sync("/hello_world", [](ice::Request req) {
        auto resp = req.create_response();
        resp.set_body("Hello world!");
        return resp;
    });
    server.route_async("/hello_world_async", [](ice::Request req) {
        auto resp = req.create_response();
        resp.set_body("Hello world! (Async)");
        resp.send();
    });
    server.route_threaded("/hello_world_threaded", [](ice::Request req) {
        auto resp = req.create_response();
        resp.set_body("Hello world! (Threaded)");
        return resp;
    });
    server.run("127.0.0.1:4121");
    return 0;
}
