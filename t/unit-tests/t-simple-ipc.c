#include "git-compat-util.h"
#include "string-list.h"
#include "test-lib.h"
#include "simple-ipc.h"
#include "strbuf.h"
#include "thread-utils.h"
#include "strvec.h"
#include "run-command.h"
#include "trace2.h"
#include "wrapper.h"
#include <pthread.h>

/*
 * The test daemon defines an "application callback" that supports a
 * series of commands (see `test_app_cb()`).
 *
 * Unknown commands are caught here and we send an error message back
 * to the client process.
 */
static int app__unhandled_command(const char *command,
				  ipc_server_reply_cb *reply_cb,
				  struct ipc_server_reply_data *reply_data)
{
	struct strbuf buf = STRBUF_INIT;
	int ret;

	strbuf_addf(&buf, "unhandled command: %s", command);
	ret = reply_cb(reply_data, buf.buf, buf.len);
	strbuf_release(&buf);

	return ret;
}

/*
 * Reply with a single very large buffer.  This is to ensure that
 * long response are properly handled -- whether the chunking occurs
 * in the kernel or in the (probably pkt-line) layer.
 */
#define BIG_ROWS (10000)
static int app__big_command(ipc_server_reply_cb *reply_cb,
			    struct ipc_server_reply_data *reply_data)
{
	struct strbuf buf = STRBUF_INIT;
	int row;
	int ret;

	for (row = 0; row < BIG_ROWS; row++)
		strbuf_addf(&buf, "big: %.75d\n", row);

	ret = reply_cb(reply_data, buf.buf, buf.len);
	strbuf_release(&buf);

	return ret;
}

/*
 * Reply with a series of lines.  This is to ensure that we can incrementally
 * compute the response and chunk it to the client.
 */
#define CHUNK_ROWS (10000)
static int app__chunk_command(ipc_server_reply_cb *reply_cb,
			      struct ipc_server_reply_data *reply_data)
{
	struct strbuf buf = STRBUF_INIT;
	int row;
	int ret;

	for (row = 0; row < CHUNK_ROWS; row++) {
		strbuf_setlen(&buf, 0);
		strbuf_addf(&buf, "big: %.75d\n", row);
		ret = reply_cb(reply_data, buf.buf, buf.len);
	}

	strbuf_release(&buf);

	return ret;
}

/*
 * Slowly reply with a series of lines.  This is to model an expensive to
 * compute chunked response (which might happen if this callback is running
 * in a thread and is fighting for a lock with other threads).
 */
#define SLOW_ROWS     (1000)
#define SLOW_DELAY_MS (10)
static int app__slow_command(ipc_server_reply_cb *reply_cb,
			     struct ipc_server_reply_data *reply_data)
{
	struct strbuf buf = STRBUF_INIT;
	int row;
	int ret;

	for (row = 0; row < SLOW_ROWS; row++) {
		strbuf_setlen(&buf, 0);
		strbuf_addf(&buf, "big: %.75d\n", row);
		ret = reply_cb(reply_data, buf.buf, buf.len);
		sleep_millisec(SLOW_DELAY_MS);
	}

	strbuf_release(&buf);

	return ret;
}

/*
 * The client sent a command followed by a (possibly very) large buffer.
 */
static int app__sendbytes_command(const char *received, size_t received_len,
				  ipc_server_reply_cb *reply_cb,
				  struct ipc_server_reply_data *reply_data)
{
	struct strbuf buf_resp = STRBUF_INIT;
	const char *p = "?";
	int len_ballast = 0;
	int k;
	int errs = 0;
	int ret;

	/*
	 * The test is setup to send:
	 *     "sendbytes" SP <n * char>
	 */
	if (received_len < strlen("sendbytes "))
		BUG("received_len is short in app__sendbytes_command");

	if (skip_prefix(received, "sendbytes ", &p))
		len_ballast = strlen(p);

	/*
	 * Verify that the ballast is n copies of a single letter.
	 * And that the multi-threaded IO layer didn't cross the streams.
	 */
	for (k = 1; k < len_ballast; k++)
		if (p[k] != p[0])
			errs++;

	if (errs)
		strbuf_addf(&buf_resp, "errs:%d\n", errs);
	else
		strbuf_addf(&buf_resp, "rcvd:%c%08d\n", p[0], len_ballast);

	ret = reply_cb(reply_data, buf_resp.buf, buf_resp.len);

	strbuf_release(&buf_resp);

	return ret;
}

/*
 * An arbitrary fixed address to verify that the application instance
 * data is handled properly.
 */
static int my_app_data = 42;

static ipc_server_application_cb test_app_cb;

/*
 * This is the "application callback" that sits on top of the
 * "ipc-server".  It completely defines the set of commands supported
 * by this application.
 */
