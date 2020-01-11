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
 * Original author:  Ajinkya Dhumal
 * Original creation date: 26-December-2019
 */

package com.seagates3.policy;

import java.util.List;

import com.seagates3.policy.BooleanCondition.BooleanComparisonType;

public
class ConditionFactory {

  /**
   * Return the Condition class depending upon the ConditionType
   * @param ConditionType
   * @param key
   * @param values
   * @return
   */
 public
  static PolicyCondition getCondition(String ConditionType, String key,
                                      List<String> values) {

    if (StringCondition.StringComparisonType.valueOf(ConditionType) != null) {
      return new StringCondition(
          StringCondition.StringComparisonType.valueOf(ConditionType), key,
          values);
    } else if (DateCondition.DateComparisonType.valueOf(ConditionType) !=
               null) {
      return new DateCondition(
          DateCondition.DateComparisonType.valueOf(ConditionType), key, values);
    } else if (NumericCondition.NumericComparisonType.valueOf(ConditionType) !=
               null) {
      return new NumericCondition(
          NumericCondition.NumericComparisonType.valueOf(ConditionType), key,
          values);
    } else if (BooleanCondition.BooleanComparisonType.valueOf(ConditionType) !=
               null) {
      return new BooleanCondition(BooleanComparisonType.valueOf(ConditionType),
                                  key, values);
    } else if ("Null".equals(ConditionType)) {
      return new NullCondition(key, values);
    } else {
      return null;
    }
  }
}
