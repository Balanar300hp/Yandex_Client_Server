/*Структура хранения данных яндекс дисков*/
struct YanDisk {
	std::string login_ya, password_ya, path_ya;
	std::vector<std::string> resources, temp_files;
};


class Server {
public:
	Server(std::string str);
	auto Entry(int param)->void;//общая ф-я запуска программы для подключения и скачивания с сервера файлов двух структур
	void download_to(CURL *curl, std::string name, int i, int param);//скачиваем с сервера
	void aes_decrypt(int i, int param);//дешифруем
	auto check_files(CURL *curl_,int param)->void;//поиск шифрованных файлов на яндекс диске
private:
	std::string server, login, password, files, client_file, path, ya_d;
	std::vector<std::string> file_names_dir, direct;
	std::vector<YanDisk> structures;

};

void Server::aes_decrypt(int i, int param)
{
	int outlen, inlen;
	/*Уберем приставку дешифрованного файла*/
	std::string del=".enc";
	size_t pos = structures[param].resources[i].find(del);
	std::string reg_name= structures[param].resources[i].erase(pos, del.size());
	/*Создадим файл в папке out в исходном виде*/
	std::string s_out = structures[param].path_ya + "/out/" + reg_name;
	FILE *in = fopen(structures[param].temp_files[i].c_str(), "rb"), *out = fopen(s_out.c_str(), "wb");
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
	remove(structures[param].temp_files[i].c_str());
}

Server::Server(std::string str) : client_file(str) {
	std::ifstream file(client_file);
	getline(file, ya_d);
	getline(file, server);
	int n = boost::lexical_cast<int> (ya_d);
	
	for (int i = 0; i < n; i++) {
		YanDisk obj;
		getline(file, obj.login_ya);
		getline(file, obj.password_ya);
		getline(file, obj.path_ya);
		structures.push_back(obj);
	}

	/*Потоки на скачивание*/
	bamthread::ThreadPool pp(2);
	for (int i=0;i<n;i++) Entry(i);
	file.close();
}

void Server::download_to(CURL *curl_, std::string name, int i, int param) {
	std::ofstream file_stream(name, std::ios::binary);
	if (file_stream.is_open()) {
		/*Сбрасываем настройки сессии до исходных , подключаемся к серверу с новым url и скачиаем файл*/
		curl_easy_reset(curl_);
		curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1);
		std::string path_server = server + "/" + structures[param].resources[i];
		curl_easy_setopt(curl_, CURLOPT_URL, path_server.c_str());
		curl_easy_setopt(curl_, CURLOPT_HTTPAUTH, (int)CURLAUTH_BASIC);
		curl_easy_setopt(curl_, CURLOPT_USERNAME, structures[param].login_ya.c_str());
		curl_easy_setopt(curl_, CURLOPT_PASSWORD, structures[param].password_ya.c_str());
		curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "GET");
		curl_easy_setopt(curl_, CURLOPT_HEADER, 0L);
		curl_easy_setopt(curl_, CURLOPT_WRITEDATA, (size_t)&file_stream);
		curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, (size_t)Download::stream);
		curl_easy_perform(curl_);
	}
	file_stream.close();
}

auto Server::Entry(int param)->void {
	/*Подключаемся к серваку*/
	CURL *curl_;
	curl_ = curl_easy_init();
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl_, CURLOPT_URL, server.c_str());
	curl_easy_setopt(curl_, CURLOPT_HTTPAUTH, (int)CURLAUTH_BASIC);
	curl_easy_setopt(curl_, CURLOPT_USERNAME, structures[param].login_ya.c_str());
	curl_easy_setopt(curl_, CURLOPT_PASSWORD, structures[param].password_ya.c_str());
	curl_easy_perform(curl_);
	/*Вызываем ф-ю подгрузки xml кода*/
	check_files(curl_,param);

	/*Начинаем скачивать файлы*/
	for (int i = 0; i < structures[param].resources.size(); i++) {
		std::string t = structures[param].path_ya + "/temp/" + structures[param].resources[i];
		structures[param].temp_files.push_back(t);
		download_to(curl_, t, i, param);
		std::string path_server = server + "/" + structures[param].resources[i];
		curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "DELETE");
		curl_easy_setopt(curl_, CURLOPT_URL, path_server.c_str());
		curl_easy_perform(curl_);
	}

	/*Запускаем потоки дешифровки, выделяем 4 потока*/
	bamthread::ThreadPool tp(4);
	for (int i = 0; i < structures[param].resources.size(); i++) {
		tp.enqueue(boost::bind(&Server::aes_decrypt, this, i, param));
	}

	
	/*Чистим сессию*/
	curl_global_cleanup();
	curl_easy_reset(curl_);
	curl_easy_cleanup(curl_);
}

auto Server::check_files(CURL *curl,int param)->void {
	Header header = {
		"Accept: */*",
		"Depth: 1"
	};
	Data data = { 0, 0, 0 };
	/*Подгружаем xml код по коду ищем тэг с именем файла , файл содержащий .enc на конце добавляем в вектор*/
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, (struct curl_slist *)header.handle);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (size_t)&data);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t)Append::buffer);
	curl_easy_perform(curl);
	size_t n;
	/*Работа xml кодом из структуры data*/
	pugi::xml_document document;
	document.load_buffer(data.buffer, (size_t)data.size);
	auto nodes = document.select_nodes("//d:displayname");
	for (auto& node : nodes)
	{
		std::string ttt = node.node().text().as_string();
		n = ttt.find(".enc");
		if (n != std::string::npos) structures[param].resources.push_back(node.node().text().as_string());
	}
}
