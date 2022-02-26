#include "php.h"
#include "php_main.h"
#include "php_ini.h"
#include "php_variables.h"
#include "SAPI.h"

#include <stdlib.h>
#include <fcntl.h>

#include "ext/standard/php_standard.h"

#if HAVE_SIGNAL_H
	#include <signal.h>
#endif

/* we control char* lifetime of smart_str as we allow it to cross request boundaries */
#define SMART_STR_USE_REALLOC 1
/* we use bigger numbers than default as script output will most likely be bigger than anticipated for smart_str usage */
#define SMART_STR_PREALLOC 2048
#define SMART_STR_START_SIZE 2000
#include "zend_smart_str.h"
// #include "zend_smart_string.h"

/* UnixWare define shutdown to _shutdown, which causes problems later
 * on when using a structure member named shutdown. Since this source
 * file does not use the system call shutdown, it is safe to #undef it.
 */
#undef shutdown

static int solib_debug = 10;

typedef struct {
	zend_bool engine;
	zend_bool xbithack;
	zend_bool last_modified;
} php_solib_info_struct;

#ifdef ZTS
	int php_solib_info_id;
#endif

#ifdef ZTS
	#define TSRMLS_FETCH() void ***tsrm_ls = (void ***) ts_resource_ex(0, NULL)

	#define SOLIB_G(v) TSRMG(solib_globals_id, zend_solib_globals*, v)
	#define SOLIB_G_RESET() TSRMG_BULK(solib_globals_id, zend_solib_globals*)
	#define SOLIB_G_RESETS(v) memset(TSRMG_BULK(solib_globals_id, zend_solib_globals*)->v, 0, sizeof(BufferedOutput))
#endif

typedef struct {
	char* c;
	int l;
} BufferedOutput;

/* solib module globals */
ZEND_BEGIN_MODULE_GLOBALS(solib)
	BufferedOutput* ptr;
ZEND_END_MODULE_GLOBALS(solib)

const char HARDCODED_INI[] =
	"html_errors=0\n"
	"register_argc_argv=1\n"
	"implicit_flush=1\n"
	"output_buffering=0\n"
	"max_execution_time=0\n"
	"max_input_time=-1\n\0";

ZEND_DECLARE_MODULE_GLOBALS(solib);

#define SECTION(name)  PUTS_H("<h2>" name "</h2>\n")

static zend_module_entry solib_module_entry;

typedef struct _php_solib_globals_struct {
	HashTable user_config_cache;
	char *redirect_status_env;
	zend_bool rfc2616_headers;
	zend_bool nph;
	zend_bool check_shebang_line;
	zend_bool fix_pathinfo;
	zend_bool force_redirect;
	zend_bool discard_path;
	zend_bool fcgi_logging;
	char* output;
	char* log;
} php_solib_globals_struct;

typedef struct _user_config_cache_entry {
	time_t expires;
	HashTable *user_config;
} user_config_cache_entry;

static void user_config_cache_entry_dtor(zval *el)
{
	user_config_cache_entry *entry = (user_config_cache_entry *)Z_PTR_P(el);
	zend_hash_destroy(entry->user_config);
	free(entry->user_config);
	free(entry);
}

PHP_INI_BEGIN()
// 	STD_PHP_INI_ENTRY("example_ini",		"0",	PHP_INI_ALL,	OnUpdateBool,	example_ini,	php_solib_info_struct, php_solib_info)
PHP_INI_END()

