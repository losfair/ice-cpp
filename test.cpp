#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <time.h>
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
    server.route_sync("/info", [](ice::Request req) {
        auto headers = req.get_headers();
        auto cookies = req.get_cookies();

        std::cout << "Headers: " << std::endl;
        for(auto& p : headers) {
            if(p.second) std::cout << p.first << ": " << p.second << std::endl;
        }

        std::cout << "Cookies: " << std::endl;
        for(auto& p : cookies) {
            if(p.second) std::cout << p.first << ": " << p.second << std::endl;
        }

        return req.create_response().set_body("OK");
    });

    std::vector<std::string> session_route_flags;
    session_route_flags.push_back(std::string("init_session"));

    server.route_sync("/session", [](ice::Request req) {
        auto session_items = req.get_session_items();

        std::cout << "Session items: " << std::endl;
        for(auto& p : session_items) {
            std::cout << p.first << ": " << p.second << std::endl;
        }

        std::stringstream ss;
        ss << clock();
        
        std::string t;
        ss >> t;
        req.set_session_item("clock", t.c_str());

        return req.create_response().set_body("OK");
    }, session_route_flags);

    server.run("127.0.0.1:4121");
    return 0;
}
