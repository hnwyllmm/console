#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <sstream>
#include "console.h"

using namespace std ;

///////////////////////////////////////////////////////////////////////////////
// 
int	trace_flag = 1 ;
FILE *g_trace_file = NULL ;
const char *trace_file = "console.log" ;

static void trace(const char *format, ...)
{
	{
		static int __init__ = (g_trace_file = fopen(trace_file, "wb+"), 1) ;
	}

	if (!trace_flag)
		return ;

	va_list ap;
	va_start(ap, format);
	vfprintf(g_trace_file, format, ap) ;
	va_end(ap) ;
	fflush(g_trace_file) ;
}

static int isspecial(int c)
{
	return (unsigned)c > 255 ;
}

static void string_split(const string &str, stringlist &liststr)
{
	const int size = str.size() ;
	string tmp ;
	for (int i = 0 ; i < size ; i++)
	{
		if (isspace(str[i]))
		{
			if (!tmp.empty())
				liststr.push_back(tmp) ;

			tmp.clear() ;
		}
		else
			tmp.push_back(str[i]) ;
	}
	if (!tmp.empty())
		liststr.push_back(tmp) ;
}

static bool isempty(const std::string &s)
{
	const int size = s.size() ;
	for (int i = 0 ; i < size ; i++)
		if (isgraph(s[i]))
			return false ;
	return true ;
}

///////////////////////////////////////////////////////////////////////////////
// complete helper
complete_helper::complete_helper(const string &prefix /* = string() */) 
	: m_list_stable_option(NULL),
	m_prefix(prefix)
{
}

void complete_helper::complete()
{
	const int len = m_prefix.size() ;

	if (m_list_stable_option)
	{
		stringlist::const_iterator itstr = m_list_stable_option->begin() ;
		stringlist::const_iterator itend = m_list_stable_option->end() ;
		for ( ; itstr != itend ; ++itstr)
		{
			if (0 == strncmp(m_prefix.c_str(), itstr->c_str(), len))
				m_list_result.push_back(*itstr) ;
		}
	}

	stringlist::const_iterator itstr = m_list_dynamic_option.begin() ;
	stringlist::const_iterator itend = m_list_dynamic_option.end() ;
	for ( ; itstr != itend ; ++itstr)
	{
		if (0 == strncmp(m_prefix.c_str(), itstr->c_str(), len) )
			m_list_result.push_back(*itstr) ;
	}
}

void complete_helper::conclusion( string &match) const
{
	if (m_list_result.empty())
	{
		match.clear() ;
		return ;
	}
	if (1 == m_list_result.size())
	{
		match = m_list_result.front() ;
		return ;
	}

	match.clear() ;

	const int size = m_list_result.front().size() ;
	bool over = false ;
	for (int i = 0 ; i < size ; i++)
	{
		const char c = m_list_result.front()[i] ;
		stringlist::const_iterator itstr = ++m_list_result.begin() ;
		stringlist::const_iterator itend = m_list_result.end() ;
		for ( ; itstr != itend ; ++itstr)
		{
			const string &str = *itstr ;
			if (str.size() <= i || c != str[i])
			{
				over = true ;
				break ;
			}
		}
		if (over)
			break ;

		match.push_back(c) ;
	}

}
///////////////////////////////////////////////////////////////////////////////
// command_handler
command_handler::command_handler(console *c) : m_console(c)
{
}

void command_handler::add_cmd(const char *cmd)
{
	m_cmdlist.push_back(cmd) ;
}

bool command_handler::receive(const char *cmd) const
{
	stringlist::const_iterator itcmd = m_cmdlist.begin() ;
	stringlist::const_iterator itend = m_cmdlist.end() ;
	for ( ; itcmd != itend ; ++itcmd)
	{
		if (0 == strcmp(itcmd->c_str(), cmd))
			return true ;
	}
	return false ;
}
void command_handler::complete(complete_helper &comp) const
{
	comp.set_option(&m_cmdlist) ;
	comp.complete() ;
}

///////////////////////////////////////////////////////////////////////////////
// functions to help

///////////////////////////////////////////////////////////////////////////////
// exit
class command_exit : public command_handler
{
public:
	command_exit(console *c) : command_handler(c)
	{
		add_cmd("exit") ;
		add_cmd("quit") ;
		add_cmd("bye") ;
	}

