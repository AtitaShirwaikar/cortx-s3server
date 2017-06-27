/*
 * COPYRIGHT 2017 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 05-Apr-2017
 */

#include <memory>

#include "mock_s3_factory.h"
#include "s3_clovis_layout.h"
#include "s3_error_codes.h"
#include "s3_get_object_action.h"
#include "s3_test_utils.h"

using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::StrEq;
using ::testing::AtLeast;

#define CREATE_BUCKET_METADATA                                            \
  do {                                                                    \
    EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), load(_, _)) \
        .Times(AtLeast(1));                                               \
    action_under_test->fetch_bucket_info();                               \
  } while (0)

#define CREATE_OBJECT_METADATA                                             \
  do {                                                                     \
    CREATE_BUCKET_METADATA;                                                \
    EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state()) \
        .WillOnce(Return(S3BucketMetadataState::present));                 \
    bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(  \
        object_list_indx_oid);                                             \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata), load(_, _))  \
        .Times(AtLeast(1));                                                \
    action_under_test->fetch_object_info();                                \
  } while (0)

static bool test_read_object_data_success(size_t num_of_blocks,
                                          std::function<void(void)> on_success,
                                          std::function<void(void)> on_failed) {
  on_success();
  return true;
}

class S3GetObjectActionTest : public testing::Test {
 protected:
  S3GetObjectActionTest() {
    S3Option::get_instance()->disable_auth();

    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();

    oid = {0x1ffff, 0x1ffff};
    object_list_indx_oid = {0x11ffff, 0x1ffff};
    zero_oid_idx = {0ULL, 0ULL};

    call_count_one = 0;

    async_buffer_factory =
        std::make_shared<MockS3AsyncBufferOptContainerFactory>(
            S3Option::get_instance()->get_libevent_pool_buffer_size());

    ptr_mock_request = std::make_shared<MockS3RequestObject>(
        req, evhtp_obj_ptr, async_buffer_factory);

    // Owned and deleted by shared_ptr in S3GetObjectAction
    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(ptr_mock_request);

    object_meta_factory = std::make_shared<MockS3ObjectMetadataFactory>(
        ptr_mock_request, object_list_indx_oid);

    layout_id =
        S3ClovisLayoutMap::get_instance()->get_best_layout_for_object_size();

    clovis_reader_factory = std::make_shared<MockS3ClovisReaderFactory>(
        ptr_mock_request, oid, layout_id);

    action_under_test.reset(
        new S3GetObjectAction(ptr_mock_request, bucket_meta_factory,
                              object_meta_factory, clovis_reader_factory));
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ObjectMetadataFactory> object_meta_factory;
  std::shared_ptr<MockS3ClovisReaderFactory> clovis_reader_factory;
  std::shared_ptr<MockS3AsyncBufferOptContainerFactory> async_buffer_factory;

  std::shared_ptr<S3GetObjectAction> action_under_test;

  struct m0_uint128 object_list_indx_oid;
  struct m0_uint128 oid;
  int layout_id;
  struct m0_uint128 zero_oid_idx;

  int call_count_one;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3GetObjectActionTest, ConstructorTest) {
  EXPECT_EQ(0, action_under_test->total_blocks_in_object);
  EXPECT_EQ(0, action_under_test->blocks_already_read);
  EXPECT_EQ(0, action_under_test->data_sent_to_client);
  EXPECT_FALSE(action_under_test->read_object_reply_started);
  EXPECT_NE(0, action_under_test->number_of_tasks());
}

TEST_F(S3GetObjectActionTest, FetchBucketInfo) {
  CREATE_BUCKET_METADATA;
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
}

TEST_F(S3GetObjectActionTest, FetchObjectInfoWhenBucketNotPresent) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_object_info();

  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata == NULL);
}

TEST_F(S3GetObjectActionTest, FetchObjectInfoWhenBucketFetchFailed) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_object_info();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata == NULL);
}

