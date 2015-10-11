#include "console.h"
#include "test.h"

using namespace std ;

int main(void)
{
	console con ;
	con.set_header("welcome to console test") ;
	con.set_title("console > ") ;
	con.set_exit_text("Bye") ;
	con.register_handler(new command_test(&con)) ;
	con.start() ;
	return 0 ;
}
