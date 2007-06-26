/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "user.h"

#include "session_data.h" // SessionData*

#include "../shared/protocol.flags.h"
#include "../shared/protocol.defaults.h"
#include "../shared/protocol.h"

#include "../shared/templates.h"

typedef std::map<octet, SessionData*>::iterator usr_session_i;
typedef std::map<octet, SessionData*>::const_iterator usr_session_const_i;

typedef std::deque<message_ref>::iterator usr_message_i;
typedef std::deque<message_ref>::const_iterator usr_message_const_i;

User::User(const octet _id, const Socket& nsock) throw()
	: sock(nsock),
	session(0),
	id(_id),
	events(0),
	state(Init),
	layer(protocol::null_layer),
	syncing(protocol::Global),
	isAdmin(false),
	// client caps
	c_acks(false),
	// extensions
	ext_deflate(false),
	ext_chat(false),
	ext_palette(false),
	// other
	inMsg(0),
	level(0),
	deadtime(0),
	session_data(0),
	strokes(0)
{
	#if defined(DEBUG_USER) and !defined(NDEBUG)
	std::cout << "User::User(" << static_cast<int>(_id)
		<< ", " << sock.fd() << ")" << std::endl;
	#endif
	assert(_id != protocol::null_user);
}

User::~User() throw()
{
	#if defined(DEBUG_USER) and !defined(NDEBUG)
	std::cout << "User::~User()" << std::endl;
	#endif
	
	delete inMsg;
	
	for (usr_session_i usi(sessions.begin()); usi != sessions.end(); ++usi)
		delete usi->second;
}

bool User::makeActive(octet session_id) throw()
{
	session_data = getSession(session_id);
	if (session_data != 0)
	{
		session = session_data->session;
		return true;
	}
	else
		return false;
}

SessionData* User::getSession(octet session_id) throw()
{
	const usr_session_const_i usi(sessions.find(session_id));
	return (usi == sessions.end() ? 0 : usi->second);
}

const SessionData* User::getConstSession(octet session_id) const throw()
{
	const usr_session_const_i usi(sessions.find(session_id));
	return (usi == sessions.end() ? 0 : usi->second);
}

void User::cacheTool(protocol::ToolInfo* ti) throw(std::bad_alloc)
{
	assert(ti != session_data->cachedToolInfo); // attempted to re-cache same tool
	assert(ti != 0);
	
	#if defined(DEBUG_USER) and !defined(NDEBUG)
	std::cout << "Caching Tool Info for user #" << static_cast<int>(id) << std::endl;
	#endif
	
	delete session_data->cachedToolInfo;
	session_data->cachedToolInfo = new protocol::ToolInfo(*ti); // use copy-ctor
}

octet User::getCapabilities() const throw()
{
	return (c_acks?(protocol::client::AckFeedback):0);
}

void User::setCapabilities(const octet flags) throw()
{
	c_acks = fIsSet(flags, static_cast<octet>(protocol::client::AckFeedback));
}

octet User::getExtensions() const throw()
{
	return (ext_deflate?(protocol::extensions::Deflate):0)
		+ (ext_chat?(protocol::extensions::Chat):0)
		+ (ext_palette?(protocol::extensions::Palette):0);
}

void User::setExtensions(const octet flags) throw()
{
	ext_deflate = fIsSet(flags, static_cast<octet>(protocol::extensions::Deflate));
	ext_chat = fIsSet(flags, static_cast<octet>(protocol::extensions::Chat));
	ext_palette = fIsSet(flags, static_cast<octet>(protocol::extensions::Palette));
}

void User::flushQueue()
{
	assert(!queue.empty());
	
	const usr_message_i f_msg(queue.begin());
	usr_message_i l_msg(f_msg+1), iter(f_msg);
	// create linked list
	size_t links=1;
	for (; l_msg != queue.end(); ++l_msg, ++iter, ++links)
	{
		if (links == std::numeric_limits<octet>::max()
			or ((*l_msg)->type != (*f_msg)->type)
			or ((*l_msg)->user_id != (*f_msg)->user_id)
			or ((*l_msg)->session_id != (*f_msg)->session_id)
		)
			break; // type changed or reached maximum size of linked list
		
		(*l_msg)->prev = boost::get_pointer(*iter);
		(*iter)->next = boost::get_pointer(*l_msg);
	}
	
	size_t len=0, size=output.canWrite();
	
	// serialize message/s
	char* buf = (*--l_msg)->serialize(len, output.wpos, size);
	
	// in case new buffer was allocated
	if (buf != output.wpos)
		output.setBuffer(buf, size, len);
	else
		output.write(len);
	
	queue.erase(f_msg, ++l_msg);
}