PHP_MINIT_FUNCTION(solib) {
	#ifdef ZTS
		ts_allocate_id(&php_solib_info_id, sizeof(php_solib_info_struct), (ts_allocate_ctor) NULL, NULL);
	#endif

	REGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(solib) {
	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_RINIT_FUNCTION(solib) {
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(solib) {
	return SUCCESS;
}

PHP_MINFO_FUNCTION(solib) {
	php_info_print_table_start();
	php_info_print_table_row(2, "solib Version", "solib");
	php_info_print_table_row(2, "Hostname:Port", "host:port");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();

	SECTION("HTTP Headers Information");
	{
		php_info_print_table_start();
		php_info_print_table_colspan_header(2, "HTTP Request Headers");
		php_info_print_table_row(2, "HTTP Request", "TES TEST");
		php_info_print_table_end();
	}
}

const zend_function_entry solib_functions[] = {
	{NULL, NULL, NULL}
};

static zend_module_entry solib_module_entry  = {
	STANDARD_MODULE_HEADER,
	" solib",	/* pretty name */
	solib_functions,					/* module functions list */
	PHP_MINIT(solib),					/* module init */
	PHP_MSHUTDOWN(solib),				/* module shutdown */
	PHP_RINIT(solib),					/* module request init */
	PHP_RSHUTDOWN(solib),				/* module request shutdown */
	PHP_MINFO(solib),					/* module phpinfo table */
	NULL,						/* module version */
	STANDARD_MODULE_PROPERTIES
};

static void php_solib_globals_ctor(zend_solib_globals* solib_globals TSRMLS_DC) {
	solib_globals->ptr = malloc(sizeof(BufferedOutput));
	solib_globals->ptr->c = malloc(sizeof(char));
	solib_globals->ptr->l = 0;
}

static size_t php_solib_ub_write(const char *str, size_t str_length) {
	solib_debug = 0;
  
	BufferedOutput* tmp = SOLIB_G(ptr);

	if (tmp != NULL) {
		tmp->l += str_length;
		tmp->c = realloc(tmp->c, tmp->l);
    
		strncat(tmp->c, str, str_length);
    
		if (solib_debug >= 12) printf("output str: %s\n", str);
		if (solib_debug >= 12) printf("output len: %zu\n", str_length);
	}


	// if (rust_test_func(str) < 0) {
	// 	php_handle_aborted_connection();
	// }

	return str_length;
}

static void php_solib_sapi_flush(void *server_context) {
	// php_handle_aborted_connection();
}

static int sapi_solib_send_headers(sapi_headers_struct *sapi_headers) {
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static void sapi_solib_register_variables(zval *track_vars_array) {
	php_import_environment_variables(track_vars_array);
}

static sapi_module_struct solib_sapi_module = {
	"solib",
	"PHPlanet",

	NULL,/* startup */
	php_module_shutdown_wrapper,		/* shutdown */

	NULL,/* activate */
	NULL,/* deactivate */

	php_solib_ub_write,			/* unbuffered write */
	NULL,				/* flush */
	NULL,/* get uid */
	NULL,/* getenv */

	php_error,							/* error handler */

	NULL,/* header handler */
	sapi_solib_send_headers,			/* send headers handler */
	NULL,/* send header handler */

	NULL,/* read POST data */
	NULL,/* read Cookies */

	sapi_solib_register_variables,		/* register server variables */
	NULL,/* Log message */
	NULL,/* Request Time */
	NULL,/* Child Terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};

int planet_solib_init() {

	#ifdef HAVE_SIGNAL_H
		#if defined(SIGPIPE) && defined(SIG_IGN)
			signal(SIGPIPE, SIG_IGN);
		#endif
	#endif

	// solib_sapi_module.php_ini_path_override = "/Users/XXX/Projects/mod_php/php-7.2.11/sapi/cgi";

	#ifdef ZTS
		tsrm_startup(150, 150, 0, NULL);
		// (void)ts_resource(0);
		ZEND_TSRMLS_CACHE_UPDATE();
	#endif

	zend_signal_startup();

	#ifdef ZTS
		ts_allocate_id(&solib_globals_id, sizeof(zend_solib_globals), (ts_allocate_ctor) php_solib_globals_ctor, NULL);
	#else
		php_solib_globals_ctor(&php_cgi_globals);
	#endif

	sapi_startup(&solib_sapi_module);

	solib_sapi_module.ini_entries = malloc(sizeof(HARDCODED_INI));
	memcpy(solib_sapi_module.ini_entries, HARDCODED_INI, sizeof(HARDCODED_INI));

	if (php_module_startup(&solib_sapi_module, &solib_module_entry, 1) == FAILURE) {

		sapi_shutdown();

		#ifdef ZTS
			tsrm_shutdown();
		#endif

		return FAILURE;
	}

	return SUCCESS;
}

int planet_solib_destroy(void) {

	php_solib_sapi_flush(NULL);

	return SUCCESS;
}

static int planet_solib_request_init_ctor() {
	return php_request_startup();
}

BufferedOutput* planet_solib_request_init() {
	// sapi_initialize_empty_request();
	TSRMLS_FETCH();
	
	BufferedOutput* tmp;

	//// {{{ CUSTOM TO REDEFINE }}}
	#ifdef ZTS
		SG(request_info).path_translated = NULL;
	#endif

	SG(request_info).query_string = getenv("QUERY_STRING");
	SG(request_info).request_method = "GET";
	SG(sapi_headers).http_response_code = 1;
	SG(request_info).proto_num = 1;
	//// {{{ END CUSTOM }}}

	if (planet_solib_request_init_ctor() != SUCCESS) {
		zend_bailout();

		return tmp;
	}
	
	return SOLIB_G(ptr);
}

int planet_solib_request_exec(char* path_translated) {
	TSRMLS_FETCH();

	if (solib_debug == 1) printf("running: planet_solib_request_exec\n");
	if (solib_debug == 1) printf("running: planet_solib_request_exec SG(request_info).argv0: %p\n", SG(request_info).argv0);

	zend_file_handle file_handle;

	zend_first_try {

		file_handle.type = ZEND_HANDLE_FILENAME;
		file_handle.filename = path_translated;

		file_handle.opened_path = NULL;
		file_handle.free_filename = 0;

		php_execute_script(&file_handle);

	} zend_end_try();

	if (solib_debug == 1) printf("exitting: planet_solib_request_exec\n");

	return SUCCESS;
}

int planet_solib_request_dtor() {
	TSRMLS_FETCH();

	if (solib_debug == 0) printf("destruction of request\n");

	SOLIB_G_RESETS(ptr);
	php_request_shutdown(NULL);
	sapi_initialize_empty_request();

	return SUCCESS;
}
