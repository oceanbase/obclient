/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates.
   Copyright (c) 2009, 2019, MariaDB Corporation.
   Copyright (c) 2021 OceanBase.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA */

/* mysql command tool
 * Commands compatible with mSQL by David J. Hughes
 *
 * Written by:
 *   Michael 'Monty' Widenius
 *   Andi Gutmans  <andi@zend.com>
 *   Zeev Suraski  <zeev@zend.com>
 *   Jani Tolonen  <jani@mysql.com>
 *   Matt Wagner   <matt@mysql.com>
 *   Jeremy Cole   <jcole@mysql.com>
 *   Tonu Samuel   <tonu@mysql.com>
 *   Harrison Fisk <harrison@mysql.com>
 *
 **/

#include "client_priv.h"
#include <m_ctype.h>
#include <stdarg.h>
#include <my_dir.h>
#define PCRE_STATIC 1  /* Important on Windows */
#include "pcreposix.h" /* pcreposix regex library */
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__		      // Skip warnings in getopt.h
#endif
#include "my_readline.h"
#include <signal.h>
#include <violite.h>
#include <my_sys.h>
#include <source_revision.h>
#if defined(USE_LIBEDIT_INTERFACE) && defined(HAVE_LOCALE_H)
#include <locale.h>
#endif

#include <ob_load_balance.h>
#include <map>

const char *VER = OBCLIENT_VERSION;

/* Don't try to make a nice table if the data is too big */
#define MAX_COLUMN_LENGTH	     1024

/* Buffer to hold 'version' and 'version_comment' */
static char *server_version= NULL;

/* Array of options to pass to libemysqld */
#define MAX_SERVER_ARGS               64

#include "sql_string.h"

extern "C" {
#if defined(HAVE_CURSES_H) && defined(HAVE_TERM_H)
#include <curses.h>
#include <term.h>
#else
#if defined(HAVE_TERMIOS_H)
#include <termios.h>
#include <unistd.h>
#elif defined(HAVE_TERMBITS_H)
#include <termbits.h>
#elif defined(HAVE_ASM_TERMBITS_H) && (!defined __GLIBC__ || !(__GLIBC__ > 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ > 0))
#include <asm/termbits.h>		// Standard linux
#endif
#undef VOID
#if defined(HAVE_TERMCAP_H)
#include <termcap.h>
#else
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#undef SYSV				// hack to avoid syntax error
#ifdef HAVE_TERM_H
#include <term.h>
#endif
#endif
#endif /* defined(HAVE_CURSES_H) && defined(HAVE_TERM_H) */

#undef bcmp				// Fix problem with new readline
#if defined(__WIN__)
#include <conio.h>
#else
#include <readline.h>
#if !defined(USE_LIBEDIT_INTERFACE)
#include <history.h>
#endif
#define HAVE_READLINE
#define USE_POPEN
#endif
}

#ifdef HAVE_VIDATTR
static int have_curses= 0;
static void my_vidattr(chtype attrs)
{
  if (have_curses)
    vidattr(attrs);
}
#else
#undef HAVE_SETUPTERM
#define my_vidattr(A) {}              // Can't get this to work
#endif

#ifdef FN_NO_CASE_SENSE
#define cmp_database(cs,A,B) my_strcasecmp((cs), (A), (B))
#else
#define cmp_database(cs,A,B) strcmp((A),(B))
#endif

#include "completion_hash.h"
#include <welcome_copyright_notice.h> // OB_WELCOME_COPYRIGHT_NOTICE

#define PROMPT_CHAR '\\'
#define DEFAULT_DELIMITER ";"

#define MAX_BUFFER_SIZE 1024
#define MAX_BATCH_BUFFER_SIZE (1024L * 1024L * 1024L)

typedef struct st_status
{
  int exit_status;
  ulong query_start_line;
  char *file_name;
  LINE_BUFFER *line_buff;
  bool batch,add_to_history;
} STATUS;


static HashTable ht;
static char **defaults_argv;

enum enum_info_type { INFO_INFO,INFO_ERROR,INFO_RESULT};
typedef enum enum_info_type INFO_TYPE;

static my_bool dbms_serveroutput = 0;
static my_bool dbms_prepared = 0;
static regex_t dbms_enable_re;
static regex_t dbms_disable_re;
static regex_t column_format_re;
MYSQL_STMT *dbms_stmt = NULL;
static const char *dbms_enable_sql = "call dbms_output.enable()";
static const char *dbms_disable_sql = "call dbms_output.disable()";
static String dbms_enable_buffer;
static String dbms_disable_buffer;
static const char *dbms_get_line = "call dbms_output.get_line(?, ?)";
static MYSQL_BIND dbms_ps_params[2];
static MYSQL_BIND dbms_rs_bind[2];
static char dbms_line_buffer[32767];
static char dbms_status;
static int put_stmt_error(MYSQL *con, MYSQL_STMT *stmt);
static void call_dbms_get_line(MYSQL *mysql, uint error);

#define  NS  13
static regex_t pl_create_sql_re;
static uint create_type_offset = 3;
static uint create_schema_offset = 8;
static uint create_schema_quote_offset = 9;
static uint create_object_offset = 11;
static uint create_object_quote_offset = 12;

static const char *pl_get_errors_sql = "SELECT TO_CHAR(LINE)||'/'||TO_CHAR(POSITION) \"LINE/COL\", TEXT \"ERROR\" FROM ALL_ERRORS A WHERE A.NAME = '%.*s'  AND A.TYPE = '%.*s' AND A.OWNER = '%.*s' ORDER BY LINE, POSITION, ATTRIBUTE, MESSAGE_NUMBER";
static regex_t pl_show_errors_sql_re;
static uint show_suffix_offset = 3;
static uint show_type_offset = 4;
static uint show_schema_quote_offset = 9;
static uint show_schema_offset = 10;
static uint show_object_quote_offset = 11;
static uint show_object_offset = 12;

bool is_pl_escape_sql = 0;
static regex_t pl_escape_sql_re;

static regex_t pl_alter_sql_re;
static uint alter_type_offset = 2;
static uint alter_schema_offset = 4;
static uint alter_schema_quote_offset = 5;
static uint alter_object_offset = 7;
static uint alter_object_quote_offset = 8;

static char *pl_last_object = 0, *pl_last_type = 0, *pl_last_schema = 0;
static char *pl_type = 0, *pl_object = 0, *pl_schema = 0;
static uint pl_type_len = 0, pl_object_len = 0, pl_schema_len = 0;
static int handle_oracle_sql(MYSQL *mysql, String *buffer);

static MYSQL mysql;			/* The connection */
static my_bool ignore_errors=0,wait_flag=0,quick=0,
               connected=0,opt_raw_data=0,unbuffered=0,output_tables=0,
	       opt_rehash=1,skip_updates=0,safe_updates=0,one_database=0,
	       opt_compress=0, using_opt_local_infile=0,
	       vertical=0, line_numbers=1, column_names=1,opt_html=0,
               opt_xml=0,opt_nopager=1, opt_outfile=0, named_cmds= 0,
	       tty_password= 0, opt_nobeep=0, opt_reconnect=1,
	       opt_secure_auth= 0,
               default_pager_set= 0, opt_sigint_ignore= 0,
               auto_vertical_output= 0,
               show_warnings= 0, executing_query= 0,
               ignore_spaces= 0, opt_binhex= 0, opt_progress_reports;
static my_bool debug_info_flag, debug_check_flag, batch_abort_on_error;
static my_bool column_types_flag;
static my_bool preserve_comments= 0;
static my_bool preserve_comments_invalid= 0;
static my_bool skip_comments = 0;
static my_bool in_com_source, aborted= 0;
static ulong opt_max_allowed_packet, opt_net_buffer_length;
static uint verbose=0,opt_silent=0,opt_mysql_port=0, opt_local_infile=0;
static uint my_end_arg;
static char * opt_mysql_unix_port=0;
static unsigned long connect_flag=CLIENT_INTERACTIVE;
static my_bool opt_binary_mode= FALSE;
static my_bool opt_connect_expired_password= FALSE;
static int interrupted_query= 0;
static char *current_host,*current_db,*current_user=0,*opt_password=0,
            *current_prompt=0, *delimiter_str= 0,
            *default_charset= (char*) MYSQL_AUTODETECT_CHARSET_NAME,
            *opt_init_command= 0;
