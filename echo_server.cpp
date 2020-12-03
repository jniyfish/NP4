#include <iostream>
#include <stdlib.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <cstdlib>
#include <cstring>
#include <string>
#include <array>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;
using namespace boost::asio;


typedef struct http_header_info
{
	string method;
	string path;
	string protocol;
	string host;
	string uri;
	string query;

}http_header_info;

class Session{
	private:
		ip::tcp::socket socket_;
		array<char, 4096> bytes_;
		http_header_info header_info;

	public:
		Session(io_service &ioservice): socket_(ioservice){}

		ip::tcp::socket &socket(){
			return socket_;
		}
		void start(){
			auto h =  boost::bind(&Session::read_handler, this, _1, _2);
			socket_.async_read_some(buffer(bytes_), h );
		}
		void SetHttpEnvVar(){
				string server_addr(header_info.host, 0, header_info.host.find(":"));
				string server_port(header_info.host.begin() + header_info.host.find(":") + 1, header_info.host.end());
				setenv("REQUEST_METHOD", header_info.method.c_str(), 1);
				setenv("REQUEST_URI", header_info.uri.c_str(), 1);
				setenv("QUERY_STRING", header_info.query.c_str(), 1);
				setenv("SERVER_PROTOCOL", header_info.protocol.c_str(), 1);
				setenv("HTTP_HOST", header_info.host.c_str(), 1);
				setenv("SERVER_ADDR", server_addr.c_str(), 1);
				setenv("SERVER_PORT", server_port.c_str(), 1);
				setenv("REMOTE_ADDR", socket_.remote_endpoint().address().to_string().c_str(), 1);
				setenv("REMOTE_PORT", to_string(socket_.remote_endpoint().port()).c_str(), 1);
		}
		void parse_header(){
			char *parseFirst=NULL;
			char *parseSec=NULL;
			//GET /panel.cgi HTTP/1.1
			//Host: nplinux1.cs.nctu.edu.tw:7001

			parseFirst = strtok(bytes_.data(), "\r\n");
			parseSec =  strtok(NULL, "\r\n");
			parseFirst = strtok(parseFirst, " \r\n");

			int q=0;
			while (q!=3){
				header_info.method = (q==0) ? parseFirst : header_info.method  ;
				header_info.path = (q==1) ? parseFirst : header_info.path ;
				header_info.uri = (q==1) ? parseFirst : header_info.uri ;
				header_info.protocol  = (q==2) ?  parseFirst : header_info.protocol;
				parseFirst = strtok(NULL, " \r\n");
				q++;
			}	
			parseSec = strtok(parseSec, " \r\n");
			parseSec =  strtok(NULL, "\r\n");
			header_info.host = parseSec;

			size_t loc = header_info.path.find("?") + 1;  //find ? position
			if (loc-1 != string::npos){
				header_info.query.assign(header_info.path, loc, header_info.path.size() - loc);
				header_info.path.erase(header_info.path.find("?"), string::npos);   //remove all after ?
			}
			else{
				header_info.query.assign("");
			}
		}
		void runCgi(){
			pid_t PID;
			int fd = socket_.lowest_layer().native_handle();
			string complete_path = ".";
			signal(SIGCHLD, Session::childHandler);
			PID = fork();
			if(PID == 0){
				dup2(fd, STDOUT_FILENO);
				close(fd);
				complete_path += header_info.path;
			/*	if (header_info.path.compare("/panel.cgi") == 0){
					if (execlp(complete_path.c_str(), "panel.cgi", NULL) == -1){
						cerr << "Exec error!" << endl;
						exit(2);
					}   
				}
				else if (header_info.path.compare(0, 12, "/console.cgi") == 0){
					if (execlp(complete_path.c_str(), "console.cgi", NULL) == -1){
						cerr << "Exec error!" << endl;
						exit(2);
					}  
				}
				else if((header_info.path.compare(0, 13, "/printenv.cgi") == 0)){
					if (execlp(complete_path.c_str(), "printenv.cgi", NULL) == -1){
						cerr << "Exec error!" << endl;
						exit(2);
					}  
				}
				else if((header_info.path.compare(0, 10, "/hello.cgi") == 0)){
					if (execlp(complete_path.c_str(), "hello.cgi", NULL) == -1){
						cerr << "Exec error!" << endl;
						exit(2);
					}  
				}
				else if((header_info.path.compare(0, 12, "/welcome.cgi") == 0)){
					if (execlp(complete_path.c_str(), "welcome.cgi", NULL) == -1){
						cerr << "Exec error!" << endl;
						exit(2);
					}  
				}*/
				if (header_info.path.find(".cgi") != string::npos){
                    header_info.path.erase(header_info.path.begin());
                    if (execlp(complete_path.c_str(), header_info.path.c_str(), NULL) == -1){
                        cerr << "Exec error!" << endl;
                        exit(2);
                    }
                }
			}
			else if(PID==-1){
				cerr << "Fork error!" << endl;
				exit(EXIT_FAILURE);
			}
			else
				socket_.close();
		}
		void read_handler(const boost::system::error_code &ec, size_t bytes_transferred){
			if (!ec){
				string reply("HTTP/1.1 200 OK\r\n");
				boost::system::error_code ec;
				socket_.write_some(buffer(reply), ec);

				Session::parse_header();
				Session::SetHttpEnvVar();
				Session::runCgi();
			}
		}
		static void childHandler(int signo){
			int status;
			while (waitpid(-1, &status, WNOHANG) > 0)	{;}
		}

};

class server{
	private:
		io_service &ioservice_;
		ip::tcp::acceptor acceptor_;

	public:
		server(io_service &ioservice, int port)
			:   ioservice_(ioservice), 
			acceptor_(ioservice, ip::tcp::endpoint(ip::tcp::v4(), port)) //bind port
		{
			do_accept();
		}
		void do_accept(){
			Session* new_session = new Session(ioservice_);
			auto h = boost::bind(&server::handler, this, _1, new_session);
			acceptor_.async_accept(new_session->socket(),h);
		}
		void handler(const boost::system::error_code &ec, Session* new_session){
			if (!ec){
				new_session->start();
				do_accept();
			}
			else    
				delete new_session;
		}
};
int main(int argc, char* argv[]){
	try{
		if (argc != 2){
			std::cerr << "Usage: async_tcp_echo_server <port>\n";
			return EXIT_FAILURE;
		}
		io_service ioservice;
		server s(ioservice, atoi(argv[1]));
		ioservice.run();
	}
	catch(std::exception& e){
		std::cerr << "Exception: " << e.what() << "\n";
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}