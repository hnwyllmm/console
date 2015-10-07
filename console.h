#ifndef _CONSOLE_H
#define _CONSOLE_H

#include <termios.h>
#include <cstdio>
#include <list>
#include <string>

typedef std::list<std::string>	stringlist ;
typedef std::pair<std::string, std::string>		helptext_t ;
typedef std::list<helptext_t>	helplist_t ;
typedef std::list<std::string>	command_args ;

typedef bool (*callback_t)(const command_args &args, stringlist &output) ;

class console ;

class complete_helper
{
public:
	explicit complete_helper(const std::string &prefix = std::string()) ;

	void set_prefix(const std::string &prefix) {m_prefix = prefix ; }
	void set_prefix(const char *prefix) {m_prefix = prefix ; }
	void *set_option(const stringlist *list) { m_list_stable_option = list ; }
	void add_option(const char *opt) { m_list_dynamic_option.push_back(opt) ; }
	void add_option(const std::string &opt) { m_list_dynamic_option.push_back(opt) ; }

	void complete();
	void conclusion(std::string &match) const ;
	bool unique() const { return m_list_result.size() == 1 ; }
	const stringlist &match_result() const { return m_list_result ; }

private:
	const stringlist *	m_list_stable_option ;
	stringlist			m_list_dynamic_option ;

	std::string			m_prefix ;
	stringlist			m_list_result ;
};
class command_handler
{
public:
	enum handle_return
	{
		CMD_SUCCESS		= 0,
		CMD_NOT_HANDLED	= 1,
		CMD_DISCARD		= 2,
		CMD_EXIT		= 3,
		CMD_REHANDLE	= 4,
	};
public:
	command_handler(console *c) ;

	virtual int	 handle(std::string &command, command_args &args, stringlist &output) = 0;
	virtual void help(const command_args &args, helplist_t &output) const = 0;
	virtual void complete(complete_helper &complete) const ;

protected:
	const console *get_console() const {return m_console ; }
	console *get_console() {return m_console ; }
	void add_cmd(const char *cmd) ;
	bool receive(const char *cmd) const ;

private:
	console *	m_console ;
	stringlist	m_cmdlist ;
};

class command_callback
{
public:
	virtual bool operator()(const command_args &args, stringlist &output) const
	{
	}
};

class command_normal : public command_handler
{
public:
	command_normal(console *c, const char *cmd, const command_callback &cb, const char *helptext = "") ;

	virtual int	 handle(std::string &command, command_args &args, stringlist &output) ;
	virtual void help(const command_args &args, helplist_t &output) const ;

private:
	const command_callback &		m_cb ;
	helptext_t						m_helptext ;
};

class command_history ;
class console
{
public:
	console() ;
	~console() ;

	void set_header(const char *header) { m_header = header ; }
	void set_title(const char *title) { m_title = title ; }
	void set_exit_text(const char *exit) {m_exit_text = exit ;}
	void set_not_supported(const char *not_supported) 
			{ m_not_supported = not_supported ; }
	void set_echo(bool on) {m_echo = on ; }

	bool get_echo() const {return m_echo ; }

	void set_output(FILE *fp) {m_output = fp ; setbuf(m_output, NULL) ;}
	void set_input(FILE *fp) ; 
	FILE *get_output() const {return m_output ;}
	FILE *get_input() const {return m_input ; }
	
	void register_handler(command_handler *handler) ;
	void register_handler(const char *cmd, callback_t cb, const char *helptext = "") ;

	bool start() ;
	void stop() ;

	void output(const char c) const ;
	void output(const char *text) const ;
	void output(const std::string &text) const ;
	void output(const stringlist &strlist, const char *split = "\r\n") const ;
	void outputln(const char *text) const ;
	void outputln(const std::string &text) const ;

	const std::list<command_handler *> &get_handler_list() const ;

protected:
	bool read_command(std::string &command) ;
	bool run() ;

private:
	int  read_char(int fd) const ;
private:
	FILE *		m_output ;
	FILE *		m_input ;
	struct termios	m_input_bakter ;

	bool		m_running ;

	bool		m_echo ;

	std::string	m_header ;
	std::string	m_title ;			// output before read command
	std::string	m_exit_text ;
	std::string	m_not_supported ;

	std::list<command_handler *>			m_cmd_handlers;
	std::list<command_handler *>::iterator	m_itr_postproc ;

	command_history *m_cmd_history ;
};
#endif // _CONSOLE_H

