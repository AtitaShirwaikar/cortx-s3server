/*
 * COPYRIGHT 2015 SEAGATE LLC
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
 * Original creation date: 1-Oct-2015
 */

#include <json/json.h>
#include <string>
#include "base64.h"
#include "s3_bucket_metadata.h"
#include "s3_datetime.h"
#include "s3_uri_to_mero_oid.h"

S3BucketMetadata::S3BucketMetadata(std::shared_ptr<S3RequestObject> req) : request(req) {
  s3_log(S3_LOG_DEBUG, "Constructor");
  account_name = request->get_account_name();
  user_name = request->get_user_name();
  bucket_name = request->get_bucket_name();
  salted_index_name = get_account_user_index_name();

  state = S3BucketMetadataState::empty;
  s3_clovis_api = std::make_shared<ConcreteClovisAPI>();
  bucket_policy = "";

  collision_salt = "index_salt_";
  collision_attempt_count = 0;

  bucket_list_index_oid = {0ULL, 0ULL};
  object_list_index_oid = {0ULL, 0ULL};  // Object List index default id
  multipart_index_oid = {0ULL, 0ULL};  // Multipart index default id

  // Set the defaults
  S3DateTime current_time;
  current_time.init_current_time();
  system_defined_attribute["Date"] = current_time.get_isoformat_string();
  system_defined_attribute["LocationConstraint"] = "us-west-2";
  system_defined_attribute["Owner-User"] = "";
  system_defined_attribute["Owner-User-id"] = "";
  system_defined_attribute["Owner-Account"] = "";
  system_defined_attribute["Owner-Account-id"] = "";
}

std::string S3BucketMetadata::get_bucket_name() {
  return bucket_name;
}

std::string S3BucketMetadata::get_creation_time() {
  return system_defined_attribute["Date"];
}

std::string S3BucketMetadata::get_location_constraint() {
  return system_defined_attribute["LocationConstraint"];
}

std::string S3BucketMetadata::get_owner_id() {
  return system_defined_attribute["Owner-User"];
}

std::string S3BucketMetadata::get_owner_name() {
  return system_defined_attribute["Owner-User-id"];
}

struct m0_uint128 S3BucketMetadata::get_bucket_list_index_oid() {
  return bucket_list_index_oid;
}

struct m0_uint128 S3BucketMetadata::get_multipart_index_oid() {
  return multipart_index_oid;
}

struct m0_uint128 S3BucketMetadata::get_object_list_index_oid() {
  return object_list_index_oid;
}

void S3BucketMetadata::set_bucket_list_index_oid(struct m0_uint128 oid) {
  bucket_list_index_oid = oid;
}

void S3BucketMetadata::set_multipart_index_oid(struct m0_uint128 oid) {
  multipart_index_oid = oid;
}

void S3BucketMetadata::set_object_list_index_oid(struct m0_uint128 oid) {
  object_list_index_oid = oid;
}

void S3BucketMetadata::set_location_constraint(std::string location) {
  system_defined_attribute["LocationConstraint"] = location;
}

void S3BucketMetadata::setpolicy(std::string & policy_str) {
  bucket_policy = policy_str;
}

void S3BucketMetadata::deletepolicy() {
  bucket_policy = "";
}

void S3BucketMetadata::setacl(std::string &acl_str) {
  std::string input_acl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  input_acl += acl_str;
  input_acl = bucket_ACL.insert_display_name(input_acl);
  bucket_ACL.set_acl_xml_metadata(input_acl);
}

void S3BucketMetadata::add_system_attribute(std::string key, std::string val) {
  system_defined_attribute[key] = val;
}

void S3BucketMetadata::add_user_defined_attribute(std::string key, std::string val) {
  user_defined_attribute[key] = val;
}

// AWS recommends that all bucket names comply with DNS naming convention
// See Bucket naming restrictions in above link.
void S3BucketMetadata::validate_bucket_name() {
  // TODO
}

void S3BucketMetadata::validate() {
  // TODO
}