static int test_app_cb(void *application_data,
		       const char *command, size_t command_len,
		       ipc_server_reply_cb *reply_cb,
		       struct ipc_server_reply_data *reply_data)
{
	/*
	 * Verify that we received the application-data that we passed
	 * when we started the ipc-server.  (We have several layers of
	 * callbacks calling callbacks and it's easy to get things mixed
	 * up (especially when some are "void*").)
	 */
	if (application_data != (void*)&my_app_data)
		BUG("application_cb: application_data pointer wrong");

	if (command_len == 4 && !strncmp(command, "quit", 4)) {
		/*
		 * The client sent a "quit" command.  This is an async
		 * request for the server to shutdown.
		 *
		 * We DO NOT send the client a response message
		 * (because we have nothing to say and the other
		 * server threads have not yet stopped).
		 *
		 * Tell the ipc-server layer to start shutting down.
		 * This includes: stop listening for new connections
		 * on the socket/pipe and telling all worker threads
		 * to finish/drain their outgoing responses to other
		 * clients.
		 *
		 * This DOES NOT force an immediate sync shutdown.
		 */
		return SIMPLE_IPC_QUIT;
	}

	if (command_len == 4 && !strncmp(command, "ping", 4)) {
		const char *answer = "pong";
		return reply_cb(reply_data, answer, strlen(answer));
	}

	if (command_len == 3 && !strncmp(command, "big", 3))
		return app__big_command(reply_cb, reply_data);

	if (command_len == 5 && !strncmp(command, "chunk", 5))
		return app__chunk_command(reply_cb, reply_data);

	if (command_len == 4 && !strncmp(command, "slow", 4))
		return app__slow_command(reply_cb, reply_data);

	if (command_len >= 10 && starts_with(command, "sendbytes "))
		return app__sendbytes_command(command, command_len,
					      reply_cb, reply_data);

	return app__unhandled_command(command, reply_cb, reply_data);
}

struct cl_args
{
	const char *subcommand;
	const char *path;
	const char *token;

	int nr_threads;
	int max_wait_sec;
	int bytecount;
	int batchsize;

	char bytevalue;
};

static struct cl_args cl_args = {
	.subcommand = NULL,
	.path = "ipc-test",
	.token = NULL,

	.nr_threads = 5,
	.max_wait_sec = 60,
	.bytecount = 1024,
	.batchsize = 10,

	.bytevalue = 'x',
};

/*
 * This process will run as a simple-ipc server and listen for IPC commands
 * from client processes.
 */
struct daemon_data {
	struct cl_args cl_args;
	int ret;
};

static void *daemon__run_server(void *data)
{
	struct daemon_data *d = data;

	struct ipc_server_opts opts = {
		.nr_threads = d->cl_args.nr_threads,
	};

	/*
	 * Synchronously run the ipc-server.  We don't need any application
	 * instance data, so pass an arbitrary pointer (that we'll later
	 * verify made the round trip).
	 */
	d->ret = ipc_server_run(d->cl_args.path, &opts, test_app_cb, (void*)&my_app_data);
	return NULL;
}

// static start_bg_wait_cb bg_wait_cb;

// static int bg_wait_cb(const struct child_process *cp UNUSED,
// 		      void *cb_data UNUSED)
// {
// 	int s = ipc_get_active_state(cl_args.path);
//
// 	switch (s) {
// 	case IPC_STATE__LISTENING:
// 		/* child is "ready" */
// 		return 0;
//
// 	case IPC_STATE__NOT_LISTENING:
// 	case IPC_STATE__PATH_NOT_FOUND:
// 		/* give child more time */
// 		return 1;
//
// 	default:
// 	case IPC_STATE__INVALID_PATH:
// 	case IPC_STATE__OTHER_ERROR:
// 		/* all the time in world won't help */
// 		return -1;
// 	}
// }

// static int daemon__start_server(struct cl_args *cl_args)
// {
// 	struct child_process cp = CHILD_PROCESS_INIT;
// 	enum start_bg_result sbgr;
//
// 	strvec_push(&cp.args, "test-tool");
// 	strvec_push(&cp.args, "simple-ipc");
// 	strvec_push(&cp.args, "run-daemon");
// 	strvec_pushf(&cp.args, "--name=%s", cl_args->path);
// 	strvec_pushf(&cp.args, "--threads=%d", cl_args->nr_threads);
//
// 	cp.no_stdin = 1;
// 	cp.no_stdout = 1;
// 	cp.no_stderr = 1;
//
// 	sbgr = start_bg_command(&cp, bg_wait_cb, NULL, cl_args->max_wait_sec);
//
// 	switch (sbgr) {
// 	case SBGR_READY:
// 		return 0;
//
// 	default:
// 	case SBGR_ERROR:
// 	case SBGR_CB_ERROR:
// 		return error("daemon failed to start");
//
// 	case SBGR_TIMEOUT:
// 		return error("daemon not online yet");
//
// 	case SBGR_DIED:
// 		return error("daemon terminated");
// 	}
// }