static char *current_host_success = 0;
static int current_port_success = 0;
static char *histfile;
static char *histfile_tmp;
static String glob_buffer,old_buffer;
static String processed_prompt;
static char *full_username=0,*part_username=0,*default_prompt=0;
static int wait_time = 5;
static STATUS status;
static ulong select_limit,max_join_size,opt_connect_timeout=0;
static char mysql_charsets_dir[FN_REFLEN+1];
static char *opt_plugin_dir= 0, *opt_default_auth= 0;
static const char *xmlmeta[] = {
  "&", "&amp;",
  "<", "&lt;",
  ">", "&gt;",
  "\"", "&quot;",
  /* Turn \0 into a space. Why not &#0;? That's not valid XML or HTML. */
  "\0", " ",
  0, 0
};
static const char *day_names[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *month_names[]={"Jan","Feb","Mar","Apr","May","Jun","Jul",
			    "Aug","Sep","Oct","Nov","Dec"};
static char default_pager[FN_REFLEN];
static char pager[FN_REFLEN], outfile[FN_REFLEN];
static FILE *PAGER, *OUTFILE;
static MEM_ROOT hash_mem_root;
static uint prompt_counter;
static char delimiter[16]= DEFAULT_DELIMITER;
static uint delimiter_length= 1;
static my_bool is_user_specify_delimiter = FALSE;
unsigned short terminal_width= 80;

#ifndef PROXY_MODE
#define PROXY_MODE 0xfe
#endif
static my_bool is_proxymode = 0;

static uint opt_protocol=0;
static const char *opt_protocol_type= "";
static CHARSET_INFO *charset_info= &my_charset_latin1;

#include "sslopt-vars.h"

const char *default_dbug_option="d:t:o,/tmp/mysql.trace";

void tee_fprintf(FILE *file, const char *fmt, ...);
void tee_fputs(const char *s, FILE *file);
void tee_puts(const char *s, FILE *file);
void tee_putc(int c, FILE *file);
static void tee_print_sized_data(const char *, unsigned int, unsigned int, bool);
/* The names of functions that actually do the manipulation. */
static int get_options(int argc,char **argv);
extern "C" my_bool get_one_option(int optid, const struct my_option *opt,
                                  char *argument);
static int com_quit(String *str,char*),
	   com_go(String *str,char*), com_ego(String *str,char*),
	   com_print(String *str,char*),
	   com_help(String *str,char*), com_clear(String *str,char*),
	   com_connect(String *str,char*), com_status(String *str,char*),
	   com_use(String *str,char*), com_source(String *str, char*),
	   com_rehash(String *str, char*), com_tee(String *str, char*),
           com_notee(String *str, char*), com_charset(String *str,char*),
           com_prompt(String *str, char*), com_delimiter(String *str, char*),
     com_warnings(String *str, char*), com_nowarnings(String *str, char*);

#ifdef USE_POPEN
static int com_nopager(String *str, char*), com_pager(String *str, char*),
           com_edit(String *str,char*), com_shell(String *str, char *);
#endif

static int read_and_execute(bool interactive);
static int sql_connect(char *host,char *database,char *user,char *password, uint silent);
static const char *server_version_string(MYSQL *mysql);
static int put_info(const char *str,INFO_TYPE info,uint error=0,
                    const char *sql_state = 0, my_bool oracle_mode = FALSE);
static int put_error(MYSQL *mysql);
static void safe_put_field(const char *pos,ulong length);
static void xmlencode_print(const char *src, uint length);
static void init_pager();
static void end_pager();
static void init_tee(const char *);
static void end_tee();
static const char* construct_prompt();
enum get_arg_mode { CHECK, GET, GET_NEXT};
static int rewrite_by_oracle(char *line);
static char *get_arg_by_index(char *line, int arg_index);
static char *get_arg(char *line, get_arg_mode mode);
static void init_username();
static void add_int_to_prompt(int toadd);
static int get_result_width(MYSQL_RES *res);
static int get_field_disp_length(MYSQL_FIELD * field);
#ifndef EMBEDDED_LIBRARY
static uint last_progress_report_length= 0;
static void report_progress(const MYSQL *mysql, uint stage, uint max_stage,
                            double progress, const char *proc_info,
                            uint proc_info_length);
#endif
static void report_progress_end();

static void get_current_db();

/*ON:1, OFF:0*/
static my_bool is_define_oracle = 1;
static char define_char_oracle = '&';
static my_bool is_feedback_oracle = 1;
static int feedback_int_oracle = -1;
static my_bool is_sqlblanklines_oracle = 1;
static my_bool is_echo_oracle = 0;
static my_bool is_termout_oracle = 1;
static int pagesize_int_oracle = -1;
static my_bool is_trimspool_oracle = 1;
static my_bool is_exitcommit_oracle = 1;
static uint obclient_num_width = 10;
static bool force_set_num_width_close = 1; //default support as oracle
static my_bool prompt_cmd_oracle(const char *str, char **text);
static my_bool is_global_comment(String *globbuffer);
static my_bool set_cmd_oracle(const char *str);
static my_bool set_param_oracle(const char *str);
static my_bool scan_define_oracle(String *buffer, String *newbuffer);
static my_bool exec_cmd_oracle(String *buffer, String *newbuffer);
static my_bool is_termout_oracle_enable(MYSQL *mysql);


static char obclient_login[MAX_BUFFER_SIZE] = { 0 };
static char* get_login_sql_path();
static void start_login_sql(MYSQL *mysql, String *buffer, const char *path);

#define IS_CMD_SLASH 1    //oracle /
#define IS_CMD_LINE 2     //\s,\h...
static String last_execute_buffer;
static int is_cmd_line(char *pos, size_t len);
static my_bool is_space_buffer(char *start, char *end);

#define COMMAND_MAX 50
#define COMMAND_NAME_LEN 32
static char *ob_disable_commands_str = 0;
static char disable_comands[COMMAND_MAX][COMMAND_NAME_LEN] = { 0 };
static int disable_command_count = 0;
static my_bool is_command_valid(const char* name);

static std::map<void*, void*> map_global;
static void free_map_global();

static char* ob_proxy_user_str = 0;
static char* ob_socket5_proxy_str = 0;

static char* ob_error_top_str = 0;
static void print_error_sqlstr(String *buffer, char* topN);

/* A structure which contains information on the commands this program
   can understand. */

typedef struct {
  const char *name;		/* User printable name of the function. */
  char cmd_char;		/* msql command character */
  int (*func)(String *str,char *); /* Function to call to do the job. */
  bool takes_params;		/* Max parameters for command */
  const char *doc;		/* Documentation for this function.  */
} COMMANDS;

static COMMANDS commands[] = {
  { "?",      '?', com_help,   1, "Synonym for `help'." },
  { "clear",  'c', com_clear,  0, "Clear the current input statement."},
  { "connect",'r', com_connect,1,
    "Reconnect to the server. Optional arguments are db and host." },
  { "conn", 0, com_connect, 1,
    "Reconnect to the server. Optional arguments are db and host." },
  { "delimiter", 'd', com_delimiter,    1,
    "Set statement delimiter." },
#ifdef USE_POPEN
  { "edit",   'e', com_edit,   0, "Edit command with $EDITOR."},
#endif
  { "ego",    'G', com_ego,    0,
    "Send command to OceanBase server, display result vertically."},
  { "exit",   'q', com_quit,   0, "Exit mysql. Same as quit."},
  { "go",     'g', com_go,     0, "Send command to OceanBase server." },
  { "help",   'h', com_help,   1, "Display this help." },
#ifdef USE_POPEN
  { "nopager",'n', com_nopager,0, "Disable pager, print to stdout." },
#endif
  { "notee",  't', com_notee,  0, "Don't write into outfile." },
#ifdef USE_POPEN
  { "pager",  'P', com_pager,  1, 
    "Set PAGER [to_pager]. Print the query results via PAGER." },
#endif
  { "print",  'p', com_print,  0, "Print current command." },
  { "prompt", 'R', com_prompt, 1, "Change your mysql prompt."},
  { "quit",   'q', com_quit,   0, "Quit mysql." },
  { "rehash", '#', com_rehash, 0, "Rebuild completion hash." },
  { "source", '.', com_source, 1,
    "Execute an SQL script file. Takes a file name as an argument."},
  { "status", 's', com_status, 0, "Get status information from the server."},
#ifdef USE_POPEN
  { "system", '!', com_shell,  1, "Execute a system shell command."},
#endif
  { "tee",    'T', com_tee,    1, 
    "Set outfile [to_outfile]. Append everything into given outfile." },
  { "use",    'u', com_use,    1,
    "Use another database. Takes database name as argument." },
  { "charset",    'C', com_charset,    1,
    "Switch to another charset. Might be needed for processing binlog with multi-byte charsets." },
  { "warnings", 'W', com_warnings,  0,
    "Show warnings after every statement." },
  { "nowarning", 'w', com_nowarnings, 0,
    "Don't show warnings after every statement." },
  /* Get bash-like expansion for some commands */
  { "create table",     0, 0, 0, ""},
  { "create database",  0, 0, 0, ""},
  { "show databases",   0, 0, 0, ""},
  { "show fields from", 0, 0, 0, ""},
  { "show keys from",   0, 0, 0, ""},
  { "show tables",      0, 0, 0, ""},
  { "load data from",   0, 0, 0, ""},
  { "alter table",      0, 0, 0, ""},
  { "set option",       0, 0, 0, ""},
  { "lock tables",      0, 0, 0, ""},
  { "unlock tables",    0, 0, 0, ""},
  /* generated 2006-12-28.  Refresh occasionally from lexer. */
  { "ACTION", 0, 0, 0, ""},
  { "ADD", 0, 0, 0, ""},
  { "AFTER", 0, 0, 0, ""},
  { "AGAINST", 0, 0, 0, ""},
  { "AGGREGATE", 0, 0, 0, ""},
  { "ALL", 0, 0, 0, ""},
  { "ALGORITHM", 0, 0, 0, ""},
  { "ALTER", 0, 0, 0, ""},
  { "ANALYZE", 0, 0, 0, ""},
  { "AND", 0, 0, 0, ""},
  { "ANY", 0, 0, 0, ""},
  { "AS", 0, 0, 0, ""},
  { "ASC", 0, 0, 0, ""},
  { "ASCII", 0, 0, 0, ""},
  { "ASENSITIVE", 0, 0, 0, ""},
  { "AUTO_INCREMENT", 0, 0, 0, ""},
  { "AVG", 0, 0, 0, ""},
  { "AVG_ROW_LENGTH", 0, 0, 0, ""},
  { "BACKUP", 0, 0, 0, ""},
  { "BDB", 0, 0, 0, ""},
  { "BEFORE", 0, 0, 0, ""},
  { "BEGIN", 0, 0, 0, ""},
  { "BERKELEYDB", 0, 0, 0, ""},
  { "BETWEEN", 0, 0, 0, ""},
  { "BIGINT", 0, 0, 0, ""},
  { "BINARY", 0, 0, 0, ""},
  { "BINLOG", 0, 0, 0, ""},
  { "BIT", 0, 0, 0, ""},
  { "BLOB", 0, 0, 0, ""},
  { "BOOL", 0, 0, 0, ""},
  { "BOOLEAN", 0, 0, 0, ""},
  { "BOTH", 0, 0, 0, ""},
  { "BTREE", 0, 0, 0, ""},
  { "BY", 0, 0, 0, ""},
  { "BYTE", 0, 0, 0, ""},
  { "CACHE", 0, 0, 0, ""},
  { "CALL", 0, 0, 0, ""},
  { "CASCADE", 0, 0, 0, ""},
  { "CASCADED", 0, 0, 0, ""},
  { "CASE", 0, 0, 0, ""},
  { "CHAIN", 0, 0, 0, ""},
  { "CHANGE", 0, 0, 0, ""},
  { "CHANGED", 0, 0, 0, ""},
  { "CHAR", 0, 0, 0, ""},
  { "CHARACTER", 0, 0, 0, ""},
  { "CHARSET", 0, 0, 0, ""},
  { "CHECK", 0, 0, 0, ""},
  { "CHECKSUM", 0, 0, 0, ""},
  { "CIPHER", 0, 0, 0, ""},
  { "CLIENT", 0, 0, 0, ""},
  { "CLOSE", 0, 0, 0, ""},
  { "CODE", 0, 0, 0, ""},
  { "COLLATE", 0, 0, 0, ""},
  { "COLLATION", 0, 0, 0, ""},
  { "COLUMN", 0, 0, 0, ""},
  { "COLUMNS", 0, 0, 0, ""},
  { "COMMENT", 0, 0, 0, ""},
  { "COMMIT", 0, 0, 0, ""},
  { "COMMITTED", 0, 0, 0, ""},
  { "COMPACT", 0, 0, 0, ""},
  { "COMPRESSED", 0, 0, 0, ""},
  { "CONCURRENT", 0, 0, 0, ""},
  { "CONDITION", 0, 0, 0, ""},
  { "CONNECTION", 0, 0, 0, ""},
  { "CONSISTENT", 0, 0, 0, ""},
  { "CONSTRAINT", 0, 0, 0, ""},
  { "CONTAINS", 0, 0, 0, ""},
  { "CONTINUE", 0, 0, 0, ""},
  { "CONVERT", 0, 0, 0, ""},
  { "CREATE", 0, 0, 0, ""},
  { "CROSS", 0, 0, 0, ""},
  { "CUBE", 0, 0, 0, ""},
  { "CURRENT_DATE", 0, 0, 0, ""},
  { "CURRENT_TIME", 0, 0, 0, ""},
  { "CURRENT_TIMESTAMP", 0, 0, 0, ""},
  { "CURRENT_USER", 0, 0, 0, ""},
  { "CURSOR", 0, 0, 0, ""},
  { "DATA", 0, 0, 0, ""},
  { "DATABASE", 0, 0, 0, ""},
  { "DATABASES", 0, 0, 0, ""},
  { "DATE", 0, 0, 0, ""},
  { "DATETIME", 0, 0, 0, ""},
  { "DAY", 0, 0, 0, ""},
  { "DAY_HOUR", 0, 0, 0, ""},
  { "DAY_MICROSECOND", 0, 0, 0, ""},
  { "DAY_MINUTE", 0, 0, 0, ""},
  { "DAY_SECOND", 0, 0, 0, ""},
  { "DEALLOCATE", 0, 0, 0, ""},     
  { "DEC", 0, 0, 0, ""},
  { "DECIMAL", 0, 0, 0, ""},
  { "DECLARE", 0, 0, 0, ""},
  { "DEFAULT", 0, 0, 0, ""},
  { "DEFINER", 0, 0, 0, ""},
  { "DELAYED", 0, 0, 0, ""},
  { "DELAY_KEY_WRITE", 0, 0, 0, ""},
  { "DELETE", 0, 0, 0, ""},
  { "DESC", 0, 0, 0, ""},
  { "DESCRIBE", 0, 0, 0, ""},
  { "DES_KEY_FILE", 0, 0, 0, ""},
  { "DETERMINISTIC", 0, 0, 0, ""},
  { "DIRECTORY", 0, 0, 0, ""},
  { "DISABLE", 0, 0, 0, ""},
  { "DISCARD", 0, 0, 0, ""},
  { "DISTINCT", 0, 0, 0, ""},
  { "DISTINCTROW", 0, 0, 0, ""},
  { "DIV", 0, 0, 0, ""},
  { "DO", 0, 0, 0, ""},
  { "DOUBLE", 0, 0, 0, ""},
  { "DROP", 0, 0, 0, ""},
  { "DUAL", 0, 0, 0, ""},
  { "DUMPFILE", 0, 0, 0, ""},
  { "DUPLICATE", 0, 0, 0, ""},
  { "DYNAMIC", 0, 0, 0, ""},
  { "EACH", 0, 0, 0, ""},
  { "ELSE", 0, 0, 0, ""},
  { "ELSEIF", 0, 0, 0, ""},
  { "ENABLE", 0, 0, 0, ""},
  { "ENCLOSED", 0, 0, 0, ""},
  { "END", 0, 0, 0, ""},
  { "ENGINE", 0, 0, 0, ""},
  { "ENGINES", 0, 0, 0, ""},
  { "ENUM", 0, 0, 0, ""},
  { "ERRORS", 0, 0, 0, ""},
  { "ESCAPE", 0, 0, 0, ""},
  { "ESCAPED", 0, 0, 0, ""},
  { "EVENTS", 0, 0, 0, ""},
  { "EXECUTE", 0, 0, 0, ""},
  { "EXISTS", 0, 0, 0, ""},
  { "EXIT", 0, 0, 0, ""},
  { "EXPANSION", 0, 0, 0, ""},
  { "EXPLAIN", 0, 0, 0, ""},
  { "EXTENDED", 0, 0, 0, ""},
  { "FALSE", 0, 0, 0, ""},
  { "FAST", 0, 0, 0, ""},
  { "FETCH", 0, 0, 0, ""},
  { "FIELDS", 0, 0, 0, ""},
  { "FILE", 0, 0, 0, ""},
  { "FIRST", 0, 0, 0, ""},
  { "FIXED", 0, 0, 0, ""},
  { "FLOAT", 0, 0, 0, ""},
  { "FLOAT4", 0, 0, 0, ""},
  { "FLOAT8", 0, 0, 0, ""},
  { "FLUSH", 0, 0, 0, ""},
  { "FOR", 0, 0, 0, ""},
  { "FORCE", 0, 0, 0, ""},
  { "FOREIGN", 0, 0, 0, ""},
  { "FOUND", 0, 0, 0, ""},
  { "FROM", 0, 0, 0, ""},
  { "FULL", 0, 0, 0, ""},
  { "FULLTEXT", 0, 0, 0, ""},
  { "FUNCTION", 0, 0, 0, ""},
  { "GEOMETRY", 0, 0, 0, ""},
  { "GEOMETRYCOLLECTION", 0, 0, 0, ""},
  { "GET_FORMAT", 0, 0, 0, ""},
  { "GLOBAL", 0, 0, 0, ""},
  { "GRANT", 0, 0, 0, ""},
  { "GRANTS", 0, 0, 0, ""},
  { "GROUP", 0, 0, 0, ""},
  { "HANDLER", 0, 0, 0, ""},
  { "HASH", 0, 0, 0, ""},
  { "HAVING", 0, 0, 0, ""},
  { "HELP", 0, 0, 0, ""},
  { "HIGH_PRIORITY", 0, 0, 0, ""},
  { "HOSTS", 0, 0, 0, ""},
  { "HOUR", 0, 0, 0, ""},
  { "HOUR_MICROSECOND", 0, 0, 0, ""},
  { "HOUR_MINUTE", 0, 0, 0, ""},
  { "HOUR_SECOND", 0, 0, 0, ""},
  { "IDENTIFIED", 0, 0, 0, ""},
  { "IF", 0, 0, 0, ""},
  { "IGNORE", 0, 0, 0, ""},
  { "IMPORT", 0, 0, 0, ""},
  { "IN", 0, 0, 0, ""},
  { "INDEX", 0, 0, 0, ""},
  { "INDEXES", 0, 0, 0, ""},
  { "INFILE", 0, 0, 0, ""},
  { "INNER", 0, 0, 0, ""},
  { "INNOBASE", 0, 0, 0, ""},
  { "INNODB", 0, 0, 0, ""},
  { "INOUT", 0, 0, 0, ""},
  { "INSENSITIVE", 0, 0, 0, ""},
  { "INSERT", 0, 0, 0, ""},
  { "INSERT_METHOD", 0, 0, 0, ""},
  { "INT", 0, 0, 0, ""},
  { "INT1", 0, 0, 0, ""},
  { "INT2", 0, 0, 0, ""},
  { "INT3", 0, 0, 0, ""},
  { "INT4", 0, 0, 0, ""},
  { "INT8", 0, 0, 0, ""},
  { "INTEGER", 0, 0, 0, ""},
  { "INTERVAL", 0, 0, 0, ""},
  { "INTO", 0, 0, 0, ""},
  { "IO_THREAD", 0, 0, 0, ""},
  { "IS", 0, 0, 0, ""},
  { "ISOLATION", 0, 0, 0, ""},
  { "ISSUER", 0, 0, 0, ""},
  { "ITERATE", 0, 0, 0, ""},
  { "INVOKER", 0, 0, 0, ""},
  { "JOIN", 0, 0, 0, ""},
  { "KEY", 0, 0, 0, ""},
  { "KEYS", 0, 0, 0, ""},
  { "KILL", 0, 0, 0, ""},
  { "LANGUAGE", 0, 0, 0, ""},
  { "LAST", 0, 0, 0, ""},
  { "LEADING", 0, 0, 0, ""},
  { "LEAVE", 0, 0, 0, ""},
  { "LEAVES", 0, 0, 0, ""},
  { "LEFT", 0, 0, 0, ""},
  { "LEVEL", 0, 0, 0, ""},
  { "LIKE", 0, 0, 0, ""},
  { "LIMIT", 0, 0, 0, ""},
  { "LINES", 0, 0, 0, ""},
  { "LINESTRING", 0, 0, 0, ""},
  { "LOAD", 0, 0, 0, ""},
  { "LOCAL", 0, 0, 0, ""},
  { "LOCALTIME", 0, 0, 0, ""},
  { "LOCALTIMESTAMP", 0, 0, 0, ""},
  { "LOCK", 0, 0, 0, ""},
  { "LOCKS", 0, 0, 0, ""},
  { "LOGS", 0, 0, 0, ""},
  { "LONG", 0, 0, 0, ""},
  { "LONGBLOB", 0, 0, 0, ""},
  { "LONGTEXT", 0, 0, 0, ""},
  { "LOOP", 0, 0, 0, ""},
  { "LOW_PRIORITY", 0, 0, 0, ""},
  { "MASTER", 0, 0, 0, ""},
  { "MASTER_CONNECT_RETRY", 0, 0, 0, ""},
  { "MASTER_HOST", 0, 0, 0, ""},
  { "MASTER_LOG_FILE", 0, 0, 0, ""},
  { "MASTER_LOG_POS", 0, 0, 0, ""},
  { "MASTER_PASSWORD", 0, 0, 0, ""},
  { "MASTER_PORT", 0, 0, 0, ""},
  { "MASTER_SERVER_ID", 0, 0, 0, ""},
  { "MASTER_SSL", 0, 0, 0, ""},
  { "MASTER_SSL_CA", 0, 0, 0, ""},
  { "MASTER_SSL_CAPATH", 0, 0, 0, ""},
  { "MASTER_SSL_CERT", 0, 0, 0, ""},
  { "MASTER_SSL_CIPHER", 0, 0, 0, ""},
  { "MASTER_SSL_KEY", 0, 0, 0, ""},
  { "MASTER_USER", 0, 0, 0, ""},
  { "MATCH", 0, 0, 0, ""},
  { "MAX_CONNECTIONS_PER_HOUR", 0, 0, 0, ""},
  { "MAX_QUERIES_PER_HOUR", 0, 0, 0, ""},
  { "MAX_ROWS", 0, 0, 0, ""},
  { "MAX_UPDATES_PER_HOUR", 0, 0, 0, ""},
  { "MAX_USER_CONNECTIONS", 0, 0, 0, ""},
  { "MEDIUM", 0, 0, 0, ""},
  { "MEDIUMBLOB", 0, 0, 0, ""},
  { "MEDIUMINT", 0, 0, 0, ""},
  { "MEDIUMTEXT", 0, 0, 0, ""},
  { "MERGE", 0, 0, 0, ""},
  { "MICROSECOND", 0, 0, 0, ""},
  { "MIDDLEINT", 0, 0, 0, ""},
  { "MIGRATE", 0, 0, 0, ""},
  { "MINUTE", 0, 0, 0, ""},
  { "MINUTE_MICROSECOND", 0, 0, 0, ""},
  { "MINUTE_SECOND", 0, 0, 0, ""},
  { "MIN_ROWS", 0, 0, 0, ""},
  { "MOD", 0, 0, 0, ""},
  { "MODE", 0, 0, 0, ""},
  { "MODIFIES", 0, 0, 0, ""},
  { "MODIFY", 0, 0, 0, ""},
  { "MONTH", 0, 0, 0, ""},
  { "MULTILINESTRING", 0, 0, 0, ""},
  { "MULTIPOINT", 0, 0, 0, ""},
  { "MULTIPOLYGON", 0, 0, 0, ""},
  { "MUTEX", 0, 0, 0, ""},
  { "NAME", 0, 0, 0, ""},
  { "NAMES", 0, 0, 0, ""},
  { "NATIONAL", 0, 0, 0, ""},
  { "NATURAL", 0, 0, 0, ""},
  { "NCHAR", 0, 0, 0, ""},
  { "NEW", 0, 0, 0, ""},
  { "NEXT", 0, 0, 0, ""},
  { "NO", 0, 0, 0, ""},
  { "NONE", 0, 0, 0, ""},
  { "NOT", 0, 0, 0, ""},
  { "NO_WRITE_TO_BINLOG", 0, 0, 0, ""},
  { "NULL", 0, 0, 0, ""},
  { "NUMERIC", 0, 0, 0, ""},
  { "NVARCHAR", 0, 0, 0, ""},
  { "OFFSET", 0, 0, 0, ""},
  { "OLD_PASSWORD", 0, 0, 0, ""},
  { "ON", 0, 0, 0, ""},
  { "ONE", 0, 0, 0, ""},
  { "OPEN", 0, 0, 0, ""},
  { "OPTIMIZE", 0, 0, 0, ""},
  { "OPTION", 0, 0, 0, ""},
  { "OPTIONALLY", 0, 0, 0, ""},
  { "OR", 0, 0, 0, ""},
  { "ORDER", 0, 0, 0, ""},
  { "OUT", 0, 0, 0, ""},
  { "OUTER", 0, 0, 0, ""},
  { "OUTFILE", 0, 0, 0, ""},
  { "PACK_KEYS", 0, 0, 0, ""},
  { "PARTIAL", 0, 0, 0, ""},
  { "PASSWORD", 0, 0, 0, ""},
  { "PHASE", 0, 0, 0, ""},
  { "POINT", 0, 0, 0, ""},
  { "POLYGON", 0, 0, 0, ""},
  { "PRECISION", 0, 0, 0, ""},
  { "PREPARE", 0, 0, 0, ""},
  { "PREV", 0, 0, 0, ""},
  { "PRIMARY", 0, 0, 0, ""},
  { "PRIVILEGES", 0, 0, 0, ""},
  { "PROCEDURE", 0, 0, 0, ""},
  { "PROCESS", 0, 0, 0, ""},
  { "PROCESSLIST", 0, 0, 0, ""},
  { "PURGE", 0, 0, 0, ""},
  { "QUARTER", 0, 0, 0, ""},
  { "QUERY", 0, 0, 0, ""},
  { "QUICK", 0, 0, 0, ""},
  { "READ", 0, 0, 0, ""},
  { "READS", 0, 0, 0, ""},
  { "REAL", 0, 0, 0, ""},
  { "RECOVER", 0, 0, 0, ""},
  { "REDUNDANT", 0, 0, 0, ""},
  { "REFERENCES", 0, 0, 0, ""},
  { "REGEXP", 0, 0, 0, ""},
  { "RELAY_LOG_FILE", 0, 0, 0, ""},
  { "RELAY_LOG_POS", 0, 0, 0, ""},
  { "RELAY_THREAD", 0, 0, 0, ""},
  { "RELEASE", 0, 0, 0, ""},
  { "RELOAD", 0, 0, 0, ""},
  { "RENAME", 0, 0, 0, ""},
  { "REPAIR", 0, 0, 0, ""},
  { "REPEATABLE", 0, 0, 0, ""},
  { "REPLACE", 0, 0, 0, ""},
  { "REPLICATION", 0, 0, 0, ""},
  { "REPEAT", 0, 0, 0, ""},
  { "REQUIRE", 0, 0, 0, ""},
  { "RESET", 0, 0, 0, ""},
  { "RESTORE", 0, 0, 0, ""},
  { "RESTRICT", 0, 0, 0, ""},
  { "RESUME", 0, 0, 0, ""},
  { "RETURN", 0, 0, 0, ""},
  { "RETURNS", 0, 0, 0, ""},
  { "REVOKE", 0, 0, 0, ""},
  { "RIGHT", 0, 0, 0, ""},
  { "RLIKE", 0, 0, 0, ""},
  { "ROLLBACK", 0, 0, 0, ""},
  { "ROLLUP", 0, 0, 0, ""},
  { "ROUTINE", 0, 0, 0, ""},
  { "ROW", 0, 0, 0, ""},
  { "ROWS", 0, 0, 0, ""},
  { "ROW_FORMAT", 0, 0, 0, ""},
  { "RTREE", 0, 0, 0, ""},
  { "SAVEPOINT", 0, 0, 0, ""},
  { "SCHEMA", 0, 0, 0, ""},
  { "SCHEMAS", 0, 0, 0, ""},
  { "SECOND", 0, 0, 0, ""},
  { "SECOND_MICROSECOND", 0, 0, 0, ""},
  { "SECURITY", 0, 0, 0, ""},
  { "SELECT", 0, 0, 0, ""},
  { "SENSITIVE", 0, 0, 0, ""},
  { "SEPARATOR", 0, 0, 0, ""},
  { "SERIAL", 0, 0, 0, ""},
  { "SERIALIZABLE", 0, 0, 0, ""},
  { "SESSION", 0, 0, 0, ""},
  { "SET", 0, 0, 0, ""},
  { "SHARE", 0, 0, 0, ""},
  { "SHOW", 0, 0, 0, ""},
  { "SHUTDOWN", 0, 0, 0, ""},
  { "SIGNED", 0, 0, 0, ""},
  { "SIMPLE", 0, 0, 0, ""},
  { "SLAVE", 0, 0, 0, ""},
  { "SNAPSHOT", 0, 0, 0, ""},
  { "SMALLINT", 0, 0, 0, ""},
  { "SOME", 0, 0, 0, ""},
  { "SONAME", 0, 0, 0, ""},
  { "SOUNDS", 0, 0, 0, ""},
  { "SPATIAL", 0, 0, 0, ""},
  { "SPECIFIC", 0, 0, 0, ""},
  { "SQL", 0, 0, 0, ""},
  { "SQLEXCEPTION", 0, 0, 0, ""},
  { "SQLSTATE", 0, 0, 0, ""},
  { "SQLWARNING", 0, 0, 0, ""},
  { "SQL_BIG_RESULT", 0, 0, 0, ""},
  { "SQL_BUFFER_RESULT", 0, 0, 0, ""},
  { "SQL_CACHE", 0, 0, 0, ""},
  { "SQL_CALC_FOUND_ROWS", 0, 0, 0, ""},
  { "SQL_NO_CACHE", 0, 0, 0, ""},
  { "SQL_SMALL_RESULT", 0, 0, 0, ""},
  { "SQL_THREAD", 0, 0, 0, ""},
  { "SQL_TSI_SECOND", 0, 0, 0, ""},
  { "SQL_TSI_MINUTE", 0, 0, 0, ""},
  { "SQL_TSI_HOUR", 0, 0, 0, ""},
  { "SQL_TSI_DAY", 0, 0, 0, ""},
  { "SQL_TSI_WEEK", 0, 0, 0, ""},
  { "SQL_TSI_MONTH", 0, 0, 0, ""},
  { "SQL_TSI_QUARTER", 0, 0, 0, ""},
  { "SQL_TSI_YEAR", 0, 0, 0, ""},
  { "SSL", 0, 0, 0, ""},
  { "START", 0, 0, 0, ""},
  { "STARTING", 0, 0, 0, ""},
  { "STATUS", 0, 0, 0, ""},
  { "STOP", 0, 0, 0, ""},
  { "STORAGE", 0, 0, 0, ""},
  { "STRAIGHT_JOIN", 0, 0, 0, ""},
  { "STRING", 0, 0, 0, ""},
  { "STRIPED", 0, 0, 0, ""},
  { "SUBJECT", 0, 0, 0, ""},
  { "SUPER", 0, 0, 0, ""},
  { "SUSPEND", 0, 0, 0, ""},
  { "TABLE", 0, 0, 0, ""},
  { "TABLES", 0, 0, 0, ""},
  { "TABLESPACE", 0, 0, 0, ""},
  { "TEMPORARY", 0, 0, 0, ""},
  { "TEMPTABLE", 0, 0, 0, ""},
  { "TERMINATED", 0, 0, 0, ""},
  { "TEXT", 0, 0, 0, ""},
  { "THEN", 0, 0, 0, ""},
  { "TIME", 0, 0, 0, ""},
  { "TIMESTAMP", 0, 0, 0, ""},
  { "TIMESTAMPADD", 0, 0, 0, ""},
  { "TIMESTAMPDIFF", 0, 0, 0, ""},
  { "TINYBLOB", 0, 0, 0, ""},
  { "TINYINT", 0, 0, 0, ""},
  { "TINYTEXT", 0, 0, 0, ""},
  { "TO", 0, 0, 0, ""},
  { "TRAILING", 0, 0, 0, ""},
  { "TRANSACTION", 0, 0, 0, ""},
  { "TRIGGER", 0, 0, 0, ""},
  { "TRIGGERS", 0, 0, 0, ""},
  { "TRUE", 0, 0, 0, ""},
  { "TRUNCATE", 0, 0, 0, ""},
  { "TYPE", 0, 0, 0, ""},
  { "TYPES", 0, 0, 0, ""},
  { "UNCOMMITTED", 0, 0, 0, ""},
  { "UNDEFINED", 0, 0, 0, ""},
  { "UNDO", 0, 0, 0, ""},
  { "UNICODE", 0, 0, 0, ""},
  { "UNION", 0, 0, 0, ""},
  { "UNIQUE", 0, 0, 0, ""},
  { "UNKNOWN", 0, 0, 0, ""},
  { "UNLOCK", 0, 0, 0, ""},
  { "UNSIGNED", 0, 0, 0, ""},
  { "UNTIL", 0, 0, 0, ""},
  { "UPDATE", 0, 0, 0, ""},
  { "UPGRADE", 0, 0, 0, ""},
  { "USAGE", 0, 0, 0, ""},
  { "USE", 0, 0, 0, ""},
  { "USER", 0, 0, 0, ""},
  { "USER_RESOURCES", 0, 0, 0, ""},
  { "USE_FRM", 0, 0, 0, ""},
  { "USING", 0, 0, 0, ""},
  { "UTC_DATE", 0, 0, 0, ""},
  { "UTC_TIME", 0, 0, 0, ""},
  { "UTC_TIMESTAMP", 0, 0, 0, ""},
  { "VALUE", 0, 0, 0, ""},
  { "VALUES", 0, 0, 0, ""},
  { "VARBINARY", 0, 0, 0, ""},
  { "VARCHAR", 0, 0, 0, ""},
  { "VARCHARACTER", 0, 0, 0, ""},
  { "VARIABLES", 0, 0, 0, ""},
  { "VARYING", 0, 0, 0, ""},
  { "WARNINGS", 0, 0, 0, ""},
  { "WEEK", 0, 0, 0, ""},
  { "WHEN", 0, 0, 0, ""},
  { "WHERE", 0, 0, 0, ""},
  { "WHILE", 0, 0, 0, ""},
  { "VIEW", 0, 0, 0, ""},
  { "WITH", 0, 0, 0, ""},
  { "WORK", 0, 0, 0, ""},
  { "WRITE", 0, 0, 0, ""},
  { "X509", 0, 0, 0, ""},
  { "XOR", 0, 0, 0, ""},
  { "XA", 0, 0, 0, ""},
  { "YEAR", 0, 0, 0, ""},
  { "YEAR_MONTH", 0, 0, 0, ""},
  { "ZEROFILL", 0, 0, 0, ""},
  { "ABS", 0, 0, 0, ""},
  { "ACOS", 0, 0, 0, ""},
  { "ADDDATE", 0, 0, 0, ""},
  { "ADDTIME", 0, 0, 0, ""},
  { "AES_ENCRYPT", 0, 0, 0, ""},
  { "AES_DECRYPT", 0, 0, 0, ""},
  { "AREA", 0, 0, 0, ""},
  { "ASIN", 0, 0, 0, ""},
  { "ASBINARY", 0, 0, 0, ""},
  { "ASTEXT", 0, 0, 0, ""},
  { "ASWKB", 0, 0, 0, ""},
  { "ASWKT", 0, 0, 0, ""},
  { "ATAN", 0, 0, 0, ""},
  { "ATAN2", 0, 0, 0, ""},
  { "BENCHMARK", 0, 0, 0, ""},
  { "BIN", 0, 0, 0, ""},
  { "BIT_COUNT", 0, 0, 0, ""},
  { "BIT_OR", 0, 0, 0, ""},
  { "BIT_AND", 0, 0, 0, ""},
  { "BIT_XOR", 0, 0, 0, ""},
  { "CAST", 0, 0, 0, ""},
  { "CEIL", 0, 0, 0, ""},
  { "CEILING", 0, 0, 0, ""},
  { "BIT_LENGTH", 0, 0, 0, ""},
  { "CENTROID", 0, 0, 0, ""},
  { "CHAR_LENGTH", 0, 0, 0, ""},
  { "CHARACTER_LENGTH", 0, 0, 0, ""},
  { "COALESCE", 0, 0, 0, ""},
  { "COERCIBILITY", 0, 0, 0, ""},
  { "COMPRESS", 0, 0, 0, ""},
  { "CONCAT", 0, 0, 0, ""},
  { "CONCAT_WS", 0, 0, 0, ""},
  { "CONNECTION_ID", 0, 0, 0, ""},
  { "CONV", 0, 0, 0, ""},
  { "CONVERT_TZ", 0, 0, 0, ""},
  { "COUNT", 0, 0, 0, ""},
  { "COS", 0, 0, 0, ""},
  { "COT", 0, 0, 0, ""},
  { "CRC32", 0, 0, 0, ""},
  { "CROSSES", 0, 0, 0, ""},
  { "CURDATE", 0, 0, 0, ""},
  { "CURTIME", 0, 0, 0, ""},
  { "DATE_ADD", 0, 0, 0, ""},
  { "DATEDIFF", 0, 0, 0, ""},
  { "DATE_FORMAT", 0, 0, 0, ""},
  { "DATE_SUB", 0, 0, 0, ""},
  { "DAYNAME", 0, 0, 0, ""},
  { "DAYOFMONTH", 0, 0, 0, ""},
  { "DAYOFWEEK", 0, 0, 0, ""},
  { "DAYOFYEAR", 0, 0, 0, ""},
  { "DECODE", 0, 0, 0, ""},
  { "DEGREES", 0, 0, 0, ""},
  { "DES_ENCRYPT", 0, 0, 0, ""},
  { "DES_DECRYPT", 0, 0, 0, ""},
  { "DIMENSION", 0, 0, 0, ""},
  { "DISJOINT", 0, 0, 0, ""},
  { "ELT", 0, 0, 0, ""},
  { "ENCODE", 0, 0, 0, ""},
  { "ENCRYPT", 0, 0, 0, ""},
  { "ENDPOINT", 0, 0, 0, ""},
  { "ENVELOPE", 0, 0, 0, ""},
  { "EQUALS", 0, 0, 0, ""},
  { "EXTERIORRING", 0, 0, 0, ""},
  { "EXTRACT", 0, 0, 0, ""},
  { "EXP", 0, 0, 0, ""},
  { "EXPORT_SET", 0, 0, 0, ""},
  { "FIELD", 0, 0, 0, ""},
  { "FIND_IN_SET", 0, 0, 0, ""},
  { "FLOOR", 0, 0, 0, ""},
  { "FORMAT", 0, 0, 0, ""},
  { "FOUND_ROWS", 0, 0, 0, ""},
  { "FROM_DAYS", 0, 0, 0, ""},
  { "FROM_UNIXTIME", 0, 0, 0, ""},
  { "GET_LOCK", 0, 0, 0, ""},
  { "GEOMETRYN", 0, 0, 0, ""},
  { "GEOMETRYTYPE", 0, 0, 0, ""},
  { "GEOMCOLLFROMTEXT", 0, 0, 0, ""},
  { "GEOMCOLLFROMWKB", 0, 0, 0, ""},
  { "GEOMETRYCOLLECTIONFROMTEXT", 0, 0, 0, ""},
  { "GEOMETRYCOLLECTIONFROMWKB", 0, 0, 0, ""},
  { "GEOMETRYFROMTEXT", 0, 0, 0, ""},
  { "GEOMETRYFROMWKB", 0, 0, 0, ""},
  { "GEOMFROMTEXT", 0, 0, 0, ""},
  { "GEOMFROMWKB", 0, 0, 0, ""},
  { "GLENGTH", 0, 0, 0, ""},
  { "GREATEST", 0, 0, 0, ""},
  { "GROUP_CONCAT", 0, 0, 0, ""},
  { "GROUP_UNIQUE_USERS", 0, 0, 0, ""},
  { "HEX", 0, 0, 0, ""},
  { "IFNULL", 0, 0, 0, ""},
  { "INET_ATON", 0, 0, 0, ""},
  { "INET_NTOA", 0, 0, 0, ""},
  { "INSTR", 0, 0, 0, ""},
  { "INTERIORRINGN", 0, 0, 0, ""},
  { "INTERSECTS", 0, 0, 0, ""},
  { "ISCLOSED", 0, 0, 0, ""},
  { "ISEMPTY", 0, 0, 0, ""},
  { "ISNULL", 0, 0, 0, ""},
  { "IS_FREE_LOCK", 0, 0, 0, ""},
  { "IS_USED_LOCK", 0, 0, 0, ""},
  { "LAST_INSERT_ID", 0, 0, 0, ""},
  { "ISSIMPLE", 0, 0, 0, ""},
  { "LAST_DAY", 0, 0, 0, ""},
  { "LAST_VALUE", 0, 0, 0, ""},
  { "LCASE", 0, 0, 0, ""},
  { "LEAST", 0, 0, 0, ""},
  { "LENGTH", 0, 0, 0, ""},
  { "LN", 0, 0, 0, ""},
  { "LINEFROMTEXT", 0, 0, 0, ""},
  { "LINEFROMWKB", 0, 0, 0, ""},
  { "LINESTRINGFROMTEXT", 0, 0, 0, ""},
  { "LINESTRINGFROMWKB", 0, 0, 0, ""},
  { "LOAD_FILE", 0, 0, 0, ""},
  { "LOCATE", 0, 0, 0, ""},
  { "LOG", 0, 0, 0, ""},
  { "LOG2", 0, 0, 0, ""},
  { "LOG10", 0, 0, 0, ""},
  { "LOWER", 0, 0, 0, ""},
  { "LPAD", 0, 0, 0, ""},
  { "LTRIM", 0, 0, 0, ""},
  { "MAKE_SET", 0, 0, 0, ""},
  { "MAKEDATE", 0, 0, 0, ""},
  { "MAKETIME", 0, 0, 0, ""},
  { "MASTER_GTID_WAIT", 0, 0, 0, ""},
  { "MASTER_POS_WAIT", 0, 0, 0, ""},
  { "MAX", 0, 0, 0, ""},
  { "MBRCONTAINS", 0, 0, 0, ""},
  { "MBRDISJOINT", 0, 0, 0, ""},
  { "MBREQUAL", 0, 0, 0, ""},
  { "MBRINTERSECTS", 0, 0, 0, ""},
  { "MBROVERLAPS", 0, 0, 0, ""},
  { "MBRTOUCHES", 0, 0, 0, ""},
  { "MBRWITHIN", 0, 0, 0, ""},
  { "MD5", 0, 0, 0, ""},
  { "MID", 0, 0, 0, ""},
  { "MIN", 0, 0, 0, ""},
  { "MLINEFROMTEXT", 0, 0, 0, ""},
  { "MLINEFROMWKB", 0, 0, 0, ""},
  { "MPOINTFROMTEXT", 0, 0, 0, ""},
  { "MPOINTFROMWKB", 0, 0, 0, ""},
  { "MPOLYFROMTEXT", 0, 0, 0, ""},
  { "MPOLYFROMWKB", 0, 0, 0, ""},
  { "MONTHNAME", 0, 0, 0, ""},
  { "MULTILINESTRINGFROMTEXT", 0, 0, 0, ""},
  { "MULTILINESTRINGFROMWKB", 0, 0, 0, ""},
  { "MULTIPOINTFROMTEXT", 0, 0, 0, ""},
  { "MULTIPOINTFROMWKB", 0, 0, 0, ""},
  { "MULTIPOLYGONFROMTEXT", 0, 0, 0, ""},
  { "MULTIPOLYGONFROMWKB", 0, 0, 0, ""},
  { "NAME_CONST", 0, 0, 0, ""},
  { "NOW", 0, 0, 0, ""},
  { "NULLIF", 0, 0, 0, ""},
  { "NUMGEOMETRIES", 0, 0, 0, ""},
  { "NUMINTERIORRINGS", 0, 0, 0, ""},
  { "NUMPOINTS", 0, 0, 0, ""},
  { "OCTET_LENGTH", 0, 0, 0, ""},
  { "OCT", 0, 0, 0, ""},
  { "ORD", 0, 0, 0, ""},
  { "OVERLAPS", 0, 0, 0, ""},
  { "PERIOD_ADD", 0, 0, 0, ""},
  { "PERIOD_DIFF", 0, 0, 0, ""},
  { "PI", 0, 0, 0, ""},
  { "POINTFROMTEXT", 0, 0, 0, ""},
  { "POINTFROMWKB", 0, 0, 0, ""},
  { "POINTN", 0, 0, 0, ""},
  { "POLYFROMTEXT", 0, 0, 0, ""},
  { "POLYFROMWKB", 0, 0, 0, ""},
  { "POLYGONFROMTEXT", 0, 0, 0, ""},
  { "POLYGONFROMWKB", 0, 0, 0, ""},
  { "POSITION", 0, 0, 0, ""},
  { "POW", 0, 0, 0, ""},
  { "POWER", 0, 0, 0, ""},
  { "QUOTE", 0, 0, 0, ""},
  { "RADIANS", 0, 0, 0, ""},
  { "RAND", 0, 0, 0, ""},
  { "RELEASE_LOCK", 0, 0, 0, ""},
  { "REVERSE", 0, 0, 0, ""},
  { "ROUND", 0, 0, 0, ""},
  { "ROW_COUNT", 0, 0, 0, ""},
  { "RPAD", 0, 0, 0, ""},
  { "RTRIM", 0, 0, 0, ""},
  { "SEC_TO_TIME", 0, 0, 0, ""},
  { "SESSION_USER", 0, 0, 0, ""},
  { "SUBDATE", 0, 0, 0, ""},
  { "SIGN", 0, 0, 0, ""},
  { "SIN", 0, 0, 0, ""},
  { "SHA", 0, 0, 0, ""},
  { "SHA1", 0, 0, 0, ""},
  { "SLEEP", 0, 0, 0, ""},
  { "SOUNDEX", 0, 0, 0, ""},
  { "SPACE", 0, 0, 0, ""},
  { "SQRT", 0, 0, 0, ""},
  { "SRID", 0, 0, 0, ""},
  { "STARTPOINT", 0, 0, 0, ""},
  { "STD", 0, 0, 0, ""},
  { "STDDEV", 0, 0, 0, ""},
  { "STDDEV_POP", 0, 0, 0, ""},
  { "STDDEV_SAMP", 0, 0, 0, ""},
  { "STR_TO_DATE", 0, 0, 0, ""},
  { "STRCMP", 0, 0, 0, ""},
  { "SUBSTR", 0, 0, 0, ""},
  { "SUBSTRING", 0, 0, 0, ""},
  { "SUBSTRING_INDEX", 0, 0, 0, ""},
  { "SUBTIME", 0, 0, 0, ""},
  { "SUM", 0, 0, 0, ""},
  { "SYSDATE", 0, 0, 0, ""},
  { "SYSTEM_USER", 0, 0, 0, ""},
  { "TAN", 0, 0, 0, ""},
  { "TIME_FORMAT", 0, 0, 0, ""},
  { "TIME_TO_SEC", 0, 0, 0, ""},
  { "TIMEDIFF", 0, 0, 0, ""},
  { "TO_DAYS", 0, 0, 0, ""},
  { "TOUCHES", 0, 0, 0, ""},
  { "TRIM", 0, 0, 0, ""},
  { "UCASE", 0, 0, 0, ""},
  { "UNCOMPRESS", 0, 0, 0, ""},
  { "UNCOMPRESSED_LENGTH", 0, 0, 0, ""},
  { "UNHEX", 0, 0, 0, ""},
  { "UNIQUE_USERS", 0, 0, 0, ""},
  { "UNIX_TIMESTAMP", 0, 0, 0, ""},
  { "UPPER", 0, 0, 0, ""},
  { "UUID", 0, 0, 0, ""},
  { "VARIANCE", 0, 0, 0, ""},
  { "VAR_POP", 0, 0, 0, ""},
  { "VAR_SAMP", 0, 0, 0, ""},
  { "VERSION", 0, 0, 0, ""},
  { "WEEKDAY", 0, 0, 0, ""},
  { "WEEKOFYEAR", 0, 0, 0, ""},
  { "WITHIN", 0, 0, 0, ""},
  { "X", 0, 0, 0, ""},
  { "Y", 0, 0, 0, ""},
  { "YEARWEEK", 0, 0, 0, ""},
  /* end sentinel */
  { (char *)NULL,       0, 0, 0, ""}
};

#define OB_MAX_NUM_LENGTH 128
#define OB_MAX_EXP_LENGTH 3

char obclient_num_array[OB_MAX_NUM_LENGTH + 2];

typedef struct STR_NORMAL_NUMBER{
  char num_info[OB_MAX_NUM_LENGTH + 2]; /* default start from 1 */
  short start_pos;
  short dot_pos; /* after positive num, -1 if not have positive num */
  short num_mark; /* 0 default, 1 contains '+', 2 contains '-' */
} STR_NORMAL_NUMBER;

typedef struct STR_NORMAL_NUMBER_BINARY{
  char num_info[OB_MAX_NUM_LENGTH + 2]; /* default start from 1 */
  short exp_info;
  short start_pos;
  short num_mark; /* 0 default, 1 contains '+', 2 contains '-' */
  short is_zero;
} STR_NORMAL_NUMBER_BINARY;

//STR_NUM num_object;
STR_NORMAL_NUMBER str_num_object;
STR_NORMAL_NUMBER_BINARY str_num_bin_object;

static const char *load_default_groups[]=
{ "mysql", "mariadb-client", "client", "client-server", "client-mariadb", 0 };

static int         embedded_server_arg_count= 0;
static char       *embedded_server_args[MAX_SERVER_ARGS];
static const char *embedded_server_groups[]=
{ "server", "embedded", "mysql_SERVER", "mariadb_SERVER", 0 };

#ifdef HAVE_READLINE
static int not_in_history(const char *line);
static void initialize_readline ();
static void fix_history(String *final_command);
#endif

static COMMANDS *find_command(char *name);
static COMMANDS *find_command(char cmd_name);
static bool add_line(String &, char *, size_t line_length, char *, bool *, bool *is_pl_escape_sql, bool);
static void remove_cntrl(String &buffer);
static void print_table_data(MYSQL_RES *result);
static void print_table_data_html(MYSQL_RES *result);
static void print_table_data_xml(MYSQL_RES *result);
static void print_tab_data(MYSQL_RES *result);
static void print_table_data_vertically(MYSQL_RES *result);
static void print_warnings(void);
static void end_timer(ulonglong start_time, char *buff);
static void nice_time(double sec,char *buff,bool part_second);
extern "C" sig_handler mysql_end(int sig) __attribute__ ((noreturn));
extern "C" sig_handler handle_sigint(int sig);
#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
static sig_handler window_resize(int sig);
#endif

static char* ob_service_name_str = NULL;

const char DELIMITER_NAME[]= "delimiter";
const uint DELIMITER_NAME_LEN= sizeof(DELIMITER_NAME) - 1;
inline bool is_delimiter_command(char *name, ulong len)
{
  /*
    Delimiter command has a parameter, so the length of the whole command
    is larger than DELIMITER_NAME_LEN.  We don't care the parameter, so
    only name(first DELIMITER_NAME_LEN bytes) is checked.
  */
  return (len >= DELIMITER_NAME_LEN &&
          !my_strnncoll(&my_charset_latin1, (uchar*) name, DELIMITER_NAME_LEN,
                        (uchar *) DELIMITER_NAME, DELIMITER_NAME_LEN));
}

/**
   Get the index of a command in the commands array.

   @param cmd_char    Short form command.

   @return int
     The index of the command is returned if it is found, else -1 is returned.
*/
inline int get_command_index(char cmd_char)
{
  /*
    All client-specific commands are in the first part of commands array
    and have a function to implement it.
  */
  for (uint i= 0; commands[i].func; i++)
    if (commands[i].cmd_char == cmd_char)
      return i;
  return -1;
}

static int delimiter_index= -1;
static int charset_index= -1;
static bool real_binary_mode= FALSE;

void mytoupper(char *s)
{
  int len = strlen(s);
  for (int i = 0; i<len; i++) {
    if (s[i] >= 'a' && s[i] <= 'z') {
      s[i] -= 32;
    }
  }
}

void init_re_comp(regex_t *re, const char* str)
{
  int err = regcomp(re, str, (REG_EXTENDED | REG_ICASE));
  if (err)
  {
    char erbuf[100];
    regerror(err, re, erbuf, sizeof(erbuf));
    put_info(erbuf, INFO_INFO);
  }
}

void init_dbms_output(void)
{
  const char *dbms_enable_re_str =
    "^("
    "[[:space:]]*SET[[:space:]]+SERVEROUTPUT[[:space:]]+ON[[:space:]]*)";

  const char *dbms_disable_re_str =
    "^("
    "[[:space:]]*SET[[:space:]]+SERVEROUTPUT[[:space:]]+OFF[[:space:]]*)";

  init_re_comp(&dbms_enable_re, dbms_enable_re_str);
  init_re_comp(&dbms_disable_re, dbms_disable_re_str);

  memset(&dbms_ps_params, 0, sizeof(dbms_ps_params));
  dbms_ps_params[0].buffer_type = MYSQL_TYPE_NULL;
  dbms_ps_params[0].is_null = 0;
  dbms_ps_params[1].buffer_type = MYSQL_TYPE_NULL;
  dbms_ps_params[1].is_null = 0;

  dbms_rs_bind[0].buffer = dbms_line_buffer;
  dbms_rs_bind[0].is_null = &dbms_rs_bind[0].is_null_value;
  dbms_rs_bind[0].buffer_length = sizeof(dbms_line_buffer);
  dbms_rs_bind[0].length = &dbms_rs_bind[0].length_value;
  dbms_rs_bind[1].buffer = &dbms_status;
  dbms_rs_bind[1].buffer_length = sizeof(dbms_status);

  dbms_enable_buffer.append(dbms_enable_sql, strlen(dbms_enable_sql));
  dbms_disable_buffer.append(dbms_disable_sql, strlen(dbms_disable_sql));
}

void free_dbms_output(){
  regfree(&dbms_enable_re);
  regfree(&dbms_disable_re);
}

void init_width_and_format_for_result_value(void)
{
  /* SET NUM (2 ~ 128) / SET NUMWIDTH (2 ~ 128) */
  /* SET NUMF format / SET NUMFORMAT format */
  /* COLUMN c1 FORMAT format*/

  const char *column_format_re_str = 
    "^("
    "[[:space:]]*COLUMN[[:space:]]+([[:alnum:]_])[[:space:]]+"
    "FORMAT[[:space:]]+([[:alnum:]_])[[:space:]]*)";
  init_re_comp(&column_format_re, column_format_re_str);
}

void free_width_and_format_for_result_value() {
  regfree(&column_format_re);
}

void init_pl_sql(void)
{
  const char *pl_create_sql_re_str =
    "^("
    //prefix create [or replace]
    "[[:space:]]*CREATE[[:space:]]+(OR[[:space:]]+REPLACE[[:space:]]+)?"
    // package( brackets 3 ~ 6 )
    "(VIEW|PROCEDURE|FUNCTION|PACKAGE([[:space:]]+BODY)?|TRIGGER|TYPE([[:space:]]+BODY)?|LIBRARY|QUEUE|JAVA[[:space:]]+(SOURCE|CLASS)|DIMENSION|ASSEMBLY|HIERARCHY|ATTRIBUTE[[:space:]]+DIMENSION|ANALYTIC VIEW)[[:space:]]+"
    // schema ( brackets 7 ~ 9)
    //"((\\\")?([[:alnum:]_]+)\\\"?\\.)?"
    "(([[:alnum:]$_]+)\\.|\\\"([^\"]*)\\\"\\.)?[[:space:]]*"
    // object ( brackets 10 ~ 12)
    //"(\\\")?([[:alnum:]_]+)\\\"?[[:space:]]*"
    "(([[:alnum:]$_]+)|\\\"([^\"]*)\\\")?[[:space:]]*"
    ")";

  //alter procedure TEST.proc_test2 compile;
  const char *pl_alter_sql_re_str = 
    "^("
    //prefix alter,
    "[[:space:]]*ALTER[[:space:]]+"
    // package( brackets 2 )
    "(VIEW|PROCEDURE|FUNCTION|PACKAGE)[[:space:]]+"
    // schema ( brackets 3 ~ 5)
    //"((\\\")?([[:alnum:]_]+)\\\"?\\.)?"
    "(([[:alnum:]$_]+)\\.|\\\"([^\"]*)\\\"\\.)?[[:space:]]*"
    // object ( brackets 6 ~ 8)
    //"(\\\")?([[:alnum:]_]+)\\\"?[[:space:]]*"
    "(([[:alnum:]$_]+)|\\\"([^\"]*)\\\")?[[:space:]]*"
    ")";

  const char *pl_show_errors_sql_re_str =
    "^("
    //prefix show err[ors]
    "[[:space:]]*SHOW[[:space:]]+ERR(OR|ORS)?"
    //package( brackets 4 ~ 7 )
    "([[:space:]]+(VIEW|PROCEDURE|FUNCTION|PACKAGE([[:space:]]+BODY)?|TRIGGER|TYPE([[:space:]]+BODY)?|LIBRARY|QUEUE|JAVA[[:space:]]+(SOURCE|CLASS)|DIMENSION|ASSEMBLY|HIERARCHY|ATTRIBUTE[[:space:]]+DIMENSION|ANALYTIC VIEW)[[:space:]]+"
    //schema( brackets 8 ~ 10 )
    "((\\\")?([[:alnum:]_]+)\\\"?\\.)?"
    //object( brackets 11 ~ 12 )
    "(\\\")?([[:alnum:]_]+)\\\"?)?"
    "[[:space:]]*"
    "$)";

  const char *pl_escape_sql_re_str =
    "^("
    // begin/declare support <<label>>
    "[[:space:]]*(<<[[:alnum:]_]+>>)?"
    // pl sql
    "[[:space:]]*(BEGIN|DECLARE)[[:space:]]*|"
    "[[:space:]]*CREATE[[:space:]]+(OR[[:space:]]+REPLACE[[:space:]]+)?"
    "(PROCEDURE|FUNCTION|PACKAGE([[:space:]]+BODY)?|TRIGGER|TYPE([[:space:]]+BODY)?)[[:space:]]+"
    ")";

  init_re_comp(&pl_create_sql_re, pl_create_sql_re_str);
  init_re_comp(&pl_alter_sql_re, pl_alter_sql_re_str);
  init_re_comp(&pl_show_errors_sql_re, pl_show_errors_sql_re_str);
  init_re_comp(&pl_escape_sql_re, pl_escape_sql_re_str);
}

void free_pl_sql(void)
{
  regfree(&pl_create_sql_re);
  regfree(&pl_alter_sql_re);
  regfree(&pl_show_errors_sql_re);
  regfree(&pl_escape_sql_re);
}

void free_pl()
{
  my_free(pl_last_object);
  my_free(pl_last_schema);
  my_free(pl_last_type);
  pl_last_object = NULL;
  pl_last_type = NULL;
  pl_last_schema = NULL;
}

void init_disable_commands() {
  int i = 0,j= 0;
  if (NULL != ob_disable_commands_str) {
    int len = strlen(ob_disable_commands_str);
    char *p = ob_disable_commands_str;

    memset(disable_comands, 0, sizeof(disable_comands));
    for (i = 0; i < len; i++) {
      if (p[i] == ',' || p[i] == ';') {
        if (j > 0)
          disable_command_count++;
        j = 0;
        continue;
      }
      if (j < COMMAND_NAME_LEN) {
        disable_comands[disable_command_count][j] = p[i];
        j++;
      }
    }
    if (j > 0)
      disable_command_count++;
  }
}
my_bool is_command_valid(const char* name) {
  int i = 0;
  my_bool ret = true;
  for (i = 0; i < disable_command_count; i++) {
    if (strlen(disable_comands[i]) == strlen(name) 
      && !strncasecmp(disable_comands[i], name, strlen(name))) {
      ret = false;
      break;
    }
  }
  return ret;
}

static void fmt_str(char* p_in, int p_in_len, char* p_out, int p_out_len){
  int i = 0, j = 0;
  while (*p_in != '\0' && i++ < p_in_len && j + 2<p_out_len){
    if (*p_in == '\''){
      *p_out++ = *p_in;
      *p_out++ = *p_in++;
      j += 2;
    } else {
      *p_out++ = *p_in++;
      j++;
    }
  }
}

int main(int argc,char *argv[])
{
  char buff[80];

  MY_INIT(argv[0]);
  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);
  
  charset_index= get_command_index('C');
  delimiter_index= get_command_index('d');
  delimiter_str= delimiter;
  default_prompt = my_strdup(getenv("MYSQL_PS1") ? 
			     getenv("MYSQL_PS1") : "obclient(\\u@\\T)[\\d]> ",MYF(MY_WME));
  current_prompt = my_strdup(default_prompt,MYF(MY_WME));
  prompt_counter=0;
  aborted= 0;
  sf_leaking_memory= 1; /* no memory leak reports yet */

  //keep a switch for set num width
  char* env = getenv("DISABLE_SET_NUM_WIDTH");
  if (env) {
    int tmp_val = atoi(env);
    if (tmp_val != 0)
    {
      force_set_num_width_close = 1;
    } else {
      force_set_num_width_close = 0;
    }
  }

  outfile[0]=0;			// no (default) outfile
  strmov(pager, "stdout");	// the default, if --pager wasn't given

  {
    char *tmp=getenv("PAGER");
    if (tmp && strlen(tmp))
    {
      default_pager_set= 1;
      strmov(default_pager, tmp);
    }
  }
  if (!isatty(0) || !isatty(1))
  {
    status.batch=1; opt_silent=1;
    ignore_errors=0;
  }
  else
    status.add_to_history=1;
  status.exit_status=1;

  {
    /* 
     The file descriptor-layer may be out-of-sync with the file-number layer,
     so we make sure that "stdout" is really open.  If its file is closed then
     explicitly close the FD layer. 
    */
    int stdout_fileno_copy;
    stdout_fileno_copy= dup(fileno(stdout)); /* Okay if fileno fails. */
    if (stdout_fileno_copy == -1)
      fclose(stdout);
    else
      close(stdout_fileno_copy);             /* Clean up dup(). */
  }

  load_defaults_or_exit("my", load_default_groups, &argc, &argv);
  defaults_argv=argv;
  if ((status.exit_status= get_options(argc, (char **) argv)))
  {
    free_defaults(defaults_argv);
    my_end(0);
    exit(status.exit_status);
  }

  if (status.batch && !status.line_buff &&
      !(status.line_buff= batch_readline_init(MAX_BATCH_BUFFER_SIZE, stdin)))
  {
    put_info("Can't initialize batch_readline - may be the input source is "
             "a directory or a block device.", INFO_ERROR, 0);
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }
  if (mysql_server_init(embedded_server_arg_count, embedded_server_args, 
                        (char**) embedded_server_groups))
  {
    put_error(NULL);
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }

  if (!opt_mysql_unix_port) {
    if (NULL == current_host)
      current_host = my_strdup("127.0.0.1", MYF(MY_WME));
    if (NULL == current_user)
      current_user = my_strdup("root@sys", MYF(MY_WME));
    if (0 == opt_mysql_port)
      opt_mysql_port = 2881;
  }

  preserve_comments = (skip_comments ? 0:1);
  sf_leaking_memory= 0;
  glob_buffer.realloc(512);
  completion_hash_init(&ht, 128);
  init_alloc_root(&hash_mem_root, "hash", 16384, 0, MYF(0));
  if (sql_connect(current_host,current_db,current_user,opt_password, opt_silent))
  {
    quick= 1;					// Avoid history
    status.exit_status= 1;
    mysql_end(-1);
  }
  if (!status.batch)
    ignore_errors=1;				// Don't abort monitor

  if (opt_sigint_ignore)
    signal(SIGINT, SIG_IGN);
  else
    signal(SIGINT, handle_sigint);              // Catch SIGINT to clean up
  signal(SIGQUIT, mysql_end);			// Catch SIGQUIT to clean up

#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
  /* Readline will call this if it installs a handler */
  signal(SIGWINCH, window_resize);
  /* call the SIGWINCH handler to get the default term width */
  window_resize(0);
#endif

  if (mysql.oracle_mode) {
    get_current_db();
  }

  if (!status.batch)
  {
    put_info("Welcome to the OceanBase.  Commands end with ; or \\g.",
             INFO_INFO);
    my_snprintf((char*) glob_buffer.ptr(), glob_buffer.alloced_length(),
            "Your OceanBase connection id is %lu\nServer version: %s\n",
            mysql_thread_id(&mysql), is_proxymode ? "":server_version_string(&mysql));
    put_info((char*) glob_buffer.ptr(),INFO_INFO);
    put_info(OB_WELCOME_COPYRIGHT_NOTICE("2000"), INFO_INFO);
  }

#ifdef HAVE_READLINE
  initialize_readline();
  if (!status.batch && !quick && !opt_html && !opt_xml)
  {
    /* read-history from file, default ~/.mysql_history*/
    if (getenv("MYSQL_HISTFILE"))
      histfile=my_strdup(getenv("MYSQL_HISTFILE"),MYF(MY_WME));
    else if (getenv("HOME"))
    {
      uint lentmp = (uint) strlen(getenv("HOME")) + (uint) strlen("/.mysql_history")+2;
      histfile=(char*) my_malloc(lentmp, MYF(MY_WME));
      if (histfile)
	snprintf(histfile, lentmp,"%s/.mysql_history",getenv("HOME"));
      char link_name[FN_REFLEN];
      if (my_readlink(link_name, histfile, 0) == 0 &&
          strncmp(link_name, "/dev/null", 10) == 0)
      {
        /* The .mysql_history file is a symlink to /dev/null, don't use it */
        my_free(histfile);
        histfile= 0;
      }
    }

    /* We used to suggest setting MYSQL_HISTFILE=/dev/null. */
    if (histfile && strncmp(histfile, "/dev/null", 10) == 0)
      histfile= NULL;

    if (histfile && histfile[0])
    {
      uint lentmp = (uint) strlen(histfile) + 5;
      if (verbose)
	tee_fprintf(stdout, "Reading history-file %s\n",histfile);
      read_history(histfile);
      if (!(histfile_tmp= (char*) my_malloc(lentmp,  MYF(MY_WME))))
      {
	fprintf(stderr, "Couldn't allocate memory for temp histfile!\n");
	exit(1);
      }
      snprintf(histfile_tmp, lentmp,"%s.TMP", histfile);
    }
  }

#endif

  snprintf(buff, sizeof(buff), "%s",
	  "Type 'help;' or '\\h' for help. Type '\\c' to clear the current input statement.\n");
  put_info(buff,INFO_INFO);

  init_dbms_output();
  init_pl_sql();
  init_width_and_format_for_result_value();
  init_disable_commands();

  //start glogin.sql
  start_login_sql(&mysql, &glob_buffer, get_login_sql_path());

  status.exit_status= read_and_execute(!status.batch);
  if (opt_outfile)
    end_tee();
  mysql_end(0);
#ifndef _lint
  DBUG_RETURN(0);				// Keep compiler happy
#endif
}

