#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "libxio.h"

/* private session data */
struct session_data {
  struct xio_context	*ctx;
  struct xio_connection	*conn;
  struct xio_msg	request;
};

/*---------------------------------------------------------------------------*/
/* on_session_event							     */
/*---------------------------------------------------------------------------*/
static int on_session_event(struct xio_session *session,
			    struct xio_session_event_data *event_data,
			    void *cb_user_context)
{
  struct session_data *session_data 
    = (struct session_data *) cb_user_context;

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

/*---------------------------------------------------------------------------*/
/* on_response								     */
/*---------------------------------------------------------------------------*/
static int on_response(struct xio_session *session,
		       struct xio_msg *rsp,
		       int last_in_rxq,
		       void *cb_user_context)
{
  struct session_data *session_data = (struct session_data *)
    cb_user_context;
  /*
   * These accessos handle the different xio_msg storage types
   */
  struct xio_iovec_ex	*isglist = vmsg_sglist(&rsp->in);
  //  int			inents = vmsg_sglist_nents(&rsp->in);

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
  xio_release_response(rsp);
  xio_disconnect(session_data->conn);
  return 0;
}

/*---------------------------------------------------------------------------*/
/* callbacks								     */
/*---------------------------------------------------------------------------*/
static struct xio_session_ops ses_ops = {
  .on_session_event		=  on_session_event,
  .on_session_established		=  NULL,
  .on_msg				=  on_response,
  .on_msg_error			=  NULL
};

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

  req->out.header.iov_base =
    strdup("hello world header request");
  req->out.header.iov_len =
    strlen((const char *)
	   req->out.header.iov_base) + 1;

  req->in.sgl_type  	   = XIO_SGL_TYPE_IOV;
  req->in.data_iov.max_nents = XIO_IOVLEN;
  req->out.data_iov.nents = 0;

  req->out.sgl_type	    = XIO_SGL_TYPE_IOV;
  req->out.data_iov.max_nents = XIO_IOVLEN;

  req->out.data_iov.sglist[0].iov_base =
    strdup("hello world data request");
  req->out.data_iov.sglist[0].iov_len =
    strlen((const char *)
	   req->out.data_iov.sglist[0].iov_base)  + 1;
  req->out.data_iov.nents = 1;


  xio_send_request(session_data.conn, req);
  /* Run event dispatch for 1000 milliseconds */
  xio_context_run_loop(session_data.ctx, 1000);

  fprintf(stdout, "exit signaled\n");

  req = &session_data.request;
  free(req->out.header.iov_base);
  free(req->out.data_iov.sglist[0].iov_base);

  printf("good bye\n");
  return 0;
}