	int handle(string &command, command_args &args, std::list<std::string> &output)
	{
		if (!receive(args.front().c_str()))
			return CMD_NOT_HANDLED;
		
		get_console()->stop() ;
		return CMD_SUCCESS ;
	}

	void help(const command_args &args, helplist_t &output) const
	{
		if (!args.empty() && !receive(args.front().c_str()))
			return ;
		
		const static helptext_t exit(string("exit"),
				string("exit console") ) ;
		const static helptext_t quit(string("quit"),
				string("exit console") ) ;
		const static helptext_t bye(string("bye"),
				string("exit console") ) ;

		output.push_back(exit) ;
		output.push_back(quit) ;
		output.push_back(bye) ;
		return ;
	}

};

///////////////////////////////////////////////////////////////////////////////
// command history
class command_history : public command_handler
{
public:
	command_history(console *c) : command_handler(c)
	{
		add_cmd("!!") ;
		add_cmd("history") ;
	}

	int handle(string &command, command_args &args, stringlist &output)
	{
		bool rehandle = false ;

again:
		const std::string &cmd = args.front() ;
		if (0 == strcmp(cmd.c_str(), "history"))
		{
			if (false == rehandle)
				add_newcmd(command) ;
			return handle_history(args, output) ;
		}

		if ('!' != cmd[0])
		{
			add_newcmd(command) ;
			return CMD_NOT_HANDLED ;
		}

		if (0 == strncmp("!!", cmd.c_str(), 2))
		{
			if (m_cmd_history.empty())
				return CMD_DISCARD ;

			string oldcmd = m_cmd_history.back() ;
			stringlist cmdarg ;
			string_split(oldcmd, cmdarg) ;
			bool isnew = false ;
			if (cmd.size() > 2)
			{
				cmdarg.back().append(cmd.c_str() + 2) ;
			}
			if (args.size() > 1)
			{
				cmdarg.insert(cmdarg.end(), ++args.begin(), args.end()) ;
			}
			if (command.size() > 2)
			{
				oldcmd.append(command.c_str() + 2) ;	
				this->add_newcmd(oldcmd) ;
			}
			args = cmdarg ;
			command = oldcmd ;
			
			output.push_back(oldcmd) ;
			rehandle = true ;
			goto again ;
		}

		string *pcmd = NULL ;
		if (isdigit(*(cmd.c_str() + 1)))
		{
			const int index = atoi(cmd.c_str() + 1) ;
			list<int>::iterator itidx = m_cmd_index.begin() ;
			list<int>::iterator itend = m_cmd_index.end() ;
			list<string>::iterator itcmd = m_cmd_history.begin() ;
			for ( ; itidx != itend ; ++itidx, ++itcmd)
			{
				if (index == *itidx)
				{
					pcmd = &(*itcmd) ;
					break ;
				}
			}
		}
		else
		{
			const char *prefix = args.front().c_str() + 1 ;
			const int len = args.front().size() - 1 ;
			stringlist::reverse_iterator itcmd = m_cmd_history.rbegin() ;
			stringlist::reverse_iterator itend = m_cmd_history.rend() ;
			for ( ; itcmd != itend ; ++itcmd)
			{
				if (0 == strncmp(prefix, (*itcmd).c_str(), len) )
				{
					pcmd = &(*itcmd) ;
					break ;
				}
			}
		}

		if (pcmd)
		{
			if (args.size() == 1)
			{
				command = *pcmd ;
				args.clear() ;
				string_split(command, args) ;
			}
			else
			{
				string tmp = *pcmd ;
				const int size = command.size() ;
				int i = 0 ;
				for ( ; i < size &&  isspace(command[i]) ; i++) ;
				for ( ; i < size && !isspace(command[i]) ; i++) ;
				tmp.append(command, i, string::npos) ;
				command = tmp ;

				stringlist tmpargs ;
				string_split(command, tmpargs) ;
				tmpargs.splice(tmpargs.end(), args, ++args.begin(), args.end()) ;
				args.swap(tmpargs) ;
			}

			output.push_back(command) ;
			rehandle = true ;
			goto again ;
		}
		else
		{
			string tmp(command) ;
			tmp.append(": event not found") ;
			output.push_back(tmp) ;
		}
			
		return CMD_DISCARD ;
	}