/*
 * This process will run a quick probe to see if a simple-ipc server
 * is active on this path.
 *
 * Returns 0 if the server is alive.
 */
static int client__probe_server(void)
{
	enum ipc_active_state s;

	s = ipc_get_active_state(cl_args.path);
	switch (s) {
	case IPC_STATE__LISTENING:
		return 0;

	case IPC_STATE__NOT_LISTENING:
		return error("no server listening at '%s'", cl_args.path);

	case IPC_STATE__PATH_NOT_FOUND:
		return error("path not found '%s'", cl_args.path);

	case IPC_STATE__INVALID_PATH:
		return error("invalid pipe/socket name '%s'", cl_args.path);

	case IPC_STATE__OTHER_ERROR:
	default:
		return error("other error for '%s'", cl_args.path);
	}
}

/*
 * Send an IPC command token to an already-running server daemon and
 * print the response.
 *
 * This is a simple 1 word command/token that `test_app_cb()` (in the
 * daemon process) will understand.
 */
static int client__send_ipc(struct strbuf *buf)
{
	const char *command = "(no-command)";
	struct ipc_client_connect_options options
		= IPC_CLIENT_CONNECT_OPTIONS_INIT;

	if (cl_args.token && *cl_args.token)
		command = cl_args.token;

	options.wait_if_busy = 1;
	options.wait_if_not_found = 0;

	if (!ipc_client_send_command(cl_args.path, &options,
				     command, strlen(command),
				     buf))
		return 0;

	return error("failed to send '%s' to '%s'", command, cl_args.path);
}

/*
 * Send an IPC command to an already-running server and ask it to
 * shutdown.  "send quit" is an async request and queues a shutdown
 * event in the server, so we spin and wait here for it to actually
 * shutdown to make the unit tests a little easier to write.
 */
static int client__stop_server(void)
{
	int ret;
	time_t time_limit, now;
	enum ipc_active_state s;
	struct strbuf buf = STRBUF_INIT;

	time(&time_limit);
	time_limit += cl_args.max_wait_sec;

	cl_args.token = "quit";

	ret = client__send_ipc(&buf);
	strbuf_release(&buf);
	if (ret)
		return ret;

	for (;;) {
		sleep_millisec(100);

		s = ipc_get_active_state(cl_args.path);

		if (s != IPC_STATE__LISTENING) {
			/*
			 * The socket/pipe is gone and/or has stopped
			 * responding.  Lets assume that the daemon
			 * process has exited too.
			 */
			return 0;
		}

		time(&now);
		if (now > time_limit)
			return error("daemon has not shutdown yet");
	}
}

/*
 * Send an IPC command followed by ballast to confirm that a large
 * message can be sent and that the kernel or pkt-line layers will
 * properly chunk it and that the daemon receives the entire message.
 */
// static int do_sendbytes(int bytecount, char byte, const char *path,
// 			const struct ipc_client_connect_options *options)
// {
// 	struct strbuf buf_send = STRBUF_INIT;
// 	struct strbuf buf_resp = STRBUF_INIT;
//
// 	strbuf_addstr(&buf_send, "sendbytes ");
// 	strbuf_addchars(&buf_send, byte, bytecount);
//
// 	if (!ipc_client_send_command(path, options,
// 				     buf_send.buf, buf_send.len,
// 				     &buf_resp)) {
// 		strbuf_rtrim(&buf_resp);
// 		printf("sent:%c%08d %s\n", byte, bytecount, buf_resp.buf);
// 		fflush(stdout);
// 		strbuf_release(&buf_send);
// 		strbuf_release(&buf_resp);
//
// 		return 0;
// 	}
//
// 	return error("client failed to sendbytes(%d, '%c') to '%s'",
// 		     bytecount, byte, path);
// }

/*
 * Send an IPC command with ballast to an already-running server daemon.
 */
// static int client__sendbytes(void)
// {
// 	struct ipc_client_connect_options options
// 		= IPC_CLIENT_CONNECT_OPTIONS_INIT;
//
// 	options.wait_if_busy = 1;
// 	options.wait_if_not_found = 0;
// 	options.uds_disallow_chdir = 0;
//
// 	return do_sendbytes(cl_args.bytecount, cl_args.bytevalue, cl_args.path,
// 			    &options);
// }

struct multiple_thread_data {
	pthread_t pthread_id;
	struct multiple_thread_data *next;
	const char *path;
	int bytecount;
	int batchsize;
	int sum_errors;
	int sum_good;
	char letter;
};

