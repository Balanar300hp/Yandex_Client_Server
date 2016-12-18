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
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")
#include <fcntl.h>
#include <boost/filesystem.hpp>
#include <algorithm>
#include <functional>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#define BUFSIZE (1024*16)
#include <openssl/md5.h>

#include <iomanip>

#include<sstream>

struct Data
{
	char * buffer;
	unsigned long long position;
	unsigned long long size;
};

auto stream(char * ptr, size_t item_size, size_t item_count, void * stream)->size_t
{
	auto in_stream = (std::istream *)stream;
	auto read_bytes = static_cast<unsigned long long>(item_size * item_count);
	auto position = static_cast<unsigned long long>(in_stream->tellg());
	in_stream->seekg(0, std::ios::end);
	auto size = static_cast<unsigned long long>(in_stream->tellg());
	in_stream->seekg(position, std::ios::beg);
	auto rest_bytes = size - position;
	read_bytes = std::min<unsigned long long>(read_bytes, rest_bytes);
	in_stream->read(ptr, read_bytes);
	return read_bytes;
}

auto buffer(char * ptr, size_t item_size, size_t item_count, void * buffer)->size_t
{
	auto data = (Data*)buffer;
	auto append_size = item_size * item_count;
	auto new_buffer_size = data->size + append_size;
	auto new_buffer = new char[new_buffer_size];
	if (data->size != 0) memcpy(new_buffer, data->buffer, data->size);
	memcpy(new_buffer + data->size, ptr, append_size);
	delete[] data->buffer;
	data->buffer = new_buffer;
	data->size = new_buffer_size;
	return append_size;
}






class Client {
public:
	Client(std::string str);
	auto Entry()->void;
	bool FindFile(const boost::filesystem::path& directory);
	std::string aes_encrypt(int i, std::string name);
	auto size_of_file(const std::string& path_file) -> unsigned long long;
	auto hash(std::string name)->void;
	auto check_hash(unsigned char* hash, size_t hex_length)->bool;
	auto ToHex(unsigned char* s, bool upper_case /* = true */, size_t hex_length)->std::string;
private:
	std::string server, login, password, path, client_file;
	std::vector<std::string> file_names_dir, direct, encrypt_files_path;
	std::vector<bool> flags;
};

Client::Client(std::string str) : client_file(str) {

	std::ifstream file(client_file);
	if (file.is_open()) {
		getline(file, server);
		getline(file, login);
		getline(file, password);
		getline(file, path);
		file.close();
		Entry();
	}
	else {
		std::cout << "File is not open";
	}
}

auto Client::ToHex(unsigned char* s, bool upper_case /* = true */,size_t hex_length)->std::string
{
	std::ostringstream ret;

	for (std::string::size_type i = 0; i < hex_length; ++i)
		ret << std::hex << static_cast<int>(s[i]);

	return ret.str();
}

auto Client::check_hash(unsigned char* hash,size_t hex_length)->bool {
	bool flag = true;
	std::string hash_path = path + "/temp/hash";
	std::ifstream hash_check(hash_path);
	std::string hex_s= ToHex(hash,true,hex_length);
	std::string ss;
	

		while (!hash_check.eof())
		{
			getline(hash_check, ss);
			if (hex_s==ss) {
				flag = false;
				break;
			}
		}
		hash_check.close();
	
	return flag;
}

auto Client::hash(std::string name)->void {
	MD5_CTX c; /* контекст хэша */
	unsigned char buf[BUFSIZE],string[BUFSIZE];
	unsigned char md_buf[MD5_DIGEST_LENGTH];

	/* В командной строке передается имя файла, для которого вычисляется хэш */
	//int inf = open(argv[1], O_RDWR);
	FILE * ptrFile = fopen(name.c_str(), "rb");
	fseek(ptrFile, 0, SEEK_END);
	long lSize = ftell(ptrFile);
	rewind(ptrFile);
	
	/* Инициализируем контекст */
	MD5_Init(&c);

	/* Вычисляем хэш */
	for (;;) {
		int i = fread(buf, 1, lSize, ptrFile);
		if (i <= 0) break;
		MD5_Update(&c, buf, (unsigned long)i);
	}

	/* Помещаем вычисленный хэш в буфер md_buf */
	MD5_Final(md_buf, &c);
	std::string str2 = path + "/temp/hash";
	std::ofstream hash(str2,std::ios::app);
	bool flag=check_hash(md_buf, MD5_DIGEST_LENGTH);
	flags.push_back(flag);
	if (flag) {
		/* Отображаем результат */
		for (int i = 0; i < MD5_DIGEST_LENGTH; i++) hash << std::hex << static_cast<int>(md_buf[i]);
		hash << "\n";
	}
	hash.close();
}

