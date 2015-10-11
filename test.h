#ifndef _CONSOLE_TEST_H
#define _CONSOLE_TEST_H

#include "console.h"

class command_test : public command_handler
{
public:
	command_test(console *c) : command_handler(c)
	{
		add_cmd("test") ;
	}

	int  handle(std::string &command, command_args &args, stringlist &output)
	{
		if (!receive(args.front().c_str()))
			return CMD_NOT_HANDLED;

		console *c = get_console() ;
		std::string pwd;
		const bool ret = c->read_command(pwd, "input password: ", '*') ;
		if (false == ret)
			return CMD_DISCARD ;

		c->output("you input is : ") ;
		c->outputln(pwd) ;
	}

	void help(const command_args &args, helplist_t &output) const
	{
	}
};
#endif  // _CONSOLE_TEST_H