	void get_last_command(int &index, string &command) const
	{
		command.clear() ;

		if (index < 0)
			index = m_cmd_history.size() - 1 ;

		if (index >= 0 && index < m_cmd_history.size())
			get_command(index, command) ;

		index-- ;

		if (index < 0)
			index = m_cmd_history.size() ;
	}

	void get_next_command(int &index, string &command) const
	{
		command.clear() ;

		if (index < 0)
			index = 0 ;

		if (index >= m_cmd_history.size())
		{
			index = 0 ;
			return ;
		}

		const bool ret = get_command(index, command) ;
		if (ret)
			index++ ;
	}

	void help(const command_args &args, helplist_t &output) const
	{
		if (!args.empty() && !receive(args.begin()->c_str()))
			return ;

		const static helptext_t history(string("history"), 
				string("print history command") ) ;
		const static helptext_t double_exclamation(string("!!"), 
				string("execute last command") ) ;
		const static helptext_t exclamation_num(string("!\'index\'"),
				string("execute history command specified by index") );
		output.push_back(history) ;
		output.push_back(double_exclamation) ;
		output.push_back(exclamation_num) ;
	}

private:
	int  handle_history(command_args &args, stringlist &output)
	{
		list<string>::const_iterator itcmd = m_cmd_history.begin() ;
		list<string>::const_iterator itend = m_cmd_history.end() ;
		list<int>::const_iterator    itidx = m_cmd_index.begin() ;
		string tmp ;
		stringstream ss ;
		for ( ; itcmd != itend ; ++itcmd, ++itidx)
		{
			tmp.clear() ;
			ss.str(tmp) ;
			ss << *itidx << " " << *itcmd;

			tmp = ss.str() ;
			output.push_back(tmp) ;
		}
		return CMD_SUCCESS ;
	}

	bool get_command(int index, string &command) const
	{
		if (index < 0 || index >= m_cmd_history.size())
			return false ;

		list<string>::const_iterator itcmd = m_cmd_history.begin() ;
		list<string>::const_iterator itend = m_cmd_history.end() ;
		for (int i = 0 ; i < index && itcmd != itend ; ++i, ++itcmd) ;

		command = *itcmd ;
		return true ;
	}

	void contact(const stringlist &strlist, string &res, const char *fill = " ") const
	{
		stringlist::const_iterator itstr = strlist.begin() ;
		stringlist::const_iterator itend = strlist.end() ;
		if (false == strlist.empty())
			res.append(*itstr) ;

		for ( ++itstr ; itstr != itend ; ++itstr)
		{
			res.append(fill) ;
			res.append(*itstr) ;
		}
	}
private:
	void add_newcmd(const string &command)
	{
		m_cmd_history.push_back(command) ;
		const int index = m_cmd_index.empty() ? 1 : m_cmd_index.back() + 1;
		m_cmd_index.push_back(index) ;
	}
private:
	list<int>			m_cmd_index ;
	list<string>		m_cmd_history ;
};

///////////////////////////////////////////////////////////////////////////////
// command_help
class command_help : public command_handler
{
public:
	command_help(console *c) : command_handler(c)
	{
		add_cmd("help") ;
	}

	int handle(string &command, command_args &args, std::list<std::string> &output)
	{
		if (!receive(args.begin()->c_str()))
			return CMD_NOT_HANDLED ;

		const std::list<command_handler *> &list_handler = 
				get_console()->get_handler_list() ;			
		command_args tmp(++args.begin(), args.end()) ;
		std::list<command_handler *>::const_iterator itcmd = list_handler.begin() ;
		std::list<command_handler *>::const_iterator itend = list_handler.end() ;
		helplist_t helplist ;
		for ( ; itcmd != itend ; ++itcmd)
		{
			(*itcmd)->help(tmp, helplist) ;
		}

		int max_cmd = 4 ;
		helplist_t::const_iterator it_hlp = helplist.begin() ;
		helplist_t::const_iterator it_end = helplist.end() ;
		for ( ; it_hlp != it_end ; ++it_hlp)
		{
			const std::string &cmd = it_hlp->first ;
			if (cmd.size() >= max_cmd)
				max_cmd = cmd.size() + 4 ;
		}

		std::string helpinfo ;
		std::string strtmp ;
		it_hlp = helplist.begin() ;
		for ( ; it_hlp != it_end ; ++it_hlp)
		{
			const std::string &cmd = it_hlp->first ;
			const std::string &info = it_hlp->second ;
			
			strtmp = print_fixlen(cmd.c_str(), max_cmd) ;
			helpinfo.append(strtmp) ;
			helpinfo.append(info) ;
			output.push_back(helpinfo) ;
			helpinfo.clear() ;
		}
		return CMD_SUCCESS ;
	}

