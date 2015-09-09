
#include "module/instance.h"
#include "mero/init.h"

#include "clovis/clovis.h"

#include "s3_clovis_config.h"
#include "s3_clovis_writer.h"
#include "s3_uri_to_mero_oid.h"

extern struct m0_clovis_scope     clovis_uber_scope;

// This is run on main thread.
extern "C" void object_cw_done_on_main_thread(evutil_socket_t, short events, void *user_data) {
  S3ClovisWriterContext *context = (S3ClovisWriterContext*) user_data;
  if (context->get_op_status() == S3AsyncOpStatus::success) {
    context->on_success_handler()();  // Invoke the handler.
  } else {
    context->on_failed_handler()();  // Invoke the handler.
  }
  delete context;
}

// Clovis callbacks, run in clovis thread
extern "C" static void s3_clovis_op_stable(struct m0_clovis_op *op) {
  struct S3ClovisWriterContext *ctx = (struct S3ClovisWriterContext*)op->op_cbs->ocb_arg;
  ctx->set_op_status(S3AsyncOpStatus::success, "Success.");
  S3PostToMainLoop(context->get_request(), context)(object_cw_done_on_main_thread);
}

extern "C" static void s3_clovis_op_failed(struct m0_clovis_op *op) {
  S3ClovisWriterContext *ctx = (struct S3ClovisWriterContext*)op->op_cbs->ocb_arg;
  ctx->clovis_writer()->mark_failed();
  ctx->set_op_status(S3AsyncOpStatus::failed, "Operation Failed.");
  S3PostToMainLoop(context->get_request(), context)(object_cw_done_on_main_thread);
}

S3ClovisWriter::S3ClovisWriter(std::shared_ptr<S3RequestObject> req) : request(req), state(S3ClovisWriterOpState::start) {

}

void S3ClovisWriter::advance_state() {
  state++;
}

void S3ClovisWriter::mark_failed() {
  state = S3ClovisWriterOpState::failed;
}

void S3ClovisWriter::create_object(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  int                  rc;
  struct s3_clovis_op_context ctx;

  create_basic_op_ctx(&ctx, 1);
  S3ClovisWriterContext *context = new S3ClovisWriterContext(request, std::bind( &S3ClovisWriter::create_object_successful, this), std::bind( &S3ClovisWriter::create_object_failed, this));

  context->set_clovis_op_ctx(ctx);

  ctx.cbs->ocb_arg = (void *)context;
  ctx.cbs->ocb_executed = NULL;
  ctx.cbs->ocb_stable = s3_clovis_op_stable;
  ctx.cbs->ocb_failed = s3_clovis_op_failed;

  struct m0_uint128 id;
  S3UriToMeroOID(request->object_name(), &id);

  m0_clovis_obj_init(ctx.obj, &clovis_uber_scope, &id);

  m0_clovis_entity_create(&ctx.obj->ob_entity, &ctx->ops[0]);

  m0_clovis_op_setup(ctx.ops[0], ctx.cbs, 0);
  m0_clovis_op_launch(ctx.ops, ARRAY_SIZE(ctx.ops));
}

void S3ClovisWriter::create_object_successful() {
  state = S3ClovisWriterOpState::created;
  this->handler_on_success();
}

void S3ClovisWriter::create_object_failed() {
  state = S3ClovisWriterOpState::failed;
  this->handler_on_failed();
}

void S3ClovisWriter::write_content(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  int                  rc = 0, i = 0;
  uint64_t           last_index = 0;
  struct s3_clovis_op_context ctx;

  create_basic_op_ctx(&ctx, 1);

  size_t clovis_block_size = ClovisConfig::instance()->clovis_block_size();
  size_t clovis_block_count = (request->get_content_length() + (clovis_block_size - 1)) / clovis_block_size;

  create_basic_rw_op_ctx(&ctx, clovis_block_count, clovis_block_size);

  S3ClovisWriterContext *context = new S3ClovisWriterContext(request, std::bind( &S3ClovisWriter::write_content_successful, this), std::bind( &S3ClovisWriter::write_content_failed, this));


  context->set_clovis_op_ctx(ctx);

  ctx.cbs->ocb_arg = (void *)context;
  ctx.cbs->ocb_executed = NULL;
  ctx.cbs->ocb_stable = s3_clovis_op_stable;
  ctx.cbs->ocb_failed = s3_clovis_op_failed;

  struct m0_uint128 id;
  S3UriToMeroOID(request->object_name(), &id);

  // Remove this. Ideally we should do
  // for i = 0 to block_count
  //   S3RequestObject::consume(ctx.data->ov_buf[i], block_size)
  set_up_clovis_data_buffers(ctx);

  m0_clovis_obj_init(ctx.obj, &clovis_uber_scope, &id);

  /* Create the write request */
  m0_clovis_obj_op(ctx.obj, M0_CLOVIS_OC_WRITE,
       ctx.ext, ctx.data, ctx.attr, 0, &ctx->ops[0]);

  m0_clovis_op_setup(ctx.ops[0], ctx.cbs, 0);
  m0_clovis_op_launch(ctx.ops, ARRAY_SIZE(ctx.ops));
}