auto Client::size_of_file(const std::string& path_file) -> unsigned long long
{
	std::ifstream file(path_file, std::ios::binary | std::ios::ate);
	return (unsigned long long)file.tellg();
}

std::string Client::aes_encrypt(int i, std::string name)
{
	int outlen, inlen;
	std::string name_out = path + "/temp/" + "encrypt_" + file_names_dir[i];
	encrypt_files_path.push_back(name_out);
	FILE *in = fopen(name.c_str(), "rb"), *out = fopen(name_out.c_str(), "wb");
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
	EVP_EncryptInit(&ctx, cipher, key, iv);
	/* шифруем данные */
	for (;;)
	{
		inlen = fread(inbuf, 1, BUFSIZE, in);
		if (inlen <= 0) break;
		EVP_EncryptUpdate(&ctx, outbuf, &outlen, inbuf, inlen);
		fwrite(outbuf, 1, outlen, out);
	}
	EVP_EncryptFinal(&ctx, outbuf, &outlen);
	fwrite(outbuf, 1, outlen, out);
	EVP_CIPHER_CTX_cleanup(&ctx);
	fclose(in);
	fclose(out);
	return file_names_dir[i];
}

bool Client::FindFile(const boost::filesystem::path& directory)
{
	bool found = false;

	boost::filesystem::recursive_directory_iterator end;
	boost::filesystem::recursive_directory_iterator dir_iter(directory);

	for (; dir_iter != end; ++dir_iter) {
		if (dir_iter->path().extension() == ".file") {
			file_names_dir.push_back(dir_iter->path().filename().string());
			direct.push_back(dir_iter->path().string());
			hash(dir_iter->path().string());
		}
	}
	if (!file_names_dir.empty()) {
		found = true;
	}
	return found;
}



auto Client::Entry()->void {



	CURL *curl_; // создаем обьект сессии
	curl_ = curl_easy_init();//инициализируем
	curl_global_init(CURL_GLOBAL_DEFAULT);

	/*начинаем закидывать опции в обьект*/
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl_, CURLOPT_PUT, 1L);

	/*выполняем рекурсивный обход директории*/
	std::string file_in_path = path + "/in";
	boost::filesystem::path SearchDir(file_in_path);
	bool flag;
	flag = FindFile(SearchDir);

	/*Если файлы найдены закидываем их на сервер*/
	if (flag) {
		for (int i = 0; i < file_names_dir.size(); i++) {
			if (flags[i]==true) {
				std::string file_out_name = aes_encrypt(i, direct[i]);//шифруем
				std::string path_server = server + "/" + file_out_name;
				std::ifstream file_stream(encrypt_files_path[i], std::ios::binary);
				auto size = size_of_file(encrypt_files_path[i]);
				hash(direct[i]);
				if (file_stream.is_open()) {
					Data response = { 0, 0, 0 };

					curl_easy_setopt(curl_, CURLOPT_URL, path_server.c_str());
					curl_easy_setopt(curl_, CURLOPT_HTTPAUTH, (int)CURLAUTH_BASIC);
					curl_easy_setopt(curl_, CURLOPT_USERNAME, login.c_str());
					curl_easy_setopt(curl_, CURLOPT_PASSWORD, password.c_str());

					curl_easy_setopt(curl_, CURLOPT_READDATA, (size_t)&file_stream);
					curl_easy_setopt(curl_, CURLOPT_READFUNCTION, (size_t)stream);

					curl_easy_setopt(curl_, CURLOPT_INFILESIZE_LARGE, (curl_off_t)size);

					curl_easy_setopt(curl_, CURLOPT_WRITEDATA, (size_t)&response);
					curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, (size_t)buffer);

					curl_easy_perform(curl_);
				}
				file_stream.close();
				remove(encrypt_files_path[i].c_str());
			}
		}
	}
}