	void help(const command_args &args, helplist_t &output) const
	{
		if (!args.empty() && !receive(args.begin()->c_str()))
			return ;

		const static helptext_t helptext(std::string("help"), 
				std::string("print this text")) ;
		const static helptext_t helpcmd(std::string("help `command`"), 
				std::string("print specific command information")) ;
		output.push_back(helptext) ;
		output.push_back(helpcmd) ;
	}

private:
	std::string print_fixlen(const char *s, 
			const int len, 
			const char fill = ' ', 
			const int flag = 0	// the unique value supported
			)
	{
		std::string tmp ;
		tmp.reserve(len) ;
		tmp = s ;
		const int strlen = tmp.size() ;
		tmp.resize(len) ;
		char *p = (char *)tmp.data() + strlen ;
		for (int i = strlen ; i < len ; i++)
		{
			*p++ = fill ;
		}
		return tmp ;
	}
};

///////////////////////////////////////////////////////////////////////////////
// class command_shell
class command_shell : public command_handler
{
public:
	command_shell(console *c) : command_handler(c)
	{
		add_cmd("sh") ;
		add_cmd("shell") ;
	}

	int handle(string &command, command_args &args, stringlist &output)
	{
		if (!receive(args.front().c_str()))
			return CMD_NOT_HANDLED ;

		if (args.size() < 2)
		{
			output.push_back("invalid argument") ;
			return CMD_DISCARD ;
		}

		int i = 0 ;
		const int size = command.size() ;
		for ( ; i < size &&  isspace(command[i]) ; i++) ;
		for ( ; i < size && !isspace(command[i]) ; i++) ;
		for ( ; i < size &&  isspace(command[i]) ; i++) ;

		string tmp("\rexecute `") ;
		tmp.append(command.c_str() + i) ;
		tmp.append("` ") ;
		FILE *fsh = popen(command.c_str() + i, "r") ;
		trace("popen return %p, errno is %d, errmsg is %s\r\n", 
				fsh, errno, strerror(errno)) ;

		if (!fsh)
		{
			tmp.append("failure") ; 
		}
		else
		{
			tmp.append("success\r\n") ;
			while (!feof(fsh))
			{
				const char c = (char)fgetc(fsh) ;
				if ('\n' == c)
					tmp.push_back('\r') ;
				tmp.push_back(c) ;
			}

			pclose(fsh) ;
		}
		output.push_back(tmp) ;
		return CMD_SUCCESS ;
	}

	void help(const command_args &args, helplist_t &output) const
	{
		if (!args.empty() && !receive(args.front().c_str()))
			return ;

		const static helptext_t sh(string("sh"),
                string("execute shell command") ) ;
        const static helptext_t shell(string("shell"),
                string("execute shell command") ) ;

		if (args.empty() || 0 == strcmp(args.front().c_str(), "sh"))
			output.push_back(sh) ;

		if (args.empty() || 0 == strcmp(args.front().c_str(), "shell"))
			output.push_back(shell) ;
        return ;
	}
};

///////////////////////////////////////////////////////////////////////////////
// class command_record
class command_record : public command_handler
{
public:
	command_record(console *c) : command_handler(c), m_fd(-1)
	{
		add_cmd("record") ;
	}
	~command_record()
	{
		if (m_fd > 0)
		{
			close(m_fd) ;
			m_fd = -1 ;
		}
	}

