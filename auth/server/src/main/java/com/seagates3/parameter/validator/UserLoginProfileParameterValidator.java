/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author: Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 22-May-2019
 */

package com.seagates3.parameter.validator;

import java.util.Map;

public
class UserLoginProfileParameterValidator extends AbstractParameterValidator {

 public
  Boolean isValidCreateParams(Map<String, String> requestBody) {
    if (!(S3ParameterValidatorUtil.isValidPassword(
             requestBody.get("Password")))) {
      return false;
    }

    return S3ParameterValidatorUtil.isValidName(requestBody.get("UserName"));
  }
}
