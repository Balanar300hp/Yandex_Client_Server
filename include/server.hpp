#include <stdio.h>
#include <openssl/rsa.h>
#include <curl/curl.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>
#include <openssl/crypto.h>
#include <openssl/lhash.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pkcs12.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <cstdio> 
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <algorithm>
#include <fcntl.h>
#include <boost/filesystem.hpp>
#include <algorithm>
#include <sstream>
#define BUFSIZE 1024
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>


namespace bamthread
{
	typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

	struct ThreadPool {
		ThreadPool(size_t threads) :service(), working(new asio_worker::element_type(service)) {
			while (threads--)
			{
				auto worker = boost::bind(&boost::asio::io_service::run, &(this->service));
				g.add_thread(new boost::thread(worker));
			}
		}

		template<class F>
		void enqueue(F f) {
			service.post(f);
		}

		~ThreadPool() {
			working.reset();
			g.join_all();
			service.stop();
		}

	private:
		boost::asio::io_service service;
		asio_worker working;
		boost::thread_group g;
	};
}

struct Data
{
	char * buffer;
	unsigned long long position;
	unsigned long long size;
};

namespace Download {
	size_t stream(char * ptr, size_t item_size, size_t item_count, void * stream)
	{
		auto out_stream = (std::ostream *)stream;
		size_t write_bytes = item_size * item_count;
		out_stream->write(ptr, write_bytes);
		return write_bytes;
	}
}

class Server {
public:
	Server(std::string str);
	auto Entry()->void;
	void download_to(CURL *curl, std::string name, int i);
	void aes_decrypt(int i);
private:
	std::string server, login, password, files, client_file, path;
	std::vector<std::string> file_names_dir, direct, files_download, temp_files;
};

void Server::aes_decrypt(int i)
{
	int outlen, inlen;
	std::string s_out = path + "/out/" + files_download[i];
	FILE *in = fopen(temp_files[i].c_str(), "rb"), *out = fopen(s_out.c_str(), "wb");
	unsigned char inbuf[BUFSIZE], outbuf[BUFSIZE];
	unsigned char key[32]; /* 256- битный ключ */
	unsigned char iv[8]; /* вектор инициализации */
	const EVP_CIPHER * cipher;
	EVP_CIPHER_CTX ctx;
	/* обнуляем структуру контекста */
	EVP_CIPHER_CTX_init(&ctx);
	/* выбираем алгоритм шифрования */
	cipher = EVP_bf_ofb();
	/* инициализируем контекст алгоритма */
	EVP_DecryptInit(&ctx, cipher, key, iv);
	/* шифруем данные */
	for (;;)
	{
		inlen = fread(inbuf, 1, BUFSIZE, in);
		if (inlen <= 0) break;
		EVP_DecryptUpdate(&ctx, outbuf, &outlen, inbuf, inlen);
		fwrite(outbuf, 1, outlen, out);
	}
	EVP_DecryptFinal(&ctx, outbuf, &outlen);
	fwrite(outbuf, 1, outlen, out);
	EVP_CIPHER_CTX_cleanup(&ctx);
	fclose(in);
	fclose(out);
	remove(temp_files[i].c_str());
}

Server::Server(std::string str) : client_file(str) {
	std::ifstream file(client_file);
	getline(file, server);
	getline(file, login);
	getline(file, password);
	getline(file, path);
	std::string temp;
	getline(file, temp);
	std::istringstream iss(temp);
	std::copy(std::istream_iterator<std::string>(iss),
		std::istream_iterator<std::string>(),
		std::back_inserter<std::vector<std::string> >(files_download));
	file.close();
	Entry();
}

void Server::download_to(CURL *curl_, std::string name, int i) {
	std::ofstream file_stream(name, std::ios::binary);
	if (file_stream.is_open()) {
		std::string path_server = server + "/" + files_download[i];
		curl_easy_setopt(curl_, CURLOPT_URL, path_server.c_str());
		curl_easy_setopt(curl_, CURLOPT_HTTPAUTH, (int)CURLAUTH_BASIC);
		curl_easy_setopt(curl_, CURLOPT_USERNAME, login.c_str());
		curl_easy_setopt(curl_, CURLOPT_PASSWORD, password.c_str());
		curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "GET");
		curl_easy_setopt(curl_, CURLOPT_HEADER, 0L);
		curl_easy_setopt(curl_, CURLOPT_WRITEDATA, (size_t)&file_stream);
		curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, (size_t)Download::stream);
		curl_easy_perform(curl_);
	}
	file_stream.close();
}

auto Server::Entry()->void {
	CURL *curl_;
	curl_ = curl_easy_init();
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1);


	for (int i = 0; i < files_download.size(); i++) {
		std::string t = path + "/temp/" + files_download[i];
		temp_files.push_back(t);
		download_to(curl_, t, i);
	}

	bamthread::ThreadPool tp(2);
	tp.enqueue(boost::bind(&Server::aes_decrypt, this, 0));
	tp.enqueue(boost::bind(&Server::aes_decrypt, this, 1));
}