	int handle(string &command, command_args &args, stringlist &output)
	{
		if (!receive(args.front().c_str()))
		{
			if (m_fd < 0 && m_filename.empty())
			{
				return CMD_NOT_HANDLED ;
			}

			if (m_fd < 0)
			{
				m_fd = open(m_filename.c_str(), O_CREAT|O_WRONLY, 0644) ;
				if (m_fd < 0)
				{
					trace("open file failure: %d %s, filename %s\r\n",
							errno, strerror(errno), m_filename.c_str()) ;
					return CMD_NOT_HANDLED ;
				}
				lseek(m_fd, 0, SEEK_END) ;
			}

			stringlist::const_iterator itstr = output.begin() ;
			stringlist::const_iterator itend = output.end() ;
			for ( ; itstr != itend ; ++itstr)
			{
				dprintf(m_fd, "%s\r\n", itstr->c_str()) ;
			}

			return CMD_NOT_HANDLED ;
		}

		static const char *default_log = "record.dat" ;
		const char *param = default_log ;
		if (args.size() > 1)
			param = (*++args.begin()).c_str() ;

		if (0 == strcasecmp("off", param))
		{
			if (m_fd > 0)
			{
				close(m_fd) ;
				m_fd = -1 ;
			}
		}
		else if (0 == strcasecmp("on", param))
		{
			if (m_fd > 0)
				return CMD_SUCCESS ;

			if (m_filename.empty())
				m_filename = default_log ;
		}
		else
		{
			if (m_fd > 0 && 0 != strcmp(m_filename.c_str(), param))
			{
				close(m_fd) ;
				m_fd = -1 ;
				m_filename = param ;
			}
			if (0 != strcmp(m_filename.c_str(), param))
				m_filename = param ;
		}

		return CMD_SUCCESS ;
	}
	void help(const command_args &args, helplist_t &output) const
	{
		if (!args.empty() && !receive(args.front().c_str()))
			return ;

		static helptext_t record(string("record"),
				string("set record file") ) ;
		output.push_back(record) ;
	}

private:
	int		m_fd ;
	string	m_filename ;
};
///////////////////////////////////////////////////////////////////////////////
// class command_normal
command_normal::command_normal(console *c,
		const char *cmd,
		const command_callback &cb,
		const char *helptext /*= "" */)
	: command_handler(c),
	m_cb(cb),
	m_helptext(string(cmd), string(helptext ? helptext : ""))
{
	add_cmd(cmd) ;
}

int command_normal::handle(std::string &command, command_args &args, stringlist &output)
{
	if (!receive(args.front().c_str()))
		return CMD_NOT_HANDLED ;

	const bool res = m_cb(args, output) ;
	return res ? CMD_SUCCESS : CMD_DISCARD ;
}

void command_normal::help(const command_args &args, helplist_t &output) const
{
	if (!args.empty() && !receive(args.front().c_str()))
		return ;

	output.push_back(m_helptext) ;
}
///////////////////////////////////////////////////////////////////////////////
//
class command_echo : public command_callback
{
public:
	command_echo(console *c) : m_console(c)
	{}

	virtual bool operator()(const command_args &args, stringlist &output) const
	{
		const char *param = "on" ;
		if (args.size() > 1)
			param = (*++args.begin()).c_str() ;

		if (0 == strcasecmp("off", param))
		{
			m_console->set_echo(false) ;
			output.push_back("echo off") ;
		}
		else
		{
			m_console->set_echo(true) ;
			output.push_back("echo on") ;
		}

		return true ;
	}
private:
	console *m_console ;
};

class command_register_helper : public command_callback
{
public:
	command_register_helper(callback_t cb) :
		m_cb(cb)
	{
	}
	bool operator()(const command_args &args, stringlist &output) const
	{
		return m_cb(args, output) ;
	}

private:
	callback_t	m_cb ;
};

///////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------------------
console::console() :
	m_output(stdout),
	m_input(NULL),
	m_running(false),
	m_cmd_history(NULL),
	m_echo(true)
{
	m_not_supported = "command can not be handled, text \'help\' "
		"to get supported commands\r\n" ;

	m_cmd_history = new command_history(this) ;
	m_cmd_handlers.push_back(m_cmd_history) ;
	m_cmd_handlers.push_back(new command_exit(this)) ;
	m_cmd_handlers.push_back(new command_help(this)) ;
	m_cmd_handlers.push_back(new command_shell(this)) ;

	static command_echo cmdecho(this) ;
	m_cmd_handlers.push_back(new command_normal(this, "echo", cmdecho, "set echo on/off")) ;

	m_cmd_handlers.push_back(new command_record(this)) ;
	m_itr_postproc = --m_cmd_handlers.end() ;

	set_input(stdin) ;
}

