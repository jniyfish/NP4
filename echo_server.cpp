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

class Session : public std::enable_shared_from_this<Session>
{
	private:
		ip::tcp::socket socket_;
		ip::tcp::resolver resolver;
		array<char, 4096> bytes_;
		http_header_info header_info;
		std::vector<char> in_buf_;
		std::vector<char> out_buf_;
		std::string remote_host_;
		std::string remote_port_;
		ip::tcp::socket out_socket_;

	public:
		Session(io_service &ioservice): 
		socket_(ioservice), 
		resolver(ioservice), 
		in_buf_(4096) ,
		out_buf_(4096) ,
		out_socket_(ioservice){}

		ip::tcp::socket &socket(){
			return socket_;
		}
		void start(){
			auto h =  boost::bind(&Session::read_handler, this, _1, _2);
			socket_.async_read_some(buffer(in_buf_), h );
		}
	
		void read_handler(const boost::system::error_code &ec, size_t bytes_transferred){
			if (!ec){
				if ( in_buf_[0] == 0x04){
					cout << "SOCK4" << endl;
					if(in_buf_[1] == 0x01){
						cout << "connect request" << endl;
						remote_host_ = boost::asio::ip::address_v4(ntohl(*((uint32_t*)&in_buf_[4]))).to_string();
						remote_port_ = std::to_string(ntohs(*((uint16_t*)&in_buf_[2])));
						cout << remote_host_ <<":"<<remote_port_ <<endl;
						do_resolve();
						
					}
				}
			}
		}
		void do_resolve(){
			//auto self(shared_from_this());
			ip::tcp::resolver::query query(remote_host_, remote_port_);
			resolver.async_resolve(query,
			[this](const boost::system::error_code& ec, ip::tcp::resolver::iterator it)
			{
				if (!ec)
				{
					cout << "a" << endl;
					do_connect(it);
				}
			});
		}
		void do_connect(ip::tcp::resolver::iterator& it){
			out_socket_.async_connect(*it, 
			[this](const boost::system::error_code& ec)
			{
				if (!ec)
				{
					cout << "b" <<endl;
					cout << "Reply" << endl;
						in_buf_[0] = 0x00; in_buf_[1] = 0x5A;
						in_buf_[2] = 0x00; in_buf_[3] = 0x00; 
						in_buf_[4] = 0x00; in_buf_[5] = 0x00; 
						in_buf_[6] = 0x00; in_buf_[7] = 0x00; 
						boost::asio::async_write(socket_, boost::asio::buffer(in_buf_, 8), // Always 10-byte according to RFC1928
						[this](boost::system::error_code ec, std::size_t length)
						{
							if (!ec)
							{}
						});
					do_read(3);
				}
			});
		}
		void do_read(int direction){
			size_t length = 4096;
			// We must divide reads by direction to not permit second read call on the same socket.
			if (direction & 0x1)
				socket_.async_receive(boost::asio::buffer(in_buf_),
					[this](boost::system::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							std::ostringstream what; what << "--> " << std::to_string(length) << " bytes";

							do_write(1, length);
						}

					});

			if (direction & 0x2)
				out_socket_.async_receive(boost::asio::buffer(out_buf_),
					[this](boost::system::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							std::ostringstream what; what << "<-- " << std::to_string(length) << " bytes";

							do_write(2, length);
						} 
					});
		}
		void do_write(int direction, std::size_t Length)
			{		
				switch (direction)
				{
				case 1:
					boost::asio::async_write(out_socket_, boost::asio::buffer(in_buf_, Length),
						[this, direction](boost::system::error_code ec, std::size_t length)
						{
							if (!ec)
								do_read(direction);

						});
					break;
				case 2:
					boost::asio::async_write(socket_, boost::asio::buffer(out_buf_, Length),
						[this, direction](boost::system::error_code ec, std::size_t length)
						{
							if (!ec)
								do_read(direction);

						});
					break;
				}
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