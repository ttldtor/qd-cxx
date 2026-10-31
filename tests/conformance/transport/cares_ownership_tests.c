// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
/* Source-level ownership unit; run through the runtime CTest profile.
 * It includes the qualified SDK implementation
 * to reach its static result seam. No function redirect, allocator replacement,
 * allocation failure, socket or packet is used. Every object is normally
 * allocated and queue-admission failure is supplied as an ordinary enum value.
 * Exit 0 means all assertions passed; nonzero means a failed check/bootstrap.
 */
#include <stdio.h>
#include "ares_private.h"
#include "record/ares_dns_private.h"
#include "ares_process.c"

typedef struct {
  ares_channel_t *channel;
  size_t callbacks;
  ares_status_t expected_status;
  ares_bool_t expected_record;
  int failures;
} unit_context_t;

static void unit_check(unit_context_t *ctx, ares_bool_t condition,
                       const char *description)
{
  if (!condition) {
    ctx->failures++;
    printf("FAILED %s\n", description);
  }
}

static void unit_callback(void *arg, ares_status_t status, size_t timeouts,
                          const ares_dns_record_t *record)
{
  unit_context_t *ctx = arg;
  const char *name = NULL;
  (void)timeouts;
  ctx->callbacks++;
  unit_check(ctx, status == ctx->expected_status, "callback status");
  unit_check(ctx, (record != NULL) == ctx->expected_record, "borrowed record presence");
  unit_check(ctx, ares_queue_active_queries(ctx->channel) == 0,
              "query detached before callback");
  if (record != NULL) {
    unit_check(ctx, ares_dns_record_query_get(record, 0, &name, NULL, NULL) ==
                   ARES_SUCCESS && ares_streq(name, "unit.invalid."),
                "record remains readable during callback");
  }
}

static ares_channel_t *unit_channel(void)
{
  struct ares_options options;
  struct in_addr server;
  ares_channel_t *channel = NULL;
  memset(&options, 0, sizeof(options));
  server.s_addr = htonl(0x7f000001);
  options.servers = &server;
  options.nservers = 1;
  options.flags = ARES_FLAG_NOSEARCH | ARES_FLAG_NOALIASES | ARES_FLAG_NO_DFLT_SVR;
  options.lookups = "b";
  options.tries = 1;
  options.qcache_max_ttl = 0;
  options.server_failover_opts.retry_chance = 0;
  if (ares_init_options(&channel, &options, ARES_OPT_SERVERS | ARES_OPT_FLAGS |
      ARES_OPT_LOOKUPS | ARES_OPT_TRIES | ARES_OPT_QUERY_CACHE |
      ARES_OPT_SERVER_FAILOVER) != ARES_SUCCESS) {
    return NULL;
  }
  return channel;
}

static ares_dns_record_t *unit_record(void)
{
  ares_dns_record_t *record = NULL;
  if (ares_dns_record_create_query(&record, "unit.invalid.", ARES_CLASS_IN,
      ARES_REC_TYPE_A, 1, ARES_FLAG_RD, 0) != ARES_SUCCESS) {
    return NULL;
  }
  return record;
}

/* Register a valid owned request in the same channel root/index/list structures
 * used by ares_send_nolock, without sending it or creating a native descriptor.
 * The connection-list fixture owns only its list; it is never a native socket.
 */
static ares_query_t *unit_query(unit_context_t *ctx, ares_conn_t *conn)
{
  ares_query_t *query = ares_malloc_zero(sizeof(*query));
  if (query == NULL) {
    return NULL;
  }
  query->channel = ctx->channel;
  query->qid = 1;
  query->serial = ++ctx->channel->query_serial;
  query->query = unit_record();
  query->callback = unit_callback;
  query->arg = ctx;
  query->node_all_queries = ares_llist_insert_last(ctx->channel->all_queries, query);
  if (query->query == NULL || query->node_all_queries == NULL ||
      !ares_htable_szvp_insert(ctx->channel->queries_by_qid, query->qid, query)) {
    ares_free_query(query);
    return NULL;
  }
  if (conn != NULL) {
    query->conn = conn;
    query->node_queries_to_conn = ares_llist_insert_last(conn->queries_to_conn, query);
    query->node_queries_by_timeout = ares_slist_insert(ctx->channel->queries_by_timeout, query);
    if (query->node_queries_to_conn == NULL || query->node_queries_by_timeout == NULL) {
      ares_free_query(query);
      return NULL;
    }
  }
  return query;
}