console::~console()
{
	std::list<command_handler *>::iterator itcmd = m_cmd_handlers.begin() ;
	std::list<command_handler *>::iterator itend = m_cmd_handlers.end() ;
	for ( ; itcmd != itend ; ++itcmd)
	{
		delete *itcmd ;
	}

	if (m_output)
		fclose(m_output) ;
	if (m_input)
	{
		if (stdin == m_input)
			tcsetattr(fileno(m_input), TCSANOW, &m_input_bakter) ;

		fclose(m_input) ;
	}
}

int console::set_input(FILE *input)
{
	if (input != m_input)
	{
		if (m_input)
		{
			if (stdin == m_input)
				tcsetattr(fileno(m_input), TCSANOW, &m_input_bakter) ;
			else
				fclose(m_input) ;
		}
		m_input = input ;
		if (stdin != m_input)
			return 0 ;

		const int fd = fileno(m_input) ;
		tcgetattr(fd, &m_input_bakter);
		struct termios newter = m_input_bakter ;
		/*
		newter.c_lflag &= ~(ECHO) ;
		newter.c_lflag |= IGNCR ;
		newter.c_lflag &= ~ICANON ;
		newter.c_cc[VMIN] = 1 ;
		//newter.c_lflag |= ECHOE|ICANON ;
		*/
		//newter.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
		//		                | INLCR | IGNCR | ICRNL | IXON);
		newter.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
				                | INLCR | IGNCR | ICRNL | IXON);
		newter.c_oflag &= ~OPOST;
		//newter.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
		newter.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
		newter.c_cflag &= ~(CSIZE | PARENB);
		newter.c_cflag |= CS8;

		tcsetattr(fd, TCSANOW, &newter) ;

		setbuf(m_input, NULL) ;
	}
	return 0 ;
}

int console::set_input(const char *file)
{
	FILE *fp = fopen(file, "rb") ;
	if (!fp)
	{
		trace("[set_input] can not open file: %s\r\n", file) ;
		return -1 ;
	}
	return set_input(fp) ;
}

bool console::start()
{
	m_running = true ;
	return run() ;
}

void console::stop()
{
	m_running = false ;
}

bool console::run()
{
	outputln(m_header) ;

	command_args args ;
	command_handler *handler ;
	stringlist output ;
	string command ;

	while (m_running)
	{
		args.clear() ;
		if (false == read_command(command) )
			goto out ;

		string_split(command, args) ;
		if (args.empty() )
			continue ;

		bool handled = false ;
		output.clear() ;
		std::list<command_handler *>::iterator it_cmd = m_cmd_handlers.begin() ;
		std::list<command_handler *>::iterator it_end = m_cmd_handlers.end() ;
		for ( ; it_cmd != it_end ; )
		{
			command_handler *handler = *it_cmd ;

			const int ret = handler->handle(command, args, output) ;
			if (command_handler::CMD_REHANDLE == ret)
			{
				this->output(output) ;
				output.clear() ;
				handled = false ;
				it_cmd = m_cmd_handlers.begin() ;
				continue ;
			}

			if (false == handled && command_handler::CMD_NOT_HANDLED != ret )
				handled = true ;

			if (command_handler::CMD_EXIT == ret)
				goto out ;

			if (command_handler::CMD_DISCARD == ret)
				break ;

			++it_cmd ;
		}

		this->output(output) ;

		if (false == handled)
		{
			this->output(m_not_supported) ;
		}
	}

out:
	m_running = false ;

	outputln(m_exit_text) ;
	return true ;
}

int console::read_char(int fd) const
{
	int c ;
	while (true)
	{
		const int res = read(STDIN_FILENO, &c, 1) ;
		if (res < 0)
			if (errno != EAGAIN)
				return 0 ;
			else
				continue ;

		if (0 == res)
			continue ;
	}
}

