#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <iostream>
#include <cstdlib>
#include <string>
#include <stdlib.h>
#include <vector>
#include <cstring>
#include <memory>
#include <array>
#include <fstream>


using namespace std;
using namespace boost::asio;
io_service ioservice;


typedef struct http_header_info
{
    string method;
    string path;
    string protocol;
    string host;
    string uri;
    string query;

}http_header_info;

class Console
{
    public:
        string host_name;
        string port_num;
        string file_name;
        fstream file;
        ip::tcp::socket tcp_socket{ioservice};
        array<char, 4096> bytes;

        Console() : host_name(""), port_num("0"), file_name("none") {}
};

class Session{
	private:
        ip::tcp::resolver resolver;
		ip::tcp::socket socket_;
		array<char, 4096> bytes_;
		http_header_info header_info;
        vector<shared_ptr<Console> > sessionList;

	public:
		~Session() {}
		Session(): 
		socket_(ioservice),
		resolver(ioservice)
		{}

		ip::tcp::socket &socket(){
			return socket_;
		}
		void start(){
			auto h =  boost::bind(&Session::read_handler, this, _1, _2);
			socket_.async_read_some(buffer(bytes_), h );
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
				header_info.protocol  = (q==2) ?  parseFirst : header_info.protocol;
				parseFirst = strtok(NULL, " \r\n");
				q++;
			}	
			parseSec = strtok(parseSec, " \r\n");
			parseSec =  strtok(NULL, "\r\n");
			header_info.host = parseSec;
			header_info.uri = header_info.path;

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
			string reply("HTTP/1.1 200 OK\r\n");
			boost::system::error_code ec;
			socket_.write_some(buffer(reply), ec);
			string req = header_info.uri;
            if (req.find("panel.cgi") != string::npos){
				panel();
            }
			else if(req.find("console.cgi") != string::npos){
				console();
			}

		}
	
		void read_handler(const boost::system::error_code &ec, size_t bytes_transferred){
			if (!ec){
				string reply("HTTP/1.1 200 OK\r\n");
				boost::system::error_code ec;
				socket_.write_some(buffer(reply), ec);
				Session::parse_header();
				Session::runCgi();
			}
		}
		void console(){
			get_info_from_panel();
			set_Web_style();
			for (auto it=sessionList.begin() ; it!=sessionList.end() ; ++it){
            	ip::tcp::resolver::query query((*it)->host_name, (*it)->port_num);
				auto h = boost::bind(&Session::resolve_handler, this, _1, _2, it);
            	resolver.async_resolve(query, h );
       		}
		}
		void resolve_handler(const boost::system::error_code &ec, ip::tcp::resolver::iterator it, vector<shared_ptr<Console> >::iterator itc){
        	auto h = boost::bind(&Session::connect_handler, this, _1, itc);
    		if (!ec)    
        		(*itc)->tcp_socket.async_connect(*it, h );  
   	 	}
		void connect_handler(const boost::system::error_code &ec, vector<shared_ptr<Console> >::iterator itc){
    		auto h =  boost::bind(&Session::c_read_handler, this, _1, _2, itc);
    		if (!ec)    
        		(*itc)->tcp_socket.async_read_some(buffer((*itc)->bytes), h );
		}
		
		void c_read_handler(const boost::system::error_code &ec, size_t bytes_transferred, vector<shared_ptr<Console> >::iterator itc){
    		if (!ec){
        		string session = "s";
        		string recei((*itc)->bytes.data(), bytes_transferred);
        		int index = distance(sessionList.begin(), itc);
        		session += to_string(index);

        		output(session, recei);
        		auto h =  boost::bind(&Session::c_read_handler, this, _1, _2, itc);
        		if (recei.find("%") == string::npos){
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
		
		void get_info_from_panel(){
			char q_str[1024];
			strcpy(q_str, header_info.query.c_str());
			char* pch = strtok(q_str, "&");
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
		
		void set_Web_style(){
        	string message;
        	message = "Content-type: text/html\r\n\r\n" 
            "<!DOCTYPE html>\n" 
             "<html lang='en'>\n" 
             "<head>\n" 
                 "<meta charset='UTF-8' />\n" 
                 "<title>NP Project 3 Console</title>\n" 
                 "<link\n" 
                 "rel='stylesheet'\n" 
                 "href='https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css'\n" 
                 "integrity='sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2'\n" 
                 " crossorigin='anonymous'\n" 
                 "/>\n" 
                 "<link\n" 
                 "href='https://fonts.googleapis.com/css?family=Source+Code+Pro'\n" 
                 "rel='stylesheet'\n" 
                 "/>\n" 
                 "<link\n" 
                 "rel='icon'\n" 
                 "type='image/png'\n" 
                 "href='https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png'\n" 
                 "/>\n" 
                 "<style>\n" 
                 "* {\n" 
                     "font-family: 'Source Code Pro', monospace;\n" 
                     "font-size: 1rem !important;\n" 
                 "}\n" 
                 "body {\n" 
                     "background-color: #212529;\n" 
                 "}\n" 
                 "pre {\n" 
                     "color: #cccccc;\n" 
                 "}\n" 
                 "b {\n" 
                     "color: #01b468;\n" 
                 "}\n" 
                 "</style>\n" 
             "</head>\n" 
             "<body>\n" 
                 "<table class='table table-dark table-bordered'>\n" 
                 "<thead>\n" 
                     "<tr>\n";

			for (auto it=sessionList.begin() ; it!=sessionList.end() ; ++it){
            	string tmp = string("<th scope='col'>") + (*it)->host_name + ":" + (*it)->port_num + "</th>\n";
            	message += tmp;
        	}

        	string tmp = string("</tr>\n") +
            "</thead>\n" +
            "<tbody>\n" +
                "<tr>\n";

			message += tmp;

			for (unsigned int i=0 ; i<sessionList.size() ; ++i){
				string tmp1 = string("<td><pre id='s") + to_string(i) + "' class='mb-0'></pre></td>\n";
				message += tmp1;
			}

			string tmp2 = string("</tr>\n") +
					"</tbody>\n" +
					"</table>\n" +
				"</body>\n" +
				"</html>\n";
			message += tmp2;
			boost::system::error_code ec;
			socket_.write_some(buffer(message), ec);
    	}	
        void panel(){
            string host_menu;
        	for (int i=0 ; i<12 ; i++){
            	string tmp = string("<option value='nplinux") + to_string(i+1) + ".cs.nctu.edu.tw'>nplinux" + to_string(i+1) + "</option>\n";
            	host_menu += tmp;
        	}
            string test_menu;
            for(int i=1;i<=5;i++){
                string tmp = string("<option value='t") + to_string(i) + ".txt'>t" 
                + to_string(i) + ".txt</option>\n";
                test_menu +=tmp;
            }
            string html = string("Content-type: text/html\r\n\r\n")+
            "<!DOCTYPE html>\n" +
            "<html lang='en'>\n" +
            "<head>\n"+
            "<title>NP Project 3 Panel</title>\n" +
            "<link\n" +
            "rel='stylesheet'\n" +
            " href='https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css'\n"+
             "crossorigin='anonymous'\n" +
            "/>\n" +
            "<link\n" +
            "href='https://fonts.googleapis.com/css?family=Source+Code+Pro'\n" +
            "rel='stylesheet'\n" +
            "/>\n" +
            "<link\n" +
            "rel='icon'\n" +
            "type='image/png'\n" +
            "href='https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png'\n" +
            "/>\n" +
            "<style>\n" +
            "* {\n" +
                "font-family: 'Source Code Pro', monospace;\n" +
            "}\n" +
            "</style>\n" +
            "</head>\n" +
            "<body class='bg-secondary pt-5'>\n" +
            "<form action='console.cgi' method='GET'>\n" +
            "<table class='table mx-auto bg-light' style='width: inherit'>\n" +
            "<thead class='thead-dark'>\n" +
            "<tr>\n" +
            "<th scope='col'>#</th>\n" +
            "<th scope='col'>Host</th>\n" +
            "<th scope='col'>Port</th>\n" +
            "<th scope='col'>Input File</th>\n" +
            "</tr>\n" +
            "</thead>\n" +
            "<tbody>\n";

            for (int i=0 ; i<5 ; i++){
            string tmp;
            tmp = string("<tr>\n") +
                "<th scope='row' class='align-middle'>Session" + to_string(i+1) + "</th>\n" +
                "<td>\n" +
                "<div class='input-group'>\n" +
                    "<select name='h" + to_string(i) + "' class='custom-select'>\n" +
                    "<option></option>\n" + host_menu +
                    "</select>\n" +
                    "<div class='input-group-append'>\n" +
                    "<span class='input-group-text'>.cs.nctu.edu.tw</span>\n" +
                    "</div>\n" +
                "</div>\n" +
                "</td>\n" +
                "<td>\n" +
                "<input name='p" + to_string(i) + "' type='text' class='form-control' size='5' />\n" +
                "</td>\n" +
                "<td>\n" +
                "<select name='f" + to_string(i) + "' class='custom-select'>\n" +
                    "<option></option>\n" + test_menu +
                "\n" +
                "</select>\n" +
                "</td>\n" +
            "</tr>\n";

            html += tmp;
            }
            string tmp = string("<tr>\n") +
                        "<td colspan='3'></td>\n" +
                        "<td>\n" +
                        "<button type='submit' class='btn btn-info btn-block'>Run</button>\n" +
                        "</td>\n" +
                    "</tr>\n" +
                    "</tbody>\n" +
                "</table>\n" +
                "</form>\n" +
            "</body>\n" +
            "</html>\n";
        
            html += tmp;

            boost::system::error_code ec;
            socket_.write_some(buffer(html), ec);
        }
		void output(string session, string content){   
        	boost::replace_all(content, "&", "&amp");
        	boost::replace_all(content, "\r\n", "\n");
       		boost::replace_all(content, "\n", "&NewLine;");
        	boost::replace_all(content, "<", "&lt;");
        	boost::replace_all(content, ">", "&gt;");
        	boost::replace_all(content, "'", "&lsquo;");
        	string message = "<script>document.getElementById('" + session + "').innerHTML += '" + content + "';</script>\n";
			boost::system::error_code ec;
			socket_.write_some(buffer(message), ec);
    	}
		void output_command(string session, string content){
        	boost::replace_all(content, "\r\n", "\n");
        	boost::replace_all(content, "\n", "&NewLine;");
        	boost::replace_all(content, "<", "&lt;");
        	boost::replace_all(content, ">", "&gt;");
        	string message = "<script>document.getElementById('" + session + "').innerHTML += '<b>" + content + "</b>';</script>\n";
        	boost::system::error_code ec;
			socket_.write_some(buffer(message), ec);
		}
};

class server{
	private:
		ip::tcp::acceptor acceptor_;

	public:
		server(int port)
			:
			acceptor_(ioservice, ip::tcp::endpoint(ip::tcp::v4(), port)) //bind port
		{
			do_accept();
		}
		void do_accept(){
			Session* new_session = new Session();
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

		server s(atoi(argv[1]));
		ioservice.run();
	}
	catch(std::exception& e){
		std::cerr << "Exception: " << e.what() << "\n";
		exit(EXIT_FAILURE);
	}
	return EXIT_SUCCESS;
}