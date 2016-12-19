#include <iostream>
#include <string>
#include <curl/curl.h>
#define BUFSIZE 1024
#include <openssl/ssl.h>
#include <fstream>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>
#include "pugixml-1.8/src/pugixml.hpp"

	class Header final
	{
	public:
		void * handle;

		Header(std::initializer_list<std::string> init_list) noexcept;
		~Header();

		void append(const std::string item) noexcept;
	};

	Header::Header(std::initializer_list<std::string> init_list) noexcept : handle(nullptr)
	{
		for (auto item : init_list)
		{
			this->append(item);
		}
	}

	Header::~Header()
	{
		curl_slist_free_all((curl_slist*)this->handle);
	}

	void
		Header::append(const std::string item) noexcept
	{
		this->handle = curl_slist_append((curl_slist*)this->handle, item.c_str());
	}


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

	namespace Append
	{
		size_t buffer(char * ptr, size_t item_size, size_t item_count, void * buffer)
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

		size_t stream(char * ptr, size_t item_size, size_t item_count, void * stream)
		{
			auto out_stream = (std::ostream *)stream;
			size_t write_bytes = item_size * item_count;
			out_stream->seekp(0, std::ios::end);
			out_stream->write(ptr, write_bytes);
			return write_bytes;
		}
	}

	namespace Download {
		size_t stream(char * ptr, size_t item_size, size_t item_count, void * stream)
		{
			auto out_stream = (std::ostream *)stream;
			size_t write_bytes = item_size * item_count;
			out_stream->write(ptr, write_bytes);
			return write_bytes;
		}
	}