bool console::read_command(string &command, 
		const char *title /*= NULL */,
		char echo /* = -1 */, 
		command_handler *handler /* = NULL */) const
{
	const int KEY_CTRLC		= 3 ;			// Ctrl + C
	const int KEY_BACK 		= 8;
	const int KEY_UP		= 0x415b1b ;
	const int KEY_DOWN		= 0x425b1b ;
	const int KEY_LEFT		= 0x445b1b ;
	const int KEY_RIGHT		= 0x435b1b ;

	command.clear() ;
	output('\r') ;
	string supp ;

	string output_buf ;

	char buf ;
	//setvbuf(m_input, &buf, _IOFBF, 1) ;
	int fd = fileno(m_input) ;
	int history_index = -1 ;
	bool read_ok = true ;
	const bool is_stdin = m_input == stdin ;

	int cursor_pos = command.size() ;
	if (is_stdin)
	{
		if (title)
			output(title) ;
		else
			output(m_title) ;
	}

	while (true)
	{
		output_buf.clear() ;

		bool over = false ;
		bool can_replace = true ;

	//	int c = fgetc(m_input) ;
		int c = 0 ;
		const int readlen = is_stdin ? sizeof(c) : 1 ;
		const int res = read(fd, &c, readlen) ;
		if (EOF == c)
		{
			trace("got a EOF\r\n") ;
			if (false == isempty(command))
				read_ok = false ;

			goto out ;
		}

		if (res <= 0 && errno != EAGAIN)
			goto out ;
		
		if (isspecial(c))
		{
			trace("read_command: special char\r\n") ;
			if (echo >= 0)	// no echo or special char
				goto output_char ;

			if (handler)
				goto output_char ;

			if (!is_stdin)
				goto output_char ;

			switch(c)
			{
			case KEY_UP:
				m_cmd_history->get_last_command(history_index, supp) ;	
				break ;
			case KEY_DOWN:
				m_cmd_history->get_next_command(history_index, supp) ;
				break ;
			case KEY_LEFT:
				if (cursor_pos > 0)
				{
					cursor_pos-- ;
					output_buf.push_back(KEY_BACK) ;
				}
				break ;
			case KEY_RIGHT:
				if (cursor_pos < command.size() )
				{
					if (cursor_pos == command.size())
						output_buf.push_back(' ') ;
					else
						output_buf.push_back(command[cursor_pos]) ;
					cursor_pos++ ;
				}
				
				break ;
			}

			if (KEY_UP == c || KEY_DOWN == c)
			{
				// clear first, then output history command
				const int size = command.size() ;
				output_buf.append(cursor_pos, KEY_BACK) ;
				output_buf.append(size, ' ') ;
				output_buf.append(size, KEY_BACK) ;
				command = supp ;
				output_buf.append(command) ;
				cursor_pos = command.size() ;
			}

			goto output_char ;
		}
		else
			history_index = -1 ;

		if (KEY_CTRLC == c)
		{
			trace("read_command: ctrl+c\r\n") ;
			if (!is_stdin)
				goto output_char ;

			if (false == command.empty() && echo < 0)
			{
				read_ok = false ;
			}
				
			command.clear() ;
			output_buf.append("\r\n") ;
			can_replace = false ;

			over = true ;
			goto output_char ;
		}

		if (KEY_BACK == c)
		{
			trace("read_command: back space, cursor_pos is %d\r\n", cursor_pos) ;
			if (!is_stdin)
				goto output_char ;

			if (cursor_pos > 0)
			{
				can_replace = false ;
				assert(false == command.empty()) ;
				command.erase(--cursor_pos, 1) ;
				trace("command is \"%s\"\r\n", command.c_str());
				output_buf.append(1, KEY_BACK) ;
				output_buf.append(command, cursor_pos, string::npos) ; 
				output_buf.append(1, ' ') ;
				output_buf.append(command.size() - cursor_pos + 1, KEY_BACK) ;
			}

			goto output_char ;
		}

		if ('\r' == c || '\n'== c)
		{
			trace("read_command: key return\r\n") ;
			over = true ;
			can_replace = false ;

			output_buf.append("\r\n") ;
			goto output_char ;
		}

		if ('\t' == c)
		{
			trace("read_command: table, command is \"%s\"\r\n", command.c_str()) ;
			bool has_space ;
			int size ;
			int i = 0 ;
			std::list<command_handler *>::const_iterator itcmd, itend ;
			string match ;
			complete_helper comp(command) ;

			if (echo >= 0)
				goto output_char ;

			if (handler)
				goto output_char ;

			if (!is_stdin)
				goto output_char ;

			has_space = false ;
			size = command.size() ;
			i = 0 ;
			for ( ; i < size &&  isspace(command[i]) ; i++) ;
			for ( ; i < size && !isspace(command[i]) ; i++) ;
			if (i < size)
				goto output_char ;

			itcmd = m_cmd_handlers.begin() ;
			itend = m_cmd_handlers.end() ;
			for ( ; itcmd != itend ; ++itcmd)
			{
				(*itcmd)->complete(comp) ;
			}
			comp.conclusion(match) ;

			output_buf.append(command.size(), KEY_BACK) ;
			command = match ;
			output_buf.append(command) ;
			cursor_pos = command.size() ;

			if (comp.unique())
			{
				command.append(1, ' ') ;
				output_buf.append(1, ' ') ;
				cursor_pos++ ;
			}
			else
			{
				output_buf.append("\r\n") ;
				const stringlist &res = comp.match_result() ;
				stringlist::const_iterator itres = res.begin() ;
				stringlist::const_iterator itend = res.end() ;
				for ( ; itres != itend ; ++itres)
				{
					output_buf.append(*itres) ;
					output_buf.append("\t") ;
				}
				output_buf.append("\r\n") ;
				output_buf.append(m_title) ;
				output_buf.append(command) ;
			}
			goto output_char ;
		}

		if (isspace(c) || isgraph(c))
		{
			trace("read char: space or graph, command is \'%s\'\r\n", command.c_str()) ;

			command.insert(cursor_pos, 1, c) ;
			trace("command is \"%s\"\r\n", command.c_str()) ;
			output_buf.append(command, cursor_pos, string::npos) ;
			cursor_pos++ ;
			output_buf.append(command.size() - cursor_pos, KEY_BACK) ;

			goto output_char ;
		}
		trace("not handled\r\n") ;

output_char:
		if (0 == echo)
			output_buf.clear() ;
		else if (echo > 0 && can_replace)
			output_buf.replace(0, output_buf.size(), output_buf.size(), echo) ;

		output(output_buf) ;

		supp.clear() ;
		/*
		if (false == supp.empty())
		{
			output(supp) ;
		}
		*/

		if (over)
		{
			goto out ;
		}
	}

out:
	output('\r') ;
	return read_ok ;
}