TEST_F(S3GetObjectActionTest,
       FetchObjectInfoWhenBucketPresentAndObjIndexAbsent) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      zero_oid_idx);
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), load(_, _))
      .Times(0);

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->fetch_object_info();

  EXPECT_STREQ("NoSuchKey", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata == NULL);
}

TEST_F(S3GetObjectActionTest, FetchObjectInfoWhenBucketAndObjIndexPresent) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::present));
  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), load(_, _))
      .Times(AtLeast(1));

  action_under_test->fetch_object_info();

  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata != NULL);
}

TEST_F(S3GetObjectActionTest, ReadObjectWhenMissingObjectReportNoSuckKey) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::missing));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(AtLeast(1));

  action_under_test->read_object();

  EXPECT_STREQ("NoSuchKey", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata != NULL);
}

TEST_F(S3GetObjectActionTest, ReadObjectWhenObjInfoFetchFailedReportError) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::failed));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(AtLeast(1));

  action_under_test->read_object();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata != NULL);
}

TEST_F(S3GetObjectActionTest, ReadObjectOfSizeZero) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length())
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length_str())
      .WillRepeatedly(Return("0"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_last_modified_gmt())
      .WillOnce(Return("Sunday, 29 January 2017 08:05:01 GMT"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_reply_start(Eq(S3HttpSuccess200)))
      .Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_reply_end()).Times(1);

  action_under_test->read_object();
}

TEST_F(S3GetObjectActionTest, ReadObjectOfSizeLessThanUnitSize) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_oid())
      .WillRepeatedly(Return(oid));

  // Object size less than unit size
  int layout_id = 1;
  size_t obj_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id) -
      1;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_layout_id())
      .WillRepeatedly(Return(layout_id));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length())
      .WillRepeatedly(Return(obj_size));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length_str())
      .WillRepeatedly(Return(std::to_string(obj_size)));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_last_modified_gmt())
      .WillOnce(Return("Sunday, 29 January 2017 08:05:01 GMT"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));

  EXPECT_CALL(*ptr_mock_request, send_reply_start(Eq(S3HttpSuccess200)))
      .Times(AtLeast(1));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_first_block(_))
      .WillOnce(Return(obj_size));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_next_block(_))
      .WillOnce(Return(0));
  EXPECT_CALL(*ptr_mock_request, send_reply_body(_, Eq(obj_size))).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_reply_end()).Times(1);

  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader),
              read_object_data(_, _, _))
      .Times(1)
      .WillOnce(Invoke(test_read_object_data_success));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3ClovisReaderOpState::success));

  action_under_test->read_object();
}

TEST_F(S3GetObjectActionTest, ReadObjectOfSizeEqualToUnitSize) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_oid())
      .WillRepeatedly(Return(oid));

  // Object size less than unit size
  int layout_id = 1;
  size_t obj_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id);

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_layout_id())
      .WillRepeatedly(Return(layout_id));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length())
      .WillRepeatedly(Return(obj_size));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length_str())
      .WillRepeatedly(Return(std::to_string(obj_size)));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_last_modified_gmt())
      .WillOnce(Return("Sunday, 29 January 2017 08:05:01 GMT"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));

  EXPECT_CALL(*ptr_mock_request, send_reply_start(Eq(S3HttpSuccess200)))
      .Times(AtLeast(1));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_first_block(_))
      .WillOnce(Return(obj_size));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_next_block(_))
      .WillOnce(Return(0));
  EXPECT_CALL(*ptr_mock_request, send_reply_body(_, Eq(obj_size))).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_reply_end()).Times(1);

  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader),
              read_object_data(_, _, _))
      .Times(1)
      .WillOnce(Invoke(test_read_object_data_success));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3ClovisReaderOpState::success));

  action_under_test->read_object();
}

