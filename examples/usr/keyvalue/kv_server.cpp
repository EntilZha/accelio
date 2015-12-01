#include <stdio.h>
#include <string>
#include <inttypes.h>
#include <errno.h>
#include <map>

#include "libxio.h"

using std::map;
using std::string;


/* server private data */
struct server_data {
  struct xio_context	*ctx;
  struct xio_connection	*connection;
};

map<string, string> store;

/*---------------------------------------------------------------------------*/
/* on_session_event							     */
/*---------------------------------------------------------------------------*/
static int on_session_event(struct xio_session *session,
			    struct xio_session_event_data *event_data,
			    void *cb_user_context)
{
  struct server_data *server_data = (struct server_data *)cb_user_context;

  printf("session event: %s. session:%p, connection:%p, reason: %s\n",
	 xio_session_event_str(event_data->event),
	 session, event_data->conn,
	 xio_strerror(event_data->reason));

  switch (event_data->event) {
  case XIO_SESSION_NEW_CONNECTION_EVENT:
    server_data->connection = event_data->conn;
    break;
  case XIO_SESSION_CONNECTION_TEARDOWN_EVENT:
    fprintf(stderr,"Tear down the connection...\n");
    xio_connection_destroy(event_data->conn);
    server_data->connection = NULL;
    break;
  case XIO_SESSION_TEARDOWN_EVENT:
    fprintf(stderr,"Destroy the session...\n");
    xio_session_destroy(session);
    xio_context_stop_loop(server_data->ctx);  /* exit */
    break;
  default:
    break;
  };

  return 0;
}

/*---------------------------------------------------------------------------*/
/* on_new_session							     */
/*---------------------------------------------------------------------------*/
static int on_new_session(struct xio_session *session,
			  struct xio_new_session_req *req,
			  void *cb_user_context)
{
  struct server_data *server_data = (struct server_data *)cb_user_context;

  /* automatically accept the request */
  fprintf(stderr,"new session event. session:%p\n", session);

  if (!server_data->connection)
    xio_accept(session, NULL, 0, NULL, 0);
  else
    xio_reject(session, (enum xio_status)EISCONN, NULL, 0);


  fprintf(stderr,"Leaving on_new_session\n");
  return 0;
}

/*---------------------------------------------------------------------------*/
/* on_request callback							     */
/*---------------------------------------------------------------------------*/
static int on_request(struct xio_session *session,
		      struct xio_msg *req,
		      int last_in_rxq,
		      void *cb_user_context)
{
  struct xio_msg	   *rsp;

  struct xio_iovec_ex	*sglist = vmsg_sglist(&req->in);
  //  int			nents = vmsg_sglist_nents(&req->in);

  fprintf(stderr,"Received request %p\n", req);

	string msg((const char *) sglist[0].iov_base, sglist[0].iov_len);

  fprintf(stderr,"Received request header: (%p) %s\n",
	  (char *) req -> in.header.iov_base,
	  (char *) req -> in.header.iov_base);

  fprintf(stderr,"Received request data: (%p) %s\n",
	  (char *) sglist[0].iov_base,
	  (char *) sglist[0].iov_base
	  );

	fprintf(stderr, "Testing: %s\n", msg.c_str());

  req->in.header.iov_base	  = NULL;
  req->in.header.iov_len	  = 0;
  req->in.data_iov.nents = 1;

  /*
   * Prepare response using same request buffer..
   */

  rsp = req;

  rsp -> request = req;
  rsp->out.header.iov_base =
    strdup("hello world header response");
  rsp->out.header.iov_len =
    strlen((const char *)
	   rsp->out.header.iov_base) + 1;

  rsp->out.sgl_type	   = XIO_SGL_TYPE_IOV;
  rsp->out.data_iov.max_nents = XIO_IOVLEN;
  rsp->out.data_iov.sglist[0].iov_base =
    strdup("hello world data response!!");
  rsp->out.data_iov.sglist[0].iov_len =
    strlen((const char *)
	   rsp->out.data_iov.sglist[0].iov_base) + 1;
  rsp->out.data_iov.nents = 1;

  fprintf(stderr,"Send response..\n");

  xio_send_response(rsp);

  return 0;
}

/*---------------------------------------------------------------------------*/
/* asynchronous callbacks						     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* main									     */
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
  struct xio_server	*server;	/* server portal */
  struct server_data	server_data;
  char			url[256];

	xio_session_ops server_ops;
	server_ops.on_session_event = on_session_event;
	server_ops.on_new_session = on_new_session;
	server_ops.on_msg_send_complete = NULL;
	server_ops.on_msg = on_request;
	server_ops.on_msg_error = NULL;


  if (argc < 3) {
    printf("Usage: %s <host> <port> <transport:optional>"\
	   "<finite run:optional>\n", argv[0]);
    exit(1);
  }

  /* initialize library */
  xio_init();

  /* Logging ... */
  if ( 0 ) {
    int level = XIO_LOG_LEVEL_DEBUG;
    xio_set_opt(NULL, XIO_OPTLEVEL_ACCELIO,
		XIO_OPTNAME_LOG_LEVEL, &level, sizeof(level));
  }

  /* create "hello world" message */
  memset(&server_data, 0, sizeof(server_data));

  /* create thread context for the client */
  server_data.ctx	= xio_context_create(NULL, 0, -1);

  /* create url to connect to */
  if (argc > 3)
    sprintf(url, "%s://%s:%s", argv[3], argv[1], argv[2]);
  else
    sprintf(url, "rdma://%s:%s", argv[1], argv[2]);

  /* bind a listener server to a portal/url */
  server = xio_bind(server_data.ctx, &server_ops,
		    url, NULL, 0, &server_data);
  if (server) {
    printf("listen to %s\n", url);
    xio_context_run_loop(server_data.ctx, XIO_INFINITE);

    /* normal exit phase */
    fprintf(stdout, "exit signaled\n");

    /* free the server */
    xio_unbind(server);
  }

  /* free the context */
  xio_context_destroy(server_data.ctx);

  xio_shutdown();

  return 0;
}