void S3ClovisWriter::write_content_successful() {
  state = S3ClovisWriterOpState::saved;
  this->handler_on_success();
}

void S3ClovisWriter::write_content_failed() {
  state = S3ClovisWriterOpState::failed;
  this->handler_on_failed();
}

void S3ClovisWriter::set_up_clovis_data_buffers(struct s3_clovis_op_context &ctx) {
  // Copy the data to clovis buffers.
  // xxx - move to S3RequestObject::consume(char* ptr, 4k);
  size_t clovis_block_size = ClovisConfig::instance()->clovis_block_size();
  size_t clovis_block_count = (request->get_content_length() + (clovis_block_size - 1)) / clovis_block_size;

  int data_to_consume = clovis_block_size;
  int pending_from_current_extent = 0;
  int idx_within_block = 0;
  int idx_within_extent = 0;
  int current_block_idx = 0;

  size_t num_of_extents = evbuffer_peek(request->buffer_in(), request->get_content_length() /*-1*/, NULL, NULL, 0);

  /* do the actual peek */
  struct evbuffer_iovec *vec_in = NULL;
  vec_in = (struct evbuffer_iovec *)malloc(num_of_extents * sizeof(struct evbuffer_iovec));

  /* do the actual peek at data */
  evbuffer_peek(request->buffer_in(), request->get_content_length(), NULL/*start of buffer*/, vec_in, num_of_extents);


  for (i = 0; i < num_of_extents; i++) {
    // printf("processing extent = %d of total %d\n", i, data_extents);
    pending_from_current_extent = vec_in[i].iov_len;
    /* KD xxx - we need to avoid this copy */
    while (1) /* consume all data in extent */
    {
      if (pending_from_current_extent == data_to_consume)
      {
        /* consume all */
        // printf("Writing @ %d bytes from extent = [%d] in block [%d] at index [%d]\n", pending_from_current_extent, i, current_block_idx, idx_within_block);
        // printf("memcpy(%p, %p, %d)\n", data.ov_buf[current_block_idx] + idx_within_block, vec_in[i].iov_base + idx_within_extent, pending_from_current_extent);
        memcpy(ctx.data->ov_buf[current_block_idx] + idx_within_block, vec_in[i].iov_base + idx_within_extent, pending_from_current_extent);
        idx_within_block = idx_within_extent = 0;
        data_to_consume = clovis_block_size;
        current_block_idx++;
        break; /* while(1) */
      } else if (pending_from_current_extent < data_to_consume)
      {
        // printf("Writing @ %d bytes from extent = [%d] in block [%d] at index [%d]\n", pending_from_current_extent, i, current_block_idx, idx_within_block);
        // printf("memcpy(%p, %p, %d)\n", data.ov_buf[current_block_idx] + idx_within_block, vec_in[i].iov_base + idx_within_extent, pending_from_current_extent);
        memcpy(ctx.data->ov_buf[current_block_idx] + idx_within_block, vec_in[i].iov_base + idx_within_extent, pending_from_current_extent);
        idx_within_block = idx_within_block + pending_from_current_extent;
        idx_within_extent = 0;
        data_to_consume = data_to_consume - pending_from_current_extent;
        break; /* while(1) */
      } else /* if (pending_from_current_extent > data_to_consume) */
      {
        // printf("Writing @ %d bytes from extent = [%d] in block [%d] at index [%d]\n", data_to_consume, i, current_block_idx, idx_within_block);
        // printf("memcpy(%p, %p, %d)\n", data.ov_buf[current_block_idx] + idx_within_block, vec_in[i].iov_base + idx_within_extent, data_to_consume);
        memcpy(ctx.data->ov_buf[current_block_idx] + idx_within_block, vec_in[i].iov_base + idx_within_extent, data_to_consume);
        idx_within_block = 0;
        idx_within_extent = idx_within_extent + data_to_consume;
        pending_from_current_extent = pending_from_current_extent - data_to_consume;
        data_to_consume = clovis_block_size;
        current_block_idx++;
      }
    }
  }
  free(vec_in);

  // Init clovis buffer attrs.
  for(i = 0; i < clovis_block_count; i++)
  {
    ctx.ext->iv_index[i] = last_index;
    ctx.ext->iv_vec.v_count[i] = /*vec_in[i].iov_len*/clovis_block_size;
    ctx.last_index += /*vec_in[i].iov_len*/clovis_block_size;

    /* we don't want any attributes */
    ctx.attr->ov_vec.v_count[i] = 0;
  }

}
