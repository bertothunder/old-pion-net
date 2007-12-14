// ------------------------------------------------------------------
// pion-net: a C++ framework for building lightweight HTTP interfaces
// ------------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file COPYING or copy at http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_HTTPMESSAGE_HEADER__
#define __PION_HTTPMESSAGE_HEADER__

#include <cstdlib>
#include <vector>
#include <boost/asio.hpp>
#include <boost/scoped_array.hpp>
#include <boost/lexical_cast.hpp>
#include <pion/PionConfig.hpp>
#include <pion/net/HTTPTypes.hpp>


namespace pion {	// begin namespace pion
namespace net {		// begin namespace net (Pion Network Library)


// forward declaration for class used by send() and receive()
class TCPConnection;


///
/// HTTPMessage: base container for HTTP messages
/// 
class PION_NET_API HTTPMessage
	: public HTTPTypes
{
public:
	
	/// data type for I/O write buffers (these wrap existing data to be sent)
	typedef std::vector<boost::asio::const_buffer>	WriteBuffers;
	
	/// used to cache chunked data
	typedef std::list<std::vector<char> >			ChunkCache;

	/// data type for library errors returned during receive() operations
	struct ReceiveError
		: public boost::system::error_category
	{
		virtual ~ReceiveError() {}
		virtual inline const char *name() const { return "ReceiveError"; }
		virtual inline std::string message(int ev) const {
			std::string result;
			switch(ev) {
				case 1:
					result = "HTTP message parsing error";
					break;
				default:
					result = "Unknown receive error";
					break;
			}
			return result;
		}
	};
		

	/// constructs a new HTTP message object
	HTTPMessage(void)
		: m_is_valid(false), m_chunks_supported(false),
		m_version_major(0), m_version_minor(0), m_content_length(0), m_is_chunked(false)
	{}

	/// copy constructor
	HTTPMessage(const HTTPMessage& http_msg)
		: m_is_valid(http_msg.m_is_valid),
		m_chunks_supported(http_msg.m_chunks_supported),
		m_remote_ip(http_msg.m_remote_ip),
		m_version_major(http_msg.m_version_major),
		m_version_minor(http_msg.m_version_minor),
		m_content_length(http_msg.m_content_length),
		m_is_chunked(http_msg.m_is_chunked),
		m_headers(http_msg.m_headers),
		m_chunk_buffers(http_msg.m_chunk_buffers)
	{
		if (http_msg.m_content_buf) {
			char *ptr = createContentBuffer();
			memcpy(ptr, http_msg.m_content_buf.get(), m_content_length);
		}
	}
	
	/// virtual destructor
	virtual ~HTTPMessage() {}

	/// clears all message data
	virtual void clear(void) {
		m_is_valid = m_chunks_supported = false;
		m_remote_ip = boost::asio::ip::address_v4(0);
		m_version_major = m_version_minor = 0;
		m_content_length = 0;
		m_is_chunked = false;
		m_content_buf.reset();
		m_chunk_buffers.clear();
		m_headers.clear();
	}

	
	/// returns true if the message is valid
	inline bool isValid(void) const { return m_is_valid; }
	
	/// returns true if chunked transfer encodings are supported
	inline bool getChunksSupported(void) const { return m_chunks_supported; }

	/// returns IP address of the remote endpoint
	inline boost::asio::ip::address& getRemoteIp(void) {
		return m_remote_ip;
	}

	/// returns the major HTTP version number
	inline unsigned int getVersionMajor(void) const { return m_version_major; }
	
	/// returns the minor HTTP version number
	inline unsigned int getVersionMinor(void) const { return m_version_minor; }
	
	/// returns the length of the payload content (in bytes)
	inline size_t getContentLength(void) const { return m_content_length; }

	/// returns true if the message content is chunked
	inline bool isChunked(void) const { return m_is_chunked; }
	
	/// returns a pointer to the payload content, or NULL if there is none
	inline char *getContent(void) { return m_content_buf.get(); }
	
	/// returns a const pointer to the payload content, or NULL if there is none
	inline const char *getContent(void) const { return m_content_buf.get(); }
	
	/// returns a reference to the chunk buffers
	inline ChunkCache& getChunkBuffers(void) { return m_chunk_buffers; }

	/// returns a value for the header if any are defined; otherwise, an empty string
	inline const std::string& getHeader(const std::string& key) const {
		return getValue(m_headers, key);
	}
	
	/// returns a reference to the HTTP headers
	inline Headers& getHeaders(void) {
		return m_headers;
	}
	
	/// returns true if at least one value for the header is defined
	inline bool hasHeader(const std::string& key) const {
		return(m_headers.find(key) != m_headers.end());
	}
	
	
	/// sets whether or not the message is valid
	inline void setIsValid(bool b = true) { m_is_valid = b; }
	
	/// set to true if chunked transfer encodings are supported
	inline void setChunksSupported(bool b) { m_chunks_supported = b; }
	
	/// sets IP address of the remote endpoint
	inline void setRemoteIp(const boost::asio::ip::address& ip) { m_remote_ip = ip; }

	/// sets the major HTTP version number
	inline void setVersionMajor(const unsigned int n) { m_version_major = n; }

	/// sets the minor HTTP version number
	inline void setVersionMinor(const unsigned int n) { m_version_minor = n; }

	/// sets the length of the payload content (in bytes)
	inline void setContentLength(const size_t n) { m_content_length = n; }
	
	/// sets the length of the payload content using the Content-Length header
	inline void updateContentLengthUsingHeader(void) {
		Headers::const_iterator i = m_headers.find(HEADER_CONTENT_LENGTH);
		if (i == m_headers.end()) m_content_length = 0;
		else m_content_length = strtoul(i->second.c_str(), 0, 10);
	}
	
	/// sets the transfer coding using the Transfer-Encoding header
	inline void updateTransferCodingUsingHeader(void) {
		m_is_chunked = false;
		Headers::const_iterator i = m_headers.find(HEADER_TRANSFER_ENCODING);
		if (i != m_headers.end()) {
			// From RFC 2616, sec 3.5: All content-coding values are case-insensitive.
			std::string lowerCaseHeaderValue;
			for (std::string::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
				lowerCaseHeaderValue.push_back(std::tolower(*j));
			}
			if (lowerCaseHeaderValue == "chunked") m_is_chunked = true;
			// ignoring other possible values for now
		}
	}
	
	/// creates a payload content buffer of size m_content_length and returns
	/// a pointer to the new buffer (memory is managed by HTTPMessage class)
	inline char *createContentBuffer(void) {
		m_content_buf.reset(new char[m_content_length + 1]);
		m_content_buf[m_content_length] = '\0';
		return m_content_buf.get();
	}
	
	/// sets the content type for the message payload
	inline void setContentType(const std::string& type) {
		changeValue(m_headers, HEADER_CONTENT_TYPE, type);
	}
	
	/// adds a value for the HTTP header named key
	inline void addHeader(const std::string& key, const std::string& value) {
		m_headers.insert(std::make_pair(key, value));
	}

	/// changes the value for the HTTP header named key
	inline void changeHeader(const std::string& key, const std::string& value) {
		changeValue(m_headers, key, value);
	}
	
	/// removes all values for the HTTP header named key
	inline void deleteHeader(const std::string& key) {
		deleteValue(m_headers, key);
	}
	
	/// returns true if the HTTP connection may be kept alive
	inline bool checkKeepAlive(void) const {
		return (getHeader(HEADER_CONNECTION) != "close"
				&& (getVersionMajor() > 1
					|| (getVersionMajor() >= 1 && getVersionMinor() >= 1)) );
	}	
		
	/**
	 * initializes a vector of write buffers with the HTTP message information
	 *
	 * @param write_buffers vector of write buffers to initialize
	 * @param keep_alive true if the connection should be kept alive
	 * @param using_chunks true if the payload content will be sent in chunks
	 */
	inline void prepareBuffersForSend(WriteBuffers& write_buffers,
									  const bool keep_alive,
									  const bool using_chunks)
	{
		// update message headers
		prepareHeadersForSend(keep_alive, using_chunks);
		// add first message line
		write_buffers.push_back(boost::asio::buffer(getFirstLine()));
		write_buffers.push_back(boost::asio::buffer(STRING_CRLF));
		// append HTTP headers
		appendHeaders(write_buffers);
	}		
	
	
	/**
	 * sends the message over a TCP connection (blocks until finished)
	 *
	 * @param ec contains error code if the send fails
	 * @return std::size_t number of bytes written to the connection
	 */
	std::size_t send(TCPConnection& tcp_conn, boost::system::error_code& ec);
	
	/**
	 * receives a new message from a TCP connection (blocks until finished)
	 *
	 * @param ec contains error code if the receive fails
	 * @return std::size_t number of bytes read from the connection
	 */
	std::size_t receive(TCPConnection& tcp_conn, boost::system::error_code& ec);

	/**
	 * pieces together all the received chunks
	 */
	void concatenateChunks(void);
	
protected:
	
	/**
	 * prepares HTTP headers for a send operation
	 *
	 * @param keep_alive true if the connection should be kept alive
	 * @param using_chunks true if the payload content will be sent in chunks
	 */
	inline void prepareHeadersForSend(const bool keep_alive,
									  const bool using_chunks)
	{
		changeHeader(HEADER_CONNECTION, (keep_alive ? "Keep-Alive" : "close") );
		if (using_chunks) {
			if (getChunksSupported())
				changeHeader(HEADER_TRANSFER_ENCODING, "chunked");
		} else {
			changeHeader(HEADER_CONTENT_LENGTH, boost::lexical_cast<std::string>(getContentLength()));
		}
	}
	
	/**
	 * appends the message's HTTP headers to a vector of write buffers
	 *
	 * @param write_buffers the buffers to append HTTP headers into
	 */
	inline void appendHeaders(WriteBuffers& write_buffers) {
		// add HTTP headers
		for (Headers::const_iterator i = m_headers.begin(); i != m_headers.end(); ++i) {
			write_buffers.push_back(boost::asio::buffer(i->first));
			write_buffers.push_back(boost::asio::buffer(HEADER_NAME_VALUE_DELIMITER));
			write_buffers.push_back(boost::asio::buffer(i->second));
			write_buffers.push_back(boost::asio::buffer(STRING_CRLF));
		}
		// add an extra CRLF to end HTTP headers
		write_buffers.push_back(boost::asio::buffer(STRING_CRLF));
	}	
	
	/**
	 * Returns the first value in a dictionary if key is found; or an empty
	 * string if no values are found
	 *
	 * @param dict the dictionary to search for key
	 * @param key the key to search for
	 * @return value if found; empty string if not
	 */
	inline static const std::string& getValue(const StringDictionary& dict,
											  const std::string& key)
	{
		StringDictionary::const_iterator i = dict.find(key);
		return ( (i==dict.end()) ? STRING_EMPTY : i->second );
	}

	/**
	 * Changes the value for a dictionary key.  Adds the key if it does not
	 * already exist.  If multiple values exist for the key, they will be
	 * removed and only the new value will remain.
	 *
	 * @param dict the dictionary object to update
	 * @param key the key to change the value for
	 * @param value the value to assign to the key
	 */
	inline static void changeValue(StringDictionary& dict,
								   const std::string& key, const std::string& value)

	{
		// retrieve all current values for key
		std::pair<StringDictionary::iterator, StringDictionary::iterator>
			result_pair = dict.equal_range(key);
		if (result_pair.first == dict.end()) {
			// no values exist -> add a new key
			dict.insert(std::make_pair(key, value));
		} else {
			// set the first value found for the key to the new one
			result_pair.first->second = value;
			// remove any remaining values
			StringDictionary::iterator i;
			++(result_pair.first);
			while (result_pair.first != result_pair.second) {
				i = result_pair.first;
				++(result_pair.first);
				dict.erase(i);
			}
		}
	}
	
	/**
	 * Deletes all values for a key
	 *
	 * @param dict the dictionary object to update
	 * @param key the key to delete
	 */
	inline static void deleteValue(StringDictionary& dict,
								   const std::string& key)
	{
		std::pair<StringDictionary::iterator, StringDictionary::iterator>
			result_pair = dict.equal_range(key);
		if (result_pair.first != dict.end())
			dict.erase(result_pair.first, result_pair.second);
	}

	
	/// returns a string containing the first line for the HTTP message
	virtual const std::string& getFirstLine(void) const = 0;

	
private:
	
	/// True if the HTTP message is valid
	bool							m_is_valid;

	/// true if chunked transfer encodings are supported
	bool							m_chunks_supported;
	
	/// IP address of the remote endpoint
	boost::asio::ip::address		m_remote_ip;

	/// HTTP major version number
	unsigned int					m_version_major;

	/// HTTP major version number
	unsigned int					m_version_minor;
	
	/// the length of the payload content (in bytes)
	size_t							m_content_length;

	/// whether the message body is chunked
	bool							m_is_chunked;

	/// the payload content, if any was sent with the message
	boost::scoped_array<char>		m_content_buf;

	/// buffers for holding chunked data
	ChunkCache						m_chunk_buffers;
	
	/// HTTP message headers
	Headers							m_headers;
};


}	// end namespace net
}	// end namespace pion

#endif