TEST_F(S3GetObjectActionTest, ReadObjectOfSizeMoreThanUnitSize) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::present));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_oid())
      .WillRepeatedly(Return(oid));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_last_modified_gmt())
      .WillOnce(Return("Sunday, 29 January 2017 08:05:01 GMT"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));

  // Object size less than unit size
  int layout_id = 1;
  size_t obj_size =
      S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id) +
      1;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_layout_id())
      .WillRepeatedly(Return(layout_id));

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length())
      .WillRepeatedly(Return(obj_size));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length_str())
      .WillRepeatedly(Return(std::to_string(obj_size)));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));

  EXPECT_CALL(*ptr_mock_request, send_reply_start(Eq(S3HttpSuccess200)))
      .Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_reply_body(_, Eq(1)));
  EXPECT_CALL(*ptr_mock_request, send_reply_body(_, Eq(obj_size - 1)));
  EXPECT_CALL(*ptr_mock_request, send_reply_end()).Times(1);
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_first_block(_))
      .WillOnce(Return(obj_size - 1));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_next_block(_))
      .WillOnce(Return(1))
      .WillOnce(Return(0));

  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader),
              read_object_data(_, _, _))
      .Times(1)
      .WillOnce(Invoke(test_read_object_data_success));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3ClovisReaderOpState::success));

  action_under_test->read_object();
}

TEST_F(S3GetObjectActionTest, ReadObjectFailedJustEndResponse) {
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);

  action_under_test->read_object_reply_started = true;
  action_under_test->read_object_data_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3GetObjectActionTest, SendResponseWhenShuttingDownAndResponseStarted) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  action_under_test->read_object_reply_started = true;

  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(0);
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(0);
  EXPECT_CALL(*ptr_mock_request, send_reply_end()).Times(1);

  // send_response_to_s3_client is called in check_shutdown_and_rollback
  action_under_test->check_shutdown_and_rollback();

  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3GetObjectActionTest,
       SendResponseWhenShuttingDownAndResponseNotStarted) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);

  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request,
              set_out_header_value(Eq("Retry-After"), Eq("1")))
      .Times(1);
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(AtLeast(1));

  // send_response_to_s3_client is called in check_shutdown_and_rollback
  action_under_test->check_shutdown_and_rollback();

  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3GetObjectActionTest, SendInternalErrorResponse) {
  action_under_test->set_s3_error("InternalError");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetObjectActionTest, SendNoSuchBucketErrorResponse) {
  action_under_test->set_s3_error("NoSuchBucket");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetObjectActionTest, SendNoSuchKeyErrorResponse) {
  action_under_test->set_s3_error("NoSuchKey");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(404, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetObjectActionTest, SendSuccessResponseForZeroSizeObject) {
  action_under_test->clovis_reader = clovis_reader_factory->mock_clovis_reader;

  action_under_test->object_metadata =
      object_meta_factory->create_object_metadata_obj(ptr_mock_request,
                                                      object_list_indx_oid);

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length())
      .Times(AtLeast(1))
      .WillOnce(Return(0));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(0);
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(0);
  EXPECT_CALL(*ptr_mock_request, send_reply_end()).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3GetObjectActionTest, SendSuccessResponseForNonZeroSizeObject) {
  action_under_test->clovis_reader = clovis_reader_factory->mock_clovis_reader;

  action_under_test->object_metadata =
      object_meta_factory->create_object_metadata_obj(ptr_mock_request,
                                                      object_list_indx_oid);

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length())
      .Times(AtLeast(1))
      .WillOnce(Return(1024));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3ClovisReaderOpState::success));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(0);
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(0);
  EXPECT_CALL(*ptr_mock_request, send_reply_end()).Times(1);

  action_under_test->send_response_to_s3_client();
}

// Reply not started
TEST_F(S3GetObjectActionTest, SendErrorResponseForErrorReadingObject) {
  action_under_test->clovis_reader = clovis_reader_factory->mock_clovis_reader;

  action_under_test->object_metadata =
      object_meta_factory->create_object_metadata_obj(ptr_mock_request,
                                                      object_list_indx_oid);

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              get_content_length())
      .Times(AtLeast(1))
      .WillOnce(Return(1024));
  EXPECT_CALL(*(clovis_reader_factory->mock_clovis_reader), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3ClovisReaderOpState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);

  action_under_test->send_response_to_s3_client();
}