// static void *multiple_thread_proc(void *_multiple_thread_data)
// {
// 	struct multiple_thread_data *d = _multiple_thread_data;
// 	int k;
// 	struct ipc_client_connect_options options
// 		= IPC_CLIENT_CONNECT_OPTIONS_INIT;
//
// 	options.wait_if_busy = 1;
// 	options.wait_if_not_found = 0;
// 	/*
// 	 * A multi-threaded client should not be randomly calling chdir().
// 	 * The test will pass without this restriction because the test is
// 	 * not otherwise accessing the filesystem, but it makes us honest.
// 	 */
// 	options.uds_disallow_chdir = 1;
//
// 	trace2_thread_start("multiple");
//
// 	for (k = 0; k < d->batchsize; k++) {
// 		if (do_sendbytes(d->bytecount + k, d->letter, d->path, &options))
// 			d->sum_errors++;
// 		else
// 			d->sum_good++;
// 	}
//
// 	trace2_thread_exit();
// 	return NULL;
// }

/*
 * Start a client-side thread pool.  Each thread sends a series of
 * IPC requests.  Each request is on a new connection to the server.
 */
// static int client__multiple(void)
// {
// 	struct multiple_thread_data *list = NULL;
// 	int k;
// 	int sum_join_errors = 0;
// 	int sum_thread_errors = 0;
// 	int sum_good = 0;
//
// 	for (k = 0; k < cl_args.nr_threads; k++) {
// 		struct multiple_thread_data *d = xcalloc(1, sizeof(*d));
// 		d->next = list;
// 		d->path = cl_args.path;
// 		d->bytecount = cl_args.bytecount + cl_args.batchsize*(k/26);
// 		d->batchsize = cl_args.batchsize;
// 		d->sum_errors = 0;
// 		d->sum_good = 0;
// 		d->letter = 'A' + (k % 26);
//
// 		if (pthread_create(&d->pthread_id, NULL, multiple_thread_proc, d)) {
// 			warning("failed to create thread[%d] skipping remainder", k);
// 			free(d);
// 			break;
// 		}
//
// 		list = d;
// 	}
//
// 	while (list) {
// 		struct multiple_thread_data *d = list;
//
// 		if (pthread_join(d->pthread_id, NULL))
// 			sum_join_errors++;
//
// 		sum_thread_errors += d->sum_errors;
// 		sum_good += d->sum_good;
//
// 		list = d->next;
// 		free(d);
// 	}
//
// 	printf("client (good %d) (join %d), (errors %d)\n",
// 	       sum_good, sum_join_errors, sum_thread_errors);
//
// 	return (sum_join_errors + sum_thread_errors) ? 1 : 0;
// }

static void t_start(void)
{
	pthread_t thread_id;
	struct daemon_data d = { .cl_args = cl_args, .ret = 0 };
	d.cl_args.nr_threads = 8;
	
	if (!check_int(pthread_create(&thread_id, NULL, daemon__run_server,
				      &cl_args), ==, 0)) {
		test_msg("could not start a thread for the server");
		return;
	}

	sleep_millisec(500); /* give some time to start the server */
	check_int(client__probe_server(), ==, 0);
}

static void t_simple(void)
{
	struct strbuf buf = STRBUF_INIT;
	cl_args.token = "ping";
	if (check_int(client__send_ipc(&buf), ==, 0))
		check_str(buf.buf, "pong");
	strbuf_release(&buf);
}

static void t_same_path(void)
{
	struct daemon_data d = {.cl_args = cl_args, .ret =0};
	d.cl_args.nr_threads = 8;
	daemon__run_server(&d);
	check_int(d.ret, ==, -2);
	check_int(client__probe_server(), ==, 0);
}

static void t_big_response(void)
{
	struct strbuf buf = STRBUF_INIT;
	cl_args.token = "big";
	if (check_int(client__send_ipc(&buf), ==, 0))
	{
		struct string_list list = STRING_LIST_INIT_DUP;

		string_list_split(&list, buf.buf, '\n', -1);
		check_int(list.nr, >=, 10000);
		string_list_clear(&list, 1);
	}
	strbuf_release(&buf);
}

static void t_stop(void)
{
	if (check_int(client__probe_server(), ==, 0))
		check_int(client__stop_server(), ==, 0);
}

int cmd_main(int argc, const char **argv)
{
#ifndef SUPPORTS_SIMPLE_IPC
	test_skip_all("simple IPC not available on this platform");
#endif
	if(!TEST(t_start(), "start"))
		test_skip_all("could not start server");
	TEST(t_simple(), "simple");
	TEST(t_same_path(), "same path");
	TEST(t_big_response(), "big response");

	TEST(t_stop(), "stop");
	return test_done();
}
