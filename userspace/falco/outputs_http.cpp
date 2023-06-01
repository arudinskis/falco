/*
Copyright (C) 2020 The Falco Authors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "outputs_http.h"
#include "logger.h"
#include "banned.h" // This raises a compilation error when certain functions are used

static size_t noop_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	// We don't want to echo anything. Just return size of bytes ignored
	return size * nmemb;
}

bool falco::outputs::output_http::init(const config& oc, bool buffered, const std::string& hostname, bool json_output, std::string &err)
{
	falco::outputs::abstract_output::init(oc, buffered, hostname, json_output, err);

	m_curl = nullptr;
	m_http_headers = nullptr;
	CURLcode res = CURLE_FAILED_INIT;

	m_curl = curl_easy_init();
	if(m_curl)
	{
		if(m_json_output)
		{
			m_http_headers = curl_slist_append(m_http_headers, "Content-Type: application/json");
		}
		else
		{
			m_http_headers = curl_slist_append(m_http_headers, "Content-Type: text/plain");
		}
		res = curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_http_headers);

		if(res == CURLE_OK)
		{
			res = curl_easy_setopt(m_curl, CURLOPT_URL, m_oc.options["url"].c_str());
		}

		if(res == CURLE_OK)
		{
			res = curl_easy_setopt(m_curl, CURLOPT_USERAGENT, m_oc.options["user_agent"].c_str());
		}

		if(res == CURLE_OK)
		{
			res = curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, -1L);
		}

		if(res == CURLE_OK)
		{
			if(m_oc.options["insecure"] == std::string("true"))
			{
				res = curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L);

				if(res == CURLE_OK)
				{
					res = curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 0L);
				}
			}
		}

		if(res == CURLE_OK)
		{
			if(!m_oc.options["ca_cert"].empty())
			{
				res = curl_easy_setopt(m_curl, CURLOPT_CAINFO, m_oc.options["ca_cert"].c_str());
			}
			else if(!m_oc.options["ca_bundle"].empty())
			{
				res = curl_easy_setopt(m_curl, CURLOPT_CAINFO, m_oc.options["ca_bundle"].c_str());
			}
			else
			{
				res = curl_easy_setopt(m_curl, CURLOPT_CAPATH, m_oc.options["ca_path"].c_str());
			}
		}

		if(m_oc.options["echo"] == std::string("false"))
		{
			// If unset, libcurl defaults to fwrite to stdout, ie: echoing
			if(res == CURLE_OK)
			{
				res = curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, noop_write_callback);
			}
		}
	}

	if(res != CURLE_OK)
	{
		err = "libcurl error: " + std::string(curl_easy_strerror(res));
		return false;
	}
	return true;
}

void falco::outputs::output_http::output(const message *msg)
{
	CURLcode res = curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, msg->msg.c_str());
	if (res == CURLE_OK)
	{
		res = curl_easy_perform(m_curl);
	}
	if(res != CURLE_OK)
	{
		falco_logger::log(LOG_ERR, "libcurl failed to perform call: " + std::string(curl_easy_strerror(res)));
	}
}

void falco::outputs::output_http::cleanup()
{
	curl_easy_cleanup(m_curl);
	m_curl = nullptr;
	curl_slist_free_all(m_http_headers);
	m_http_headers = nullptr;
}