sig_handler mysql_end(int sig)
{
  if (is_exitcommit_oracle && mysql.oracle_mode) {
    mysql_commit(&mysql);
  }
  
#ifndef _WIN32
  /*
    Ignoring SIGQUIT and SIGINT signals when cleanup process starts.
    This will help in resolving the double free issues, which occurs in case
    the signal handler function is started in between the clean up function.
  */
  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);
#endif

  mysql_close(&mysql);
#ifdef HAVE_READLINE
  if (!status.batch && !quick && !opt_html && !opt_xml &&
      histfile && histfile[0])
  {
    /* write-history */
    if (verbose)
      tee_fprintf(stdout, "Writing history-file %s\n",histfile);
    if (!write_history(histfile_tmp))
      my_rename(histfile_tmp, histfile, MYF(MY_WME));
  }
  batch_readline_end(status.line_buff);
  completion_hash_free(&ht);
  free_root(&hash_mem_root,MYF(0));

#endif
  if (sig >= 0)
    put_info(sig ? "Aborted" : "Bye", INFO_RESULT);
  glob_buffer.free();
  old_buffer.free();
  processed_prompt.free();
  my_free(server_version);
  my_free(opt_password);
  my_free(opt_mysql_unix_port);
  my_free(histfile);
  my_free(histfile_tmp);
  my_free(current_db);
  my_free(current_host);
  my_free(current_user);
  my_free(full_username);
  my_free(part_username);
  my_free(default_prompt);
  my_free(current_prompt);
  my_free(current_host_success);
  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count]);
  mysql_server_end();
  free_defaults(defaults_argv);
  free_dbms_output();
  free_pl_sql();
  free_width_and_format_for_result_value();
  free_pl();
  free_map_global();
  my_end(my_end_arg);
  exit(status.exit_status);
}

