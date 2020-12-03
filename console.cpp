#include <iostream>
#include <cstdlib>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <cstring>
#include <memory>
#include <array>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/replace.hpp>

using namespace std;
using namespace boost::asio;


io_service ioservice;

class Console
{
    public:
        string host_name;
        string port_num;
        string file_name;
        fstream file;
        ip::tcp::resolver resolv{ioservice};
        ip::tcp::socket tcp_socket{ioservice};
        array<char, 4096> bytes;

        Console() : host_name(""), port_num("0"), file_name("none") {}
};

vector<shared_ptr<Console> > sessionList;

void output(string session, string content);
void output_command(string session, string content);
void set_Web_style();
void getinfo_from_panel();
void resolve_handler(const boost::system::error_code &ec, ip::tcp::resolver::iterator it, vector<shared_ptr<Console> >::iterator itc);
void connect_handler(const boost::system::error_code &ec, vector<shared_ptr<Console> >::iterator itc);
void read_handler(const boost::system::error_code &ec, size_t bytes_transferred, vector<shared_ptr<Console> >::iterator itc);

int main(int argc, char **argv){
    getinfo_from_panel();
    set_Web_style();
    for (auto it=sessionList.begin() ; it!=sessionList.end() ; ++it){
        ip::tcp::resolver::query query((*it)->host_name, (*it)->port_num);
        auto h =  boost::bind(&resolve_handler, _1, _2, it);
        (*it)->resolv.async_resolve(query, h);
    }
    ioservice.run();
    return 0;
}


void resolve_handler(const boost::system::error_code &ec, ip::tcp::resolver::iterator it, vector<shared_ptr<Console> >::iterator itc)
{
    auto h = boost::bind(&connect_handler, _1, itc);
    if (!ec)    
        (*itc)->tcp_socket.async_connect(*it, h );
}

void connect_handler(const boost::system::error_code &ec, vector<shared_ptr<Console> >::iterator itc)
{
    auto h =  boost::bind(&read_handler, _1, _2, itc);
    if (!ec)    
        (*itc)->tcp_socket.async_read_some(buffer((*itc)->bytes), h );
}

void read_handler(const boost::system::error_code &ec, size_t bytes_transferred, vector<shared_ptr<Console> >::iterator itc)
{
    if (!ec){
        string session = "s";
        string recei((*itc)->bytes.data(), bytes_transferred);
        int index = distance(sessionList.begin(), itc);
        session += to_string(index);

        output(session, recei);
        auto h = boost::bind(&read_handler, _1, _2, itc);
        if (recei.find("% ") == string::npos){
            (*itc)->tcp_socket.async_read_some(buffer((*itc)->bytes), h);  
        }
        else{
            string command;
            getline((*itc)->file, command);       //get a line of test file to string 
            command += '\n';
            output_command(session, command);

			boost::system::error_code ec;
			(*itc)->tcp_socket.write_some(buffer(command), ec);
            (*itc)->tcp_socket.async_read_some(buffer((*itc)->bytes), h);
        }
    }
}

void getinfo_from_panel()
{   
    char* q_str = getenv("QUERY_STRING");
    char* pch;

    pch = strtok(q_str, "&");
    while (pch != NULL){
        char* tmp = strchr(pch, '=') + 1;
        if(pch[0]=='h'){
            sessionList.push_back(shared_ptr<Console>(new Console()));
            if (strlen(tmp) != 0)       
                sessionList.back()->host_name = tmp; 
            else     
                sessionList.pop_back();
        }
        else if(pch[0]=='p'){
            if (strlen(tmp) != 0)       
                sessionList.back()->port_num = tmp;
        }
        else if(pch[0]=='f'){
            if (strlen(tmp) != 0){
                string file_path = "./test_case/";
                sessionList.back()->file_name = tmp;
                file_path += tmp;
                sessionList.back()->file.open(file_path, fstream::in);
            }
        }
        pch = strtok(NULL, "&");
    }
}

void set_Web_style()
{
    cout << "Content-type: text/html\r\n\r\n";

    cout << "<!DOCTYPE html>" << endl
         << "<html lang='en'>" << endl
         << "<head>" << endl
         <<     "<meta charset='UTF-8' />" << endl
         <<     "<title>NP Project 3 Console</title>" << endl
         <<     "<link" << endl
         <<     "rel='stylesheet'" << endl
         <<     "href='https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css'" << endl
         <<     "integrity='sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2'" << endl
         <<     " crossorigin='anonymous'" << endl
         <<     "/>" << endl
         <<     "<link" << endl
         <<     "href='https://fonts.googleapis.com/css?family=Source+Code+Pro'" << endl
         <<     "rel='stylesheet'" << endl
         <<     "/>" << endl
         <<     "<link" << endl
         <<     "rel='icon'" << endl
         <<     "type='image/png'" << endl
         <<     "href='https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png'" << endl
         <<     "/>" << endl
         <<     "<style>" << endl
         <<     "* {" << endl
         <<         "font-family: 'Source Code Pro', monospace;" << endl
         <<         "font-size: 1rem !important;" << endl
         <<     "}" << endl
         <<     "body {" << endl
         <<         "background-color: #212529;" << endl
         <<     "}" << endl
         <<     "pre {" << endl
         <<         "color: #cccccc;" << endl
         <<     "}" << endl
         <<     "b {" << endl
         <<         "color: #01b468;" << endl
         <<     "}" << endl
         <<     "</style>" << endl
         << "</head>" << endl
         << "<body>" << endl
         <<     "<table class='table table-dark table-bordered'>" << endl
         <<     "<thead>" << endl
         <<         "<tr>" << endl;
    for (auto it=sessionList.begin() ; it!=sessionList.end() ; ++it){
        string host = (*it)->host_name + ":" + (*it)->port_num;
        cout << "<th scope='col'>" << host << "</th>" << endl;
    }
    cout <<         "</tr>" << endl
         <<     "</thead>" << endl
         <<     "<tbody>" << endl
         <<         "<tr>" << endl;
    for (unsigned int i=0 ; i<sessionList.size() ; ++i){
        cout << "<td><pre id='s" << i << "' class='mb-0'></pre></td>" << endl;
    }
    cout <<         "</tr>" << endl
         <<     "</tbody>" << endl
         <<     "</table>" << endl
         << "</body>" << endl
         << "</html>" << endl;
}

void output(string session, string content)
{   
    boost::replace_all(content, "&", "&amp");
    boost::replace_all(content, "\r\n", "\n");
    boost::replace_all(content, "\n", "&NewLine;");
    boost::replace_all(content, "<", "&lt;");
    boost::replace_all(content, ">", "&gt;");
    boost::replace_all(content, "'", "&lsquo;");
    cout << "<script>document.getElementById('" + session + "').innerHTML += '" 
    + content + "';</script>" << endl;
}

void output_command(string session, string content)
{
    boost::replace_all(content, "\r\n", "\n");
    boost::replace_all(content, "\n", "&NewLine;");
    boost::replace_all(content, "<", "&lt;");
    boost::replace_all(content, ">", "&gt;");
    cout << "<script>document.getElementById('" + session + "').innerHTML += '<b>" 
    + content + "</b>';</script>" << endl;
}