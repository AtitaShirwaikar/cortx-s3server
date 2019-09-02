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
 * Original author:  Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 09-July-2019
 */

package com.seagates3.acl;

import java.io.IOException;
import java.util.HashSet;

import javax.xml.parsers.ParserConfigurationException;

import org.xml.sax.SAXException;

import com.novell.ldap.LDAPConnection;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.dao.ldap.LDAPUtils;
import com.seagates3.exception.GrantListFullException;
import com.seagates3.model.Account;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AuthenticationResponseGenerator;
import com.seagates3.util.XMLValidatorUtil;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public
class ACLValidation {

  AuthenticationResponseGenerator responseGenerator;
  ServerResponse serverResponse;
 private
  final static Logger LOGGER =
      LoggerFactory.getLogger(ACLValidation.class.getName());
 private
  static String xsdPath;
  boolean validation, aclFlag = true;
 private
  String ACLXML;
  XMLValidatorUtil xmlValidator;
  AccessControlPolicy acp;
  HashSet<String> permissions = new HashSet<String>();

 public
  ACLValidation(String xmlString) throws ParserConfigurationException,
      SAXException, IOException, GrantListFullException {
    responseGenerator = new AuthenticationResponseGenerator();

    this.ACLXML = xmlString;
    xsdPath = AuthServerConfig.authResourceDir + AuthServerConfig.XSD_PATH;
    xmlValidator = new XMLValidatorUtil(xsdPath);

    validation = validateAclxsd();

    if (validation) {

      acp = new AccessControlPolicy(xmlString);
    }
  }

 public
  ServerResponse validate() {

    if (!validation) {
      return responseGenerator.invalidACL();
    }
    return validateACL();
  }

  /**
   * validate acl against xsd.
   *
   */
 public
  boolean validateAclxsd() {

    boolean validation = xmlValidator.validateXMLSchema(ACLXML);

    return validation;
  }

  /**
   * Validate semanticity of ACL.
   *
   */

 private
  ServerResponse validateACL() {

    if (acp.accessControlList.getGrantList().size() >
        AuthServerConfig.MAX_GRANT_SIZE)
      return responseGenerator.grantListSizeViolation();

    int counter = 0;

    /**
     * validate owner id and owner displayName.
     */

    aclFlag =
        checkIdExists(acp.owner.getCanonicalId(), acp.owner.getDisplayName());

    if (aclFlag) {
      for (counter = 0; counter < acp.accessControlList.getGrantList().size();
           counter++) {
        /**
         * validate grantee id and grantee name
         */

        aclFlag = checkIdExists(acp.accessControlList.getGrantList()
                                    .get(counter)
                                    .grantee.getCanonicalId(),
                                acp.accessControlList.getGrantList()
                                    .get(counter)
                                    .grantee.getDisplayName());

        if (!aclFlag) {
          return responseGenerator.invalidID();
        }
      }

    } else {
      return responseGenerator.invalidACL();
    }

    return responseGenerator.ok();
  }

  /**
   * Check existance of canonical Id.
   * @param canonicalID
   * @param displayname
   * @return
   */

 public
  static boolean checkIdExists(String canonicalID, String displayname) {

    Account account = new Account();
    String[] attrs = {LDAPUtils.ORGANIZATIONAL_NAME, LDAPUtils.ACCOUNT_ID};
    String filter =
        String.format("(&(%s=%s)(%s=%s))", LDAPUtils.CANONICAL_ID, canonicalID,
                      LDAPUtils.OBJECT_CLASS, LDAPUtils.ACCOUNT_OBJECT_CLASS);

    LOGGER.debug("dn:" + filter + "\nattrs:" + LDAPUtils.ORGANIZATIONAL_NAME +
                 LDAPUtils.ACCOUNT_ID);
    LOGGER.debug("Searching canonical id: " + canonicalID);

    LDAPSearchResults ldapResults;
    try {
      ldapResults = LDAPUtils.search(LDAPUtils.BASE_DN,
                                     LDAPConnection.SCOPE_SUB, filter, attrs);
    }
    catch (LDAPException ex) {

      LOGGER.error("Failed to search account " + "of canonical Id " +
                   canonicalID);
      return false;
    }

    if (ldapResults.hasMore()) {
      try {
        LDAPEntry entry = ldapResults.next();
        account.setName(
            entry.getAttribute(LDAPUtils.ORGANIZATIONAL_NAME).getStringValue());
        account.setId(
            entry.getAttribute(LDAPUtils.ACCOUNT_ID).getStringValue());
      }
      catch (LDAPException ex) {
        LOGGER.error("Failed to find account details." + "of canonical id: " +
                     canonicalID);
        return false;
      }
    }

    if (!(account.exists())) {
      return false;
    }

    if (displayname != null) {
      if (!(account.getName().equals(displayname))) {

        return false;
      }
    }
    return true;
  }
}
