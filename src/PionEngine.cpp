// -----------------------------------------------------------------
// libpion: a C++ framework for building lightweight HTTP interfaces
// -----------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.
// 
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//

#include <libpion/PionEngine.hpp>
#include <boost/bind.hpp>


namespace pion {	// begin namespace pion


// static members of PionEngine
const unsigned int		PionEngine::DEFAULT_NUM_THREADS = 5;
PionEngine *			PionEngine::m_instance_ptr = NULL;
boost::once_flag		PionEngine::m_instance_flag = BOOST_ONCE_INIT;


// PionEngine member functions

void PionEngine::createInstance(void)
{
	static PionEngine pion_instance;
	m_instance_ptr = &pion_instance;
}

void PionEngine::start(void)
{
	// check for errors
	if (m_is_running) throw AlreadyStartedException();
	if (m_servers.empty()) throw NoServersException();

	// lock mutex for thread safety
	boost::mutex::scoped_lock engine_lock(m_mutex);

	PION_LOG_INFO(m_logger, "Starting up");

	// schedule async tasks to listen for each server
	for (TCPServerMap::iterator i = m_servers.begin(); i!=m_servers.end(); ++i) {
		i->second->start();
	}

	// start multiple threads to handle async tasks
	for (unsigned int n = 0; n < m_num_threads; ++n) {
		boost::shared_ptr<boost::thread> new_thread(new boost::thread( boost::bind(&PionEngine::run, this) ));
		m_thread_pool.push_back(new_thread);
	}

	m_is_running = true;
}

void PionEngine::stop(void)
{
	// lock mutex for thread safety
	boost::mutex::scoped_lock engine_lock(m_mutex);

	if (m_is_running) {

		PION_LOG_INFO(m_logger, "Shutting down");

		// stop listening for new connections
		for (TCPServerMap::iterator i = m_servers.begin(); i!=m_servers.end(); ++i) {
			i->second->stop();
		}

		if (! m_thread_pool.empty()) {
			PION_LOG_DEBUG(m_logger, "Waiting for threads to shutdown");

			// wait until all threads in the pool have stopped
			std::for_each(m_thread_pool.begin(), m_thread_pool.end(),
						  boost::bind(&boost::thread::join, _1));

			// clear the thread pool (also deletes thread objects)
			m_thread_pool.clear();
		}

		PION_LOG_INFO(m_logger, "Pion has shutdown");

		m_is_running = false;
		m_engine_has_stopped.notify_all();
	}
}

void PionEngine::join(void)
{
	boost::mutex::scoped_lock engine_lock(m_mutex);
	if (m_is_running) {
		// sleep until engine_has_stopped condition is signaled
		m_engine_has_stopped.wait(engine_lock);
	}
}

void PionEngine::run(void)
{
	try {
		// handle I/O events managed by the service
		m_asio_service.run();
	} catch (std::exception& e) {
		PION_LOG_FATAL(m_logger, "Caught exception in pool thread: " << e.what());
	}
}

bool PionEngine::addServer(TCPServerPtr tcp_server)
{
	// lock mutex for thread safety
	boost::mutex::scoped_lock engine_lock(m_mutex);
	
	// attempt to insert tcp_server into the server map
	std::pair<TCPServerMap::iterator, bool> result =
		m_servers.insert( std::make_pair(tcp_server->getPort(), tcp_server) );

	return result.second;
}

HTTPServerPtr PionEngine::addHTTPServer(const unsigned int tcp_port)
{
	HTTPServerPtr http_server(HTTPServer::create(tcp_port));

	// lock mutex for thread safety
	boost::mutex::scoped_lock engine_lock(m_mutex);
	
	// attempt to insert http_server into the server map
	std::pair<TCPServerMap::iterator, bool> result =
		m_servers.insert( std::make_pair(tcp_port, http_server) );

	if (! result.second) http_server.reset();
	
	return http_server;
}

TCPServerPtr PionEngine::getServer(const unsigned int tcp_port)
{
	// lock mutex for thread safety
	boost::mutex::scoped_lock engine_lock(m_mutex);
	
	// check if a server already exists
	TCPServerMap::iterator i = m_servers.find(tcp_port);
	
	return (i==m_servers.end() ? TCPServerPtr() : i->second);
}

}	// end namespace pion