static int unit_failed_admission(ares_bool_t already_detached)
{
  unit_context_t ctx;
  ares_conn_t conn;
  ares_query_t *query;
  ares_dns_record_t *response;
  ares_status_t status;
  memset(&ctx, 0, sizeof(ctx));
  memset(&conn, 0, sizeof(conn));
  ctx.expected_status = ARES_EDESTRUCTION;
  ctx.channel = unit_channel();
  conn.queries_to_conn = ares_llist_create(NULL);
  if (ctx.channel == NULL || conn.queries_to_conn == NULL) {
    ares_destroy(ctx.channel);
    ares_llist_destroy(conn.queries_to_conn);
    return 1;
  }
  query = unit_query(&ctx, &conn);
  response = unit_record();
  if (query == NULL || response == NULL) {
    ares_dns_record_destroy(response);
    ares_destroy(ctx.channel);
    ares_llist_destroy(conn.queries_to_conn);
    return 1;
  }
  if (already_detached) {
    ares_query_remove_from_conn(query);
  }
  status = end_query_deferred_result(query, response, ARES_ENOMEM);
  response = NULL; /* The seam consumed the normally allocated record. */
  unit_check(&ctx, status == ARES_ENOMEM, "admission error returned");
  unit_check(&ctx, query->conn == NULL && query->node_queries_to_conn == NULL &&
                  query->node_queries_by_timeout == NULL,
              "connection and timeout linkage cleared");
  unit_check(&ctx, ares_llist_len(conn.queries_to_conn) == 0 &&
                  ares_slist_len(ctx.channel->queries_by_timeout) == 0,
              "connection and timeout lists empty");
  unit_check(&ctx, ares_queue_active_queries(ctx.channel) == 1 &&
                  ares_htable_szvp_get_direct(ctx.channel->queries_by_qid, 1) == query,
              "channel root and QID retain query");
  unit_check(&ctx, ctx.callbacks == 0, "no immediate callback on admission failure");
  ares_destroy(ctx.channel);
  unit_check(&ctx, ctx.callbacks == 1, "once-only destruction callback");
  ares_llist_destroy(conn.queries_to_conn);
  printf("failed-admission already-detached=%d callbacks=%zu failures=%d\n",
         already_detached, ctx.callbacks, ctx.failures);
  return ctx.failures;
}

static int unit_delivery(ares_bool_t deferred)
{
  unit_context_t ctx;
  ares_query_t *query;
  ares_dns_record_t *response;
  ares_array_t *queue = NULL;
  ares_timeval_t now;
  ares_status_t status;
  memset(&ctx, 0, sizeof(ctx));
  memset(&now, 0, sizeof(now));
  ctx.expected_status = ARES_SUCCESS;
  ctx.expected_record = ARES_TRUE;
  ctx.channel = unit_channel();
  if (ctx.channel == NULL) {
    return 1;
  }
  query = unit_query(&ctx, NULL);
  response = unit_record();
  if (query == NULL || response == NULL) {
    ares_dns_record_destroy(response);
    ares_destroy(ctx.channel);
    return 1;
  }
  status = end_query(ctx.channel, NULL, query, ARES_SUCCESS, response,
                     deferred ? &queue : NULL);
  response = NULL; /* end_query always consumes it, including synchronous use. */
  unit_check(&ctx, status == ARES_SUCCESS, "delivery/adoption succeeds");
  if (deferred) {
    unit_check(&ctx, ctx.callbacks == 0 && ares_array_len(queue) == 1,
                "successful queue owns deferred response");
    unit_check(&ctx, ares_flush_requeue(ctx.channel, &now, &queue) == ARES_SUCCESS,
                "successful queue drains");
  }
  unit_check(&ctx, ctx.callbacks == 1 && ares_queue_active_queries(ctx.channel) == 0,
              "once-only normal callback and no active query");
  ares_destroy(ctx.channel);
  unit_check(&ctx, ctx.callbacks == 1, "destroy adds no second callback");
  printf("delivery deferred=%d callbacks=%zu failures=%d\n",
         deferred, ctx.callbacks, ctx.failures);
  return ctx.failures;
}

int main(void)
{
  int failures = 0;
  if (ARES_QDCXX_PROFILE != 2 || !ares_streq(ares_version(NULL), "1.34.9+qdcxx2") ||
      ares_library_init(ARES_LIB_INIT_ALL) != ARES_SUCCESS) {
    return 2;
  }
  failures += unit_failed_admission(ARES_FALSE);
  failures += unit_failed_admission(ARES_TRUE);
  failures += unit_delivery(ARES_FALSE);
  failures += unit_delivery(ARES_TRUE);
  ares_library_cleanup();
  printf("cases=4 failures=%d\n", failures);
  return failures == 0 ? 0 : 1;
}