void console::output(const char c) const
{
	if (m_echo)
	{
		fprintf(m_output, "%c", c) ;
		fflush(m_output) ;
	}
}
void console::output(const char *text) const
{
	if (m_echo)
	{
		fprintf(m_output, "%s", text) ;
		fflush(m_output) ;
	}
}

void console::output(const std::string &text) const
{
	output(text.c_str() ) ;
}

void console::output(const stringlist &strlist, const char *split /* = "\r\n" */) const
{
	if (m_echo)
	{
		stringlist::const_iterator itstr = strlist.begin() ;
		stringlist::const_iterator itend = strlist.end() ;
		for ( ; itstr != itend ; ++itstr)
		{
			output(*itstr) ;
			output(split) ;
		}
	}
}

void console::outputln(const char *text) const
{
	if (m_echo)
	{
		fprintf(m_output, "%s\r\n", text) ;
		fflush(m_output) ;
	}
}

void console::outputln(const std::string &text) const
{
	outputln(text.c_str()) ;
}

const std::list<command_handler *> &console::get_handler_list() const
{
	return m_cmd_handlers ;
}

void console::register_handler(command_handler *handler)
{
	m_cmd_handlers.insert(m_itr_postproc, handler) ;
}

void console::register_handler(const char *cmd, callback_t cb, const char *helptext /* = "" */)
{
	m_cmd_handlers.insert(m_itr_postproc, new command_normal(this, cmd, command_register_helper(cb), helptext)) ;
}
