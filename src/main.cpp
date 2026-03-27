// Simple HTTP server using POSIX sockets - no external dependencies
#include <iostream>
#include <string>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static const int PORT = 8080;
static time_t start_time = std::time(nullptr);

static std::string make_response(const std::string& content_type,
                                 const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: " + content_type + "\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" + body;
}

static std::string handle_request(const char* raw) {
    std::string req(raw);

    if (req.find("GET /health") != std::string::npos) {
        long uptime = static_cast<long>(std::time(nullptr) - start_time);
        std::string body = R"({"status":"ok","uptime":)" +
                           std::to_string(uptime) + "}";
        return make_response("application/json", body);
    }

    std::string html = R"(<!DOCTYPE html>
<html lang="ja">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hello Server</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: "Helvetica Neue", Arial, sans-serif;
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    }
    .card {
      background: #fff; padding: 3rem 4rem; border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3); text-align: center;
      max-width: 480px;
    }
    h1 { color: #ee0000; font-size: 1.8rem; margin-bottom: 1rem; }
    p  { color: #555; line-height: 1.6; margin-bottom: 0.5rem; }
    a  { color: #667eea; text-decoration: none; font-weight: bold; }
    a:hover { text-decoration: underline; }
    .badge {
      display: inline-block; margin-top: 1.5rem; padding: 0.4rem 1rem;
      background: #ee0000; color: #fff; border-radius: 20px; font-size: 0.85rem;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Hello from C++ on OpenShift!</h1>
    <p>Simple CI/CD Demo Application</p>
    <p><a href="/health">/health</a> エンドポイントで状態を確認</p>
    <span class="badge">OpenShift + Tekton</span>
  </div>
</body>
</html>)";

    return make_response("text/html; charset=utf-8", html);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket() failed" << std::endl;
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed on port " << PORT << std::endl;
        return 1;
    }
    if (listen(server_fd, 10) < 0) {
        std::cerr << "listen() failed" << std::endl;
        return 1;
    }

    std::cout << "Listening on 0.0.0.0:" << PORT << std::endl;

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;

        char buf[4096]{};
        ssize_t n = read(client, buf, sizeof(buf) - 1);
        if (n > 0) {
            std::string resp = handle_request(buf);
            write(client, resp.c_str(), resp.size());
        }
        close(client);
    }

    close(server_fd);
    return 0;
}