void S3BucketMetadata::load(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  state = S3BucketMetadataState::fetching;
  fetch_bucket_list_index_oid();

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::fetch_bucket_list_index_oid() {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  account_user_index_metadata = std::make_shared<S3AccountUserIdxMetadata>(request);
  account_user_index_metadata->load(
      std::bind(&S3BucketMetadata::fetch_bucket_list_index_oid_success, this),
      std::bind(&S3BucketMetadata::fetch_bucket_list_index_oid_failed, this));

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::fetch_bucket_list_index_oid_success() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  bucket_list_index_oid = account_user_index_metadata->get_bucket_list_index_oid();
  if (state == S3BucketMetadataState::saving) {
    save_bucket_info();
  } else if (state == S3BucketMetadataState::fetching) {
    load_bucket_info();
  } else if (state == S3BucketMetadataState::deleting) {
    remove_bucket_info();
  }
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::fetch_bucket_list_index_oid_failed() {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  if (account_user_index_metadata->get_state() == S3AccountUserIdxMetadataState::missing) {
    if (state == S3BucketMetadataState::saving ||
        state == S3BucketMetadataState::fetching) {
      create_bucket_list_index();
    } else {
      state = S3BucketMetadataState::missing;
      this->handler_on_failed();
    }
  } else {
    s3_log(S3_LOG_ERROR,
           "Failed to fetch Bucket List index oid from Account User index. Please "
           "retry after some time\n");
    state = S3BucketMetadataState::failed;
    this->handler_on_failed();
  }

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::load_bucket_info() {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  clovis_kv_reader =
      std::make_shared<S3ClovisKVSReader>(request, s3_clovis_api);
  clovis_kv_reader->get_keyval(bucket_list_index_oid, bucket_name,
      std::bind( &S3BucketMetadata::load_bucket_info_successful, this),
      std::bind( &S3BucketMetadata::load_bucket_info_failed, this));

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::load_bucket_info_successful() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  this->from_json(clovis_kv_reader->get_value());
  state = S3BucketMetadataState::present;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::load_bucket_info_failed() {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    s3_log(S3_LOG_DEBUG, "Bucket metadata is missing\n");
    state = S3BucketMetadataState::missing;
  } else {
    s3_log(S3_LOG_ERROR, "Loading of bucket metadata failed\n");
    state = S3BucketMetadataState::failed;
  }
  this->handler_on_failed();

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::save(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;
  state = S3BucketMetadataState::saving;
  if (bucket_list_index_oid.u_lo == 0ULL ||
      bucket_list_index_oid.u_hi == 0ULL) {
    // If there is no index oid then read it
    fetch_bucket_list_index_oid();
  } else {
    save_bucket_info();
  }
}

void S3BucketMetadata::create_bucket_list_index() {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  state = S3BucketMetadataState::missing;
  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request, s3_clovis_api);
  clovis_kv_writer->create_index(
      salted_index_name,
      std::bind(&S3BucketMetadata::create_bucket_list_index_successful,
                this),
      std::bind(&S3BucketMetadata::create_bucket_list_index_failed, this));

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::create_bucket_list_index_successful() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  bucket_list_index_oid = clovis_kv_writer->get_oid();
  save_bucket_list_index_oid();
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}


void S3BucketMetadata::create_bucket_list_index_failed() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  if (clovis_kv_writer->get_state() == S3ClovisKVSWriterOpState::exists) {
    // create_bucket_list_index is called when there is no id for that index,
    // Hence if clovis returned its present then its due to collision.
    handle_collision();
  } else {
    s3_log(S3_LOG_ERROR, "Index creation failed.\n");
    state = S3BucketMetadataState::failed;
    this->handler_on_failed();
  }
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::handle_collision() {
  if (clovis_kv_writer->get_state() == S3ClovisKVSWriterOpState::exists &&
      collision_attempt_count < MAX_COLLISION_TRY) {
    s3_log(S3_LOG_INFO, "Index ID collision happened for index %s\n",
           salted_index_name.c_str());
    // Handle Collision
    regenerate_new_oid();
    collision_attempt_count++;
    if (collision_attempt_count > 5) {
      s3_log(S3_LOG_INFO, "Index ID collision happened %d times for index %s\n",
             collision_attempt_count, salted_index_name.c_str());
    }
    create_bucket_list_index();
  } else {
    s3_log(S3_LOG_ERROR,
           "Failed to resolve index id collision %d times for index %s\n",
           collision_attempt_count, salted_index_name.c_str());
    state = S3BucketMetadataState::failed;
    this->handler_on_failed();
  }
}

void S3BucketMetadata::regenerate_new_oid() {
  salted_index_name = get_account_user_index_name() + collision_salt + std::to_string(collision_attempt_count);
}

void S3BucketMetadata::save_bucket_list_index_oid() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  account_user_index_metadata = std::make_shared<S3AccountUserIdxMetadata>(request);
  account_user_index_metadata->set_bucket_list_index_oid(bucket_list_index_oid);

  account_user_index_metadata->save(
        std::bind(&S3BucketMetadata::save_bucket_list_index_oid_successful, this),
        std::bind(&S3BucketMetadata::save_bucket_list_index_oid_failed, this));

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::save_bucket_list_index_oid_successful() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  if (state == S3BucketMetadataState::saving) {
    save_bucket_info();
  } else {
    this->handler_on_success();
  }
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::save_bucket_list_index_oid_failed() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  s3_log(S3_LOG_ERROR, "Saving of Bucket list index oid metadata failed\n");
  state = S3BucketMetadataState::failed;
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::save_bucket_info() {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  // Set up system attributes
  system_defined_attribute["Owner-User"] = user_name;
  system_defined_attribute["Owner-User-id"] = request->get_user_id();
  system_defined_attribute["Owner-Account"] = account_name;
  system_defined_attribute["Owner-Account-id"] = request->get_account_id();

  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request, s3_clovis_api);
  clovis_kv_writer->put_keyval(bucket_list_index_oid,
      bucket_name, this->to_json(),
      std::bind( &S3BucketMetadata::save_bucket_info_successful, this),
      std::bind( &S3BucketMetadata::save_bucket_info_failed, this));

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::save_bucket_info_successful() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  state = S3BucketMetadataState::saved;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::save_bucket_info_failed() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  s3_log(S3_LOG_ERROR, "Saving of Bucket metadata failed\n");
  state = S3BucketMetadataState::failed;
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::remove(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  if (state == S3BucketMetadataState::present) {
    remove_bucket_info();
  } else {
    state = S3BucketMetadataState::deleting;
    fetch_bucket_list_index_oid();
  }
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::remove_bucket_info() {
  s3_log(S3_LOG_DEBUG, "Entering\n");

  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request, s3_clovis_api);
  clovis_kv_writer->delete_keyval(bucket_list_index_oid, bucket_name,
      std::bind( &S3BucketMetadata::remove_bucket_info_successful, this),
      std::bind( &S3BucketMetadata::remove_bucket_info_failed, this));

  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::remove_bucket_info_successful() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  state = S3BucketMetadataState::deleted;
  this->handler_on_success();
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

void S3BucketMetadata::remove_bucket_info_failed() {
  s3_log(S3_LOG_DEBUG, "Entering\n");
  s3_log(S3_LOG_ERROR, "Removal of Bucket metadata failed\n");
  state = S3BucketMetadataState::failed;
  this->handler_on_failed();
  s3_log(S3_LOG_DEBUG, "Exiting\n");
}

std::string S3BucketMetadata::create_default_acl() {
  std::string acl_str;
  acl_str =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<AccessControlPolicy xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n"
         "  <Owner>\n"
         "    <ID>" + request->get_account_id() + "</ID>\n"
         "  </Owner>\n"
         "  <AccessControlList>\n"
         "    <Grant>\n"
         "      <Grantee xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:type=\"CanonicalUser\">\n"
         "        <ID>" + request->get_account_id() + "</ID>\n"
         "        <DisplayName>" + request->get_account_name() + "</DisplayName>\n"
         "      </Grantee>\n"
         "      <Permission>FULL_CONTROL</Permission>\n"
         "    </Grant>\n"
         "  </AccessControlList>\n"
         "</AccessControlPolicy>\n";
  return acl_str;
}

// Streaming to json
std::string S3BucketMetadata::to_json() {
  s3_log(S3_LOG_DEBUG, "Called\n");
  Json::Value root;
  root["Bucket-Name"] = bucket_name;

  for (auto sit: system_defined_attribute) {
    root["System-Defined"][sit.first] = sit.second;
  }
  for (auto uit: user_defined_attribute) {
    root["User-Defined"][uit.first] = uit.second;
  }
  std::string xml_acl = bucket_ACL.get_xml_str();
  if (xml_acl == "") {
    xml_acl = create_default_acl();
  }
  root["ACL"] = base64_encode((const unsigned char*)xml_acl.c_str(), xml_acl.size());
  root["Policy"] = bucket_policy;

  root["mero_object_list_index_oid_u_hi"] =
      base64_encode((unsigned char const*)&object_list_index_oid.u_hi,
                    sizeof(object_list_index_oid.u_hi));
  root["mero_object_list_index_oid_u_lo"] =
      base64_encode((unsigned char const*)&object_list_index_oid.u_lo,
                    sizeof(object_list_index_oid.u_lo));

  root["mero_multipart_index_oid_u_hi"] =
      base64_encode((unsigned char const*)&multipart_index_oid.u_hi,
                    sizeof(multipart_index_oid.u_hi));
  root["mero_multipart_index_oid_u_lo"] =
      base64_encode((unsigned char const*)&multipart_index_oid.u_lo,
                    sizeof(multipart_index_oid.u_lo));

  Json::FastWriter fastWriter;
  return fastWriter.write(root);
}

void S3BucketMetadata::from_json(std::string content) {
  s3_log(S3_LOG_DEBUG, "Called\n");
  Json::Value newroot;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(content.c_str(), newroot);
  if (!parsingSuccessful)
  {
    s3_log(S3_LOG_ERROR, "Json Parsing failed.\n");
    return;
  }

  bucket_name = newroot["Bucket-Name"].asString();

  Json::Value::Members members = newroot["System-Defined"].getMemberNames();
  for(auto it : members) {
    system_defined_attribute[it.c_str()] = newroot["System-Defined"][it].asString();
  }
  members = newroot["User-Defined"].getMemberNames();
  for(auto it : members) {
    user_defined_attribute[it.c_str()] = newroot["User-Defined"][it].asString();
  }
  user_name = system_defined_attribute["Owner-User"];
  account_name = system_defined_attribute["Owner-Account"];

  std::string dec_object_list_index_oid_u_hi_str =
      base64_decode(newroot["mero_object_list_index_oid_u_hi"].asString());
  std::string dec_object_list_index_oid_u_lo_str =
      base64_decode(newroot["mero_object_list_index_oid_u_lo"].asString());

  std::string dec_multipart_index_oid_u_hi_str =
      base64_decode(newroot["mero_multipart_index_oid_u_hi"].asString());
  std::string dec_multipart_index_oid_u_lo_str =
      base64_decode(newroot["mero_multipart_index_oid_u_lo"].asString());

  memcpy((void*)&object_list_index_oid.u_hi, dec_object_list_index_oid_u_hi_str.c_str(),
         dec_object_list_index_oid_u_hi_str.length());
  memcpy((void*)&object_list_index_oid.u_lo, dec_object_list_index_oid_u_lo_str.c_str(),
         dec_object_list_index_oid_u_lo_str.length());

  memcpy((void*)&multipart_index_oid.u_hi, dec_multipart_index_oid_u_hi_str.c_str(),
         dec_multipart_index_oid_u_hi_str.length());
  memcpy((void*)&multipart_index_oid.u_lo, dec_multipart_index_oid_u_lo_str.c_str(),
         dec_multipart_index_oid_u_lo_str.length());

  bucket_ACL.from_json((newroot["ACL"]).asString());
  bucket_policy = newroot["Policy"].asString();
}

std::string& S3BucketMetadata::get_encoded_bucket_acl() {
  //base64 acl encoded
  return bucket_ACL.get_acl_metadata();
}

std::string& S3BucketMetadata::get_acl_as_xml() {
  return bucket_ACL.get_xml_str();
}

std::string& S3BucketMetadata::get_policy_as_json() {
  return bucket_policy;
}