/*
  set connection-specific options and call mysql_real_connect
*/
static bool do_connect(MYSQL *mysql, const char *host, const char *user,
                       const char *password, const char *database, ulong flags)
{
  bool is_success = 0;
  if (opt_secure_auth)
    mysql_options(mysql, MYSQL_SECURE_AUTH, (char *) &opt_secure_auth);
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  if (opt_use_ssl)
  {
    mysql_ssl_set(mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
    mysql_options(mysql, MYSQL_OPT_SSL_CRL, opt_ssl_crl);
    mysql_options(mysql, MYSQL_OPT_SSL_CRLPATH, opt_ssl_crlpath);
    mysql_options(mysql, MARIADB_OPT_TLS_VERSION, opt_tls_version);
  }
  mysql_options(mysql,MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                (char*)&opt_ssl_verify_server_cert);
#endif
  if (opt_protocol)
    mysql_options(mysql,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(mysql, MYSQL_DEFAULT_AUTH, opt_default_auth);

  mysql_options(mysql, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name", "mysql");

  //proxy mode
  if(is_proxymode) {
    mysql->ob_server_version = PROXY_MODE;
  }
  //oracle proxy user
  if (ob_proxy_user_str && ob_proxy_user_str[0]) {
    mysql_options(mysql, OB_OPT_PROXY_USER, ob_proxy_user_str);
  }
  if (mysql_real_connect(mysql, host, user, password, database, opt_mysql_port, opt_mysql_unix_port, flags)) {
    is_success = 1;
    current_host_success = my_strdup(host?host:"", MYF(MY_WME));
    current_port_success = opt_mysql_port;
  }
  return is_success;
}


/*
  This function handles sigint calls
  If query is in process, kill query
  If 'source' is executed, abort source command
  no query in process, terminate like previous behavior
 */

sig_handler handle_sigint(int sig)
{
  char kill_buffer[40];
  MYSQL *kill_mysql= NULL;

  /* terminate if no query being executed, or we already tried interrupting */
  if (!executing_query || (interrupted_query == 2))
  {
    tee_fprintf(stdout, "Ctrl-C -- exit!\n");
    goto err;
  }

  kill_mysql= mysql_init(kill_mysql);
  if (!do_connect(kill_mysql,current_host, current_user, opt_password, "", 0))
  {
    tee_fprintf(stdout, "Ctrl-C -- sorry, cannot connect to server to kill query, giving up ...\n");
    goto err;
  }

  /* First time try to kill the query, second time the connection */
  interrupted_query++;

  /* mysqld < 5 does not understand KILL QUERY, skip to KILL CONNECTION */
  if ((interrupted_query == 1) && (mysql_get_server_version(&mysql) < 50000))
    interrupted_query= 2;

  /* kill_buffer is always big enough because max length of %lu is 15 */
  snprintf(kill_buffer, sizeof(kill_buffer),"KILL %s%lu",
          (interrupted_query == 1) ? "QUERY " : "",
          mysql_thread_id(&mysql));
  if (verbose)
    tee_fprintf(stdout, "Ctrl-C -- sending \"%s\" to server ...\n",
                kill_buffer);
  mysql_real_query(kill_mysql, kill_buffer, (uint) strlen(kill_buffer));
  mysql_close(kill_mysql);
  tee_fprintf(stdout, "Ctrl-C -- query killed. Continuing normally.\n");
  if (in_com_source)
    aborted= 1;                                 // Abort source command
  return;

err:
#ifdef _WIN32
  /*
   When SIGINT is raised on Windows, the OS creates a new thread to handle the
   interrupt. Once that thread completes, the main thread continues running 
   only to find that it's resources have already been free'd when the sigint 
   handler called mysql_end(). 
  */
  mysql_thread_end();
#else
  mysql_end(sig);
#endif  
}


#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
sig_handler window_resize(int sig)
{
  struct winsize window_size;

  if (ioctl(fileno(stdin), TIOCGWINSZ, &window_size) == 0)
    if (window_size.ws_col > 0)
      terminal_width= window_size.ws_col;
}
#endif

static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"help", 'I', "Synonym for -?", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"abort-source-on-error", OPT_ABORT_SOURCE_ON_ERROR,
   "Abort 'source filename' operations in case of errors",
   &batch_abort_on_error, &batch_abort_on_error, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"auto-rehash", OPT_AUTO_REHASH,
   "Enable automatic rehashing. One doesn't need to use 'rehash' to get table "
   "and field completion, but startup and reconnecting may take a longer time. "
   "Disable with --disable-auto-rehash.",
   &opt_rehash, &opt_rehash, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0,
   0, 0},
  {"no-auto-rehash", 'A',
   "No automatic rehashing. One has to use 'rehash' to get table and field "
   "completion. This gives a quicker start of mysql and disables rehashing "
   "on reconnect.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
   {"auto-vertical-output", OPT_AUTO_VERTICAL_OUTPUT,
    "Automatically switch to vertical output mode if the result is wider "
    "than the terminal width.",
    &auto_vertical_output, &auto_vertical_output, 0, GET_BOOL, NO_ARG, 0,
    0, 0, 0, 0, 0},
  {"batch", 'B',
   "Don't use history file. Disable interactive behavior. (Enables --silent.)",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"binary-as-hex", 0, "Print binary data as hex", &opt_binhex, &opt_binhex,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory for character set files.", &charsets_dir,
   &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"column-type-info", OPT_COLUMN_TYPES, "Display column type information.",
   &column_types_flag, &column_types_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"comments", 'c', "Comment stripping is deprecated. ",
   &preserve_comments_invalid, &preserve_comments_invalid,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-comments", 0, "Preserve comments. Send comments to the server."
   " The default is comments, disable with --skip-comments", 
   &skip_comments, &skip_comments,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
   &opt_compress, &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit.",
   0,0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log.", &default_dbug_option,
   &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit.",
   &debug_check_flag, &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", 'T', "Print some debug info at exit.", &debug_info_flag,
   &debug_info_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"database", 'D', "Database to use.", &current_db,
   &current_db, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", &default_charset,
   &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"delimiter", OPT_DELIMITER, "Delimiter to be used.", &delimiter_str,
   &delimiter_str, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"execute", 'e', "Execute command and quit. (Disables --force and history file.)", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"vertical", 'E', "Print the output of a query (rows) vertically.",
   &vertical, &vertical, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"force", 'f', "Continue even if we get an SQL error. Sets abort-source-on-error to 0",
   &ignore_errors, &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"named-commands", 'G',
   "Enable named commands. Named commands mean this program's internal "
   "commands; see mysql> help . When enabled, the named commands can be "
   "used from any line of the query, otherwise only from the first line, "
   "before an enter. Disable with --disable-named-commands. This option "
   "is disabled by default.",
   &named_cmds, &named_cmds, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"ignore-spaces", 'i', "Ignore space after function names.",
   &ignore_spaces, &ignore_spaces, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"init-command", OPT_INIT_COMMAND,
   "SQL Command to execute when connecting to OceanBase server. Will "
   "automatically be re-executed when reconnecting.",
   &opt_init_command, &opt_init_command, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"local-infile", OPT_LOCAL_INFILE, "Enable/disable LOAD DATA LOCAL INFILE.",
   &opt_local_infile, &opt_local_infile, 0, GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},
  {"no-beep", 'b', "Turn off beep on error.", &opt_nobeep,
   &opt_nobeep, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", &current_host,
   &current_host, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"html", 'H', "Produce HTML output.", &opt_html, &opt_html,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"xml", 'X', "Produce XML output.", &opt_xml, &opt_xml, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"line-numbers", OPT_LINE_NUMBERS, "Write line numbers for errors.",
   &line_numbers, &line_numbers, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
  {"skip-line-numbers", 'L', "Don't write line number for errors.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"unbuffered", 'n', "Flush buffer after each query.", &unbuffered,
   &unbuffered, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"column-names", OPT_COLUMN_NAMES, "Write column names in results.",
   &column_names, &column_names, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
  {"skip-column-names", 'N',
   "Don't write column names in results.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sigint-ignore", OPT_SIGINT_IGNORE, "Ignore SIGINT (CTRL-C).",
   &opt_sigint_ignore,  &opt_sigint_ignore, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"one-database", 'o',
   "Ignore statements except those that occur while the default "
   "database is the one named at the command line.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef USE_POPEN
  {"pager", OPT_PAGER,
   "Pager to use to display results. If you don't supply an option, the "
   "default pager is taken from your ENV variable PAGER. Valid pagers are "
   "less, more, cat [> filename], etc. See interactive help (\\h) also. "
   "This option does not work in batch mode. Disable with --disable-pager. "
   "This option is disabled by default.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &opt_mysql_port,
   &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0,  0},
  {"progress-reports", OPT_REPORT_PROGRESS,
   "Get progress reports for long running commands (like ALTER TABLE)",
   &opt_progress_reports, &opt_progress_reports, 0, GET_BOOL, NO_ARG, 1, 0,
   0, 0, 0, 0},
  {"prompt", OPT_PROMPT, "Set the command line prompt to this value.",
   &current_prompt, &current_prompt, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol to use for connection (tcp, socket, pipe).",
   &opt_protocol_type, &opt_protocol_type, 0, GET_STR,  REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"quick", 'q',
   "Don't cache result, print it row by row. This may slow down the server "
   "if the output is suspended. Doesn't use history file.",
   &quick, &quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"raw", 'r', "Write fields without conversion. Used with --batch.",
   &opt_raw_data, &opt_raw_data, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"reconnect", OPT_RECONNECT, "Reconnect if the connection is lost. Disable "
   "with --disable-reconnect. This option is enabled by default.",
   &opt_reconnect, &opt_reconnect, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"silent", 's', "Be more silent. Print results with a tab as separator, "
   "each row on new line.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "The socket file to use for connection.",
   &opt_mysql_unix_port, &opt_mysql_unix_port, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include "sslopt-longopts.h"
  {"table", 't', "Output in table format.", &output_tables,
   &output_tables, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"tee", OPT_TEE,
   "Append everything into outfile. See interactive help (\\h) also. "
   "Does not work in batch mode. Disable with --disable-tee. "
   "This option is disabled by default.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", &current_user,
   &current_user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"safe-updates", 'U', "Only allow UPDATE and DELETE that uses keys.",
   &safe_updates, &safe_updates, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"i-am-a-dummy", 'U', "Synonym for option --safe-updates, -U.",
   &safe_updates, &safe_updates, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"verbose", 'v', "Write more. (-v -v -v gives the table output format).", 0,
   0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"wait", 'w', "Wait and retry if connection is down.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT,
   "Number of seconds before connection timeout.",
   &opt_connect_timeout, &opt_connect_timeout, 0, GET_ULONG, REQUIRED_ARG,
   0, 0, 3600*12, 0, 0, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   "The maximum packet length to send to or receive from server.",
   &opt_max_allowed_packet, &opt_max_allowed_packet, 0,
   GET_ULONG, REQUIRED_ARG, 16 *1024L*1024L, 4096,
   (longlong) 2*1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"net_buffer_length", OPT_NET_BUFFER_LENGTH,
   "The buffer size for TCP/IP and socket communication.",
   &opt_net_buffer_length, &opt_net_buffer_length, 0, GET_ULONG,
   REQUIRED_ARG, 16384, 1024, 512*1024*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"select_limit", OPT_SELECT_LIMIT,
   "Automatic limit for SELECT when using --safe-updates.",
   &select_limit, &select_limit, 0, GET_ULONG, REQUIRED_ARG, 1000L,
   1, ULONG_MAX, 0, 1, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   "Automatic limit for rows in a join when using --safe-updates.",
   &max_join_size, &max_join_size, 0, GET_ULONG, REQUIRED_ARG, 1000000L,
   1, ULONG_MAX, 0, 1, 0},
  {"secure-auth", OPT_SECURE_AUTH, "Refuse client connecting to server if it"
    " uses old (pre-4.1.1) protocol.", &opt_secure_auth,
    &opt_secure_auth, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"server-arg", OPT_SERVER_ARG, "Send embedded server this as a parameter.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"show-warnings", OPT_SHOW_WARNINGS, "Show warnings after every statement.",
    &show_warnings, &show_warnings, 0, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"plugin_dir", OPT_PLUGIN_DIR, "Directory for client-side plugins.",
    &opt_plugin_dir, &opt_plugin_dir, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default_auth", OPT_DEFAULT_AUTH,
    "Default authentication client-side plugin to use.",
    &opt_default_auth, &opt_default_auth, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"binary-mode", 0,
   "By default, ASCII '\\0' is disallowed and '\\r\\n' is translated to '\\n'. "
   "This switch turns off both features, and also turns off parsing of all client"
   "commands except \\C and DELIMITER, in non-interactive mode (for input "
   "piped to mysql or loaded using the 'source' command). This is necessary "
   "when processing output from mysqlbinlog that may contain blobs.",
   &opt_binary_mode, &opt_binary_mode, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"connect-expired-password", 0,
   "Notify the server that this client is prepared to handle expired "
   "password sandbox mode even if --batch was specified.",
   &opt_connect_expired_password, &opt_connect_expired_password, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  { "proxy-mode", OPT_PROXY_MODE, "Proxy mode is not support getObServerVersion.",
   &is_proxymode, &is_proxymode, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { "ob-disable-commands", OPT_OB_DISABLE_COMMANDS, "disable commands. example: --ob-disable-commands connect,conn",
    &ob_disable_commands_str,&ob_disable_commands_str, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "ob-service-name", OPT_OB_SERVICE_NAME, "service name. example: --ob-service-name TEST",
    &ob_service_name_str,&ob_service_name_str, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "proxy_user", OPT_OB_PROXY_USER , "proxy user. example --proxy_user test",
    &ob_proxy_user_str, &ob_proxy_user_str,0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "socket5_proxy", OPT_OB_SOCKET5_PROXY , "socket5 proxy. example --socket5_proxy [host],[port],[user],[pwd]",
    &ob_socket5_proxy_str, &ob_socket5_proxy_str,0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "error-sql", OPT_OB_ERROR_SQL, "--error-sql topN",
     &ob_error_top_str, &ob_error_top_str,0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage(int version)
{
#ifdef HAVE_READLINE
#if defined(USE_LIBEDIT_INTERFACE)
  const char* readline= "";
#else
  const char* readline= "readline";
#endif
  printf("%s  Ver %s Distrib %s, for %s (%s) using %s %s\n",
	 my_progname, VER, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE,
         readline, rl_library_version);
#else
  printf("%s  Ver %s Distrib %s, for %s (%s), source revision %s\n", my_progname, VER,
	MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE,SOURCE_REVISION);
#endif

  if (version)
    return;
  puts(OB_WELCOME_COPYRIGHT_NOTICE("2000"));
  printf("Usage: %s [OPTIONS] [database]\n", my_progname);
  print_defaults("my", load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}


my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case OPT_CHARSETS_DIR:
    strmake_buf(mysql_charsets_dir, argument);
    charsets_dir = mysql_charsets_dir;
    break;
  case OPT_DELIMITER:
    if (argument == disabled_my_option) 
    {
      strmov(delimiter, DEFAULT_DELIMITER);
    }
    else 
    {
      /* Check that delimiter does not contain a backslash */
      if (!strstr(argument, "\\")) 
      {
        strmake_buf(delimiter, argument);
      }
      else 
      {
        put_info("DELIMITER cannot contain a backslash character", INFO_ERROR);
        return 0;
      } 
    }
    delimiter_length= (uint)strlen(delimiter);
    delimiter_str= delimiter;
    is_user_specify_delimiter = TRUE;
    if (delimiter_length == 1 && delimiter && delimiter[0] == ';') {
      is_user_specify_delimiter = FALSE;
    }
    break;
  case OPT_LOCAL_INFILE:
    using_opt_local_infile=1;
    break;
  case OPT_TEE:
    if (argument == disabled_my_option)
    {
      if (opt_outfile)
	end_tee();
    }
    else
      init_tee(argument);
    break;
  case OPT_PAGER:
    if (argument == disabled_my_option)
      opt_nopager= 1;
    else
    {
      opt_nopager= 0;
      if (argument && strlen(argument))
      {
	default_pager_set= 1;
	strmake_buf(pager, argument);
	strmov(default_pager, pager);
      }
      else if (default_pager_set)
	strmov(pager, default_pager);
      else
	opt_nopager= 1;
    }
    break;
  case OPT_MYSQL_PROTOCOL:
#ifndef EMBEDDED_LIBRARY
    if (!argument[0])
      opt_protocol= 0;
    else if ((opt_protocol= find_type_with_warning(argument, &sql_protocol_typelib,
                                                   opt->name)) <= 0)
      exit(1);
#endif
    break;
  case OPT_SERVER_ARG:
#ifdef EMBEDDED_LIBRARY
    /*
      When the embedded server is being tested, the client needs to be
      able to pass command-line arguments to the embedded server so it can
      locate the language files and data directory.
    */
    if (!embedded_server_arg_count)
    {
      embedded_server_arg_count= 1;
      embedded_server_args[0]= (char*) "";
    }
    if (embedded_server_arg_count == MAX_SERVER_ARGS-1 ||
        !(embedded_server_args[embedded_server_arg_count++]=
          my_strdup(argument, MYF(MY_FAE))))
    {
        put_info("Can't use server argument", INFO_ERROR);
        return 0;
    }
#else /*EMBEDDED_LIBRARY */
    printf("WARNING: --server-arg option not supported in this configuration.\n");
#endif
    break;
  case 'A':
    opt_rehash= 0;
    break;
  case 'N':
    column_names= 0;
    break;
  case 'e':
    status.batch= 1;
    status.add_to_history= 0;
    if (!status.line_buff)
      ignore_errors= 0;                         // do it for the first -e only
    if (!(status.line_buff= batch_readline_command(status.line_buff, argument)))
      return 1;
    break;
  case 'o':
    if (argument == disabled_my_option)
      one_database= 0;
    else
      one_database= skip_updates= 1;
    break;
  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";			// Don't require password
    if (argument)
    {
      char *start= argument;
      my_free(opt_password);
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		// Destroy argument
      if (*start)
	start[1]=0 ;
      tty_password= 0;
    }
    else
      tty_password= 1;
    break;
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    debug_info_flag= 1;
    break;
  case 's':
    if (argument == disabled_my_option)
      opt_silent= 0;
    else
      opt_silent++;
    break;
  case 'v':
    if (argument == disabled_my_option)
      verbose= 0;
    else
      verbose++;
    break;
  case 'B':
    status.batch= 1;
    status.add_to_history= 0;
    set_if_bigger(opt_silent,1);                         // more silent
    break;
  case 'W':
#ifdef __WIN__
    opt_protocol = MYSQL_PROTOCOL_PIPE;
    opt_protocol_type= "pipe";
#endif
    break;
#include <sslopt-case.h>
  case 'f':
    batch_abort_on_error= 0;
    break;
  case 'V':
    usage(1);
    status.exit_status= 0;
    mysql_end(-1);
    break;
  case 'I':
  case '?':
    usage(0);
    status.exit_status= 0;
    mysql_end(-1);
  }
  return 0;
}


static int get_options(int argc, char **argv)
{
  char *tmp, *pagpoint;
  int ho_error;
  MYSQL_PARAMETERS *mysql_params= mysql_get_parameters();

  tmp= (char *) getenv("MYSQL_HOST");
  if (tmp)
    current_host= my_strdup(tmp, MYF(MY_WME));

  pagpoint= getenv("PAGER");
  if (!((char*) (pagpoint)))
  {
    strmov(pager, "stdout");
    opt_nopager= 1;
  }
  else
    strmov(pager, pagpoint);
  strmov(default_pager, pager);

  opt_max_allowed_packet= *mysql_params->p_max_allowed_packet;
  opt_net_buffer_length= *mysql_params->p_net_buffer_length;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    return(ho_error);

  *mysql_params->p_max_allowed_packet= opt_max_allowed_packet;
  *mysql_params->p_net_buffer_length= opt_net_buffer_length;

  if (status.batch) /* disable pager and outfile in this case */
  {
    strmov(default_pager, "stdout");
    strmov(pager, "stdout");
    opt_nopager= 1;
    default_pager_set= 0;
    opt_outfile= 0;
    opt_reconnect= 0;
    connect_flag= 0; /* Not in interactive mode */
    opt_progress_reports= 0;
  }
  
  if (argc > 1)
  {
    usage(0);
    exit(1);
  }
  if (argc == 1)
  {
    skip_updates= 0;
    my_free(current_db);
    current_db= my_strdup(*argv, MYF(MY_WME));
  }
  if (tty_password)
    opt_password= get_tty_password(NullS);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;

  if (ignore_spaces)
    connect_flag|= CLIENT_IGNORE_SPACE;

  if (opt_progress_reports)
    connect_flag|= CLIENT_PROGRESS_OBSOLETE;

  return(0);
}

static int read_and_execute(bool interactive)
{
#if defined(__WIN__)
  String tmpbuf;
  String buffer;
#endif

  char	*line= NULL;
  char	in_string=0;
  ulong line_number=0;
  bool ml_comment= 0;  
  COMMANDS *com;
  size_t line_length= 0;
  status.exit_status=1;
  
  real_binary_mode= !interactive && opt_binary_mode;
  while (!aborted)
  {
    if (!interactive)
    {
      /*
        batch_readline can return 0 on EOF or error.
        In that case, we need to double check that we have a valid
        line before actually setting line_length to read_length.
      */
      line= batch_readline(status.line_buff, real_binary_mode);
      if (line) 
      {
        line_length= status.line_buff->read_length;

        /*
          ASCII 0x00 is not allowed appearing in queries if it is not in binary
          mode.
        */
        if (!real_binary_mode && strlen(line) != line_length)
        {
          status.exit_status= 1;
          String msg;
          msg.append("ASCII '\\0' appeared in the statement, but this is not "
                     "allowed unless option --binary-mode is enabled and mysql is "
                     "run in non-interactive mode. Set --binary-mode to 1 if ASCII "
                     "'\\0' is expected. Query: '");
          msg.append(glob_buffer);
          msg.append(line);
          msg.append("'.");
          put_info(msg.c_ptr(), INFO_ERROR);
          break;
        }

        /*
          Skip UTF8 Byte Order Marker (BOM) 0xEFBBBF.
          Editors like "notepad" put this marker in
          the very beginning of a text file when
          you save the file using "Unicode UTF-8" format.
        */
        if (!line_number &&
             (uchar) line[0] == 0xEF &&
             (uchar) line[1] == 0xBB &&
             (uchar) line[2] == 0xBF)
        {
          line+= 3;
          // decrease the line length accordingly to the 3 bytes chopped
          line_length -=3;
        }
      }
      line_number++;
      if (!glob_buffer.length())
	status.query_start_line=line_number;
    }
    else
    {
      char *prompt= (char*) (ml_comment ? "   /*> " :
                             glob_buffer.is_empty() ?  construct_prompt() :
			     !in_string ? "    -> " :
			     in_string == '\'' ?
			     "    '> " : (in_string == '`' ?
			     "    `> " :
			     "    \"> "));
      if (opt_outfile && glob_buffer.is_empty())
	    fflush(OUTFILE);

#if defined(__WIN__)
      tee_fputs(prompt, stdout);
      if (!tmpbuf.is_alloced())
        tmpbuf.alloc(65535);
      tmpbuf.length(0);
      buffer.length(0);
      size_t clen;
      do
      {
	line= my_cgets((char*)tmpbuf.ptr(), tmpbuf.alloced_length()-1, &clen);
        buffer.append(line, clen);
        /* 
           if we got buffer fully filled than there is a chance that
           something else is still in console input buffer
        */
      } while (tmpbuf.alloced_length() <= clen);
      /* 
        An empty line is returned from my_cgets when there's error reading :
        Ctrl-c for example
      */
      if (line)
        line= buffer.c_ptr();
#else
      if (opt_outfile)
	fputs(prompt, OUTFILE);
      /*
        free the previous entered line.
        Note: my_free() cannot be used here as the memory was allocated under
        the readline/libedit library.
      */
      if (line)
        free(line);
      line= readline(prompt);
#endif /* defined(__WIN__) */

      /*
        When Ctrl+d or Ctrl+z is pressed, the line may be NULL on some OS
        which may cause coredump.
      */
      if (opt_outfile && line)
	fprintf(OUTFILE, "%s\n", line);

      line_length= line ? strlen(line) : 0;
    }
    // End of file or system error
    if (!line)
    {
      if (status.line_buff && status.line_buff->error)
        status.exit_status= 1;
      else
        status.exit_status= 0;
      break;
    }
    
    //skip space line(buffer is empty)
    {
      char *pos = NULL, *end_of_line = line + strlen(line);
      for (pos = line; pos < end_of_line; pos++) {
        uchar inchar = (uchar)*pos;
        if (my_isspace(charset_info, inchar) && glob_buffer.is_empty())
          continue;
        else
          break;
      }
      if (pos >= end_of_line)
        continue;
    }

    //support oracle mode prompt
    char *text = NULL;
    if (mysql.oracle_mode && !ml_comment && !in_string && glob_buffer.is_empty() && prompt_cmd_oracle(line, &text)) {
      if (text == NULL) {
        text = (char*)"";
      }
      tee_puts(text, stdout);
      continue;
    }

    //support oracle mode sqlblanklines, except pl 
    if (mysql.oracle_mode && !is_sqlblanklines_oracle && !glob_buffer.is_empty()){
      char *p = line;
      while (my_isspace(charset_info, *p)) p++;

      if (strlen(p) == 0 && 0 != (regexec(&pl_escape_sql_re, glob_buffer.c_ptr_safe(), 0, 0, 0))) {
        glob_buffer.length(0);
        continue;
      }
    }
    

    /*
      Check if line is a mysql command line
      (We want to allow help, print and clear anywhere at line start
    */
    if ((named_cmds || glob_buffer.is_empty())
	&& !ml_comment && !in_string && (com= find_command(line)))
    {
      if ((*com->func)(&glob_buffer,line) > 0)
	break;
      if (glob_buffer.is_empty())		// If buffer was emptied
	in_string=0;
#ifdef HAVE_READLINE
      if (interactive && status.add_to_history && not_in_history(line))
	add_history(line);
#endif
      continue;
    }
    if (add_line(glob_buffer, line, line_length, &in_string, &ml_comment, &is_pl_escape_sql,
                 status.line_buff ? status.line_buff->truncated : 0))
      break;
  }
  /* if in batch mode, send last query even if it doesn't end with \g or go */

  if (!interactive && !status.exit_status)
  {
    remove_cntrl(glob_buffer);
    if (!glob_buffer.is_empty())
    {
      status.exit_status=1;
      if (com_go(&glob_buffer,line) <= 0)
	status.exit_status=0;
    }
  }

#if defined(__WIN__)
  buffer.free();
  tmpbuf.free();
#else
  if (interactive)
    /*
      free the last entered line.
      Note: my_free() cannot be used here as the memory was allocated under
      the readline/libedit library.
    */
    free(line);
#endif

  /*
    If the function is called by 'source' command, it will return to interactive
    mode, so real_binary_mode should be FALSE. Otherwise, it will exit the
    program, it is safe to set real_binary_mode to FALSE.
  */
  real_binary_mode= FALSE;

  return status.exit_status;
}


/**
   It checks if the input is a short form command. It returns the command's
   pointer if a command is found, else return NULL. Note that if binary-mode
   is set, then only \C is searched for.

   @param cmd_char    A character of one byte.

   @return
     the command's pointer or NULL.
*/
static COMMANDS *find_command(char cmd_char)
{
  DBUG_ENTER("find_command");
  DBUG_PRINT("enter", ("cmd_char: %d", cmd_char));

  int index= -1;

  /*
    In binary-mode, we disallow all mysql commands except '\C'
    and DELIMITER.
  */
  if (real_binary_mode)
  {
    if (cmd_char == 'C')
      index= charset_index;
  }
  else
    index= get_command_index(cmd_char);

  if (index >= 0 && is_command_valid(commands[index].name))
  {
    DBUG_PRINT("exit",("found command: %s", commands[index].name));
    DBUG_RETURN(&commands[index]);
  }
  else
    DBUG_RETURN((COMMANDS *) 0);
}

/**
   It checks if the input is a long form command. It returns the command's
   pointer if a command is found, else return NULL. Note that if binary-mode 
   is set, then only DELIMITER is searched for.

   @param name    A string.
   @return
     the command's pointer or NULL.
*/
static COMMANDS *find_command(char *name)
{
  uint len;
  char *end;
  DBUG_ENTER("find_command");

  DBUG_ASSERT(name != NULL);
  DBUG_PRINT("enter", ("name: '%s'", name));

  while (my_isspace(charset_info, *name))
    name++;
  /*
    If there is an \\g in the row or if the row has a delimiter but
    this is not a delimiter command, let add_line() take care of
    parsing the row and calling find_command().
  */
  if ((!real_binary_mode && strstr(name, "\\g")) ||
      (strstr(name, delimiter) &&
       !is_delimiter_command(name, DELIMITER_NAME_LEN)))
      DBUG_RETURN((COMMANDS *) 0);

  if ((end=strcont(name, " \t")))
  {
    len=(uint) (end - name);
    while (my_isspace(charset_info, *end))
      end++;
    if (!*end)
      end= 0;					// no arguments to function
  }
  else
    len= (uint) strlen(name);

  int index= -1;
  if (real_binary_mode)
  {
    if (is_delimiter_command(name, len))
      index= delimiter_index;
  }
  else
  {
    /*
      All commands are in the first part of commands array and have a function
      to implement it.
    */
    for (uint i= 0; commands[i].func; i++)
    {
      if (!my_strnncoll(&my_charset_latin1, (uchar*) name, len,
                        (uchar*) commands[i].name, len) &&
          (commands[i].name[len] == '\0') &&
          (!end || (commands[i].takes_params && get_arg(name, CHECK))))
      {
        index= i;
        break;
      }
    }
  }

  if (index >= 0 && is_command_valid(commands[index].name))
  {
    DBUG_PRINT("exit", ("found command: %s", commands[index].name));
    DBUG_RETURN(&commands[index]);
  }
  DBUG_RETURN((COMMANDS *) 0);
}


static bool add_line(String &buffer, char *line, size_t line_length,
  char *in_string, bool *ml_comment, bool *is_pl_escape_sql, bool truncated)
{
  uchar inchar;
  char buff[80], *pos, *out;
  COMMANDS *com;
  bool need_space= 0;
  bool ss_comment= 0;
  bool is_slash = 0;
  bool is_cmd = 0;
  DBUG_ENTER("add_line");

  if (!line[0] && buffer.is_empty())
    DBUG_RETURN(0);
#ifdef HAVE_READLINE
  if (status.add_to_history && line[0] && not_in_history(line))
    add_history(line);
#endif
  char *end_of_line= line + line_length;
  
  switch (is_cmd_line(line, line_length)) {
  case IS_CMD_SLASH: is_slash = true; break;
  case IS_CMD_LINE: is_cmd = true; break;
  }

  if (mysql.oracle_mode) {
    int slash_cnt = 0;
    for (pos = line; pos < end_of_line; pos++) {
      if (my_isspace(charset_info, *pos) || *pos =='/') {
        if (*pos == '/')
          slash_cnt++;
      } else {
        break;
      }
    }

    //no delimiter, oracle mode / must execute
    if (!is_user_specify_delimiter || is_prefix("/", delimiter)) {
      if (is_slash && !buffer.is_empty()) {
        if ((com = find_command(buffer.c_ptr()))) {
          if ((*com->func)(&buffer, buffer.c_ptr()) > 0)
            DBUG_RETURN(1);                       // Quit 
        } else {
          if (com_go(&buffer, 0) > 0)             // < 0 is not fatal
            DBUG_RETURN(1);
        }
        buffer.length(0);
        *in_string = 0;
        *ml_comment = 0;
        *is_pl_escape_sql = 0;
        return 0;
      } else if (buffer.is_empty() && (is_slash || slash_cnt > 1)) {
        if (!last_execute_buffer.is_empty()) {
          String tmpbuffer = last_execute_buffer;
          if ((com = find_command(tmpbuffer.c_ptr()))) {
            if ((*com->func)(&buffer, tmpbuffer.c_ptr()) > 0)
              DBUG_RETURN(1);                       // Quit 
          } else {
            if (com_go(&tmpbuffer, 0) > 0)             // < 0 is not fatal
              DBUG_RETURN(1);
          }
          *in_string = 0;
          *ml_comment = 0;
          *is_pl_escape_sql = 0;
        } else {
          put_info("SP2-0103: Nothing in SQL buffer to run.", INFO_INFO);
        }
        return 0;
      }
    }
  }

  for (pos= out= line; pos < end_of_line; pos++)
  {
    inchar= (uchar) *pos;
    if (!preserve_comments)
    {
      // Skip spaces at the beginning of a statement
      if (my_isspace(charset_info,inchar) && (out == line) &&
          buffer.is_empty())
        continue;
    }
        
#ifdef USE_MB
    // Accept multi-byte characters as-is
    int length;
    if (use_mb(charset_info) &&
        (length= my_ismbchar(charset_info, pos, end_of_line)))
    {
      if (!*ml_comment || preserve_comments)
      {
        while (length--)
          *out++ = *pos++;
        pos--;
      }
      else
        pos+= length - 1;
      continue;
    }
#endif
    if (!*ml_comment && inchar == '\\' && *in_string != '`' &&
        !(*in_string == '"' &&
          (mysql.server_status & SERVER_STATUS_ANSI_QUOTES)) &&
        !(*in_string &&
          (mysql.oracle_mode || mysql.server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES)))
    {
      // Found possbile one character command like \c

      if (!(inchar = (uchar) *++pos))
	break;				// readline adds one '\'
      if (*in_string || inchar == 'N')	// \N is short for NULL
      {					// Don't allow commands in string
	*out++='\\';
	*out++= (char) inchar;
	continue;
      }
      //fix oracle mode select json_array('{"data":'\e'}') from dual;
      if (mysql.oracle_mode && (*is_pl_escape_sql || !is_cmd )) {
        *out++ = '\\';
        *out++ = (char)inchar;
        continue;
      }

      if ((com= find_command((char) inchar)))
      {
        // Flush previously accepted characters
        if (out != line)
        {
          buffer.append(line, (uint) (out-line));
          out= line;
        }
        
        if ((*com->func)(&buffer,pos-1) > 0)
          DBUG_RETURN(1);                       // Quit
        if (com->takes_params)
        {
          if (ss_comment)
          {
            /*
              If a client-side macro appears inside a server-side comment,
              discard all characters in the comment after the macro (that is,
              until the end of the comment rather than the next delimiter)
            */
            for (pos++; *pos && (*pos != '*' || *(pos + 1) != '/'); pos++)
              ;
            pos--;
          }
          else
          {
            for (pos++ ;
                 *pos && (*pos != *delimiter ||
                          !is_prefix(pos + 1, delimiter + 1)) ; pos++)
              ;	// Remove parameters
            if (!*pos)
              pos--;
            else 
              pos+= delimiter_length - 1; // Point at last delim char
          }
        }
      }
      else
      {
	sprintf(buff,"Unknown command '\\%c'.",inchar);
	if (put_info(buff,INFO_ERROR) > 0)
	  DBUG_RETURN(1);
	*out++='\\';
	*out++=(char) inchar;
	continue;
      }
    }
    else if (!*ml_comment && ((*in_string && mysql.oracle_mode && pos + 1 == end_of_line) || !*in_string) && 
      ((!*is_pl_escape_sql && is_prefix(pos, delimiter)) || (*is_pl_escape_sql && *pos == '/' && is_slash) ))
    {
      /*
       * ��������������Զ��ָ��ת��:
       * 1. ��� delimiter �������� /
       * 2. ����� Mysql ģʽ
       * 3. ����û�ָ���� delimiter,
       * ������������� is_pl_escape_sql, ����Ҫȥ�ж�һ��
       */
      if (mysql.oracle_mode
        && !is_user_specify_delimiter
        && !is_prefix("/", delimiter)
        && !*is_pl_escape_sql) {
        //�Ȱ�ǰ������ݶ�װ�� buffer ��, ��Ϊ�п����Ƿ����������
        if (out != line) {
          buffer.append(line, (uint32)(out - line));
          out = line;
        }

        if (0 == (regexec(&pl_escape_sql_re, buffer.c_ptr_safe(), 0, 0, 0))) {
          *is_pl_escape_sql = 1;
          //ԭ���ķָ���ҲҪ�Ž�ȥ
          buffer.append(pos, delimiter_length);
          //for ѭ�����1, ����������Ҫ��1
          pos += delimiter_length - 1;
          continue;
        }
      }
      
      // Found a statement. Continue parsing after the delimiter
      pos+= delimiter_length;

      if (preserve_comments)
      {
        while (my_isspace(charset_info, *pos))
          *out++= *pos++;
      }
      // Flush previously accepted characters
      if (out != line)
      {
        buffer.append(line, (uint32) (out-line));
        out= line;
      }

      if (preserve_comments && ((*pos == '#' && !mysql.oracle_mode) ||
                                (((*pos == '-') && (pos[1] == '-') && mysql.oracle_mode) ||
                                 ((*pos == '-') && (pos[1] == '-') && my_isspace(charset_info, pos[2])))))
      {
        // Add trailing single line comments to this statement
        buffer.append(pos);
        pos+= strlen(pos);
      }

      pos--;

      if ((com= find_command(buffer.c_ptr())))
      {
          
        if ((*com->func)(&buffer, buffer.c_ptr()) > 0)
          DBUG_RETURN(1);                       // Quit 
      }
      else
      {
        if (com_go(&buffer, 0) > 0)             // < 0 is not fatal
          DBUG_RETURN(1);
      }
      if (mysql.oracle_mode) {
        *in_string = 0;
      }
      buffer.length(0);
      *is_pl_escape_sql=0;
    }
    else if (!*ml_comment &&
             (!*in_string &&
              ((inchar == '#' && !mysql.oracle_mode)||
               (inchar == '-' && pos[1] == '-' && mysql.oracle_mode) ||
               (inchar == '-' && pos[1] == '-' &&
               /*
                 The third byte is either whitespace or is the end of
                 the line -- which would occur only because of the
                 user sending newline -- which is itself whitespace
                 and should also match.
                 We also ignore lines starting with '--', even if there
                 isn't a whitespace after. (This makes it easier to run
                 mysql-test-run cases through the client)
               */
                ((my_isspace(charset_info,pos[2]) || !pos[2]) ||
                 (buffer.is_empty() && out == line))))))
    {
      // Flush previously accepted characters
      if (out != line)
      {
        buffer.append(line, (uint32) (out - line));
        out= line;
      }

      // comment to end of line
      if (preserve_comments)
      {
        buffer.append(pos);

        /*
          A single-line comment by itself gets sent immediately so that
          client commands (delimiter, status, etc) will be interpreted on
          the next line.
        */
        if (is_global_comment(&buffer))
        {
          if (com_go(&buffer, 0) > 0)             // < 0 is not fatal
            DBUG_RETURN(1);
          buffer.length(0);
        }
      }

      break;
    }
    else if (!*in_string && inchar == '/' && *(pos+1) == '*' &&
      !(*(pos + 2) == '!' || *(pos + 2) == '+' || (*(pos + 2) == 'M' && *(pos + 3) == '!')))
    {
      if (preserve_comments)
      {
        *out++= *pos++;                       // copy '/'
        *out++= *pos;                         // copy '*'
      }
      else
        pos++;
      *ml_comment= 1;
      if (out != line)
      {
        buffer.append(line,(uint) (out-line));
        out=line;
      }
    }
    else if (*ml_comment && !ss_comment && inchar == '*' && *(pos + 1) == '/')
    {
      if (preserve_comments)
      {
        *out++= *pos++;                       // copy '*'
        *out++= *pos;                         // copy '/'
      }
      else
        pos++;
      *ml_comment= 0;
      if (out != line)
      {
        buffer.append(line, (uint32) (out - line));
        out= line;
      }

      if (preserve_comments) {
        if (is_global_comment(&buffer)) {
          if (com_go(&buffer, 0) > 0)             // < 0 is not fatal
            DBUG_RETURN(1);
          buffer.length(0);
        }
      }
      // Consumed a 2 chars or more, and will add 1 at most,
      // so using the 'line' buffer to edit data in place is ok.
      need_space= 1;
    }      
    else
    {						// Add found char to buffer
      if (!*in_string && inchar == '/' && *(pos + 1) == '*' &&
          (*(pos + 2) == '!' || *(pos+2)=='+'))
        ss_comment= 1;
      else if (!*in_string && ss_comment && inchar == '*' && *(pos + 1) == '/'){
        ss_comment= 0;
        *out++ = *pos++;                       // copy '*'
        inchar = (uchar)*pos;                 // later copy '/'
      }
      if (inchar == *in_string)
	*in_string= 0;
      else if (!*ml_comment && !*in_string &&
	       (inchar == '\'' || inchar == '"' || inchar == '`'))
	*in_string= (char) inchar;
      if (!*ml_comment || preserve_comments)
      {
        if (need_space && !my_isspace(charset_info, (char)inchar))
          *out++= ' ';
        need_space= 0;
        *out++= (char) inchar;
      }
    }
  }
  if (out != line || !buffer.is_empty())
  {
    uint length=(uint) (out-line);

    if (!truncated && (!is_delimiter_command(line, length) ||
                       (*in_string || *ml_comment)))
    {
      /* 
        Don't add a new line in case there's a DELIMITER command to be 
        added to the glob buffer (e.g. on processing a line like 
        "<command>;DELIMITER <non-eof>") : similar to how a new line is 
        not added in the case when the DELIMITER is the first command 
        entered with an empty glob buffer. However, if the delimiter is
        part of a string or a comment, the new line should be added. (e.g.
        SELECT '\ndelimiter\n';\n)
      */
      *out++='\n';
      length++;
    }
    if (buffer.length() + length >= buffer.alloced_length())
      buffer.realloc(buffer.length()+length+IO_SIZE);
    if ((!*ml_comment || preserve_comments) && buffer.append(line, length))
      DBUG_RETURN(1);
  }
  if (is_space_buffer(buffer.c_ptr(), buffer.c_ptr()+buffer.length())) {
    buffer.length(0);
  }
  DBUG_RETURN(0);
}

/*****************************************************************
	    Interface to Readline Completion
******************************************************************/

#ifdef HAVE_READLINE

C_MODE_START
static char *new_command_generator(const char *text, int);
static char **new_mysql_completion(const char *text, int start, int end);
C_MODE_END

/*
  Tell the GNU Readline library how to complete.  We want to try to complete
  on command names if this is the first word in the line, or on filenames
  if not.
*/

#if defined(USE_NEW_READLINE_INTERFACE) 
static int fake_magic_space(int, int);
extern "C" char *no_completion(const char*,int)
#elif defined(USE_LIBEDIT_INTERFACE)
static int fake_magic_space(const char *, int);
extern "C" int no_completion(const char*,int)
#else
extern "C" char *no_completion()
#endif
{
  return 0;					/* No filename completion */
}

/*	glues pieces of history back together if in pieces   */
static void fix_history(String *final_command) 
{
  int total_lines = 1;
  char *ptr = final_command->c_ptr();
  String fixed_buffer; 	/* Converted buffer */
  char str_char = '\0';  /* Character if we are in a string or not */
  
  /* find out how many lines we have and remove newlines */
  while (*ptr != '\0') 
  {
    switch (*ptr) {
      /* string character */
    case '"':
    case '\'':
    case '`':
      if (str_char == '\0')	/* open string */
	str_char = *ptr;
      else if (str_char == *ptr)   /* close string */
	str_char = '\0';
      fixed_buffer.append(ptr,1);
      break;
    case '\n':
      /* 
	 not in string, change to space
	 if in string, leave it alone 
      */
      fixed_buffer.append(str_char == '\0' ? " " : "\n");
      total_lines++;
      break;
    case '\\':
      fixed_buffer.append('\\');
      /* need to see if the backslash is escaping anything */
      if (str_char) 
      {
	ptr++;
	/* special characters that need escaping */
	if (*ptr == '\'' || *ptr == '"' || *ptr == '\\')
	  fixed_buffer.append(ptr,1);
	else
	  ptr--;
      }
      break;
      
    default:
      fixed_buffer.append(ptr,1);
    }
    ptr++;
  }
  if (total_lines > 1)			
    add_history(fixed_buffer.ptr());
}

/*	
  returns 0 if line matches the previous history entry
  returns 1 if the line doesn't match the previous history entry
*/
static int not_in_history(const char *line) 
{
  HIST_ENTRY *oldhist = history_get(history_length);
  
  if (oldhist == 0)
    return 1;
  if (strcmp(oldhist->line,line) == 0)
    return 0;
  return 1;
}


#if defined(USE_NEW_READLINE_INTERFACE)
static int fake_magic_space(int, int)
#else
static int fake_magic_space(const char *, int)
#endif
{
  rl_insert(1, ' ');
  return 0;
}


static void initialize_readline ()
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name= (char *) "mysql";
  rl_terminal_name= getenv("TERM");

  /* Tell the completer that we want a crack first. */
#if defined(USE_NEW_READLINE_INTERFACE)
  rl_attempted_completion_function= (rl_completion_func_t*)&new_mysql_completion;
  rl_completion_entry_function= (rl_compentry_func_t*)&no_completion;

  rl_add_defun("magic-space", (rl_command_func_t *)&fake_magic_space, -1);
#elif defined(USE_LIBEDIT_INTERFACE)
#ifdef HAVE_LOCALE_H
  setlocale(LC_ALL,""); /* so as libedit use isprint */
#endif
  rl_attempted_completion_function= (CPPFunction*)&new_mysql_completion;
  rl_completion_entry_function= &no_completion;
  rl_add_defun("magic-space", (Function*)&fake_magic_space, -1);
#else
  rl_attempted_completion_function= (CPPFunction*)&new_mysql_completion;
  rl_completion_entry_function= &no_completion;
#endif
}

/*
  Attempt to complete on the contents of TEXT.  START and END show the
  region of TEXT that contains the word to complete.  We can use the
  entire line in case we want to do some simple parsing.  Return the
  array of matches, or NULL if there aren't any.
*/

static char **new_mysql_completion(const char *text,
                                   int start __attribute__((unused)),
                                   int end __attribute__((unused)))
{
  if (!status.batch && !quick)
#if defined(USE_NEW_READLINE_INTERFACE)
    return rl_completion_matches(text, new_command_generator);
#else
    return completion_matches((char *)text, (CPFunction *)new_command_generator);
#endif
  else
    return (char**) 0;
}

static char *new_command_generator(const char *text,int state)
{
  static int textlen;
  char *ptr;
  static Bucket *b;
  static entry *e;
  static uint i;

  if (!state)
    textlen=(uint) strlen(text);

  if (textlen>0)
  {						/* lookup in the hash */
    if (!state)
    {
      uint len;

      b = find_all_matches(&ht,text,(uint) strlen(text),&len);
      if (!b)
	return NullS;
      e = b->pData;
    }

    if (e)
    {
      ptr= strdup(e->str);
      e = e->pNext;
      return ptr;
    }
  }
  else
  { /* traverse the entire hash, ugly but works */

    if (!state)
    {
      /* find the first used bucket */
      for (i=0 ; i < ht.nTableSize ; i++)
      {
	if (ht.arBuckets[i])
	{
	  b = ht.arBuckets[i];
	  e = b->pData;
	  break;
	}
      }
    }
    ptr= NullS;
    while (e && !ptr)
    {					/* find valid entry in bucket */
      if ((uint) strlen(e->str) == b->nKeyLength)
	ptr = strdup(e->str);
      /* find the next used entry */
      e = e->pNext;
      if (!e)
      { /* find the next used bucket */
	b = b->pNext;
	if (!b)
	{
	  for (i++ ; i<ht.nTableSize; i++)
	  {
	    if (ht.arBuckets[i])
	    {
	      b = ht.arBuckets[i];
	      e = b->pData;
	      break;
	    }
	  }
	}
	else
	  e = b->pData;
      }
    }
    if (ptr)
      return ptr;
  }
  return NullS;
}


/* Build up the completion hash */

static void build_completion_hash(bool rehash, bool write_info)
{
  COMMANDS *cmd=commands;
  MYSQL_RES *databases=0,*tables=0;
  MYSQL_RES *fields;
  static char ***field_names= 0;
  MYSQL_ROW database_row,table_row;
  MYSQL_FIELD *sql_field;
  char buf[NAME_LEN*2+2];		 // table name plus field name plus 2
  int i,j,num_fields;
  DBUG_ENTER("build_completion_hash");

  if (status.batch || quick || !current_db)
    DBUG_VOID_RETURN;			// We don't need completion in batches
  if (!rehash)
    DBUG_VOID_RETURN;

  /* Free old used memory */
  if (field_names)
    field_names=0;
  completion_hash_clean(&ht);
  free_root(&hash_mem_root,MYF(0));

  /* hash this file's known subset of SQL commands */
  while (cmd->name) {
    add_word(&ht,(char*) cmd->name);
    cmd++;
  }

  /* hash MySQL functions (to be implemented) */

  /* hash all database names */
  if (mysql_query(&mysql,"show databases") == 0)
  {
    if (!(databases = mysql_store_result(&mysql)))
      put_info(mysql_error(&mysql),INFO_INFO);
    else
    {
      while ((database_row=mysql_fetch_row(databases)))
      {
	char *str=strdup_root(&hash_mem_root, (char*) database_row[0]);
	if (str)
	  add_word(&ht,(char*) str);
      }
      mysql_free_result(databases);
    }
  }
  /* hash all table names */
  if (mysql_query(&mysql,"show tables")==0)
  {
    if (!(tables = mysql_store_result(&mysql)))
      put_info(mysql_error(&mysql),INFO_INFO);
    else
    {
      if (mysql_num_rows(tables) > 0 && !opt_silent && write_info)
      {
	tee_fprintf(stdout, "\
Reading table information for completion of table and column names\n\
You can turn off this feature to get a quicker startup with -A\n\n");
      }
      while ((table_row=mysql_fetch_row(tables)))
      {
	char *str=strdup_root(&hash_mem_root, (char*) table_row[0]);
	if (str &&
	    !completion_hash_exists(&ht,(char*) str, (uint) strlen(str)))
	  add_word(&ht,str);
      }
    }
  }

  /* hash all field names, both with the table prefix and without it */
  if (!tables)					/* no tables */
  {
    DBUG_VOID_RETURN;
  }
  mysql_data_seek(tables,0);
  if (!(field_names= (char ***) alloc_root(&hash_mem_root,sizeof(char **) *
					   (uint) (mysql_num_rows(tables)+1))))
  {
    mysql_free_result(tables);
    DBUG_VOID_RETURN;
  }
  i=0;
  while ((table_row=mysql_fetch_row(tables)))
  {
    if ((fields=mysql_list_fields(&mysql,(const char*) table_row[0],NullS)))
    {
      num_fields=mysql_num_fields(fields);
      if (!(field_names[i] = (char **) alloc_root(&hash_mem_root,
						  sizeof(char *) *
						  (num_fields*2+1))))
      {
        mysql_free_result(fields);
        break;
      }
      field_names[i][num_fields*2]= NULL;
      j=0;
      while ((sql_field=mysql_fetch_field(fields)))
      {
	sprintf(buf,"%.64s.%.64s",table_row[0],sql_field->name);
	field_names[i][j] = strdup_root(&hash_mem_root,buf);
	add_word(&ht,field_names[i][j]);
	field_names[i][num_fields+j] = strdup_root(&hash_mem_root,
						   sql_field->name);
	if (!completion_hash_exists(&ht,field_names[i][num_fields+j],
				    (uint) strlen(field_names[i][num_fields+j])))
	  add_word(&ht,field_names[i][num_fields+j]);
	j++;
      }
      mysql_free_result(fields);
    }
    else
      field_names[i]= 0;

    i++;
  }
  mysql_free_result(tables);
  field_names[i]=0;				// End pointer
  DBUG_VOID_RETURN;
}

	/* for gnu readline */

#ifndef HAVE_INDEX
extern "C" {
extern char *index(const char *,int c),*rindex(const char *,int);

char *index(const char *s,int c)
{
  for (;;)
  {
     if (*s == (char) c) return (char*) s;
     if (!*s++) return NullS;
  }
}

char *rindex(const char *s,int c)
{
  reg3 char *t;

  t = NullS;
  do if (*s == (char) c) t = (char*) s; while (*s++);
  return (char*) t;
}
}
#endif
#endif /* HAVE_READLINE */


static int reconnect(void)
{
  /* purecov: begin tested */
  if (opt_reconnect)
  {
    put_info("No connection. Trying to reconnect...",INFO_INFO);
    (void) com_connect((String *) 0, 0);
    if (opt_rehash)
      com_rehash(NULL, NULL);
  }
  if (!connected)
    return put_info("Can't connect to the server\n",INFO_ERROR);
  my_free(server_version);
  server_version= 0;
  /* purecov: end */
  return 0;
}

static void get_current_db()
{
  MYSQL_RES *res;
  const char *sql = NULL;

  /* If one_database is set, current_db is not supposed to change. */
  if (one_database)
    return;

  if (mysql.oracle_mode) {
    sql = "SELECT (SYS_CONTEXT('USERENV', 'CURRENT_SCHEMA')) FROM SYS.DUAL";
  } else {
    sql = "SELECT DATABASE()";
  }

  my_free(current_db);
  current_db= NULL;
  /* In case of error below current_db will be NULL */
  if (!mysql_query(&mysql, sql) && (res= mysql_use_result(&mysql)))
  {
    MYSQL_ROW row= mysql_fetch_row(res);
    if (row && row[0])
      current_db= my_strdup(row[0], MYF(MY_WME));
    mysql_free_result(res);
  }
}

/***************************************************************************
 The different commands
***************************************************************************/

int mysql_real_query_for_lazy(const char *buf, size_t length)
{
  for (uint retry=0;; retry++)
  {
    int error;
    if (!mysql_real_query(&mysql,buf,(ulong)length))
      return 0;
    error= put_error(&mysql);
    if (mysql_errno(&mysql) != CR_SERVER_GONE_ERROR || retry > 1 ||
        !opt_reconnect)
      return error;
    if (reconnect())
      return error;
  }
}

int mysql_store_result_for_lazy(MYSQL_RES **result)
{
  if ((*result=mysql_store_result(&mysql)))
    return 0;

  if (mysql_error(&mysql)[0])
    return put_error(&mysql);
  return 0;
}

static void print_help_item(MYSQL_ROW *cur, int num_name, int num_cat, char *last_char)
{
  char ccat= (*cur)[num_cat][0];
  if (*last_char != ccat)
  {
    put_info(ccat == 'Y' ? "categories:" : "topics:", INFO_INFO);
    *last_char= ccat;
  }
  tee_fprintf(PAGER, "   %s\n", (*cur)[num_name]);
}


static int com_server_help(String *buffer __attribute__((unused)),
			   char *line __attribute__((unused)), char *help_arg)
{
  MYSQL_ROW cur;
  const char *server_cmd;
  char cmd_buf[100 + 1];
  MYSQL_RES *result;
  int error;
  
  if (help_arg[0] != '\'')
  {
	char *end_arg= strend(help_arg);
	if(--end_arg)
	{
		while (my_isspace(charset_info,*end_arg))
          end_arg--;
		*++end_arg= '\0';
	}
	(void) strxnmov(cmd_buf, sizeof(cmd_buf), "help '", help_arg, "'", NullS);
  }
  else
    (void) strxnmov(cmd_buf, sizeof(cmd_buf), "help ", help_arg, NullS);

  server_cmd= cmd_buf;

  if (!status.batch)
  {
    old_buffer= *buffer;
    old_buffer.copy();
  }

  if (!connected && reconnect())
    return 1;

  if ((error= mysql_real_query_for_lazy(server_cmd,(int)strlen(server_cmd))) ||
      (error= mysql_store_result_for_lazy(&result)))
    return error;

  if (result)
  {
    unsigned int num_fields= mysql_num_fields(result);
    my_ulonglong num_rows= mysql_num_rows(result);
    if (num_fields==3 && num_rows==1)
    {
      if (!(cur= mysql_fetch_row(result)))
      {
	error= -1;
	goto err;
      }

      init_pager();
      tee_fprintf(PAGER,   "Name: \'%s\'\n", cur[0]);
      tee_fprintf(PAGER,   "Description:\n%s", cur[1]);
      if (cur[2] && *((char*)cur[2]))
	tee_fprintf(PAGER, "Examples:\n%s", cur[2]);
      tee_fprintf(PAGER,   "\n");
      end_pager();
    }
    else if (num_fields >= 2 && num_rows)
    {
      init_pager();
      char last_char= 0;

      int UNINIT_VAR(num_name), UNINIT_VAR(num_cat);
      num_name = 0;
      num_cat = 0;
      if (num_fields == 2)
      {
	put_info("Many help items for your request exist.", INFO_INFO);
	put_info("To make a more specific request, please type 'help <item>',\nwhere <item> is one of the following", INFO_INFO);
	num_name= 0;
	num_cat= 1;
      }
      else if ((cur= mysql_fetch_row(result)))
      {
	tee_fprintf(PAGER, "You asked for help about help category: \"%s\"\n", cur[0]);
	put_info("For more information, type 'help <item>', where <item> is one of the following", INFO_INFO);
	num_name= 1;
	num_cat= 2;
	print_help_item(&cur,1,2,&last_char);
      }

      while ((cur= mysql_fetch_row(result)))
	print_help_item(&cur,num_name,num_cat,&last_char);
      tee_fprintf(PAGER, "\n");
      end_pager();
    }
    else
    {
      put_info("\nNothing found", INFO_INFO);
      if (strncasecmp(server_cmd, "help 'contents'", 15) == 0)
      {
         put_info("\nPlease check if 'help tables' are loaded.\n", INFO_INFO); 
         goto err;
      }
      put_info("Please try to run 'help contents' for a list of all accessible topics\n", INFO_INFO);
    }
  }

err:
  mysql_free_result(result);
  return error;
}

static int
com_help(String *buffer __attribute__((unused)),
	 char *line __attribute__((unused)))
{
  int i, j;
  char * help_arg= strchr(line,' '), buff[32], *end;
  if (help_arg)
  {
    while (my_isspace(charset_info,*help_arg))
      help_arg++;
	if (*help_arg)	  
	  return com_server_help(buffer,line,help_arg);
  }

  put_info("\nGeneral information about OceanBase can be found at\n"
           "https://www.oceanbase.com\n", INFO_INFO);
  put_info("List of all client commands:", INFO_INFO);
  if (!named_cmds)
    put_info("Note that all text commands must be first on line and end with ';'",INFO_INFO);
  for (i = 0; commands[i].name; i++)
  {
    if (!is_command_valid(commands[i].name)) {
      continue;
    }
    end= strmov(buff, commands[i].name);
    for (j= (int)strlen(commands[i].name); j < 10; j++)
      end= strmov(end, " ");
    if (commands[i].func)
      tee_fprintf(stdout, "%s(\\%c) %s\n", buff,
		  commands[i].cmd_char, commands[i].doc);
  }
  if (connected && mysql_get_server_version(&mysql) >= 40100)
    put_info("\nFor server side help, type 'help contents'\n", INFO_INFO);
  return 0;
}


	/* ARGSUSED */
static int
com_clear(String *buffer,char *line __attribute__((unused)))
{
#ifdef HAVE_READLINE
  if (status.add_to_history)
    fix_history(buffer);
#endif
  buffer->length(0);
  return 0;
}

	/* ARGSUSED */
static int
com_charset(String *buffer __attribute__((unused)), char *line)
{
  char buff[256], *param;
  CHARSET_INFO * new_cs;
  strmake_buf(buff, line);
  param= get_arg(buff, GET);
  if (!param || !*param)
  {
    return put_info("Usage: \\C charset_name | charset charset_name", 
		    INFO_ERROR, 0);
  }
  new_cs= get_charset_by_csname(param, MY_CS_PRIMARY, MYF(MY_WME));
  if (new_cs)
  {
    charset_info= new_cs;
    mysql_set_character_set(&mysql, charset_info->csname);
    default_charset= (char *)charset_info->csname;
    put_info("Charset changed", INFO_INFO);
  }
  else put_info("Charset is not found", INFO_INFO);
  return 0;
}

/*
  Execute command
  Returns: 0  if ok
          -1 if not fatal error
	  1  if fatal error
*/


static int
com_go(String *buffer,char *line __attribute__((unused)))
{
  char		buff[200]; /* about 110 chars used so far */
  char		time_buff[53+3+1]; /* time max + space & parens + NUL */
  MYSQL_RES	*result;
  ulonglong	timer;
  ulong		warnings= 0;
  uint		error= 0;
  int           err= 0;

  interrupted_query= 0;
  if (!status.batch)
  {
    old_buffer= *buffer;			// Save for edit command
    old_buffer.copy();
  }

  /* Remove garbage for nicer messages */
  LINT_INIT_STRUCT(buff[0]);
  remove_cntrl(*buffer);

  if (buffer->is_empty())
  {
    if (status.batch)				// Ignore empty queries.
      return 0;
    return put_info("No query specified\n",INFO_ERROR);

  }
  if (!connected && reconnect())
  {
    buffer->length(0);				// Remove query on error
    return opt_reconnect ? -1 : 1;          // Fatal error
  }

  if (mysql.oracle_mode){
    //oracle mode source supprt echo
    if (verbose || (is_echo_oracle && in_com_source)) {
      if (is_termout_oracle_enable(&mysql))
        (void)com_print(buffer, 0);
    }
  } else {
    if (verbose)
      (void)com_print(buffer, 0);
  }

  if (mysql.oracle_mode) {
    String newbuffer;
    /*set cmd oracle return 1 support set cmd ok, 0 unknown cmd*/
    if (set_cmd_oracle(buffer->c_ptr_safe()))
      return 0;
    if (set_param_oracle(buffer->c_ptr_safe()))
      return 0;

    //oracle mode define '&' replace
    if (is_define_oracle){
      newbuffer.length(0);
      if (scan_define_oracle(buffer, &newbuffer)){
        buffer->length(0);
        buffer->append(newbuffer);
      }
    }

    //execute ...
    newbuffer.length(0);
    if (exec_cmd_oracle(buffer, &newbuffer)) {
      buffer->length(0);
      buffer->append(newbuffer);
    }

    //handle pl_show_errors_sql_re, pl_create_sql_re
    handle_oracle_sql(&mysql, buffer);
  }

  if (skip_updates &&
      (buffer->length() < 4 || my_strnncoll(charset_info,
					    (const uchar*)buffer->ptr(),4,
					    (const uchar*)"SET ",4)))
  {
    (void) put_info("Ignoring query to other database",INFO_INFO);
    return 0;
  }

  timer= microsecond_interval_timer();
  executing_query= 1;
  
  error = mysql_real_query_for_lazy(buffer->ptr(), buffer->length());
  report_progress_end();

  last_execute_buffer.length(0);
  last_execute_buffer.append(*buffer);

#ifdef HAVE_READLINE
  if (status.add_to_history) 
  {  
    buffer->append(vertical ? "\\G" : is_pl_escape_sql ? "/":delimiter);
    /* Append final command onto history */
    fix_history(buffer);
  }
#endif

  buffer->length(0);

  if (error)
    goto end;

  do
  {
    char *pos;

    if (quick)
    {
      if (!(result=mysql_use_result(&mysql)) && mysql_field_count(&mysql))
      {
        error= put_error(&mysql);
        goto end;
      }
    }
    else
    {
      error= mysql_store_result_for_lazy(&result);
      if (error)
        goto end;
    }

    if (verbose >= 3 || !opt_silent)
      end_timer(timer, time_buff);
    else
      time_buff[0]= '\0';

    /* Every branch must truncate buff. */
    if (result)
    {
      if (!mysql_num_rows(result) && ! quick && !column_types_flag)
      {
	strmov(buff, "Empty set");
        if (opt_xml)
        { 
          /*
            We must print XML header and footer
            to produce a well-formed XML even if
            the result set is empty (Bug#27608).
          */
          init_pager();
          print_table_data_xml(result);
          end_pager();
        }
      }
      else
      {
	init_pager();
	if (opt_html)
	  print_table_data_html(result);
	else if (opt_xml)
	  print_table_data_xml(result);
        else if (vertical || (auto_vertical_output &&
                (terminal_width < get_result_width(result))))
	  print_table_data_vertically(result);
	else if (opt_silent && verbose <= 2 && !output_tables)
	  print_tab_data(result);
	else
	  print_table_data(result);
	snprintf(buff, sizeof(buff), "%ld %s in set",
		(long) mysql_num_rows(result),
		(long) mysql_num_rows(result) == 1 ? "row" : "rows");
	end_pager();
        if (mysql_errno(&mysql))
          error= put_error(&mysql);
      }
    }
    else if (mysql_affected_rows(&mysql) == ~(ulonglong) 0)
      strmov(buff,"Query OK");
    else
      snprintf(buff, sizeof(buff), "Query OK, %ld %s affected",
	      (long) mysql_affected_rows(&mysql),
	      (long) mysql_affected_rows(&mysql) == 1 ? "row" : "rows");

    pos=strend(buff);
    if ((warnings= mysql_warning_count(&mysql)))
    {
      *pos++= ',';
      *pos++= ' ';
      pos=int10_to_str(warnings, pos, 10);
      pos=strmov(pos, " warning");
      if (warnings != 1)
	*pos++= 's';
    }
    strmov(pos, time_buff);

    if (mysql.oracle_mode){
      if (is_feedback_oracle && is_termout_oracle_enable(&mysql)){
        if (feedback_int_oracle <= 0){
          put_info(buff, INFO_RESULT);
        } else if (!result){
          put_info(buff, INFO_RESULT);
        } else if (result && (mysql_num_rows(result)==0 || mysql_num_rows(result) >= (my_ulonglong)feedback_int_oracle)){
          put_info(buff, INFO_RESULT);
        }
      }
    } else {
      put_info(buff, INFO_RESULT);
    }

    if (is_termout_oracle_enable(&mysql)) {
      if (mysql_info(&mysql))
        put_info(mysql_info(&mysql), INFO_RESULT);
      put_info("", INFO_RESULT);			// Empty row
    }

    if (result && !mysql_eof(result))	/* Something wrong when using quick */
      error= put_error(&mysql);
    else if (unbuffered)
      fflush(stdout);
    mysql_free_result(result);
  } while (!(err= mysql_next_result(&mysql)));
  if (err >= 1)
    error= put_error(&mysql);

end:

  //print error sql
  if (error && ob_error_top_str) {
    print_error_sqlstr(&last_execute_buffer, ob_error_top_str);
  }

 /* Show warnings if any or error occurred */
  if (!mysql.oracle_mode && show_warnings == 1 && (warnings >= 1 || error))
    print_warnings();

  if (!error && !status.batch && 
      (mysql.server_status & SERVER_STATUS_DB_DROPPED))
    get_current_db();

  if (dbms_serveroutput) {
    call_dbms_get_line(&mysql, error);
  }

  executing_query= 0;
  return error;				/* New command follows */
}


static void init_pager()
{
#ifdef USE_POPEN
  if (!opt_nopager)
  {
    if (!(PAGER= popen(pager, "w")))
    {
      tee_fprintf(stdout, "popen() failed! defaulting PAGER to stdout!\n");
      PAGER= stdout;
    }
  }
  else
#endif
    PAGER= stdout;
}

static void end_pager()
{
#ifdef USE_POPEN
  if (!opt_nopager)
    pclose(PAGER);
#endif
}


static void init_tee(const char *file_name)
{
  FILE* new_outfile;
  if (opt_outfile)
    end_tee();
  if (!(new_outfile= my_fopen(file_name, O_APPEND | O_WRONLY, MYF(MY_WME))))
  {
    tee_fprintf(stdout, "Error logging to file '%s'\n", file_name);
    return;
  }
  OUTFILE = new_outfile;
  strmake_buf(outfile, file_name);
  tee_fprintf(stdout, "Logging to file '%s'\n", file_name);
  opt_outfile= 1;
  return;
}


static void end_tee()
{
  my_fclose(OUTFILE, MYF(0));
  OUTFILE= 0;
  opt_outfile= 0;
  return;
}


static int
com_ego(String *buffer,char *line)
{
  int result;
  bool oldvertical=vertical;
  vertical=1;
  result=com_go(buffer,line);
  vertical=oldvertical;
  return result;
}


static const char *fieldtype2str(enum enum_field_types type)
{
  switch (type) {
    case MYSQL_TYPE_BIT:         return "BIT";
    case MYSQL_TYPE_BLOB:        return "BLOB";
    case MYSQL_TYPE_ORA_BLOB:    return "OB_BLOB";
    case MYSQL_TYPE_ORA_CLOB:    return "OB_CLOB";
    case MYSQL_TYPE_OB_RAW:      return "RAW";
    case MYSQL_TYPE_OB_INTERVAL_YM:  return "INTERVAL_YEAR_TO_MONTH";
    case MYSQL_TYPE_OB_INTERVAL_DS:  return "INTERVAL_DAY_TO_SECOND";
    case MYSQL_TYPE_OB_NUMBER_FLOAT: return "MYSQL_TYPE_OB_NUMBER_FLOAT";
    case MYSQL_TYPE_OB_NVARCHAR2:    return "MYSQL_TYPE_OB_NVARCHAR2";
    case MYSQL_TYPE_OB_NCHAR:        return "MYSQL_TYPE_OB_NCHAR";
    case MYSQL_TYPE_DATE:        return "DATE";
    case MYSQL_TYPE_DATETIME:    return "DATETIME";
    case MYSQL_TYPE_NEWDECIMAL:  return "NEWDECIMAL";
    case MYSQL_TYPE_DECIMAL:     return "DECIMAL";
    case MYSQL_TYPE_DOUBLE:      return "DOUBLE";
    case MYSQL_TYPE_ENUM:        return "ENUM";
    case MYSQL_TYPE_FLOAT:       return "FLOAT";
    case MYSQL_TYPE_GEOMETRY:    return "GEOMETRY";
    case MYSQL_TYPE_INT24:       return "INT24";
    case MYSQL_TYPE_LONG:        return "LONG";
    case MYSQL_TYPE_LONGLONG:    return "LONGLONG";
    case MYSQL_TYPE_LONG_BLOB:   return "LONG_BLOB";
    case MYSQL_TYPE_MEDIUM_BLOB: return "MEDIUM_BLOB";
    case MYSQL_TYPE_NEWDATE:     return "NEWDATE";
    case MYSQL_TYPE_NULL:        return "NULL";
    case MYSQL_TYPE_SET:         return "SET";
    case MYSQL_TYPE_SHORT:       return "SHORT";
    case MYSQL_TYPE_STRING:      return "STRING";
    case MYSQL_TYPE_TIME:        return "TIME";
    case MYSQL_TYPE_TIMESTAMP:   return "TIMESTAMP";
    case MYSQL_TYPE_OB_TIMESTAMP_WITH_TIME_ZONE:         return "TIMESTAMP_WITH_TIME_ZONE";
    case MYSQL_TYPE_OB_TIMESTAMP_WITH_LOCAL_TIME_ZONE:   return "TIMESTAMP_WITH_LOCAL_TIME_ZONE";
    case MYSQL_TYPE_OB_TIMESTAMP_NANO:                   return "TIMESTAMP_NANO";
    case MYSQL_TYPE_TINY:        return "TINY";
    case MYSQL_TYPE_TINY_BLOB:   return "TINY_BLOB";
    case MYSQL_TYPE_VAR_STRING:  return "VAR_STRING";
    case MYSQL_TYPE_YEAR:        return "YEAR";
    case MYSQL_TYPE_CURSOR:      return "CURSOR";
    case MYSQL_TYPE_OB_UROWID:   return "UROWID";
    default:                     return "?-unknown-?";
  }
}

static char *fieldflags2str(uint f) {
  static char buf[1024];
  char *s=buf;
  *s=0;
#define ff2s_check_flag(X) \
                if (f & X ## _FLAG) { s=strmov(s, # X " "); f &= ~ X ## _FLAG; }
  ff2s_check_flag(NOT_NULL);
  ff2s_check_flag(PRI_KEY);
  ff2s_check_flag(UNIQUE_KEY);
  ff2s_check_flag(MULTIPLE_KEY);
  ff2s_check_flag(BLOB);
  ff2s_check_flag(UNSIGNED);
  ff2s_check_flag(ZEROFILL);
  ff2s_check_flag(BINARY);
  ff2s_check_flag(ENUM);
  ff2s_check_flag(AUTO_INCREMENT);
  ff2s_check_flag(TIMESTAMP);
  ff2s_check_flag(SET);
  ff2s_check_flag(NO_DEFAULT_VALUE);
  ff2s_check_flag(NUM);
  ff2s_check_flag(PART_KEY);
  ff2s_check_flag(GROUP);
  ff2s_check_flag(BINCMP);
  ff2s_check_flag(ON_UPDATE_NOW);
#undef ff2s_check_flag
  if (f)
    sprintf(s, " unknows=0x%04x", f);
  return buf;
}

static void
print_field_types(MYSQL_RES *result)
{
  MYSQL_FIELD   *field;
  uint i=0;

  while ((field = mysql_fetch_field(result)))
  {
    tee_fprintf(PAGER, "Field %3u:  `%s`\n"
                       "Catalog:    `%s`\n"
                       "Database:   `%s`\n"
                       "Table:      `%s`\n"
                       "Org_table:  `%s`\n"
                       "Type:       %s\n"
                       "Collation:  %s (%u)\n"
                       "Length:     %lu\n"
                       "Max_length: %lu\n"
                       "Decimals:   %u\n"
                       "Flags:      %s\n\n",
                ++i,
                field->name, field->catalog, field->db, field->table,
                field->org_table, fieldtype2str(field->type),
                get_charset_name(field->charsetnr), field->charsetnr,
                field->length, field->max_length, field->decimals,
                fieldflags2str(field->flags));
  }
  tee_puts("", PAGER);
}


/* Used to determine if we should invoke print_as_hex for this field */

static bool
is_binary_field(MYSQL_FIELD *field)
{
  if ((field->charsetnr == 63) &&
      (field->type == MYSQL_TYPE_BIT ||
       field->type == MYSQL_TYPE_BLOB ||
       field->type == MYSQL_TYPE_LONG_BLOB ||
       field->type == MYSQL_TYPE_MEDIUM_BLOB ||
       field->type == MYSQL_TYPE_TINY_BLOB ||
       field->type == MYSQL_TYPE_VAR_STRING ||
       field->type == MYSQL_TYPE_OB_NVARCHAR2 ||
       field->type == MYSQL_TYPE_OB_NCHAR ||
       field->type == MYSQL_TYPE_STRING ||
       field->type == MYSQL_TYPE_VARCHAR ||
       field->type == MYSQL_TYPE_GEOMETRY))
    return 1;
  return 0;
}

static bool
is_binary_field_oracle(MYSQL_FIELD *field)
{
  if ((field->charsetnr == 63) &&
    (field->type == MYSQL_TYPE_BLOB ||
    field->type == MYSQL_TYPE_LONG_BLOB ||
    field->type == MYSQL_TYPE_MEDIUM_BLOB ||
    field->type == MYSQL_TYPE_TINY_BLOB))
    return 1;
  return 0;
}

/* Print binary value as hex literal (0x ...) */

static void
print_as_hex(FILE *output_file, const char *str, size_t len, size_t total_bytes_to_send)
{
  const char *ptr= str, *end= ptr+len;
  size_t i;
  fprintf(output_file, "0x");
  for(; ptr < end; ptr++)
    fprintf(output_file, "%02X", *((uchar*)ptr));
  for (i= 2*len+2; i < total_bytes_to_send; i++)
    tee_putc((int)' ', output_file);
}

static void
print_as_hex_oracle(FILE *output_file, const char *str, ulong len, ulong total_bytes_to_send)
{
  const char *ptr = str, *end = ptr + len;
  ulong i;
  for (; ptr < end; ptr++)
    fprintf(output_file, "%02X", *((uchar*)ptr));
  for (i = 2 * len; i < total_bytes_to_send; i++)
    tee_putc((int)' ', output_file);
}

int auto_incr(char *str, int pos) {
  int overflow = 0;
  int i = pos;
  for (; i >= 0; i--) {
    if (str[i] == '.')
      continue;
    if (str[i] < '9' && str[i] >= '0') {
      str[i] += 1;
      overflow = 0;
      break;
    } else if (str[i] == '9') {
      str[i] = '0';
      overflow = 1;
      continue;
    } else {
      break;
    }
  }
  return overflow;
}

static int
parse_normal_number_object_from_str(const char *str, unsigned long len,
    STR_NORMAL_NUMBER *str_num_object)
{
  int my_ret = 0;
  unsigned long i = 0, j = 1;
  int positive = 1;
  if (NULL != str_num_object && NULL != str) {
    memset(str_num_object, 0, sizeof(STR_NORMAL_NUMBER));
    str_num_object->dot_pos = -1;
    if (len > 0) {
      switch (str[0]) {
      case '+':
        str_num_object->num_mark = 1;
        i++;
        break;
      case '-':
        i++;
        str_num_object->num_mark = 2;
        break;
      case '.':
        positive = 0;
        str_num_object->dot_pos = -1;
        str_num_object->num_mark = 0;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        str_num_object->num_mark = 0;
        break;
      default:
        my_ret = -1;
        break;
      }
      for (; -1 != my_ret && positive && i < len; i++) {
        if (str[i] == '.') {
          positive = 0;
        }
        if (str[i] != '0')
          break;
      }
      for (; -1 != my_ret && i < len; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
          if (positive)
            str_num_object->dot_pos = j;
          str_num_object->num_info[j++] = str[i];
        } else if (str[i] == '.') {
          positive = 0;
        } else {
          my_ret = -1;
        }
      }
      str_num_object->start_pos = 1;
      for (i = j; i > 0; i--){
        if ((unsigned long)(str_num_object->dot_pos - 1) > i || str_num_object->num_info[i] != '0')
           break;
        else
          str_num_object->num_info[i] = 0;
      }
    }
  }

  return my_ret;
}

static int
parse_binary_number_object_from_str(const char *str, unsigned long len, 
    STR_NORMAL_NUMBER_BINARY *str_num_bin_object)
{
  int my_ret = 0;
  unsigned long i = 0, j = 1;
  if (NULL != str_num_bin_object && NULL != str) {
    memset(str_num_bin_object, 0, sizeof(STR_NORMAL_NUMBER_BINARY));
    if (len > 0) {
      switch (str[0]) {
      case '+':
        str_num_bin_object->num_mark = 1;
        i++;
        break;
      case '-':
        i++;
        str_num_bin_object->num_mark = 2;
        break;
      case '.':
        str_num_bin_object->num_mark = 0;
        break;
      case '0':
        str_num_bin_object->is_zero = 1;
        break;
      case '1':
      case '2':
      case '3':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        str_num_bin_object->num_mark = 0;
        if (len < 8 || str[1] != '.') {
          my_ret = -1;
        } else {
          str_num_bin_object->num_info[j++] = str[0];
          i+=2;
        }
        break;
      default:
        my_ret = -1;
        break;
      }

      if (!str_num_bin_object->is_zero) {
        for (; -1 != my_ret && i < len; i++) {
          if (str[i] >= '0' && str[i] <= '9') {
            str_num_bin_object->num_info[j++] = str[i];
          } else if (str[i] == '.') {
            continue;
          } else if (str[i] == 'E') {
            if (len < i + 5 || (str[i+1] != '+' && str[i+1] != '-') ||
               (str[i+2] < '0' || str[i+2] > '9') ||
               (str[i+3] < '0' || str[i+3] > '9') ||
               (str[i+4] < '0' || str[i+4] > '9')) {
              my_ret = -1;
            } else {
              str_num_bin_object->exp_info = 100 * (str[i+2] - '0');
              str_num_bin_object->exp_info += 10 * (str[i+3] - '0');
              str_num_bin_object->exp_info += (str[i+3] - '0');
              if (str[i+1] == '-')
                str_num_bin_object->exp_info = -1 * str_num_bin_object->exp_info;
            }
            break;
          } else {
            my_ret = -1;
          }
        }
        str_num_bin_object->start_pos = 1;
        for (i = j-1; i > 0; i--) {
          if ((unsigned long)(str_num_bin_object->start_pos) >= i || str_num_bin_object->num_info[i] != '0')
             break;
          else
            str_num_bin_object->num_info[i] = 0;
        }
      }
    }
  }
  return my_ret;
}

static void
format_fix_width_num(const char *str, unsigned long len)
{
  if (2 > obclient_num_width || 128 < obclient_num_width) {
    obclient_num_width = 10;
  }
  memset(obclient_num_array, 0, obclient_num_width + 1);
  if (0 == parse_normal_number_object_from_str(str, len, &str_num_object)) {
    int syb_len = str_num_object.num_mark == 2 ? 1 : 0;
    int fwidth = obclient_num_width - syb_len;
    int offset = 0;
    int i = 0;
    if (syb_len > 0) {
      obclient_num_array[offset++] = '-';
    }

    const char *str_tmp = str_num_object.num_info + str_num_object.start_pos;
    if (strlen(str_tmp) == 0) { /* 1. it is 0 */
      obclient_num_array[0] = '0';
    } else if (str_num_object.dot_pos > 0) {
      if ((int)strlen(str_tmp) < fwidth
             || ((int)(strlen(str_tmp)) == fwidth && (int)(strlen(str_tmp)) == str_num_object.dot_pos)) {
        /*
         * 1.format lenth < width; 2.format lenth == width and is integer
         * */
        //Exp. 11123456 fwidth 8  str_num_object.dot_pos 8
        memcpy(obclient_num_array + offset, str_tmp, str_num_object.dot_pos);
        offset += str_num_object.dot_pos;
        if ((int)strlen(str_tmp) > str_num_object.dot_pos) {
          obclient_num_array[offset++] = '.';
          memcpy(obclient_num_array + offset, str_tmp + str_num_object.dot_pos, strlen(str_tmp) - str_num_object.dot_pos);
        }
      } else { /* need add */
        //Exp.1 12345678.9012 fwidth 8  dot 8 other: 99999999.9
        //Exp.2 1234567.89012 fwidth 8  dot 7
        //Exp.3 123456.789012 fwidth 8  dot 6
        // 11.00123435  fwidth 8 dot 2
        if (fwidth >= (int)(str_num_object.dot_pos - str_num_object.start_pos + 1)) {
          if (fwidth - (str_num_object.dot_pos - str_num_object.start_pos + 1) < 2) { // 0 / 1, only integer
            if (str_num_object.num_info[str_num_object.dot_pos + 1] > '4') {
              if(auto_incr(str_num_object.num_info + str_num_object.start_pos, str_num_object.dot_pos - str_num_object.start_pos)) {
                str_num_object.num_info[0] = '1';
                str_num_object.start_pos = 0;
              }
            }  
            str_num_object.num_info[str_num_object.dot_pos + 1] = 0;
          } else {
            if (str_num_object.num_info[fwidth - 1 + str_num_object.start_pos] > '4') {
              if(auto_incr(str_num_object.num_info + str_num_object.start_pos, fwidth - 1)) {
                str_num_object.num_info[0] = '1';
                str_num_object.start_pos = 0;
              }
            }
            str_num_object.num_info[fwidth - 1 + str_num_object.start_pos] = 0;
          }

          for (i = fwidth - 1 + str_num_object.start_pos - 1; i > str_num_object.dot_pos ; i--) {
            if (str_num_object.num_info[i] != '0')
              break;
            str_num_object.num_info[i] = 0;
          }

          if (str_num_object.start_pos == 0 && str_num_object.dot_pos - str_num_object.start_pos + 1 > fwidth) { //1.00E+XX
            int exp_len = str_num_object.dot_pos - str_num_object.start_pos;
            if (exp_len > 100) {
              memset(obclient_num_array, '#', obclient_num_width);
            } else {
              obclient_num_array[offset++] = '1';
              obclient_num_array[offset++] = '.';
              while(offset < fwidth - 4) {
                obclient_num_array[offset++] = '0';
              }
              obclient_num_array[offset++] = 'E';
              obclient_num_array[offset++] = '+';
              obclient_num_array[offset++] = '0' + exp_len/10;
              obclient_num_array[offset++] = '0' + exp_len%10;
            }
          } else if (strlen(str_num_object.num_info + str_num_object.start_pos) == 0) {
            obclient_num_array[offset++] = '0';
          } else {
            memcpy(obclient_num_array + offset, str_num_object.num_info + str_num_object.start_pos, str_num_object.dot_pos - str_num_object.start_pos + 1);
            offset += str_num_object.dot_pos - str_num_object.start_pos + 1;
            if (strlen(str_num_object.num_info + str_num_object.dot_pos + 1) > 0) {
              obclient_num_array[offset++] = '.';
              memcpy(obclient_num_array + offset, str_num_object.num_info + str_num_object.dot_pos + 1, strlen(str_num_object.num_info + str_num_object.dot_pos + 1));
              offset += strlen(str_num_object.num_info + str_num_object.start_pos + 1);
            }
          }
        } else { /* fwidth < str_num_object.dot_pos */
          //Exp.1 123456789.012 fwidth 8
          if (fwidth < 7) {
            memset(obclient_num_array, '#', obclient_num_width);
          } else {
            int valid_pos = fwidth - 5;
            if (str_num_object.num_info[valid_pos + 1] > '4'
                  && auto_incr(str_num_object.num_info + str_num_object.start_pos, valid_pos + 1)) {
              str_num_object.num_info[0] = '1';
              str_num_object.start_pos = 0;
            }

            //2343434343  fwidth 8
            str_num_object.num_info[str_num_object.start_pos + valid_pos] = 0;

            obclient_num_array[offset++] = str_num_object.num_info[str_num_object.start_pos];
            obclient_num_array[offset++] = '.';
            if (strlen(str_num_object.num_info + str_num_object.start_pos + 1) == 0) {
              obclient_num_array[offset++] = '0';
            } else {
              memcpy(obclient_num_array + offset, str_num_object.num_info + str_num_object.start_pos + 1,
                  strlen(str_num_object.num_info + str_num_object.start_pos + 1));
              offset += strlen(str_num_object.num_info + str_num_object.start_pos + 1);
            }

            int exp_len = str_num_object.dot_pos - str_num_object.start_pos;
            if (exp_len > 100) {
              memset(obclient_num_array, '#', obclient_num_width);
            } else {
              while(offset < fwidth - 4) {
                obclient_num_array[offset++] = '0';
              }
              obclient_num_array[offset++] = 'E';
              obclient_num_array[offset++] = '+';
              obclient_num_array[offset++] = '0' + exp_len / 10;
              obclient_num_array[offset++] = '0' + exp_len % 10;
            }
          }
        }
      }
    } else { /* only has decimal, such as .00001 | -.2343 */
      // example: -.00123  fwidth = 7
      if ((int)(strlen(str_num_object.num_info + str_num_object.start_pos)) <= fwidth - 1) {
        obclient_num_array[offset++] = '.';
        memcpy(obclient_num_array + offset, str_num_object.num_info + str_num_object.start_pos, strlen(str_num_object.num_info + str_num_object.start_pos));
        offset += strlen(str_num_object.num_info + str_num_object.start_pos);
      } else {
        int valid_pos = 0;
        int zero_prefix_len = 0;
        for (i = str_num_object.start_pos; i < (int)strlen(str_num_object.num_info + str_num_object.start_pos); i++) {
          if (str_num_object.num_info[i] == '0')
            zero_prefix_len++;
          else
            break;
        }
        if (fwidth - 1 - zero_prefix_len < fwidth - 5 && fwidth >= 7) { // convert it to x.xxE-xx
          // Exp.1: -.0000001234 fwidth = 7 zero_prefix_len 6
          valid_pos = fwidth - 5 + zero_prefix_len; //skip zero
          if (str_num_object.num_info[str_num_object.start_pos + valid_pos] > '4' &&
              auto_incr(str_num_object.num_info + str_num_object.start_pos, valid_pos)) {
            obclient_num_array[offset++] = '1'; //not to be here
          } else {
            str_num_object.num_info[str_num_object.start_pos + valid_pos] = 0;
            for (i = valid_pos - 1 + str_num_object.start_pos; i > 0; i--) {
              if (str_num_object.num_info[i] == '0')
                str_num_object.num_info[i] = 0;
              else
                break;
            }
            if (str_num_object.num_info[zero_prefix_len - 1 + str_num_object.start_pos] != '0') {
              zero_prefix_len--;
            }
            obclient_num_array[offset++] = str_num_object.num_info[zero_prefix_len + str_num_object.start_pos];
            obclient_num_array[offset++] = '.';
            if (strlen(str_num_object.num_info + zero_prefix_len + 1 + str_num_object.start_pos) == 0) {
              obclient_num_array[offset++] = '0';
            } else {
              memcpy(obclient_num_array+offset, str_num_object.num_info + zero_prefix_len  + 1 + str_num_object.start_pos,
                       strlen(str_num_object.num_info + zero_prefix_len + 1 + str_num_object.start_pos));
              offset += strlen(str_num_object.num_info + zero_prefix_len + 1 + str_num_object.start_pos);

              int exp_len = zero_prefix_len + 1;
              if (exp_len > 100) {
                memset(obclient_num_array, '#', obclient_num_width);
              } else {
                obclient_num_array[offset++] = 'E';
                obclient_num_array[offset++] = '-';
                obclient_num_array[offset++] = '0' + exp_len/10;
                obclient_num_array[offset++] = '0' + exp_len%10;
              }
            }
          }
        } else {
          // Exp.2: -.001234556  fwidth = 7 zero_prefix_len 2 valid_pos 4
          valid_pos = fwidth - 1 - zero_prefix_len;
          if (valid_pos < 0) { // -.0000000001234 -> '0'
            memset(obclient_num_array, 0, obclient_num_width);
            obclient_num_array[0] = '0';
          } else if (str_num_object.num_info[str_num_object.start_pos + fwidth - 1] > '4' &&
                auto_incr(str_num_object.num_info + str_num_object.start_pos, fwidth - 1)) {
            obclient_num_array[offset++] = '1';
          } else {
            str_num_object.num_info[str_num_object.start_pos + fwidth - 1] = 0;
            for (i = fwidth - 2 + str_num_object.start_pos; i >= str_num_object.start_pos ; i--) {
              if (str_num_object.num_info[i] == '0')
                str_num_object.num_info[i] = 0;
              else
                break;
            }
            if (strlen(str_num_object.num_info + str_num_object.start_pos) == 0) {
              memset(obclient_num_array, 0, obclient_num_width);
              obclient_num_array[offset++] = '0';
            } else {
              obclient_num_array[offset++] = '.';
              memcpy(obclient_num_array+offset, str_num_object.num_info + str_num_object.start_pos,
                       strlen(str_num_object.num_info + str_num_object.start_pos));
              offset += strlen(str_num_object.num_info + str_num_object.start_pos);
            }
          }
        }
      }
    }
  } else {
    memset(obclient_num_array, '#', obclient_num_width);
  }
}

static void
format_fix_width_num_binary(const char *str, ulong len)
{
  if (2 > obclient_num_width || 128 < obclient_num_width) {
    obclient_num_width = 10;
  }
  memset(obclient_num_array, 0, obclient_num_width + 1);
  if (0 == parse_binary_number_object_from_str(str, len, &str_num_bin_object)) {
    if (str_num_bin_object.is_zero) {
      obclient_num_array[0] = '0';
    } else {
      int syb_len = str_num_object.num_mark == 2 ? 1 : 0;
      int offset = 0;
      int i = 0;
      int valid_size = 0;
      if (syb_len > 0) {
        obclient_num_array[offset++] = '-';
      }
      valid_size = obclient_num_width - syb_len - 5 - 1; // -1.11E+003
      if (valid_size < 2) {
        memset(obclient_num_array, '#', obclient_num_width);
      } else {
        if (valid_size < (int)strlen(str_num_bin_object.num_info + str_num_bin_object.start_pos)) {
          if (str_num_bin_object.num_info[str_num_bin_object.start_pos + valid_size] > '4') {
            if (auto_incr(str_num_bin_object.num_info + str_num_bin_object.start_pos,
                    str_num_bin_object.start_pos + valid_size)) {
              str_num_bin_object.start_pos = 0;
              str_num_bin_object.num_info[0] = '1';
            }
          }
          str_num_bin_object.num_info[str_num_bin_object.start_pos + valid_size] = 0;
          for (i = strlen(str_num_bin_object.num_info +  str_num_bin_object.start_pos) - 1;
               i > str_num_bin_object.start_pos; i--) {
            if (str_num_bin_object.num_info[i] != '0')
              break;
            else {
              str_num_bin_object.num_info[i] = 0;
            }
          }
        }
        

        obclient_num_array[offset++] = str_num_bin_object.num_info[str_num_bin_object.start_pos];
        obclient_num_array[offset++] = '.';
        if (strlen(str_num_bin_object.num_info + str_num_bin_object.start_pos + 1) > 0) {
          memcpy(obclient_num_array + offset, str_num_bin_object.num_info + str_num_bin_object.start_pos + 1,
              strlen(str_num_bin_object.num_info + str_num_bin_object.start_pos + 1));
          offset += strlen(str_num_bin_object.num_info + str_num_bin_object.start_pos + 1);
        } else {
          obclient_num_array[offset++] = '0';
        }

        if (0 == str_num_bin_object.start_pos)
          str_num_bin_object.exp_info += 1;

        obclient_num_array[offset++] = 'E';
        if (str_num_bin_object.exp_info > 999 || str_num_bin_object.exp_info < -999) {
          memset(obclient_num_array, '#', obclient_num_width);
        } else {
          if (str_num_bin_object.exp_info >= 0) {
            obclient_num_array[offset++] = '+';
          } else {
            obclient_num_array[offset++] = '-';
            str_num_bin_object.exp_info = -1 * str_num_bin_object.exp_info;
          }
          obclient_num_array[offset++] = '0' + str_num_bin_object.exp_info / 100;
          obclient_num_array[offset++] = '0' + (str_num_bin_object.exp_info % 100) / 10;
          obclient_num_array[offset++] = '0' + str_num_bin_object.exp_info % 10;
        }
      }
    }
  } else {
    memset(obclient_num_array, '#', obclient_num_width);
  }
}

static void
print_table_data(MYSQL_RES *result)
{
  String separator(256);
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;
  bool		*num_flag;

  num_flag=(bool*) my_alloca(sizeof(bool)*mysql_num_fields(result));
  if (column_types_flag)
  {
    print_field_types(result);
    if (!mysql_num_rows(result))
      return;
    mysql_field_seek(result,0);
  }
  separator.copy("+",1,charset_info);
  while ((field = mysql_fetch_field(result)))
  {
    uint length= column_names ? field->name_length : 0;
    if (quick) {
      if (mysql.oracle_mode) {
        // for oracle mode, use max_length, max_length for use result change
        length= MY_MAX(length, field->max_length);
      } else {
        length= MY_MAX(length,field->length);
      }
    } else
      length= MY_MAX(length,field->max_length);
    if (length < 4 /*&& !IS_NOT_NULL(field->flags)*/)
      length=4;					// Room for "NULL"
    if (mysql.oracle_mode && is_binary_field_oracle(field)) {
      length = length * 2;
    } else if (opt_binhex && is_binary_field(field)) {
      length = 2 + length * 2;
    }
    field->max_length=length;
    num_flag[mysql_field_tell(result) - 1]= IS_NUM(field->type);
    separator.fill(separator.length()+length+2,'-');
    separator.append('+');
  }
  separator.append('\0');                       // End marker for \0

  if (is_termout_oracle_enable(&mysql)) {
    tee_puts((char*)separator.ptr(), PAGER);
    if (column_names)
    {
      mysql_field_seek(result, 0);
      (void)tee_fputs("|", PAGER);
      for (uint off = 0; (field = mysql_fetch_field(result)); off++)
      {
        size_t name_length = (uint)strlen(field->name);
        size_t numcells = charset_info->cset->numcells(charset_info,
          field->name,
          field->name + name_length);
        size_t display_length = field->max_length + name_length - numcells;
        tee_fprintf(PAGER, " %-*s |", (int)MY_MIN(display_length,
          MAX_COLUMN_LENGTH),
          field->name);
      }
      (void)tee_fputs("\n", PAGER);
      tee_puts((char*)separator.ptr(), PAGER);
    }
  }

  while ((cur= mysql_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    if (!is_termout_oracle_enable(&mysql))
      continue;
    
    ulong *lengths= mysql_fetch_lengths(result);
    (void) tee_fputs("| ", PAGER);
    mysql_field_seek(result, 0);
    for (uint off= 0; off < mysql_num_fields(result); off++)
    {
      const char *buffer;
      uint data_length;
      uint field_max_length;
      uint extra_padding;

      if (off)
        (void) tee_fputs(" ", PAGER);

      if (cur[off] == NULL)
      {
        buffer= "NULL";
        data_length= 4;
      } 
      else 
      {
        buffer= cur[off];
        data_length= (uint) lengths[off];
      }

      field= mysql_fetch_field(result);
      if (!field)
        continue;

      field_max_length= field->max_length;

      /* 
       How many text cells on the screen will this string span?  If it contains
       multibyte characters, then the number of characters we occupy on screen
       will be fewer than the number of bytes we occupy in memory.

       We need to find how much screen real-estate we will occupy to know how 
       many extra padding-characters we should send with the printing function.
      */
      size_t visible_length= charset_info->cset->numcells(charset_info, buffer, buffer + data_length);
      extra_padding= (uint) (data_length - visible_length);

      if ((!force_set_num_width_close) && mysql.oracle_mode && cur[off] && IS_NUM_TERMINAL(field->type) && IS_NUM_BINARY_TERMINAL(field->type)) {
        /* 1.23456E+008 */
        format_fix_width_num_binary(cur[off], lengths[off]);
        if (num_flag[off] != 0) /* if it is numeric, we right-justify it */
          tee_print_sized_data(obclient_num_array, strlen(obclient_num_array), field_max_length, TRUE);
        else 
          tee_print_sized_data(obclient_num_array, strlen(obclient_num_array), field_max_length, FALSE);
      }
      else if ((!force_set_num_width_close) && mysql.oracle_mode && cur[off] && IS_NUM_TERMINAL(field->type)) {
        /* 123456.6754321 */
        format_fix_width_num(cur[off], lengths[off]);
        //if (num_flag[off] != 0) /* if it is numeric, we right-justify it */
        tee_print_sized_data(obclient_num_array, strlen(obclient_num_array), field_max_length+extra_padding, TRUE);
        //else 
        //tee_print_sized_data(obclient_num_array, strlen(obclient_num_array), field_max_length+extra_padding, FALSE);
        
      }
      else if (mysql.oracle_mode && cur[off] && is_binary_field_oracle(field))
        print_as_hex_oracle(PAGER, cur[off], lengths[off], field_max_length);
      else if (opt_binhex && cur[off] && is_binary_field(field))
        print_as_hex(PAGER, cur[off], lengths[off], field_max_length);
      else if (field_max_length > MAX_COLUMN_LENGTH)
        tee_print_sized_data(buffer, data_length, MAX_COLUMN_LENGTH+extra_padding, FALSE);
      else
      {
        if (num_flag[off] != 0) /* if it is numeric, we right-justify it */
          tee_print_sized_data(buffer, data_length, field_max_length+extra_padding, TRUE);
        else 
          tee_print_sized_data(buffer, data_length, field_max_length+extra_padding, FALSE);
      }
      tee_fputs(" |", PAGER);
    }
    (void) tee_fputs("\n", PAGER);
  }
  if (is_termout_oracle_enable(&mysql)) {
    tee_puts((char*)separator.ptr(), PAGER);
  }
  my_afree((uchar*) num_flag);
}

/**
  Return the length of a field after it would be rendered into text.

  This doesn't know or care about multibyte characters.  Assume we're
  using such a charset.  We can't know that all of the upcoming rows 
  for this column will have bytes that each render into some fraction
  of a character.  It's at least possible that a row has bytes that 
  all render into one character each, and so the maximum length is 
  still the number of bytes.  (Assumption 1:  This can't be better 
  because we can never know the number of characters that the DB is 
  going to send -- only the number of bytes.  2: Chars <= Bytes.)

  @param  field  Pointer to a field to be inspected

  @returns  number of character positions to be used, at most
*/
static int get_field_disp_length(MYSQL_FIELD *field)
{
  uint length= column_names ? field->name_length : 0;

  if (quick)
    length= MY_MAX(length, field->length);
  else
    length= MY_MAX(length, field->max_length);

  if (length < 4 && !IS_NOT_NULL(field->flags))
    length= 4;				/* Room for "NULL" */

  return length;
}

/**
  For a new result, return the max number of characters that any
  upcoming row may return.

  @param  result  Pointer to the result to judge

  @returns  The max number of characters in any row of this result
*/

static int get_result_width(MYSQL_RES *result)
{
  unsigned int len= 0;
  MYSQL_FIELD *field;
  MYSQL_FIELD_OFFSET offset;
  
#ifndef DBUG_OFF
  offset= mysql_field_tell(result);
  DBUG_ASSERT(offset == 0);
#else
  offset= 0;
#endif

  while ((field= mysql_fetch_field(result)) != NULL)
    len+= get_field_disp_length(field) + 3; /* plus bar, space, & final space */

  (void) mysql_field_seek(result, offset);	

  return len + 1; /* plus final bar. */
}

static void
tee_print_sized_data(const char *data, unsigned int data_length, unsigned int total_bytes_to_send, bool right_justified)
{
  /* 
    For '\0's print ASCII spaces instead, as '\0' is eaten by (at
    least my) console driver, and that messes up the pretty table
    grid.  (The \0 is also the reason we can't use fprintf() .) 
  */
  unsigned int i;
  const char *p;

  if (right_justified) 
    for (i= data_length; i < total_bytes_to_send; i++)
      tee_putc((int)' ', PAGER);

  for (i= 0, p= data; i < data_length; i+= 1, p+= 1)
  {
    if (*p == '\0')
      tee_putc((int)' ', PAGER);
    else
      tee_putc((int)*p, PAGER);
  }

  if (! right_justified) 
    for (i= data_length; i < total_bytes_to_send; i++)
      tee_putc((int)' ', PAGER);
}



static void
print_table_data_html(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;

  mysql_field_seek(result,0);
  (void) tee_fputs("<TABLE BORDER=1>", PAGER);
  if (column_names)
  {
    (void) tee_fputs("<TR>", PAGER);
    while((field = mysql_fetch_field(result)))
    {
      tee_fputs("<TH>", PAGER);
      if (field->name && field->name[0])
        xmlencode_print(field->name, field->name_length);
      else
        tee_fputs(field->name ? " &nbsp; " : "NULL", PAGER);
      tee_fputs("</TH>", PAGER);
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  while ((cur = mysql_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    ulong *lengths=mysql_fetch_lengths(result);
    field= mysql_fetch_fields(result);
    (void) tee_fputs("<TR>", PAGER);
    for (uint i=0; i < mysql_num_fields(result); i++)
    {
      (void) tee_fputs("<TD>", PAGER);
      if (mysql.oracle_mode && is_binary_field_oracle(&field[i])) {
        if ((!force_set_num_width_close) && cur[i] && IS_NUM_BINARY_TERMINAL(field->type)) {
          format_fix_width_num_binary(cur[i], lengths[i]); 
          print_as_hex_oracle(PAGER, obclient_num_array, strlen(obclient_num_array), lengths[i]);
        } else {
          print_as_hex_oracle(PAGER, cur[i], lengths[i], lengths[i]);
        }
      }
      else if (opt_binhex && is_binary_field(&field[i]))
        print_as_hex(PAGER, cur[i], lengths[i], lengths[i]);
      else {
        if ((!force_set_num_width_close) && mysql.oracle_mode && cur[i] && IS_NUM_TERMINAL(field->type)) {
          format_fix_width_num(cur[i], lengths[i]);
          xmlencode_print(obclient_num_array, strlen(obclient_num_array));
        } else {
          xmlencode_print(cur[i], lengths[i]);
        }
      }
      (void) tee_fputs("</TD>", PAGER);
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  (void) tee_fputs("</TABLE>", PAGER);
}


static void
print_table_data_xml(MYSQL_RES *result)
{
  MYSQL_ROW   cur;
  MYSQL_FIELD *fields;

  mysql_field_seek(result,0);

  tee_fputs("<?xml version=\"1.0\"?>\n\n<resultset statement=\"", PAGER);
  xmlencode_print(glob_buffer.ptr(), (int)strlen(glob_buffer.ptr()));
  tee_fputs("\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">",
            PAGER);

  fields = mysql_fetch_fields(result);
  while ((cur = mysql_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    ulong *lengths=mysql_fetch_lengths(result);
    (void) tee_fputs("\n  <row>\n", PAGER);
    for (uint i=0; i < mysql_num_fields(result); i++)
    {
      tee_fprintf(PAGER, "\t<field name=\"");
      xmlencode_print(fields[i].name, (uint) strlen(fields[i].name));
      if (cur[i])
      {
        tee_fprintf(PAGER, "\">");
        if (mysql.oracle_mode && is_binary_field_oracle(&fields[i])) {
          if ((!force_set_num_width_close) && cur[i] && IS_NUM_BINARY_TERMINAL(fields[i].type)) {
            format_fix_width_num_binary(cur[i], lengths[i]);
            print_as_hex_oracle(PAGER, obclient_num_array, strlen(obclient_num_array), lengths[i]);
          } else {
            print_as_hex_oracle(PAGER, cur[i], lengths[i], lengths[i]);
          }
        }
        else if (opt_binhex && is_binary_field(&fields[i]))
          print_as_hex(PAGER, cur[i], lengths[i], lengths[i]);
        else {
          if ((!force_set_num_width_close) && mysql.oracle_mode && cur[i] && IS_NUM_TERMINAL(fields[i].type)) {
            format_fix_width_num(cur[i], lengths[i]);
            xmlencode_print(obclient_num_array, strlen(obclient_num_array));
          } else {
            xmlencode_print(cur[i], lengths[i]);
          }
        }
        tee_fprintf(PAGER, "</field>\n");
      }
      else
        tee_fprintf(PAGER, "\" xsi:nil=\"true\" />\n");
    }
    (void) tee_fputs("  </row>\n", PAGER);
  }
  (void) tee_fputs("</resultset>\n", PAGER);
}


static void
print_table_data_vertically(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  uint		max_length=0;
  MYSQL_FIELD	*field;

  while ((field = mysql_fetch_field(result)))
  {
    uint length= field->name_length;
    if (length > max_length)
      max_length= length;
    field->max_length=length;
  }

  mysql_field_seek(result,0);
  for (uint row_count=1; (cur= mysql_fetch_row(result)); row_count++)
  {
    if (interrupted_query)
      break;
    mysql_field_seek(result,0);
    tee_fprintf(PAGER, 
		"*************************** %d. row ***************************\n", row_count);

    ulong *lengths= mysql_fetch_lengths(result);

    for (uint off=0; off < mysql_num_fields(result); off++)
    {
      field= mysql_fetch_field(result);
      if (!field)
        continue;

      if (column_names)
        tee_fprintf(PAGER, "%*s: ",(int) max_length,field->name);
      if (cur[off])
      {
        unsigned int i;
        const char *p;
        unsigned long len;
        if (opt_binhex && is_binary_field(field) && !mysql.oracle_mode)
           fprintf(PAGER, "0x");

        if ((!force_set_num_width_close) && mysql.oracle_mode && cur[off] && IS_NUM_BINARY_TERMINAL(field->type)) {
          format_fix_width_num_binary(cur[off], lengths[off]);
          p = obclient_num_array;
          len = strlen(obclient_num_array);
        } else if ((!force_set_num_width_close) && mysql.oracle_mode && cur[off] && IS_NUM_TERMINAL(field->type)) {
          format_fix_width_num(cur[off], lengths[off]);
          p = obclient_num_array;
          len = strlen(obclient_num_array);
        } else {
          p = cur[off];
          len = lengths[off];
        }
        for (i= 0; i < len; i+= 1, p+= 1)
        {
          if (opt_binhex && is_binary_field(field))
            fprintf(PAGER, "%02X", *((uchar*)p));
          else
          {
            if (*p == '\0')
              tee_putc((int)' ', PAGER);
            else
              tee_putc((int)*p, PAGER);
          }
        }
        tee_putc('\n', PAGER);
      }
      else
        tee_fprintf(PAGER, "NULL\n");
    }
  }
}

/* print_warnings should be called right after executing a statement */

static void print_warnings()
{
  const char   *query = NULL;
  MYSQL_RES    *result = NULL;
  MYSQL_ROW    cur;
  my_ulonglong num_rows;
  
  /* Save current error before calling "show warnings" */
  uint error= mysql_errno(&mysql);

  /* Get the warnings */
  query= "show warnings";
  if(mysql_real_query_for_lazy(query, strlen(query))){
    goto end;
  }
  if(mysql_store_result_for_lazy(&result)) {
    goto end;
  }

  /* Bail out when no warnings */
  if (!result || !(num_rows= mysql_num_rows(result)))
    goto end;

  cur= mysql_fetch_row(result);

  /*
    Don't print a duplicate of the current error.  It is possible for SHOW
    WARNINGS to return multiple errors with the same code, but different
    messages.  To be safe, skip printing the duplicate only if it is the only
    warning.
  */
  if (!cur || (num_rows == 1 && error == (uint) strtoul(cur[1], NULL, 10)))
    goto end;

  /* Print the warnings */
  init_pager();
  do
  {
    tee_fprintf(PAGER, "%s (Code %s): %s\n", cur[0], cur[1], cur[2]);
  } while ((cur= mysql_fetch_row(result)));
  end_pager();

end:
  if(result)
    mysql_free_result(result);
}


static const char *array_value(const char **array, char key)
{
  for (; *array; array+= 2)
    if (**array == key)
      return array[1];
  return 0;
}


static void
xmlencode_print(const char *src, uint length)
{
  if (!src)
    tee_fputs("NULL", PAGER);
  else
  {
    for (const char *p = src; length; p++, length--)
    {
      const char *t;
      if ((t = array_value(xmlmeta, *p)))
	tee_fputs(t, PAGER);
      else
	tee_putc(*p, PAGER);
    }
  }
}


static void
safe_put_field(const char *pos,ulong length)
{
  if (!pos)
    tee_fputs("NULL", PAGER);
  else
  {
    if (opt_raw_data)
    {
      unsigned long i;
      /* Can't use tee_fputs(), it stops with NUL characters. */
      for (i= 0; i < length; i++, pos++)
        tee_putc(*pos, PAGER);
    }
    else for (const char *end=pos+length ; pos != end ; pos++)
    {
#ifdef USE_MB
      int l;
      if (use_mb(charset_info) &&
          (l = my_ismbchar(charset_info, pos, end)))
      {
	  while (l--)
	    tee_putc(*pos++, PAGER);
	  pos--;
	  continue;
      }
#endif
      if (!*pos)
	tee_fputs("\\0", PAGER); // This makes everything hard
      else if (*pos == '\t')
	tee_fputs("\\t", PAGER); // This would destroy tab format
      else if (*pos == '\n')
	tee_fputs("\\n", PAGER); // This too
      else if (*pos == '\\')
	tee_fputs("\\\\", PAGER);
	else
	tee_putc(*pos, PAGER);
    }
  }
}


static void
print_tab_data(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;
  ulong		*lengths;

  if (is_termout_oracle_enable(&mysql)) {
    if (opt_silent < 2 && column_names)
    {
      int first = 0;
      while ((field = mysql_fetch_field(result)))
      {
        if (first++)
          (void)tee_fputs("\t", PAGER);
        (void)tee_fputs(field->name, PAGER);
      }
      (void)tee_fputs("\n", PAGER);
    }
  }
  
  while ((cur = mysql_fetch_row(result)))
  {
    if (!is_termout_oracle_enable(&mysql))
      continue;

    lengths=mysql_fetch_lengths(result);
    field= mysql_fetch_fields(result);
    if (mysql.oracle_mode && is_binary_field_oracle(&field[0]))
      print_as_hex_oracle(PAGER, cur[0], lengths[0], lengths[0]);
    else if (opt_binhex && is_binary_field(&field[0]))
      print_as_hex(PAGER, cur[0], lengths[0], lengths[0]);
    else
      safe_put_field(cur[0],lengths[0]);

    for (uint off=1 ; off < mysql_num_fields(result); off++)
    {
      (void) tee_fputs("\t", PAGER);
      if (mysql.oracle_mode && field && is_binary_field_oracle(&field[off]))
        print_as_hex_oracle(PAGER, cur[off], lengths[off], lengths[off]);
      else if (opt_binhex && field && is_binary_field(&field[off]))
        print_as_hex(PAGER, cur[off], lengths[off], lengths[off]);
      else
        safe_put_field(cur[off], lengths[off]);
    }
    (void) tee_fputs("\n", PAGER);
  }
}

static int
com_tee(String *buffer __attribute__((unused)),
        char *line __attribute__((unused)))
{
  char file_name[FN_REFLEN], *end, *param;

  //if (status.batch)
  //  return 0;
  while (my_isspace(charset_info,*line))
    line++;
  if (!(param = strchr(line, ' '))) // if outfile wasn't given, use the default
  {
    if (!strlen(outfile))
    {
      printf("No previous outfile available, you must give a filename!\n");
      return 0;
    }
    else if (opt_outfile)
    {
      tee_fprintf(stdout, "Currently logging to file '%s'\n", outfile);
      return 0;
    }
    else
      param = outfile;			//resume using the old outfile
  }

  /* eliminate the spaces before the parameters */
  while (my_isspace(charset_info,*param))
    param++;
  end= strmake_buf(file_name, param);
  /* remove end space from command line */
  while (end > file_name && (my_isspace(charset_info,end[-1]) || 
			     my_iscntrl(charset_info,end[-1])))
    end--;
  end[0]= 0;
  if (end == file_name)
  {
    printf("No outfile specified!\n");
    return 0;
  }
  init_tee(file_name);
  return 0;
}


static int
com_notee(String *buffer __attribute__((unused)),
	  char *line __attribute__((unused)))
{
  if (opt_outfile)
    end_tee();
  tee_fprintf(stdout, "Outfile disabled.\n");
  return 0;
}

/*
  Sorry, this command is not available in Windows.
*/

#ifdef USE_POPEN
static int
com_pager(String *buffer __attribute__((unused)),
          char *line __attribute__((unused)))
{
  char pager_name[FN_REFLEN], *end, *param;

  if (status.batch)
    return 0;
  /* Skip spaces in front of the pager command */
  while (my_isspace(charset_info, *line))
    line++;
  /* Skip the pager command */
  param= strchr(line, ' ');
  /* Skip the spaces between the command and the argument */
  while (param && my_isspace(charset_info, *param))
    param++;
  if (!param || !strlen(param)) // if pager was not given, use the default
  {
    if (!default_pager_set)
    {
      tee_fprintf(stdout, "Default pager wasn't set, using stdout.\n");
      opt_nopager=1;
      strmov(pager, "stdout");
      PAGER= stdout;
      return 0;
    }
    strmov(pager, default_pager);
  }
  else
  {
    end= strmake_buf(pager_name, param);
    while (end > pager_name && (my_isspace(charset_info,end[-1]) || 
                                my_iscntrl(charset_info,end[-1])))
      end--;
    end[0]=0;
    strmov(pager, pager_name);
    strmov(default_pager, pager_name);
  }
  opt_nopager=0;
  tee_fprintf(stdout, "PAGER set to '%s'\n", pager);
  return 0;
}


static int
com_nopager(String *buffer __attribute__((unused)),
	    char *line __attribute__((unused)))
{
  strmov(pager, "stdout");
  opt_nopager=1;
  PAGER= stdout;
  tee_fprintf(stdout, "PAGER set to stdout\n");
  return 0;
}
#endif


/*
  Sorry, you can't send the result to an editor in Win32
*/

#ifdef USE_POPEN
static int
com_edit(String *buffer,char *line __attribute__((unused)))
{
  char	filename[FN_REFLEN],buff[160];
  int	fd,tmp,error;
  const char *editor;
  MY_STAT stat_arg;

  if ((fd= create_temp_file(filename,NullS,"sql", 0, MYF(MY_WME))) < 0)
    goto err;
  if (buffer->is_empty() && !old_buffer.is_empty())
    (void) my_write(fd,(uchar*) old_buffer.ptr(),old_buffer.length(),
		    MYF(MY_WME));
  else
    (void) my_write(fd,(uchar*) buffer->ptr(),buffer->length(),MYF(MY_WME));
  (void) my_close(fd,MYF(0));

  if (!(editor = (char *)getenv("EDITOR")) &&
      !(editor = (char *)getenv("VISUAL")))
    editor = "vi";
  strxmov(buff,editor," ",filename,NullS);
  if ((error= system(buff)))
  {
    char errmsg[100];
    sprintf(errmsg, "Command '%.40s' failed", buff);
    put_info(errmsg, INFO_ERROR, 0, NullS);
    goto err;
  }

  if (!my_stat(filename,&stat_arg,MYF(MY_WME)))
    goto err;
  if ((fd = my_open(filename,O_RDONLY, MYF(MY_WME))) < 0)
    goto err;
  (void) buffer->alloc((uint) stat_arg.st_size);
  if ((tmp=read(fd,(char*) buffer->ptr(),buffer->alloced_length())) >= 0L)
    buffer->length((uint) tmp);
  else
    buffer->length(0);
  (void) my_close(fd,MYF(0));
  (void) my_delete(filename,MYF(MY_WME));
err:
  return 0;
}
#endif


/* If arg is given, exit without errors. This happens on command 'quit' */

static int
com_quit(String *buffer __attribute__((unused)),
	 char *line __attribute__((unused)))
{
  status.exit_status=0;
  return 1;
}

static int
com_rehash(String *buffer __attribute__((unused)),
	 char *line __attribute__((unused)))
{
#ifdef HAVE_READLINE
  build_completion_hash(1, 0);
#endif
  return 0;
}


#ifdef USE_POPEN
static int
com_shell(String *buffer __attribute__((unused)),
          char *line __attribute__((unused)))
{
  char *shell_cmd;

  /* Skip space from line begin */
  while (my_isspace(charset_info, *line))
    line++;
  if (!(shell_cmd = strchr(line, ' ')))
  {
    put_info("Usage: \\! shell-command", INFO_ERROR);
    return -1;
  }
  /*
    The output of the shell command does not
    get directed to the pager or the outfile
  */
  if (system(shell_cmd) == -1)
  {
    put_info(strerror(errno), INFO_ERROR, errno);
    return -1;
  }
  return 0;
}
#endif


static int
com_print(String *buffer,char *line __attribute__((unused)))
{
  tee_puts("--------------", stdout);
  (void) tee_fputs(buffer->c_ptr(), stdout);
  if (!buffer->length() || (*buffer)[buffer->length()-1] != '\n')
    tee_putc('\n', stdout);
  tee_puts("--------------\n", stdout);
  return 0;					/* If empty buffer */
}

	/* ARGSUSED */
static int
com_connect(String *buffer, char *line)
{
  char *tmp, buff[256];
  my_bool save_rehash= opt_rehash;
  int error;

  //To relink, need to reinitialize this global variable
  dbms_serveroutput = 0;
  dbms_prepared = 0;
  if (dbms_stmt) {
    mysql_stmt_close(dbms_stmt);
    dbms_stmt = NULL;
  }

  bzero(buff, sizeof(buff));
  if (buffer)
  {
    /*
      Two null bytes are needed in the end of buff to allow
      get_arg to find end of string the second time it's called.
    */
    tmp= strmake(buff, line, sizeof(buff)-2);
#ifdef EXTRA_DEBUG
    tmp[1]= 0;
#endif
    if (rewrite_by_oracle(buff)) {
      int i = 1;
      // connect username@tenant[/password[@[//]host[:port]]]
      tmp = get_arg(buff, GET);
      if (tmp && *tmp) {
        my_free(current_user);
        current_user = my_strdup(tmp, MYF(MY_WME));
        tmp = get_arg_by_index(buff, i++);
        if (tmp) {
          my_free(opt_password);
          opt_password = my_strdup(tmp, MYF(MY_WME));
          tmp = get_arg_by_index(buff, i++);
          if (tmp) {
            my_free(current_host);
            current_host = my_strdup(tmp, MYF(MY_WME));
            my_free(current_host_success);
            current_host_success = my_strdup(tmp, MYF(MY_WME));

            tmp = get_arg_by_index(buff, i++);
            if (tmp) {
              errno = 0;
              char *endchar;
              ulonglong port;
              port = strtoull(tmp, &endchar, 10);
              if (errno == ERANGE) {
                my_getopt_error_reporter(ERROR_LEVEL, "Incorrect unsigned integer value: '%s'", tmp);
                return 1;
              } else {
                opt_mysql_port = (uint)port;
                current_port_success = (int)port;
              }
            }
          }
        }
      } else {
        /* Quick re-connect */
        opt_rehash = 0;                            /* purecov: tested */
      }
    } else {
      // connect db host
      tmp= get_arg(buff, GET);
      if (tmp && *tmp)
      {
        my_free(current_db);
        current_db= my_strdup(tmp, MYF(MY_WME));
        tmp= get_arg(buff, GET_NEXT);
        if (tmp)
        {
          my_free(current_host);
          current_host=my_strdup(tmp,MYF(MY_WME));
          my_free(current_host_success);
          current_host_success = my_strdup(tmp, MYF(MY_WME));
        }
      } else {
        /* Quick re-connect */
        opt_rehash= 0;                            /* purecov: tested */
      }
    }
    buffer->length(0);				// command used
    opt_mysql_port = current_port_success;
    error = sql_connect(current_host_success, current_db, current_user, opt_password, 100);
  } else {
    opt_rehash= 0;
    error = sql_connect(current_host, current_db, current_user, opt_password, 0);
  }
  opt_rehash= save_rehash;

  if (connected)
  {
    if (mysql.oracle_mode) {
      get_current_db();
    }

    sprintf(buff,"Connection id:    %lu",mysql_thread_id(&mysql));
    put_info(buff,INFO_INFO);
    sprintf(buff,"Current database: %.128s\n", current_db ? current_db : "*** NONE ***");
    put_info(buff,INFO_INFO);

    //relink success
    start_login_sql(&mysql, &glob_buffer, get_login_sql_path());
  }
  return error;
}

static int com_source(String *buffer __attribute__((unused)),
                      char *line)
{
  char source_name[FN_REFLEN], *end, *param;
  LINE_BUFFER *line_buff;
  int error;
  STATUS old_status;
  FILE *sql_file;
  my_bool save_ignore_errors;

  /* Skip space from file name */
  while (my_isspace(charset_info,*line))
    line++;
  if (!(param = strchr(line, ' ')))		// Skip command name
    return put_info("Usage: \\. <filename> | source <filename>", 
		    INFO_ERROR, 0);
  while (my_isspace(charset_info,*param))
    param++;
  end=strmake_buf(source_name, param);
  while (end > source_name && (my_isspace(charset_info,end[-1]) || 
                               my_iscntrl(charset_info,end[-1])))
    end--;
  end[0]=0;
  unpack_filename(source_name,source_name);
  /* open file name */
  if (!(sql_file = my_fopen(source_name, O_RDONLY | O_BINARY,MYF(0))))
  {
    char buff[FN_REFLEN+60];
    snprintf(buff, sizeof(buff), "Failed to open file '%s', error: %d", source_name,errno);
    return put_info(buff, INFO_ERROR, 0);
  }

  if (!(line_buff= batch_readline_init(MAX_BATCH_BUFFER_SIZE, sql_file)))
  {
    my_fclose(sql_file,MYF(0));
    return put_info("Can't initialize batch_readline", INFO_ERROR, 0);
  }

  /* Save old status */
  old_status=status;
  save_ignore_errors= ignore_errors;
  bfill((char*) &status,sizeof(status),(char) 0);

  status.batch=old_status.batch;		// Run in batch mode
  status.line_buff=line_buff;
  status.file_name=source_name;
  glob_buffer.length(0);			// Empty command buffer
  ignore_errors= !batch_abort_on_error;
  in_com_source= 1;
  error= read_and_execute(false);
  ignore_errors= save_ignore_errors;
  status=old_status;				// Continue as before
  in_com_source= aborted= 0;
  my_fclose(sql_file,MYF(0));
  batch_readline_end(line_buff);
  /*
    If we got an error during source operation, don't abort the client
    if ignore_errors is set
  */
  if (error && ignore_errors)
    error= -1;                                  // Ignore error
  return error;
}


	/* ARGSUSED */
static int
com_delimiter(String *buffer __attribute__((unused)), char *line)
{
  char buff[256], *tmp;

  strmake_buf(buff, line);
  tmp= get_arg(buff, GET);

  if (!tmp || !*tmp)
  {
    put_info("DELIMITER must be followed by a 'delimiter' character or string",
	     INFO_ERROR);
    return 0;
  }
  else
  {
    if (strstr(tmp, "\\")) 
    {
      put_info("DELIMITER cannot contain a backslash character", INFO_ERROR);
      return 0;
    }
  }
  strmake_buf(delimiter, tmp);
  delimiter_length= (int)strlen(delimiter);
  delimiter_str= delimiter;
  is_user_specify_delimiter = TRUE;
  if (delimiter_length == 1 && delimiter && delimiter[0] == ';') {
    is_user_specify_delimiter = FALSE;
  }
  return 0;
}

	/* ARGSUSED */
static int
com_use(String *buffer __attribute__((unused)), char *line)
{
  char *tmp, buff[FN_REFLEN + 1];
  int select_db;

  if (mysql.oracle_mode)
  {
    my_snprintf(buff, FN_REFLEN + 1, "unknown command \"%.*s\" - rest of line ignored.", (int)strlen(line), line);
    return put_info(buff, INFO_ERROR, 42, NULL, mysql.oracle_mode);
  }

  bzero(buff, sizeof(buff));
  strmake_buf(buff, line);
  tmp= get_arg(buff, GET);
  if (!tmp || !*tmp)
  {
    put_info("USE must be followed by a database name", INFO_ERROR);
    return 0;
  }
  /*
    We need to recheck the current database, because it may change
    under our feet, for example if DROP DATABASE or RENAME DATABASE
    (latter one not yet available by the time the comment was written)
  */
  get_current_db();

  if (!current_db || cmp_database(charset_info, current_db,tmp))
  {
    if (one_database)
    {
      skip_updates= 1;
      select_db= 0;    // don't do mysql_select_db()
    }
    else
      select_db= 2;    // do mysql_select_db() and build_completion_hash()
  }
  else
  {
    /*
      USE to the current db specified.
      We do need to send mysql_select_db() to make server
      update database level privileges, which might
      change since last USE (see bug#10979).
      For performance purposes, we'll skip rebuilding of completion hash.
    */
    skip_updates= 0;
    select_db= 1;      // do only mysql_select_db(), without completion
  }

  if (select_db)
  {
    /*
      reconnect once if connection is down or if connection was found to
      be down during query
    */
    if (!connected && reconnect())
      return opt_reconnect ? -1 : 1;                        // Fatal error
    if (mysql_select_db(&mysql,tmp))
    {
      if (mysql_errno(&mysql) != CR_SERVER_GONE_ERROR)
        return put_error(&mysql);

      if (reconnect())
        return opt_reconnect ? -1 : 1;                      // Fatal error
      if (mysql_select_db(&mysql,tmp))
        return put_error(&mysql);
    }
    my_free(current_db);
    current_db=my_strdup(tmp,MYF(MY_WME));
#ifdef HAVE_READLINE
    if (select_db > 1)
      build_completion_hash(opt_rehash, 1);
#endif
  }

  put_info("Database changed",INFO_INFO);
  return 0;
}

static int
com_warnings(String *buffer __attribute__((unused)),
   char *line __attribute__((unused)))
{
  show_warnings = 1;
  put_info("Show warnings enabled.",INFO_INFO);
  return 0;
}

static int
com_nowarnings(String *buffer __attribute__((unused)),
   char *line __attribute__((unused)))
{
  show_warnings = 0;
  put_info("Show warnings disabled.",INFO_INFO);
  return 0;
}

int rewrite_by_oracle(char *line)
{
  char *ptr;
  ptr = line;

  /* skip leading white spaces */
  while (my_isspace(charset_info, *ptr)) {
    ptr++;
  }

  if (*ptr == '\\') { // short command was used
    ptr += 2;
  } else {
    while (*ptr &&!my_isspace(charset_info, *ptr)) { // skip command
      ptr++;
    }
  }

  // username@tenant[/password[@[//]host[:port]]]
  while (*ptr && *ptr != '@') {
    ptr++;
  }

  if (*ptr) {
    while (*ptr && *ptr != '/') {
      ptr++;
    }

    if (*ptr) {
      *ptr++ = ' ';

      while (*ptr && *ptr != '@') {
        ptr++;
      }

      if (*ptr) {
        *ptr++ = ' ';
        if (*ptr && *ptr == '/'
          && *(ptr + 1) && *(ptr + 1) == '/') {
          *ptr++ = ' ';
          *ptr++ = ' ';
        }

        while (*ptr && *ptr != ':') {
          ptr++;
        }

        if (*ptr) {
          *ptr = ' ';
        }
      }
    }
    return true;
  } else {
    return false;
  }
}

char *get_arg_by_index(char *line, int arg_index) {
  char *ptr;

  ptr = line;
  while (arg_index > 0)
  {
    while (*ptr && !my_isspace(charset_info, *ptr)) { // skip arg
      ptr++;
    }

    while (my_isspace(charset_info, *ptr)) {
      ptr++;
    }

    if (*ptr || *(++ptr)) {
      arg_index--;
    }
    else {
      break;
    }
  }
  return get_arg(ptr, GET_NEXT);
}

/*
  Gets argument from a command on the command line. If mode is not GET_NEXT,
  skips the command and returns the first argument. The line is modified by
  adding zero to the end of the argument. If mode is GET_NEXT, then the
  function searches for end of string first, after found, returns the next
  argument and adds zero to the end. If you ever wish to use this feature,
  remember to initialize all items in the array to zero first.
*/

static char *get_arg(char *line, get_arg_mode mode)
{
  char *ptr, *start;
  bool short_cmd= false;
  char qtype= 0;

  ptr= line;
  if (mode == GET_NEXT)
  {
    for (; *ptr; ptr++) ;
    if (*(ptr + 1))
      ptr++;
  }
  else
  {
    /* skip leading white spaces */
    while (my_isspace(charset_info, *ptr))
      ptr++;
    if ((short_cmd= *ptr == '\\')) // short command was used
      ptr+= 2;
    else
      while (*ptr &&!my_isspace(charset_info, *ptr)) // skip command
        ptr++;
  }
  if (!*ptr)
    return NullS;
  while (my_isspace(charset_info, *ptr))
    ptr++;
  if (*ptr == '\'' || *ptr == '\"' || *ptr == '`')
  {
    qtype= *ptr;
    ptr++;
  }
  for (start=ptr ; *ptr; ptr++)
  {
    /* if short_cmd use historical rules (only backslash) otherwise SQL rules */
    if (short_cmd
        ? (*ptr == '\\' && ptr[1])                     // escaped character
        : (*ptr == '\\' && ptr[1] && qtype != '`') ||  // escaped character
          (qtype && *ptr == qtype && ptr[1] == qtype)) // quote
    {
      // Remove (or skip) the backslash (or a second quote)
      if (mode != CHECK)
        strmov_overlapp(ptr, ptr+1);
      else
        ptr++;
    }
    else if (*ptr == (qtype ? qtype : ' '))
    {
      qtype= 0;
      if (mode != CHECK)
        *ptr= 0;
      break;
    }
  }
  return ptr != start && !qtype ? start : NullS;
}


/**
  An example of mysql_authentication_dialog_ask callback.

  The C function with the name "mysql_authentication_dialog_ask", if exists,
  will be used by the "dialog" client authentication plugin when user
  input is needed. This function should be of mysql_authentication_dialog_ask_t
  type. If the function does not exists, a built-in implementation will be
  used.

  @param mysql          mysql
  @param type           type of the input
                        1 - normal string input
                        2 - password string
  @param prompt         prompt
  @param buf            a buffer to store the use input
  @param buf_len        the length of the buffer

  @retval               a pointer to the user input string.
                        It may be equal to 'buf' or to 'mysql->password'.
                        In all other cases it is assumed to be an allocated
                        string, and the "dialog" plugin will free() it.
*/

MYSQL_PLUGIN_EXPORT
char *mysql_authentication_dialog_ask(MYSQL *mysql, int type,
                                      const char *prompt,
                                      char *buf, int buf_len)
{
  char *s=buf;

  fputs("[mariadb] ", stdout);
  fputs(prompt, stdout);
  fputs(" ", stdout);

  if (type == 2) /* password */
  {
    s= get_tty_password("");
    strnmov(buf, s, buf_len);
    buf[buf_len-1]= 0;
    my_free(s);
  }
  else
  {
    if (!fgets(buf, buf_len-1, stdin))
      buf[0]= 0;
    else if (buf[0] && (s= strend(buf))[-1] == '\n')
      s[-1]= 0;
  }

  return buf;
}

static void parse_socket5_proxy(char *socket5_proxy, char**host, int*port, char **user, char**pwd)
{
  char *p = socket5_proxy, *str = NULL;
  int idx = 0;
  my_bool in_string = 0;
  my_bool split = 0;

  str = p;
  while (*p != 0) {
    if (*p == '[') {
      in_string = 1;
      str = p + 1;
    } else if (*p == ']' && in_string == 1) {
      in_string = 0;
      *p = 0;
    } else if (*p == ',' && in_string == 0) {
      *p = 0;
      if (idx == 0) {
        *host = str;
      } else if (idx == 1) {
        *port = atoi(str);
      } else if (idx == 2) {
        *user = str;
      } else if (idx == 3) {
        *pwd = str;
      }
      str = p + 1;
      idx++;
    }
    p++;
  }
  if (idx == 0) {
    *host = str;
  } else if (idx == 1) {
    *port = atoi(str);
  } else if (idx == 2) {
    *user = str;
  } else if (idx == 3) {
    *pwd = str;
  }
}
static void parse_multi_host(char *host, std::map<char*, int>* mhost)
{
  char *p = host, *str = NULL;
  char *myhost = NULL, *myport = NULL;
  my_bool in_string = 0;
  if (NULL == host || NULL == mhost) {
    return;
  }

  myhost = p;
  while (*p != 0) {
    if (*p == '[') {
      in_string = 1;
      myhost = p + 1;
    } else if (*p == ']' && in_string == 1) {
      in_string = 0;
      *p = 0;
    } else if (*p == ':' && in_string == 0) {
      myport = p + 1;
      *p = 0;
    } else if (*p == ',' && in_string == 0) {
      myhost = p + 1;
      myport = NULL;
    }
    if (myhost && myport) {
      int port = atoi(myport);
      if (port > 0) {
        (*mhost)[myhost] = port;
      }
      myhost = NULL;
      myport = NULL;
    }
    p++;
  }
}

static int sql_real_connect(char *host,char *database,char *user,char *password, uint silent)
{
  String socket5_proxy;
  if (connected)
  {
    connected= 0;
    mysql_close(&mysql);
  }
  mysql_init(&mysql);

  if (ob_socket5_proxy_str && *ob_socket5_proxy_str) {
    char *socket5_host = NULL;
    char *socket5_user = NULL;
    char *socket5_pwd = NULL;
    int socket5_port = 0;
    socket5_proxy.append(ob_socket5_proxy_str);
    parse_socket5_proxy(socket5_proxy.c_ptr_safe(), &socket5_host, &socket5_port, &socket5_user, &socket5_pwd);
    if (socket5_host && socket5_port > 0) {
      ob_set_socket5_proxy(&mysql, 0, socket5_host, socket5_port, socket5_user, socket5_pwd);
    }
  }

  if (opt_init_command)
    mysql_options(&mysql, MYSQL_INIT_COMMAND, opt_init_command);
  if (opt_connect_timeout)
  {
    uint timeout=opt_connect_timeout;
    mysql_options(&mysql,MYSQL_OPT_CONNECT_TIMEOUT,
		  (char*) &timeout);
  }
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);
  if (using_opt_local_infile)
    mysql_options(&mysql,MYSQL_OPT_LOCAL_INFILE, (char*) &opt_local_infile);
  if (safe_updates)
  {
    char init_command[100];
    sprintf(init_command,
	    "SET SQL_SAFE_UPDATES=1,SQL_SELECT_LIMIT=%lu,MAX_JOIN_SIZE=%lu",
	    select_limit,max_join_size);
    mysql_options(&mysql, MYSQL_INIT_COMMAND, init_command);
  }
  if (!strcmp(default_charset,MYSQL_AUTODETECT_CHARSET_NAME))
    default_charset= (char *)my_default_csname();
  if (strncasecmp(default_charset, "GB18030-2022", 12) == 0) {
    default_charset = (char*)"GB18030";
  }
  if (strncasecmp(default_charset, "BIG5-HKSCS", 10) == 0 || 
    strncasecmp(default_charset, "BIG5HKSCS", 9) == 0) {
    default_charset = (char*)"big5";
  }
  mysql_options(&mysql, MYSQL_SET_CHARSET_NAME, default_charset);

  my_bool can_handle_expired= opt_connect_expired_password || !status.batch;
  mysql_options(&mysql, MYSQL_OPT_CAN_HANDLE_EXPIRED_PASSWORDS, &can_handle_expired);

  if (!do_connect(&mysql, host, user, password, database,
                  connect_flag | CLIENT_MULTI_STATEMENTS))
  {
    if (!silent ||
      (mysql_errno(&mysql) != CR_CONN_HOST_ERROR &&
        mysql_errno(&mysql) != CR_CONNECTION_ERROR))
    {
      (void) put_error(&mysql);
      (void) fflush(stdout);
      return ignore_errors ? -1 : 1;		// Abort
    }
    return -1;					// Retryable
  }

  charset_info= get_charset_by_name(mysql.charset->name, MYF(0));

  
  connected=1;
#ifndef EMBEDDED_LIBRARY
  mysql_options(&mysql, MYSQL_OPT_RECONNECT, &debug_info_flag);

  /*
    CLIENT_PROGRESS_OBSOLETE is set only if we requested it in
    mysql_real_connect() and the server also supports it
*/
  if (mysql.client_flag & CLIENT_PROGRESS_OBSOLETE)
    mysql_options(&mysql, MYSQL_PROGRESS_CALLBACK, (void*) report_progress);
#else
  {
    my_bool reconnect= 1;
    mysql_options(&mysql, MYSQL_OPT_RECONNECT, &reconnect);
  }
#endif
#ifdef HAVE_READLINE
  build_completion_hash(opt_rehash, 1);
#endif
  return 0;
}
static int sql_real_connect_multi(const char* tns_name, ObClientLbAddressList *list, char *database, char *user, char *password, uint silent)
{
  bool success;
  ObClientLbAddress address;
  ObClientLbConfig config;
  String socket5_proxy;

  memset(&config, 0, sizeof(config));
  memset(&address, 0, sizeof(address));

  if (connected) {
    connected = 0;
    mysql_close(&mysql);
  }
  mysql_init(&mysql);

  if (ob_socket5_proxy_str && *ob_socket5_proxy_str) {
    char *socket5_host = NULL;
    char *socket5_user = NULL;
    char *socket5_pwd = NULL;
    int socket5_port = 0;
    socket5_proxy.append(ob_socket5_proxy_str);
    parse_socket5_proxy(socket5_proxy.c_ptr_safe(), &socket5_host, &socket5_port, &socket5_user, &socket5_pwd);
    if (socket5_host && socket5_port > 0) {
      ob_set_socket5_proxy(&mysql, 0, socket5_host, socket5_port, socket5_user, socket5_pwd);
    }
  }

  config.black_remove_strategy = 2001;
  config.black_remove_timeout = 0;
  config.black_append_strategy = 3002;
  config.black_append_duration = 60000;
  config.black_append_retrytimes = 1;
  config.retry_all_downs = 0;

  if (opt_init_command)
    config.mysql_init_command = opt_init_command;
  if (opt_connect_timeout)
    config.mysql_connect_timeout = opt_connect_timeout;
  if (opt_compress)
    config.mysql_is_compress = 1;
  if (using_opt_local_infile) {
    config.mysql_is_local_infile = 1;
    config.mysql_local_infile = opt_local_infile;
  }
  if (opt_protocol)
    config.mysql_opt_protocol = opt_protocol;
  if (opt_secure_auth)
    config.mysql_opt_secure_auth = opt_secure_auth;
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  if (opt_use_ssl) {
    config.mysql_opt_use_ssl = opt_use_ssl;
    config.mysql_opt_ssl_key = opt_ssl_key;
    config.mysql_opt_ssl_cert = opt_ssl_cert;
    config.mysql_opt_ssl_ca = opt_ssl_ca;
    config.mysql_opt_ssl_capath = opt_ssl_capath;
    config.mysql_opt_ssl_cipher = opt_ssl_cipher;
    config.mysql_opt_ssl_crl = opt_ssl_crl;
    config.mysql_opt_ssl_crlpath = opt_ssl_crlpath;
    config.mysql_opt_tls_version = opt_tls_version;
  }
#endif
  config.mysql_opt_ssl_verify_server_cert = opt_ssl_verify_server_cert;

  if (!strcmp(default_charset, MYSQL_AUTODETECT_CHARSET_NAME))
    default_charset = (char *)my_default_csname();
  if (strncasecmp(default_charset, "GB18030-2022", 12) == 0) {
    default_charset = (char*)"GB18030";
  }
  if (strncasecmp(default_charset, "BIG5-HKSCS", 10) == 0 || 
    strncasecmp(default_charset, "BIG5HKSCS", 9) == 0) {
    default_charset = (char*)"big5";
  }
  config.mysql_charset_name = default_charset;
  //oracle proxy user
  if (ob_proxy_user_str && ob_proxy_user_str[0])
    config.mysql_ob_proxy_user = ob_proxy_user_str;


  if (NULL != ob_mysql_real_connect(&mysql, tns_name, list, &config,
    user, password, database, NULL, connect_flag | CLIENT_MULTI_STATEMENTS, &address)) {
    success = 1;
    current_host_success = my_strdup(address.host, MYF(MY_WME));
    current_port_success = address.port;
  } else {
    success = 0;
  }

  if (!success) {
    int err_no = mysql_errno(&mysql);
    if (err_no != 0) {
      put_info(mysql_error(&mysql), INFO_ERROR, mysql_errno(&mysql), mysql_sqlstate(&mysql), mysql.oracle_mode);
      return -1;
    } else {
      return -2;					// Retryable
    }
  }

  charset_info = get_charset_by_name(mysql.charset->name, MYF(0));
  connected = 1;
#ifndef EMBEDDED_LIBRARY
  mysql_options(&mysql, MYSQL_OPT_RECONNECT, &debug_info_flag);

  if (mysql.client_flag & CLIENT_PROGRESS_OBSOLETE)
    mysql_options(&mysql, MYSQL_PROGRESS_CALLBACK, (void*)report_progress);
#else
  {
    my_bool reconnect = 1;
    mysql_options(&mysql, MYSQL_OPT_RECONNECT, &reconnect);
  }
#endif
#ifdef HAVE_READLINE
  build_completion_hash(opt_rehash, 1);
#endif
  return 0;
}

static int
sql_connect(char *host,char *database,char *user,char *password,uint silent)
{
  bool message=0;
  uint count=0;
  int error;
  bool is_multi_host = 0;
  String hostinfo;  //Automatically release local memory
  std::map<char*, int> multi_host;
  bool is_connect_cmd = 0;
  if (silent == 100) {  //conn user@oracle/pass@//host:port
    silent = 0;
    is_connect_cmd = 1;
  }

  hostinfo.append(host?host:"");
  parse_multi_host(hostinfo.c_ptr_safe(), &multi_host);

  if (!is_connect_cmd && (ob_service_name_str || multi_host.size() > 0)) {
    //-hxxx.xxx.xxx.xxx:3306,xxx.xxx.xxx.xxx:3308
    //--ob-service-name=TEST
    ObClientLbAddressList list;
    memset(&list, 0, sizeof(ObClientLbAddressList));
    int multis = multi_host.size();
    if (multis > 0) {
      int i = 0;
      std::map<char*, int>::iterator iter = multi_host.begin();
      list.address_list_count = multi_host.size();
      list.address_list = (ObClientLbAddress*)malloc(multi_host.size() * sizeof(ObClientLbAddress));
      memset(list.address_list, 0, multi_host.size() * sizeof(ObClientLbAddress));
      for (iter = multi_host.begin(),i=0; iter != multi_host.end(); iter++, i++) {
        char *p = iter->first;
        memcpy(list.address_list[i].host, p, strlen(p));
        list.address_list[i].port = iter->second;
      }
    }

    error = sql_real_connect_multi(ob_service_name_str, &list, database, user, password, silent);
    if (0 != error) {
      if (error == -2) {
        if (ob_service_name_str) {
          put_info("ob-service-name invalid tnsnames.ora", INFO_ERROR, -1, 0, mysql.oracle_mode);
        } else if (is_multi_host) {
          put_info("-hip1:port1,ip2:port2...", INFO_ERROR, -1, 0, mysql.oracle_mode);
        }
      }
    }
    if (list.address_list) {
      free(list.address_list);
      list.address_list = NULL;
      list.address_list_count = 0;
    }
    return error;
  } else {
    //-hxxx.xxx.xxx.xxx -p3306
    for (;;)
    {
      if ((error = sql_real_connect(host, database, user, password, wait_flag)) >= 0)
      {
        if (count)
        {
          tee_fputs("\n", stderr);
          (void)fflush(stderr);
        }
        return error;
      }
      if (!wait_flag)
        return ignore_errors ? -1 : 1;
      if (!message && !silent)
      {
        message = 1;
        tee_fputs("Waiting", stderr); (void)fflush(stderr);
      }
      (void)sleep(wait_time);
      if (!silent)
      {
        putc('.', stderr); (void)fflush(stderr);
        count++;
      }
    }
  }
}



static int
com_status(String *buffer __attribute__((unused)),
	   char *line __attribute__((unused)))
{
  const char *status_str;
  char buff[40];
  ulonglong id;
  MYSQL_RES *UNINIT_VAR(result);

  const char *sql = NULL;
  if (mysql.oracle_mode) {
    sql = "select sys_context('USERENV','CURRENT_SCHEMA'), sys_context('USERENV','CURRENT_USER') from dual where rownum <= 1";
  } else {
    sql = "select DATABASE(), USER() limit 1";
  }

  if (mysql_real_query_for_lazy(sql, strlen(sql))) {
    return 0;
  }

  tee_puts("--------------", stdout);
  usage(1);					/* Print version */
  tee_fprintf(stdout, "\nConnection id:\t\t%lu\n",mysql_thread_id(&mysql));
  /*
    Don't remove "limit 1",
    it is protection against SQL_SELECT_LIMIT=0
  */
  if (!mysql_store_result_for_lazy(&result))
  {
    MYSQL_ROW cur=mysql_fetch_row(result);
    if (cur)
    {
      tee_fprintf(stdout, "Current database:\t%s\n", cur[0] ? cur[0] : "");
      tee_fprintf(stdout, "Current user:\t\t%s\n", cur[1]);
    }
    mysql_free_result(result);
  }

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  if ((status_str= mysql_get_ssl_cipher(&mysql)))
    tee_fprintf(stdout, "SSL:\t\t\tCipher in use is %s\n",
                status_str);
  else
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */
    tee_puts("SSL:\t\t\tNot in use", stdout);

  if (skip_updates)
  {
    my_vidattr(A_BOLD);
    tee_fprintf(stdout, "\nAll updates ignored to this database\n");
    my_vidattr(A_NORMAL);
  }
#ifdef USE_POPEN
  tee_fprintf(stdout, "Current pager:\t\t%s\n", pager);
  tee_fprintf(stdout, "Using outfile:\t\t'%s'\n", opt_outfile ? outfile : "");
#endif
  tee_fprintf(stdout, "Using delimiter:\t%s\n", delimiter);
  //tee_fprintf(stdout, "Server:\t\t\t%s\n", mysql_get_server_name(&mysql));
  tee_fprintf(stdout, "Server version:\t\t%s\n", is_proxymode ? "":server_version_string(&mysql));
  tee_fprintf(stdout, "Protocol version:\t%d\n", mysql_get_proto_info(&mysql));
  tee_fprintf(stdout, "Connection:\t\t%s\n", mysql_get_host_info(&mysql));
  if ((id= mysql_insert_id(&mysql)))
    tee_fprintf(stdout, "Insert id:\t\t%s\n", llstr(id, buff));

  /* "limit 1" is protection against SQL_SELECT_LIMIT=0 */
  if (mysql.oracle_mode) {
    sql = "select @@character_set_client, @@character_set_connection, "
      "@@character_set_server, @@character_set_database from dual where rownum <= 1";
  } else {
    sql = "select @@character_set_client, @@character_set_connection, "
      "@@character_set_server, @@character_set_database limit 1";
  }

  if (mysql_real_query_for_lazy(sql, strlen(sql)))
  {
    if (mysql_errno(&mysql) == CR_SERVER_GONE_ERROR)
      return 0;
  }
  if (!mysql_store_result_for_lazy(&result))
  {
    MYSQL_ROW cur=mysql_fetch_row(result);
    if (cur)
    {
      tee_fprintf(stdout, "Server characterset:\t%s\n", cur[2] ? cur[2] : "");
      tee_fprintf(stdout, "Db     characterset:\t%s\n", cur[3] ? cur[3] : "");
      tee_fprintf(stdout, "Client characterset:\t%s\n", cur[0] ? cur[0] : "");
      tee_fprintf(stdout, "Conn.  characterset:\t%s\n", cur[1] ? cur[1] : "");
    }
    mysql_free_result(result);
  }
  else
  {
    /* Probably pre-4.1 server */
    tee_fprintf(stdout, "Client characterset:\t%s\n", charset_info->csname);
    tee_fprintf(stdout, "Server characterset:\t%s\n", mysql.charset->csname);
  }

#ifndef EMBEDDED_LIBRARY
  if (strstr(mysql_get_host_info(&mysql),"TCP/IP") || ! mysql.unix_socket)
    tee_fprintf(stdout, "TCP port:\t\t%d\n", mysql.port);
  else
    tee_fprintf(stdout, "UNIX socket:\t\t%s\n", mysql.unix_socket);
  if (mysql.net.compress)
    tee_fprintf(stdout, "Protocol:\t\tCompressed\n");
#endif

  const char *pos;
  if ((status_str= mysql_stat(&mysql)) && !mysql_error(&mysql)[0] &&
      (pos= strchr(status_str,' ')))
  {
    ulong sec;
    /* print label */
    tee_fprintf(stdout, "%.*s\t\t\t", (int) (pos-status_str), status_str);
    if ((status_str= str2int(pos,10,0,LONG_MAX,(long*) &sec)))
    {
      nice_time((double) sec,buff,0);
      tee_puts(buff, stdout);			/* print nice time */
      while (*status_str == ' ')
        status_str++;  /* to next info */
      tee_putc('\n', stdout);
      tee_puts(status_str, stdout);
    }
  }
  if (safe_updates)
  {
    my_vidattr(A_BOLD);
    tee_fprintf(stdout, "\nNote that you are running in safe_update_mode:\n");
    my_vidattr(A_NORMAL);
    tee_fprintf(stdout, "\
UPDATEs and DELETEs that don't use a key in the WHERE clause are not allowed.\n\
(One can force an UPDATE/DELETE by adding LIMIT # at the end of the command.)\n\
SELECT has an automatic 'LIMIT %lu' if LIMIT is not used.\n\
Max number of examined row combination in a join is set to: %lu\n\n",
select_limit, max_join_size);
  }
  tee_puts("--------------\n", stdout);
  return 0;
}

static const char *
server_version_string(MYSQL *con)
{
  /* Only one thread calls this, so no synchronization is needed */
  if (server_version == NULL)
  {
    MYSQL_RES *result;

    /* "limit 1" is protection against SQL_SELECT_LIMIT=0 */
    char const *sql = con->oracle_mode ? "select @@version_comment from dual where rownum <= 1" : "select @@version_comment limit 1";
    if (!mysql_query(con, sql) &&
        (result = mysql_use_result(con)))
    {
      MYSQL_ROW cur = mysql_fetch_row(result);
      if (cur && cur[0])
      {
        /* version, space, comment, \0 */
        size_t len= strlen(mysql_get_server_info(con)) + strlen(cur[0]) + 2;

        if ((server_version= (char *) my_malloc(len, MYF(MY_WME))))
        {
          //char *bufp;
          //bufp = strmov(server_version, mysql_get_server_info(con));
          //bufp = strmov(bufp, " ");
          //(void) strmov(bufp, cur[0]);
          strmov(server_version, cur[0]);
        }
      }
      mysql_free_result(result);
    }

    /*
      If for some reason we didn't get a version_comment, we'll
      keep things simple.
    */

    if (server_version == NULL)
      server_version= my_strdup(mysql_get_server_info(con), MYF(MY_WME));
  }

  return server_version ? server_version : "";
}

static int trim_oracle_errmsg(const char **str, int need_trim)
{
  int ret = false;
  if (str != NULL && *str != NULL && strlen(*str) > 11) {
    const char *tmp_str = strchr(*str, '-');
    if (tmp_str != NULL
      && tmp_str - *str <= 5
      && strlen(tmp_str) > 8
      && isdigit(tmp_str[1])
      && isdigit(tmp_str[2])
      && isdigit(tmp_str[3])
      && isdigit(tmp_str[4])
      && isdigit(tmp_str[5])
      && tmp_str[6] == ':'
      && tmp_str[7] == ' ') {
      if (need_trim) {
        *str = tmp_str + 8;
      }
      ret = true;
    }
  }
  return ret;
}


static int
put_info(const char *str, INFO_TYPE info_type, uint error, const char *sqlstate, my_bool oracle_mode)
{
  FILE *file = (info_type == INFO_ERROR ? stderr : stdout);
  static int inited = 0;
  my_bool is_new_errmsg = oracle_mode && trim_oracle_errmsg(&str, false);

  /*When 2>&1 is redirected in non-interactive mode, 
  fflush is forced to prevent stderr and stdout from being in the wrong order*/
  if (info_type == INFO_ERROR){
    fflush(stdout);
  }
  
  if (status.batch) {
    if (info_type == INFO_ERROR) {
      (void)fflush(file);
      if (is_new_errmsg) {
        (void)fprintf(file, "%s\n", str);
      } else {
        if (oracle_mode && error) {
          (void)fprintf(file, "ERROR-%05d: ", error);
        } else {
          fprintf(file, "ERROR");
          if (error) {
            if (sqlstate) {
              (void)fprintf(file, " %d (%s)", error, sqlstate);
            } else {
              (void)fprintf(file, " %d", error);
            }
          }
        }

        if (status.query_start_line && line_numbers) {
          (void)fprintf(file, " at line %lu", status.query_start_line);
          if (status.file_name) {
            (void)fprintf(file, " in file: '%s'", status.file_name);
          }
        }
        (void)fprintf(file, ": %s\n", str);
      }
      (void)fflush(file);
      if (!ignore_errors) {
        return 1;
      }
    } else if (info_type == INFO_RESULT && verbose > 1) {
      tee_puts(str, file);
    }

    if (unbuffered) {
      fflush(file);
    }

    return info_type == INFO_ERROR ? -1 : 0;
  }

  if (!opt_silent || info_type == INFO_ERROR) {
    if (!inited) {
      inited = 1;
    }

    if (info_type == INFO_ERROR) {
      if (!opt_nobeep) {
        putchar('\a');		      	/* This should make a bell */
      }

      if (error) {
        if (is_new_errmsg) {
          //do nothing
        } else if (oracle_mode) {
          (void)tee_fprintf(file, "ERROR-%05d: ", error);
        } else {
          if (sqlstate) {
            (void)tee_fprintf(file, "ERROR %d (%s): ", error, sqlstate);
          } else {
            (void)tee_fprintf(file, "ERROR %d: ", error);
          }
        }
      } else {
        tee_fprintf(file, "ERROR: ");
        //tee_puts("ERROR: ", file);
      }
    }
    (void)tee_puts(str, file);
  }

  if (unbuffered) {
    fflush(file);
  }
  return info_type == INFO_ERROR ? (ignore_errors ? -1 : 1) : 0;
}


static int
put_error(MYSQL *con)
{
  return put_info(mysql_error(con), INFO_ERROR, mysql_errno(con),
		  mysql_sqlstate(con), con->oracle_mode);
}  

static int
put_stmt_error(MYSQL *con, MYSQL_STMT *stmt)
{
  return put_info(mysql_stmt_error(stmt), INFO_ERROR, mysql_stmt_errno(stmt),
    mysql_stmt_sqlstate(stmt), mysql.oracle_mode);
}


static void remove_cntrl(String &buffer)
{
  char *start,*end;
  end=(start=(char*) buffer.ptr())+buffer.length();
  while (start < end && !my_isgraph(charset_info,end[-1]))
    end--;
  buffer.length((uint) (end-start));
}


void tee_fprintf(FILE *file, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  (void) vfprintf(file, fmt, args);
  va_end(args);

  if (opt_outfile)
  {
    va_start(args, fmt);
    (void) vfprintf(OUTFILE, fmt, args);
    va_end(args);
  }
}


void tee_fputs(const char *s, FILE *file)
{
  fputs(s, file);
  if (opt_outfile)
    fputs(s, OUTFILE);
}


void tee_puts(const char *s, FILE *file)
{
  fputs(s, file);
  fputc('\n', file);
  if (opt_outfile)
  {
    fputs(s, OUTFILE);
    fputc('\n', OUTFILE);
  }
}

void tee_putc(int c, FILE *file)
{
  putc(c, file);
  if (opt_outfile)
    putc(c, OUTFILE);
}

void tee_outfile(const char *s) {
  if (opt_outfile)
  {
    fputs(s, OUTFILE);
  }
}

/** 
  Write as many as 52+1 bytes to buff, in the form of a legible duration of time.

  len("4294967296 days, 23 hours, 59 minutes, 60.000 seconds")  ->  53
*/
static void nice_time(double sec,char *buff,bool part_second)
{
  ulong tmp;
  if (sec >= 3600.0*24)
  {
    tmp=(ulong) floor(sec/(3600.0*24));
    sec-=3600.0*24*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
    buff=strmov(buff,tmp > 1 ? " days " : " day ");
  }
  if (sec >= 3600.0)
  {
    tmp=(ulong) floor(sec/3600.0);
    sec-=3600.0*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
    buff=strmov(buff,tmp > 1 ? " hours " : " hour ");
  }
  if (sec >= 60.0)
  {
    tmp=(ulong) floor(sec/60.0);
    sec-=60.0*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
    buff=strmov(buff," min ");
  }
  if (part_second)
    sprintf(buff,"%.3f sec",sec);
  else
    sprintf(buff,"%d sec",(int) sec);
}


static void end_timer(ulonglong start_time, char *buff)
{
  double sec;

  buff[0]=' ';
  buff[1]='(';
  sec= (microsecond_interval_timer() - start_time) / (double) (1000 * 1000);
  nice_time(sec, buff + 2, 1);
  strmov(strend(buff),")");
}

static void get_tenant_clust_name(const char *username, char* tenant, int tenant_len, char* clust, int clust_len)
{
  int i = 0, j = 0;
  my_bool in_tenant = 0;
  my_bool in_clust = 0;
  char quto = 0;
  if (username) {
    char *s = (char*)username, *e = s + strlen(s);
    while (s < e) {
      if ((*s == '\"' || *s == '\'') && quto == 0) {
        quto = *s;
      } else if ((*s == '\"' || *s == '\'') && quto != 0){
        quto = 0;
      } else if (*s == '@' && in_tenant == 0 && quto == 0) {
        in_tenant = 1;
      } else if (*s == '#' && in_tenant == 1 && quto == 0) {
        in_tenant = 0;
        in_clust = 1;
      } else if (*s == ':' && in_clust == 1 && quto == 0) {
        in_clust = 0;
        break;
      }
      if (in_tenant == 1 && *s != '@') {
        if (tenant && i < tenant_len)
          tenant[i++] = *s;
      }
      if (in_clust == 1 && *s != '#') {
        if (clust && j < clust_len)
          clust[j++] = *s;
      }
      s++;
    }
  }
}
static const char *construct_prompt()
{
  processed_prompt.free();			// Erase the old prompt
  time_t  lclock = time(NULL);			// Get the date struct
  struct tm *t = localtime(&lclock);

  /* parse through the settings for the prompt */
  for (char *c = current_prompt; *c ; c++)
  {
    if (*c != PROMPT_CHAR)
	processed_prompt.append(*c);
    else
    {
      switch (*++c) {
      case '\0':
	c--;			// stop it from going beyond if ends with %
	break;
      case 'c':
	add_int_to_prompt(++prompt_counter);
	break;
      case 'v':
	if (connected)
	  processed_prompt.append(mysql_get_server_info(&mysql));
	else
	  processed_prompt.append("not_connected");
	break;
      case 'd':
	processed_prompt.append(current_db ? current_db : "(none)");
	break;
      case 'N':
        if (connected)
          processed_prompt.append(mysql_get_server_name(&mysql));
        else
          processed_prompt.append("unknown");
        break;
      case 'h':
      case 'H':
      {
        const char *prompt;
        prompt= connected ? mysql_get_host_info(&mysql) : "not_connected";
        if (strstr(prompt, "Localhost") || strstr(prompt, "localhost "))
        {
          if (*c == 'h')
            processed_prompt.append("localhost");
          else
          {
            static char hostname[FN_REFLEN];
            if (hostname[0])
              processed_prompt.append(hostname);
            else if (gethostname(hostname, sizeof(hostname)) == 0)
              processed_prompt.append(hostname);
            else
              processed_prompt.append("gethostname(2) failed");
          }
        }
        else
        {
          const char *end=strcend(prompt,' ');
          processed_prompt.append(prompt, (uint) (end-prompt));
        }
        break;
      }
      case 'p':
      {
#ifndef EMBEDDED_LIBRARY
	if (!connected)
	{
	  processed_prompt.append("not_connected");
	  break;
	}

	const char *host_info = mysql_get_host_info(&mysql);
	if (strstr(host_info, "memory")) 
	{
		processed_prompt.append( mysql.host );
	}
	else if (strstr(host_info,"TCP/IP") ||
	    !mysql.unix_socket)
	  add_int_to_prompt(mysql.port);
	else
	{
	  char *pos=strrchr(mysql.unix_socket,'/');
 	  processed_prompt.append(pos ? pos+1 : mysql.unix_socket);
	}
#endif
      }
	break;
      case 'U':
	if (!full_username)
	  init_username();
        processed_prompt.append(full_username ? full_username :
                                (current_user ?  current_user : "(unknown)"));
	break;
      case 'u':
	if (!full_username)
	  init_username();
        processed_prompt.append(part_username ? part_username :
                                (current_user ?  current_user : "(unknown)"));
	break;
      case PROMPT_CHAR:
	processed_prompt.append(PROMPT_CHAR);
	break;
      case 'n':
	processed_prompt.append('\n');
	break;
      case ' ':
      case '_':
	processed_prompt.append(' ');
	break;
      case 'R':
	if (t->tm_hour < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(t->tm_hour);
	break;
      case 'r':
	int getHour;
	getHour = t->tm_hour % 12;
	if (getHour == 0)
	  getHour=12;
	if (getHour < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(getHour);
	break;
      case 'm':
	if (t->tm_min < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(t->tm_min);
	break;
      case 'y':
	int getYear;
	getYear = t->tm_year % 100;
	if (getYear < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(getYear);
	break;
      case 'Y':
	add_int_to_prompt(t->tm_year+1900);
	break;
      case 'D':
	char* dateTime;
	dateTime = ctime(&lclock);
	processed_prompt.append(strtok(dateTime,"\n"));
	break;
      case 's':
	if (t->tm_sec < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(t->tm_sec);
	break;
      case 'w':
	processed_prompt.append(day_names[t->tm_wday]);
	break;
      case 'P':
	processed_prompt.append(t->tm_hour < 12 ? "am" : "pm");
	break;
      case 'o':
	add_int_to_prompt(t->tm_mon+1);
	break;
      case 'O':
	processed_prompt.append(month_names[t->tm_mon]);
	break;
      case '\'':
	processed_prompt.append("'");
	break;
      case '"':
	processed_prompt.append('"');
	break;
      case 'S':
	processed_prompt.append(';');
	break;
      case 't':
	processed_prompt.append('\t');
	break;
      case 'l':
	processed_prompt.append(delimiter_str);
	break;
      case 'T': {
        char tenant[256] = { 0 };
        get_tenant_clust_name(current_user, tenant, 256, NULL, 0);
        if (strlen(tenant) > 0) {
          processed_prompt.append(tenant);
        } else {
          processed_prompt.append("(none)");
        }
      }
      break;
      case 'C': {
        char clust[256] = { 0 };
        get_tenant_clust_name(current_user, NULL, 0, clust, 256);
        if (strlen(clust) > 0) {
          processed_prompt.append(clust);
        } else {
          processed_prompt.append("(none)");
        }
      }
      break;
      default:
	processed_prompt.append(c);
      }
    }
  }
  processed_prompt.append('\0');
  return processed_prompt.ptr();
}


static void add_int_to_prompt(int toadd)
{
  char buffer[16];
  int10_to_str(toadd,buffer,10);
  processed_prompt.append(buffer);
}

static void init_username()
{
  my_free(full_username);
  my_free(part_username);

  MYSQL_RES *UNINIT_VAR(result);
  char const *sql = NULL;
  if (mysql.oracle_mode) {
    sql = "select USER from dual";
  } else {
    sql = "select USER()";
  }

  if (!mysql_query(&mysql, sql) &&
      (result=mysql_use_result(&mysql)))
  {
    MYSQL_ROW cur=mysql_fetch_row(result);
    if (cur) {
      full_username=my_strdup(cur[0],MYF(MY_WME));
      part_username=my_strdup(strtok(cur[0],"@"),MYF(MY_WME));
    }
    mysql_free_result(result);
  }
}

static int com_prompt(String *buffer __attribute__((unused)),
                      char *line)
{
  char *ptr=strchr(line, ' ');
  prompt_counter = 0;
  my_free(current_prompt);
  current_prompt=my_strdup(ptr ? ptr+1 : default_prompt,MYF(MY_WME));
  if (!ptr)
    tee_fprintf(stdout, "Returning to default PROMPT of %s\n", default_prompt);
  else
    tee_fprintf(stdout, "PROMPT set to '%s'\n", current_prompt);
  return 0;
}

#ifndef EMBEDDED_LIBRARY
static void report_progress(const MYSQL *mysql, uint stage, uint max_stage,
                            double progress, const char *proc_info,
                            uint proc_info_length)
{
  uint length= printf("Stage: %d of %d '%.*s' %6.3g%% of stage done",
                      stage, max_stage, proc_info_length, proc_info, 
                      progress);
  if (length < last_progress_report_length)
    printf("%*s", last_progress_report_length - length, "");
  putc('\r', stdout);
  fflush(stdout);
  last_progress_report_length= length;
}

static void report_progress_end()
{
  if (last_progress_report_length)
  {
    printf("%*s\r", last_progress_report_length, "");
    last_progress_report_length= 0;
  }
}
#else
static void report_progress_end()
{
}
#endif


static my_bool is_global_comment(String *buffer)
{
  /*comment*/
  my_bool ret = 1;
  if (NULL == buffer || buffer->is_empty()) {
    ret = 0;
  } else {
    //uint32 i = 0;
    //uint32 len = buffer->length();
    char *p = buffer->c_ptr();
    
    while (*p != 0 && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
      p++;
    }

    if (*p == '/' && *(p + 1) == '*') {
      ret = 1;
    } else if (*p == '-' && *(p + 1) == '-') {
      ret = 1;
    } else {
      ret = 0;
    }
  }
  return ret;
}
static int is_cmd_line(char *pos, size_t len){
  // 这里不用管comment、instring， 调用外面有判断
  int ret = 0; //1:/ 2:\s,\h...
  size_t i = 0;
  my_bool is_cmd = 0;
  char first_char = 0;
  char last_char = 0;
  int cmd_chars = 0;
  int all_chars = 0;
  char *end_pos = pos + len;
  for (i = 0; i < len; i++) {
    if (pos[i] == ' ' || pos[i] == '\t' || pos[i] == '\r' || pos[i] == '\n' || pos[i]==0) {
      continue;
    }
    if (first_char == 0) {
      first_char = pos[i];
    }

#ifdef USE_MB
    int length = 0;
    if (use_mb(charset_info) &&
      (length = my_ismbchar(charset_info, &pos[i], end_pos)))
    {
      if (length > 1) {
        i += length - 1;
        continue;
      }
    }
#endif
    if (pos[i] == '\\') {
      is_cmd = 1;
      cmd_chars = 0;
    }
    cmd_chars++;
    all_chars++;
    last_char = pos[i];
  }
  if (first_char == '/' && all_chars == 1) {
    ret = IS_CMD_SLASH;
  } else if (first_char == '\\' || 
          (is_cmd == 1 && cmd_chars == 2) || 
          (is_cmd==1 && cmd_chars == 3 && last_char == ';')) { // select 1 from dual \s;
    ret = IS_CMD_LINE;//\s,\g,\! date...
  }
  return ret;
}
static my_bool is_space_buffer(char *start, char *end) {
  my_bool ret = 1;
  while (start < end) {
    if (!my_isspace(charset_info, *start) && *start !='\r' && *start != '\n') {
      ret = 0;
      break;
    }
    start++;
  }
  return ret;
}
static my_bool is_termout_oracle_enable(MYSQL *mysql) {
  my_bool ret = 0;
  if (!mysql->oracle_mode ||
    !(mysql->oracle_mode && in_com_source && !is_termout_oracle)) {
    ret = 1;
  }
  return ret;
}

static my_bool prompt_cmd_oracle(const char *str, char **text) {
  my_bool rst = 0;
  char *p = (char*)str;
  char *start = NULL;
  int len = 0;
 
  while (my_isspace(charset_info, *p))
    p++;
 
  start = p;
  while (*p != '\0' && !my_isspace(charset_info, *p))
    p++;
  len = p - start;
  if (strncasecmp("prompt", start, len ) != 0 || len<3){
    return rst;
  }
 
  rst = 1;
  while (my_isspace(charset_info, *p))
    p++;
 
  *text = p;
  return rst;
}
static my_bool exec_cmd_oracle(String *buffer, String *newbuffer) {
  my_bool rst = 0;
  char *p = (char*)buffer->c_ptr_safe();
  char *exec_start = NULL, *exec_end = NULL;
  char *start = NULL, *end = NULL;
  int exec_len = 0, len = 0;
  exec_end = exec_end;
  end = end;

  while (my_isspace(charset_info, *p))
    p++;

  exec_start = p;
  while (*p != '\0' && !my_isspace(charset_info, *p))
    p++;
  exec_len = p - exec_start;

  while (my_isspace(charset_info, *p))
    p++;

  start = p;
  len = strlen(p);

  if (exec_len < 4 || len <= 0) {
    return rst;
  }

  if (strncasecmp("execute", exec_start, exec_len) == 0 && exec_len >= 4) {
    rst = 1;
    newbuffer->append("begin ");
    newbuffer->append(start);
    newbuffer->append("; end;");
  }
  return rst;
}

static int is_on_off(const char* str, int len){
  if (strncasecmp("on", str, len)==0 && len == 2){
    return 1;
  } else if(strncasecmp("off", str, len) == 0 && len == 3){
    return 0;
  } else {
    return -1;
  }
}
static int to_digit(const char* str, int len){
  int i = 0;
  for(i =0; i < len; i++){
    if (!my_isdigit(charset_info, *(str+i)))
      return -1;
  }
  return atoi(str);
}
static my_bool set_cmd_oracle(const char *str){
  //set define on/off/* ...
  //set feedback on/off/n
  //set echo on/off
  //set sqlblanklines on/off
  //set serveroutput on/off
  //set heading on/off

  my_bool rst = 0;
  char *p = (char*)str;
  char *name_start = NULL, *name_end = NULL;
  char *start = NULL, *end = NULL;
  int name_len = 0, len = 0;
  name_end = name_end;
  end = end;

  while (my_isspace(charset_info, *p))
    p++;

  if (strncasecmp("set ", p, 4) == 0) {
    p = p + 4;
  } else {
    return rst;
  }

  while (my_isspace(charset_info, *p))
    p++;

  name_start = p;
  while (*p != '\0' && !my_isspace(charset_info, *p))
    p++;
  name_len = p - name_start;

  while (my_isspace(charset_info, *p))
    p++;

  start = p;
  len = strlen(p);

  if (name_len<=0 || len<=0){
    return rst;
  }

  if (strncasecmp("define", name_start, name_len)==0 && name_len>=3){
    rst = 1;
    if (len == 1){
      if (my_isdigit(charset_info, *start) || my_isalpha(charset_info, *start) || *start==' '||*start ==' '){
        put_info("SP2-0272: define Characters cannot be alphanumeric or blank", INFO_INFO);
      } else {
        define_char_oracle = *start;
        is_define_oracle = 1;
      }
    } else if (is_on_off(start, len)>=0){
      is_define_oracle = is_on_off(start, len);
      if (is_define_oracle==1){
        define_char_oracle = '&';
      }
    } else {
      put_info("SP2-0268: define value is invalid", INFO_INFO);
    }
  } else if (strncasecmp("feedback", name_start, name_len)==0 && name_len>=4){
    rst = 1;
    feedback_int_oracle = to_digit(start, len);
    if (feedback_int_oracle>=0){
      if (feedback_int_oracle>50000){
        put_info("SP2-0267: feedback value is (0-50000)", INFO_INFO);
      } else {
        is_feedback_oracle = feedback_int_oracle > 0 ? 1 : 0;
      }
    } else if (is_on_off(start, len) >= 0) {
      is_feedback_oracle = is_on_off(start, len);
      feedback_int_oracle = -1;
    } else {
      put_info("SP2-0268: feedback value is invalid", INFO_INFO);
    }
  } else if (strncasecmp("echo", name_start, 4)== 0){
    rst = 1;
    if (is_on_off(start, len) >= 0){
      is_echo_oracle = is_on_off(start, len);
    } else {
      put_info("SP2-0265: echo value is ON or OFF", INFO_INFO);
    }
  } else if (strncasecmp("sqlblanklines", name_start, name_len)==0 && name_len>=5){
    rst = 1;
    if (is_on_off(start, len) >= 0){
      is_sqlblanklines_oracle = is_on_off(start, len);
    } else {
      put_info("SP2-0265: sqlblanklines value is ON or OFF", INFO_INFO);
    }
  } else if (strncasecmp("heading", name_start, name_len) == 0 && name_len >=3) {
    rst = 1;
    if (is_on_off(start, len) >= 0) {
      column_names = is_on_off(start, len);
    } else {
      put_info("SP2-0265: heading value is ON or OFF", INFO_INFO);
    }
  } else if (strncasecmp("termout", name_start, name_len) ==0 && name_len >=4) {
    rst = 1;
    if (is_on_off(start, len) >= 0) {
      is_termout_oracle = is_on_off(start, len);
    } else {
      put_info("SP2-0265: termout value is ON or OFF", INFO_INFO);
    }
  } else if (strncasecmp("pagesize", name_start, name_len) == 0 && name_len >=5) {
    //The setting is OK, but it doesn’t actually take effect.
    rst = 1;
    pagesize_int_oracle = to_digit(start, len);
  } else if (strncasecmp("trimspool", name_start, name_len) == 0 && name_len >= 4) {
    //The setting is OK, but it doesn’t actually take effect.
    rst = 1;
    if (is_on_off(start, len) >= 0) {
      is_trimspool_oracle = is_on_off(start, len);
    } else {
      put_info("SP2-0265: trimspool value is ON or OFF", INFO_INFO);
    }
  } else if (strncasecmp("exitcommit", name_start, name_len) == 0 && name_len >=5) {
    rst = 1;
    if (is_on_off(start, len) >= 0) {
      is_exitcommit_oracle = is_on_off(start, len);
    } else {
      put_info("SP2-0265: exitcommit value is ON or OFF", INFO_INFO);
    }
  } else if (strncasecmp("numwidth", name_start, name_len) == 0 && name_len >= 3) {
    rst = 1;
    int numwidth = to_digit(start, len);
    if (numwidth == -1) {
      put_info("SP2-0268: numwidth option not a valid number\n", INFO_INFO);
    } else {
      if (numwidth < 2 || numwidth >128) {
        put_info("SP2-0267: numwidth option 349 out of range (2 through 128)\n", INFO_INFO);
      } else {
        obclient_num_width = numwidth;
      }
    }
  } else if (strncasecmp("numformat", name_start, name_len) == 0 && name_len >= 4) {
    put_info("SP2: set num format not supported\n", INFO_INFO);
  } else if (strncasecmp("serveroutput", name_start, 12) == 0) {
    rst = 1;
    if (is_on_off(start, len) == 1){
      uint error = mysql_real_query_for_lazy(dbms_enable_buffer.ptr(), dbms_enable_buffer.length());
      if (!error) {
        dbms_serveroutput = 1;
      } else {
        put_info("set serveroutput on error!", INFO_ERROR);
      }
    } else if (is_on_off(start, len) == 0){
      uint error = mysql_real_query_for_lazy(dbms_disable_buffer.ptr(), dbms_disable_buffer.length());
      if (!error) {
        dbms_serveroutput = 0;
      } else {
        put_info("set serveroutput off error!", INFO_ERROR);
      }
    } else {
      put_info("SP2-0265: serveroutput value is ON or OFF", INFO_INFO);
    }
  }
  return rst;
}
static my_bool set_param_oracle(const char *str) {
  //COLUMN c1 FORMAT format
  my_bool rst = 0;
  regmatch_t subs[NS];

  if (0 == regexec(&column_format_re, str, (size_t)NS, subs, 0)) {
    rst = 1;
    put_info("SP2: column xxx format not supported\n", INFO_INFO);
  }
  return rst;
}
static void get_end_str(char define, String *endstr1, String *endstr2, String *endstr3){
  char *p = (char*)"~!@#$%^&*()+`=-{}|[]:\";'<>?,./\t\r\n\\ ";
  endstr1->length(0);
  endstr2->length(0);
  endstr3->length(0);
  while (*p!='\0'){
    endstr1->append_char(*p);
    if (*p != define) {
      endstr2->append_char(*p);
      if (!my_isspace(charset_info, *p)) {
        endstr3->append_char(*p);
      }
    }
    
    p++;
  }
}
static void get_input_str(String *varkey, String *varval, int defcount){
#define MAX_FGETS 4096
  char buf[MAX_FGETS] = { 0 };
  std::map<void*, void*>::iterator iter;
  //find from map_global
  varval->length(0);
  for(iter = map_global.begin(); iter != map_global.end(); iter++){
    if (strlen((char*)(iter->first)) == varkey->length() && 0 == strncasecmp((char*)iter->first, varkey->c_ptr_safe(), varkey->length())) {
      varval->append((char*)iter->second, strlen((char*)iter->second));
      return;
    }
  }

  tee_fprintf(stdout, "Please input %s val:  ", varkey->c_ptr_safe());
  fgets(buf, MAX_FGETS, stdin);
  if (buf[strlen(buf)-1] == '\n'){
    buf[strlen(buf) - 1] = '\0';
  }
  tee_outfile(buf);
  tee_outfile("\n");
  varval->append(buf);
  if (defcount>1){
    void *key = my_malloc(strlen(varkey->c_ptr_safe())+1, MYF(MY_WME));
    void *val = my_malloc(strlen(varval->c_ptr_safe())+1, MYF(MY_WME));
    if (key != NULL && val != NULL){
      strcpy((char*)key, varkey->c_ptr_safe());
      strcpy((char*)val, varval->c_ptr_safe());
      map_global[key] = val;
    } else {
      my_free(key); my_free(val);
    }
  }
}
static void free_map_global(){
  std::map<void*, void*>::iterator iter;
  for (iter = map_global.begin(); iter != map_global.end(); iter++) {
    my_free(iter->first);
    my_free(iter->second);
  }
  map_global.clear();
}
static my_bool scan_define_oracle(String *buffer, String *newbuffer){
  int line_idx = 1, def_count = 0;
  char *p = buffer->c_ptr_safe();
  my_bool is_scan = 0;
  my_bool in_string = 0;
  String endstr1, endstr2, endstr3, old_line, new_line, var_key, var_value;
  get_end_str(define_char_oracle, &endstr1, &endstr2, &endstr3);
  
  //check define char to end
  while (*p != '\0'){
    //if (*p == '\'' && define_char_oracle != '\'') {
    //  in_string = 1;
    //} else if (in_string && *p == '\'') {
    //  in_string = 0;
    //}
    if (!in_string && *p == define_char_oracle) {  //define start
      my_bool have_space = 0;
      char *start = NULL; //var address
      char *ptmp = p;
      while(*p != '\0' && (my_isspace(charset_info, *p) || NULL != strchr(endstr1.c_ptr_safe(), *p) || *p==define_char_oracle)){
        if (*p == define_char_oracle) {  //second define
          if (have_space == 1) {
            def_count = 0;
            old_line.append(ptmp, p - ptmp);
            new_line.append(ptmp, p - ptmp);
            newbuffer->append(ptmp, p - ptmp);
            ptmp = p;
          }
          have_space = 0;
          def_count++;
        } else if (NULL != strchr(endstr2.c_ptr_safe(), *p)) {
          my_bool have_note = 0;
          have_space = 1;
          while (*p != '\0' && NULL != strchr(endstr2.c_ptr_safe(), *p)) {
            if (NULL != strchr(endstr3.c_ptr_safe(), *p)) {
              have_note = 1;
            }
            p++;
          }

          if (*p == '\0' || have_note) {
            old_line.append(ptmp, p - ptmp);
            new_line.append(ptmp, p - ptmp);
            newbuffer->append(ptmp, p - ptmp);
            ptmp = p;
            def_count = 0;
            have_space = 0;
          }
          continue;
        } else {
          def_count = 0;
          have_space = 0;
          old_line.append(ptmp, p - ptmp);
          new_line.append(ptmp, p - ptmp);
          newbuffer->append(ptmp, p - ptmp);
          ptmp = p;
        }
        p++;
      }

      old_line.append(ptmp, p - ptmp);
      if (def_count >= 3) {
        if (def_count % 2 == 1) {
          new_line.append(ptmp, def_count - 1);
          newbuffer->append(ptmp, def_count - 1);
          def_count = 1;
        } else {
          new_line.append(ptmp, def_count - 2);
          newbuffer->append(ptmp, def_count - 2);
          def_count = 2;
        }
      } else if (def_count == 1 || def_count == 2) {
        ;
      } else if (def_count == 0) {
        new_line.append(ptmp, p - ptmp);
        newbuffer->append(ptmp, p - ptmp);
      }

      start = p;
      while(*p!='\0' && def_count != 0){
        char* p1 = strchr(endstr1.c_ptr_safe(), *p); //define end
        if (p1 != NULL)
          break;
        old_line.append_char(*p++);
      }

      if (p != start){  //var is valid, else invalid return
        var_key.length(0);
        var_key.append(start, p - start);
        get_input_str(&var_key, &var_value, def_count);
        newbuffer->append(var_value);
        new_line.append(var_value);
        is_scan = 1;
        def_count = 0;
      } else {
        //var error, return
        is_scan = 0;
        def_count = 0;
        continue;
      }
    }

    if (!in_string && *p == define_char_oracle)
      continue;

    if (*p != '\0'){
      newbuffer->append_char(*p);
      if (*p != '\n') {
        new_line.append_char(*p);
        old_line.append_char(*p);
      }
    }
    //output info...
    if (*p == '\n' || *p == '\0' || *(p + 1) == '\0') {
      if (is_scan) {
        is_scan = 0;
        tee_fprintf(stdout, "Old Val  %d:   %s\n", line_idx, old_line.c_ptr_safe());
        tee_fprintf(stdout, "New Val  %d:   %s\n", line_idx, new_line.c_ptr_safe());
      }
      old_line.length(0);
      new_line.length(0);
      line_idx++;
    }

    if (*p != '\0')
      p++;
  }
  return 1;
}

static char* get_login_sql_path()
{
  char *path = NULL;
  path = getenv("OBCLIENT_LOGIN_FILE");
  if (path != NULL) {
    snprintf(obclient_login, sizeof(obclient_login), "%s", path);
    return obclient_login;
  }

  path = getenv("OBCLIENT_PATH"); //obclient_login.sql
  if (path != NULL) {
#ifdef _WIN32
    snprintf(obclient_login, sizeof(obclient_login), "%s\\%s", path, "obclient_login.sql");
#else
    snprintf(obclient_login, sizeof(obclient_login), "%s/%s", path, "obclient_login.sql");
#endif
    return obclient_login;
  }
  return path;
}
static void start_login_sql(MYSQL *mysql, String *buffer, const char *path)
{
  if (NULL != path && access(path, R_OK)==0) {
    char buf[MAX_BUFFER_SIZE] = { 0 };
    snprintf(buf, sizeof(buf), "source %s", path);
    is_feedback_oracle = 0;
    com_source(buffer, (char*)buf);
    is_feedback_oracle = 1;
  }
}

static int handle_oracle_sql(MYSQL *mysql, String *buffer) {
  int ret = 0;
  regmatch_t subs[NS];

  if (0 == regexec(&pl_show_errors_sql_re, buffer->c_ptr_safe(), (size_t)NS, subs, 0)) {
#define GET_ERRORS_MAX_SQL_LENGTH 512
    bool is_need_free_type = false;
    bool is_need_free_object = false;
    bool is_need_free_schema = false;
    char sql[GET_ERRORS_MAX_SQL_LENGTH] = { '\0' };

    if (-1 != subs[show_suffix_offset].rm_so && -1 != subs[show_suffix_offset].rm_eo) {
      pl_type_len = subs[show_type_offset].rm_eo - subs[show_type_offset].rm_so;
      pl_type = my_strndup(buffer->c_ptr_safe() + subs[show_type_offset].rm_so, pl_type_len, MYF(MY_WME));
      mytoupper(pl_type);
      is_need_free_type = true;

      pl_object_len = subs[show_object_offset].rm_eo - subs[show_object_offset].rm_so;
      pl_object = buffer->c_ptr_safe() + subs[show_object_offset].rm_so;
      if (-1 == subs[show_object_quote_offset].rm_eo || -1 == subs[show_object_quote_offset].rm_so) {
        pl_object = my_strndup(pl_object, pl_object_len, MYF(MY_WME));
        mytoupper(pl_object);
        is_need_free_object = true;
      }

      if (-1 != subs[show_schema_offset].rm_so || -1 != subs[show_schema_offset].rm_eo) {
        pl_schema_len = subs[show_schema_offset].rm_eo - subs[show_schema_offset].rm_so;
        pl_schema = buffer->c_ptr_safe() + subs[show_schema_offset].rm_so;
        if (-1 == subs[show_schema_quote_offset].rm_eo || -1 == subs[show_schema_quote_offset].rm_so) {
          pl_schema = my_strndup(pl_schema, pl_schema_len, MYF(MY_WME));
          mytoupper(pl_schema);
          is_need_free_schema = true;
        }
      } else {
        pl_schema = current_db;
        pl_schema_len = strlen(pl_schema);
      }
    } else {
      pl_schema = pl_last_schema == NULL ? current_db : pl_last_schema;
      pl_schema_len = strlen(pl_schema);

      pl_type = pl_last_type;
      pl_type_len = 0;
      if (NULL != pl_type) {
        pl_type_len = strlen(pl_last_type);
      }

      pl_object = pl_last_object;
      pl_object_len = 0;
      if (NULL != pl_object) {
        pl_object_len = strlen(pl_last_object);
      }
    }


    my_snprintf(sql, GET_ERRORS_MAX_SQL_LENGTH, pl_get_errors_sql,
      pl_object_len, pl_object, pl_type_len, pl_type, pl_schema_len, pl_schema);

    if (is_need_free_type) {
      my_free(pl_type);
    }
    if (is_need_free_object) {
      my_free(pl_object);
    }
    if (is_need_free_schema) {
      my_free(pl_schema);
    }

    buffer->length(0);
    buffer->append(sql, strlen(sql));
  } else if(0 == (regexec(&pl_create_sql_re, buffer->c_ptr_safe(), (size_t)NS, subs, 0))){
    my_free(pl_last_type);
    pl_last_type = my_strndup(buffer->c_ptr_safe() + subs[create_type_offset].rm_so,
      subs[create_type_offset].rm_eo - subs[create_type_offset].rm_so, MYF(MY_WME));
    mytoupper(pl_last_type);

    my_free(pl_last_object);
    pl_last_object = NULL;
    if (-1 != subs[create_object_offset].rm_eo && -1 != subs[create_object_offset].rm_so) {
      pl_last_object = my_strndup(buffer->c_ptr_safe() + subs[create_object_offset].rm_so,
        subs[create_object_offset].rm_eo - subs[create_object_offset].rm_so, MYF(MY_WME));
      mytoupper(pl_last_object);
    }

    if (-1 != subs[create_object_quote_offset].rm_eo && -1 != subs[create_object_quote_offset].rm_so) {
      char buf[1024] = { 0 };
      char* p_in = buffer->c_ptr_safe() + subs[create_object_quote_offset].rm_so;
      int p_in_len = subs[create_object_quote_offset].rm_eo - subs[create_object_quote_offset].rm_so;
      fmt_str(p_in, p_in_len, buf, 1024);
      my_free(pl_last_object);
      pl_last_object = NULL;
      pl_last_object = my_strndup(buf, strlen(buf), MYF(MY_WME));
      //mytoupper(pl_last_object);
    }

    my_free(pl_last_schema);
    pl_last_schema = NULL;
    if (-1 != subs[create_schema_offset].rm_eo && -1 != subs[create_schema_offset].rm_so) {
      pl_last_schema = my_strndup(buffer->c_ptr_safe() + subs[create_schema_offset].rm_so,
        subs[create_schema_offset].rm_eo - subs[create_schema_offset].rm_so, MYF(MY_WME));
      mytoupper(pl_last_schema);
    }
    if (-1 != subs[create_schema_quote_offset].rm_eo && -1 != subs[create_schema_quote_offset].rm_so) {
      char buf[1024] = { 0 };
      char* p_in = buffer->c_ptr_safe() + subs[create_schema_quote_offset].rm_so;
      int p_in_len = subs[create_schema_quote_offset].rm_eo - subs[create_schema_quote_offset].rm_so;
      fmt_str(p_in, p_in_len, buf, 1024);
      my_free(pl_last_schema);
      pl_last_schema = NULL;
      pl_last_schema = my_strndup(buf, strlen(buf), MYF(MY_WME));
      //mytoupper(pl_last_schema);
    }
  } else if (0 == (regexec(&pl_alter_sql_re, buffer->c_ptr_safe(), (size_t)NS, subs, 0))) {
    my_free(pl_last_type);
    pl_last_type = my_strndup(buffer->c_ptr_safe() + subs[alter_type_offset].rm_so,
      subs[alter_type_offset].rm_eo - subs[alter_type_offset].rm_so, MYF(MY_WME));
    mytoupper(pl_last_type);

    my_free(pl_last_object);
    pl_last_object = NULL;
    if (-1 != subs[alter_object_offset].rm_eo && -1 != subs[alter_object_offset].rm_so) {
      pl_last_object = my_strndup(buffer->c_ptr_safe() + subs[alter_object_offset].rm_so,
        subs[alter_object_offset].rm_eo - subs[alter_object_offset].rm_so, MYF(MY_WME));
      mytoupper(pl_last_object);
    }

    if (-1 != subs[alter_object_quote_offset].rm_eo && -1 != subs[alter_object_quote_offset].rm_so) {
      char buf[1024] = { 0 };
      char* p_in = buffer->c_ptr_safe() + subs[alter_object_quote_offset].rm_so;
      int p_in_len = subs[alter_object_quote_offset].rm_eo - subs[alter_object_quote_offset].rm_so;
      fmt_str(p_in, p_in_len, buf, 1024);
      my_free(pl_last_object);
      pl_last_object = NULL;
      pl_last_object = my_strndup(buf, strlen(buf), MYF(MY_WME));
      //mytoupper(pl_last_object);
    }

    my_free(pl_last_schema);
    pl_last_schema = NULL;
    if (-1 != subs[alter_schema_offset].rm_eo && -1 != subs[alter_schema_offset].rm_so) {
      pl_last_schema = my_strndup(buffer->c_ptr_safe() + subs[alter_schema_offset].rm_so,
        subs[alter_schema_offset].rm_eo - subs[alter_schema_offset].rm_so, MYF(MY_WME));
      mytoupper(pl_last_schema);
    }
    if (-1 != subs[alter_schema_quote_offset].rm_eo && -1 != subs[alter_schema_quote_offset].rm_so) {
      char buf[1024] = { 0 };
      char* p_in = buffer->c_ptr_safe() + subs[alter_schema_quote_offset].rm_so;
      int p_in_len = subs[alter_schema_quote_offset].rm_eo - subs[alter_schema_quote_offset].rm_so;
      fmt_str(p_in, p_in_len, buf, 1024);
      my_free(pl_last_schema);
      pl_last_schema = NULL;
      pl_last_schema = my_strndup(buf, strlen(buf), MYF(MY_WME));
      //mytoupper(pl_last_schema);
    }
  }
  return ret;
}

static void call_dbms_get_line(MYSQL *mysql, uint error){
  uint inner_error = 0;
  do {
    if (NULL == dbms_stmt && !(dbms_stmt = mysql_stmt_init(mysql))) {
      inner_error = put_error(mysql);
    } else if (!dbms_prepared && mysql_stmt_prepare(dbms_stmt, dbms_get_line, strlen(dbms_get_line))) {
      inner_error = put_stmt_error(mysql, dbms_stmt);
    } else {
      dbms_prepared = 1;

      if (mysql_stmt_bind_param(dbms_stmt, dbms_ps_params)) {
        inner_error = put_stmt_error(mysql, dbms_stmt);
      } else if (mysql_stmt_execute(dbms_stmt)) {
        inner_error = put_stmt_error(mysql, dbms_stmt);
      } else if (mysql_stmt_store_result(dbms_stmt)) {
        inner_error = put_stmt_error(mysql, dbms_stmt);
      } else if (mysql_stmt_bind_result(dbms_stmt, dbms_rs_bind)) {
        inner_error = put_stmt_error(mysql, dbms_stmt);
      } else if (mysql_stmt_fetch(dbms_stmt)) {
        inner_error = put_stmt_error(mysql, dbms_stmt);
      } else {
        if (!error && dbms_status == '0' && !(*dbms_rs_bind[0].is_null)) {
          int i = 0;
          //put_info(dbms_line_buffer, INFO_RESULT);
          //for print space padding
          for (i = 0; i < *dbms_rs_bind[0].length; i++) {
            if (dbms_line_buffer[i] == 0) {
              dbms_line_buffer[i] = ' ';
            }
          }
          tee_puts(dbms_line_buffer, stdout); //dbms enable, Certain output
        }

        mysql_stmt_free_result(dbms_stmt);
        if (mysql_stmt_more_results(dbms_stmt)) { // for ob3.2.3.3 skip ok package
          mysql_stmt_next_result(dbms_stmt);
        }
      }
    }
  } while (!inner_error && dbms_status == '0');
}

static void print_error_sqlstr(String *buffer, char* topN)
{
  int i = 0;
  int top = 5;
  char *p = (char*)buffer->ptr();
  String topstr;

  //no --error-sql=topN
  if (NULL == topN || NULL == buffer) {
    return;
  }
  if (strncasecmp(topN, "top", 3) == 0 && topN[3] >= '0' && topN[3] <= '9') {
    top = atoi(topN + 3);
    top = top < 0 ? 5 : top;
  }

  //trim space line
  while (*p != 0 && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
    p++;
  }

  while (*p != 0 && (top == 0 || i < top)) {
    topstr.append_char(*p);
    if (*p == '\n')
      i++;
    p++;
  }
  topstr.append_char('\0');

  fprintf(stdout, "----------------------------\n");
  fprintf(stdout, "%s\n\n", topstr.ptr());
  fflush(stdout);
}