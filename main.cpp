#include "server_ws.hpp"
#include "server_http.hpp"

#include "process.hpp"

#include <boost/filesystem.hpp>

#include <fstream>
#include <unordered_map>

using namespace std;

const string ip_address="localhost";
const boost::filesystem::path screenshot_directory(".");
const string screenshot_filename="screenshot_resized.png";
//ImageMagick commands:
#if defined(__APPLE__)
const string screenshot_command("screencapture -x screenshot.png && convert screenshot.png -resize 50% "+screenshot_filename);
#else
const string screenshot_command("import -window root -resize 50% "+screenshot_filename);
#endif
const size_t delay=100; //delay in milliseconds

typedef SimpleWeb::SocketServer<SimpleWeb::WS> WsServer;
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

const string html="<html>\n"
"<head>\n"
"<meta http-equiv='Content-Type' content='text/html; charset=utf-8' />\n"
"<title>Desktop Stream</title>\n"
"</head>\n"
"<body>\n"
"<div id='status'></div>\n"
"<img id=\"image\"/>\n"
"<script>\n"
"var ws;\n"
"window.onload=function(){\n"
"  ws=new WebSocket('ws://"+ip_address+":8081/desktop');\n"
"  ws.onmessage=function(evt){\n"
"    var blob = new Blob([evt.data], {type: 'application/octet-binary'});\n"
"    console.log(evt.data.size);\n"
"    var image = document.getElementById('image');\n"
"\n"
"    var reader = new FileReader();\n"
"    reader.onload = function(e) {\n"
"      image.src = e.target.result;\n"
"    };\n"
"    reader.readAsDataURL(blob);\n"
"  };\n"
"  ws.onclose=function(evt){\n"
"    document.getElementById('status').innerHTML = '<b>Connection closed, reload page to reconnect.</b>';\n"
"  };\n"
"  ws.onerror=function(evt){\n"
"    document.getElementById('status').innerHTML = '<b>Connection error, reload page to reconnect.</b>';\n"
"  };\n"
"};\n"
"</script>\n"
"</body>\n"
"</html>\n";

int main() {
    //WebSocket (WS)-server at port 8080 using 4 threads
    HttpServer http_server(8080, 4);
    WsServer ws_server(8081, 4);
    
    auto& desktop_endpoint=ws_server.endpoint["^/desktop/?$"];
    
    vector<char> image_buffer;
    mutex image_buffer_mutex;
    
    unordered_map<WsServer::Connection*, bool> connection_retrieving;
    mutex connection_retrieving_mutex;
    
    if(!boost::filesystem::exists(screenshot_directory)) {
        cerr << screenshot_directory << " does not exist, please create it or change the screenshot directory path" << endl;
        return 1;
    }
    
    boost::filesystem::path screenshot_path=screenshot_directory/screenshot_filename;
    if(!boost::filesystem::exists(screenshot_path)) {
        Process process(screenshot_command, screenshot_directory.string(), nullptr, [](const char *bytes, size_t n) {
            cerr << string(bytes, n);
        });
        if(process.get_exit_status()!=0)
            return 1;
    }
    
    ifstream ifs;
    ifs.open(screenshot_path.string(), ifstream::in | ios::binary);
    ifs.seekg(0, ios::end);
    size_t length=ifs.tellg();
    ifs.seekg(0, ios::beg);
    
    image_buffer_mutex.lock();
    image_buffer.resize(length);
    ifs.read(&image_buffer[0], length);
    image_buffer_mutex.unlock();
    
    ifs.close();
    
    thread update_image_thread([&](){
        while(true) {
            Process process(screenshot_command, screenshot_directory.string(), nullptr, [](const char *bytes, size_t n) {
                cerr << string(bytes, n);
            });
            if(process.get_exit_status()==0) {
                ifs.open(screenshot_path.string(), ifstream::in | ios::binary);
                ifs.seekg(0, ios::end);
                size_t length=ifs.tellg();
                ifs.seekg(0, ios::beg);
                
                image_buffer_mutex.lock();
                image_buffer.resize(length);
                ifs.read(&image_buffer[0], length);
                image_buffer_mutex.unlock();
                
                ifs.close();
                
                for(auto a_connection: desktop_endpoint.get_connections()) {
                    connection_retrieving_mutex.lock();
                    //Skip connection if it is not yet inserted into the map
                    auto it=connection_retrieving.find(a_connection.get());
                    if(it==connection_retrieving.end()) {
                        connection_retrieving_mutex.unlock();
                        continue;
                    }
                    //skip connection if it is already retrieving data
                    auto skip_connection=connection_retrieving[a_connection.get()];
                    connection_retrieving_mutex.unlock();
                    if(!skip_connection) {
                        auto send_stream=make_shared<WsServer::SendStream>();
                        
                        image_buffer_mutex.lock();
                        send_stream->write(image_buffer.data(), image_buffer.size());
                        image_buffer_mutex.unlock();
                        
                        connection_retrieving_mutex.lock();
                        connection_retrieving[a_connection.get()]=true;
                        connection_retrieving_mutex.unlock();
                        ws_server.send(a_connection, send_stream, [&connection_retrieving, &connection_retrieving_mutex, a_connection](const boost::system::error_code &ec) {
                            connection_retrieving_mutex.lock();
                            connection_retrieving[a_connection.get()]=false;
                            connection_retrieving_mutex.unlock();
                        }, 130);
                    }
                }
            }
            this_thread::sleep_for(chrono::milliseconds(delay));
        }
    });
    
    desktop_endpoint.onopen=[&](shared_ptr<WsServer::Connection> connection) {
        auto send_stream=make_shared<WsServer::SendStream>();
        
        image_buffer_mutex.lock();
        send_stream->write(image_buffer.data(), image_buffer.size());
        image_buffer_mutex.unlock();
        
        connection_retrieving_mutex.lock();
        connection_retrieving[connection.get()]=true;
        connection_retrieving_mutex.unlock();
        ws_server.send(connection, send_stream, [&connection_retrieving, &connection_retrieving_mutex, connection](const boost::system::error_code &ec) {
            connection_retrieving_mutex.lock();
            connection_retrieving[connection.get()]=false;
            connection_retrieving_mutex.unlock();
        }, 130);
    };
    
    desktop_endpoint.onclose=[&](shared_ptr<WsServer::Connection> connection, int status, const string& reason) {
        connection_retrieving_mutex.lock();
        auto it=connection_retrieving.find(connection.get());
        if(it!=connection_retrieving.end())
            connection_retrieving.erase(it);
        connection_retrieving_mutex.unlock();
    };
    
    desktop_endpoint.onerror=[&](shared_ptr<WsServer::Connection> connection, const boost::system::error_code& ec) {
        connection_retrieving_mutex.lock();
        auto it=connection_retrieving.find(connection.get());
        if(it!=connection_retrieving.end())
            connection_retrieving.erase(it);
        connection_retrieving_mutex.unlock();
        
        cout << "Server: Error in connection " << (size_t)connection.get() << ". " << 
                "Error: " << ec << ", error message: " << ec.message() << endl;
    };
    
    http_server.default_resource["GET"]=[](HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
        response << "HTTP/1.1 200 OK\r\nContent-Length: " << html.size() << "\r\n\r\n" << html;
    };
    
    thread server_thread([&http_server](){
        //Start server
        http_server.start();
    });
    
    ws_server.start();
    server_thread.join();
    update_image_thread.join();
    
    return 0;
}
