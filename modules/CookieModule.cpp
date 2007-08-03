// -----------------------------------------------------------------
// libpion: a C++ framework for building lightweight HTTP interfaces
// -----------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file COPYING or copy at http://www.boost.org/LICENSE_1_0.txt
//

#include "CookieModule.hpp"
#include <libpion/HTTPResponse.hpp>

using namespace pion;


// CookieModule member functions

/// handles requests for CookieModule
bool CookieModule::handleRequest(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn)
{
	static const std::string HEADER_HTML = "<html>\n<head>\n<title>Cookie Module</title>\n"
		"</head>\n<body>\n\n<h1>Cookie Module</h1>\n";
	static const std::string FOOTER_HTML = "\n</body>\n</html>\n";

	// Set Content-type for HTML and write the header
	HTTPResponsePtr response(HTTPResponse::create());
	response->setContentType(HTTPTypes::CONTENT_TYPE_HTML);
	response->writeNoCopy(HEADER_HTML);

	// Check if we have an action to perform
	if (request->hasQuery("action")) {
		if (HTTPTypes::url_decode(request->getQuery("action")) == "Add Cookie") {
			// add a new cookie
			const std::string cookie_name(request->getQuery("cookie_name"));
			const std::string cookie_value(request->getQuery("cookie_value"));
			if (cookie_name.empty() || cookie_value.empty()) {
				response << "\n<p>[Error: You must specify a name and value to add a cookie]</p>\n\n";
			} else {
				response->setCookie(cookie_name, cookie_value);
				response << "\n<p>[Added cookie "
					<< cookie_name << '=' << cookie_value << "]</p>\n\n";
			}
		} else if (request->getQuery("action") == "delete") {
			const std::string cookie_name(request->getQuery("cookie_name"));
			if (cookie_name.empty()) {
				response << "\n<p>[Error: You must specify a name to delete a cookie]</p>\n\n";
			} else {
				response->deleteCookie(cookie_name);
				response << "\n<p>[Deleted cookie " << cookie_name << "]</p>\n\n";
			}
		} else {
			response << "\n<p>[Error: Unrecognized action]</p>\n\n";
		}
	}
	
	// display cookie headers in request
	if (request->hasHeader(HTTPTypes::HEADER_COOKIE)) {
		response << "\n<h2>Cookie Headers</h2>\n<ul>\n";
		std::pair<HTTPTypes::Headers::const_iterator, HTTPTypes::Headers::const_iterator>
			header_pair = request->getHeaders().equal_range(HTTPTypes::HEADER_COOKIE);
		for (HTTPTypes::Headers::const_iterator header_iterator = header_pair.first;
			 header_iterator != request->getHeaders().end()
			 && header_iterator != header_pair.second; ++header_iterator)
		{
			response << "<li>Cookie: " << header_iterator->second << "\n";
		}
		response << "</ul>\n\n";
	} else {
		response << "\n<h2>No Cookie Headers</h2>\n\n";
	}
	
	// display existing cookies
	HTTPTypes::CookieParams& cookie_params = request->getCookieParams();
	if (! cookie_params.empty()) {
		response << "\n<h2>Cookie Variables</h2>\n<ul>\n";
		for (HTTPTypes::CookieParams::const_iterator i = cookie_params.begin();
			 i != cookie_params.end(); ++i)
		{
			response << "<li>" << i->first << ": " << i->second
				<< " <a href=\"" << request->getResource()
				<< "?action=delete&cookie_name=" << i->first
				<< "\">[Delete]</a>\n";
		}
		response << "</ul>\n\n";
	} else {
		response << "\n<h2>No Cookie Variables</h2>\n\n";
	}

	// display form to add a cookie
	response << "\n<h2>Add Cookie</h2>\n"
		"<p><form action=\"" << request->getResource() << "\" method=\"POST\">\n"
		"Name: <input type=\"text\" name=\"cookie_name\"><br />\n"
		"Value: <input type=\"text\" name=\"cookie_value\"><br />\n"
		"<input type=\"submit\" name=\"action\" value=\"Add Cookie\"></p>\n"
		"</form>\n\n";
	
	// write the footer
	response->writeNoCopy(FOOTER_HTML);
	
	// send the response
	response->send(tcp_conn);
	return true;
}


/// creates new CookieModule objects
extern "C" CookieModule *pion_create_CookieModule(void)
{
	return new CookieModule();
}


/// destroys CookieModule objects
extern "C" void pion_destroy_CookieModule(CookieModule *module_ptr)
{
	delete module_ptr;
}
