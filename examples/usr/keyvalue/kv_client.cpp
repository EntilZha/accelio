#include <stdio.h>
#include <string>
#include <map>
#include <inttypes.h>
#include <sstream>
#include <iostream>

#include "libxio.h"

using std::string;
using std::map;
using std::cout;

/* private session data */
struct session_data {
  struct xio_context	*ctx;
  struct xio_connection	*conn;
  struct xio_msg	request;
};

static int n_messages = 2;
int n_sent = 0;
string put_header = "PUT";
string get_header = "GET";
string key_str1 = "KEY1";
string value_str1 = "VALUE1";
string key_str2 = "KEY2";
string value_str2 = "VALUE2";

/*---------------------------------------------------------------------------*/
/* on_session_event							     */
/*---------------------------------------------------------------------------*/
static int on_session_event(
		struct xio_session *session,
		struct xio_session_event_data *event_data,
		void *cb_user_context) {
  struct session_data *session_data = (struct session_data *) cb_user_context;

  printf("session event: %s. reason: %s\n",
	 xio_session_event_str(event_data->event),
	 xio_strerror(event_data->reason));

  switch (event_data->event) {
  case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
    xio_connection_destroy(event_data->conn);
    break;
  case XIO_SESSION_TEARDOWN_EVENT:
    xio_session_destroy(session);
    xio_context_stop_loop(session_data->ctx);  /* exit */
    break;
  default:
    break;
  };

  return 0;
}

static void send_msg(string header, string key, string value, xio_msg *req, session_data *session_data) {
	req->out.header.iov_base = (void *) header.c_str();
	req->out.header.iov_len = header.length() + 1;

	req->in.sgl_type = XIO_SGL_TYPE_IOV;
	req->in.data_iov.max_nents = XIO_IOVLEN;

	req->out.sgl_type = XIO_SGL_TYPE_IOV;
	req->out.data_iov.max_nents = XIO_IOVLEN;

	cout << "BEGIN Message Send\n";
	fprintf(stdout, "Header: %s\n", header.c_str());
	fprintf(stdout, "Sending: %s and %s\n", key.c_str(), value.c_str());

	req->out.data_iov.sglist[0].iov_base = (void *) key.c_str();
	req->out.data_iov.sglist[0].iov_len = key.length() + 1;

	req->out.data_iov.sglist[1].iov_base = (void *) value.c_str();
	req->out.data_iov.sglist[1].iov_len = value.length() + 1;

	req->out.data_iov.nents = 2;
	xio_send_request(session_data->conn, req);
}

/*---------------------------------------------------------------------------*/
/* on_response								     */
/*---------------------------------------------------------------------------*/
static int on_response(struct xio_session *session, struct xio_msg *rsp, int last_in_rxq, void *cb_user_context) {
  struct session_data *session_data = (struct session_data *) cb_user_context;
  struct xio_iovec_ex	*isglist = vmsg_sglist(&rsp->in);
	struct xio_msg *req = rsp;

	// Process the request
  fprintf(stderr,"Received response to : %p, message #%lldd\n",
	  (char *) rsp -> request,
	  (long long) rsp -> request -> sn
	  );

  fprintf(stderr,"Response header: (%p) %s\n",
	  (char *) rsp -> in.header.iov_base,
	  (char *) rsp -> in.header.iov_base
	  );

  fprintf(stderr,"response received is : (%p) %s\n",
	  (char *) isglist[0].iov_base,
	  (char *) isglist[0].iov_base
	  );
	// End process request

  xio_release_response(rsp);

	if (n_sent < n_messages) {
		req->in.header.iov_base = NULL;
		req->in.header.iov_len = 0;
		vmsg_sglist_set_nents(&req->in, 0);
		send_msg(get_header, key_str2, value_str2, req, session_data);
		n_sent++;
	}


  return 0;
}

/*---------------------------------------------------------------------------*/
/* main									     */
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
  struct xio_session		*session;
  char				url[256];
  struct session_data		session_data;
  struct xio_session_params	params;
  struct xio_connection_params	cparams;
  struct xio_msg			*req;

  if (argc < 3) {
    printf("Usage: %s <host> <port> <transport:optional>" \
	   "<finite run:optional>\n", argv[0]);
    exit(1);
  }
	struct xio_session_ops ses_ops;

	ses_ops.on_session_event = on_session_event;
	ses_ops.on_session_established = NULL;
	ses_ops.on_msg = on_response;
	ses_ops.on_msg_error = NULL;

  /*
   * Initialie XIO data structures
   */
  memset(&session_data, 0, sizeof(session_data));
  memset(&params, 0, sizeof(params));
  memset(&cparams, 0, sizeof(cparams));

  /* initialize library */
  xio_init();

  /* Logging ... */
  if ( 0 ) {
    int level = XIO_LOG_LEVEL_DEBUG;
    xio_set_opt(NULL, XIO_OPTLEVEL_ACCELIO,
		XIO_OPTNAME_LOG_LEVEL, &level, sizeof(level));
  }

  /* create thread context for the client */
  session_data.ctx = xio_context_create(NULL, 0, -1);

  /* create url to connect to */
  if (argc > 3)
    sprintf(url, "%s://%s:%s", argv[3], argv[1], argv[2]);
  else
    sprintf(url, "rdma://%s:%s", argv[1], argv[2]);

  params.type		= XIO_SESSION_CLIENT;
  params.ses_ops		= &ses_ops;
  params.user_context	= &session_data;
  params.uri		= url;

  session = xio_session_create(&params);

  cparams.session			= session;
  cparams.ctx			= session_data.ctx;
  cparams.conn_user_context	= &session_data;

  /* connect the session  */
  session_data.conn = xio_connect(&cparams);

  /* create "hello world" message */
  req = &session_data.request;

  fprintf(stderr,"Prepare request at %p\n", req);

	send_msg(put_header, key_str1, value_str1, req, &session_data);

	xio_context_run_loop(session_data.ctx, 5000);
  xio_disconnect(session_data.conn);


  /* Run event dispatch for 1000 milliseconds */

  fprintf(stdout, "exit signaled\n");

  printf("good bye\n");
  return 0;
}

