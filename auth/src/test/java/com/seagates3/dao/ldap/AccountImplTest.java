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
 * Original author:  Arjun Hariharan <arjun.hariharan@seagate.com>
 * Original creation date: 28-Dec-2015
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Matchers;
import org.mockito.Mockito;
import org.mockito.internal.matchers.apachecommons.ReflectionEquals;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PrepareForTest(LDAPUtils.class)
public class AccountImplTest {

    private final String BASE_DN = "dc=s3,dc=seagate,dc=com";
    private final String FIND_FILTER = "(&(o=s3test)(objectclass=account))";
    private final String[] FIND_ATTRS = {"accountid"};

    private final String ACCOUNT_DN
            = "o=s3test,ou=accounts,dc=s3,dc=seagate,dc=com";
    private final String USERS_DN
            = "ou=users,o=s3test,ou=accounts,dc=s3,dc=seagate,dc=com";
    private final String ROLES_DN
            = "ou=roles,o=s3test,ou=accounts,dc=s3,dc=seagate,dc=com";

    private final AccountImpl accountImpl;
    private final LDAPSearchResults ldapResults;
    private final LDAPEntry entry;
    private final LDAPAttribute attr;

    private final LDAPEntry accountEntry;
    private final LDAPEntry userEntry;
    private final LDAPEntry roleEntry;

    @Rule
    public final ExpectedException exception = ExpectedException.none();

    public AccountImplTest() {
        accountImpl = new AccountImpl();
        ldapResults = Mockito.mock(LDAPSearchResults.class);
        entry = Mockito.mock(LDAPEntry.class);
        attr = Mockito.mock(LDAPAttribute.class);

        LDAPAttributeSet accountAttributeSet = new LDAPAttributeSet();
        accountAttributeSet.add(new LDAPAttribute("objectclass", "Account"));
        accountAttributeSet.add(new LDAPAttribute("accountid", "98765test"));
        accountAttributeSet.add(new LDAPAttribute("o", "s3test"));
        accountEntry = new LDAPEntry(ACCOUNT_DN, accountAttributeSet);

        LDAPAttributeSet userAttributeSet = new LDAPAttributeSet();
        userAttributeSet.add(new LDAPAttribute("objectclass",
                "organizationalunit"));
        userAttributeSet.add(new LDAPAttribute("ou", "users"));
        userEntry = new LDAPEntry(USERS_DN, userAttributeSet);

        LDAPAttributeSet roleAttributeSet = new LDAPAttributeSet();
        roleAttributeSet.add(new LDAPAttribute("objectclass",
                "organizationalunit"));
        roleAttributeSet.add(new LDAPAttribute("ou", "roles"));
        roleEntry = new LDAPEntry(ROLES_DN, roleAttributeSet);
    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(LDAPUtils.class);
        Mockito.when(ldapResults.next()).thenReturn(entry);
        Mockito.when(entry.getAttribute("accountid")).thenReturn(attr);
        Mockito.when(attr.getStringValue()).thenReturn("12345");
    }

    /**
     * Find account. Return true when account exists.
     *
     * @throws Exception
     */
    @Test
    public void FindAccount_AccountExists_ReturnAccountObject() throws Exception {
        Account expectedAccount = new Account();
        expectedAccount.setName("s3test");
        expectedAccount.setId("12345");

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                BASE_DN, 2, FIND_FILTER, FIND_ATTRS
        );

        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);

        Account account = accountImpl.find("s3test");
        Assert.assertThat(expectedAccount, new ReflectionEquals(account));
    }

    @Test
    public void Find_AccountDoesNotExists_ReturnAccountObject() throws Exception {
        Account expectedAccount = new Account();
        expectedAccount.setName("s3test");

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                BASE_DN, 2, FIND_FILTER, FIND_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        Account account = accountImpl.find("s3test");
        Assert.assertThat(expectedAccount, new ReflectionEquals(account));
    }

    @Test
    public void FindAccount_LdapSearchFailed_ThrowException() throws Exception {
        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "search",
                BASE_DN, 2, FIND_FILTER, FIND_ATTRS
        );
        exception.expect(DataAccessException.class);

        accountImpl.find("s3test");
    }

    /**
     * Account created successfully.
     *
     * @throws Exception
     */
    @Test
    public void Save_Success() throws Exception {
        Account account = new Account();
        account.setId("98765test");
        account.setName("s3test");

        accountImpl.save(account);

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(Matchers.refEq(accountEntry));

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(Matchers.refEq(userEntry));

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(Matchers.refEq(roleEntry));
    }

    /**
     * Exception while creating account.
     *
     * @throws Exception
     */
    @Test
    public void Save_AccountAddFail_ThrowException() throws Exception {
        Account account = new Account();
        account.setId("98765test");
        account.setName("s3test");

        PowerMockito.doThrow(new LDAPException()).when(
                LDAPUtils.class, "add", Mockito.refEq(accountEntry));
        exception.expect(DataAccessException.class);

        accountImpl.save(account);
    }

    /**
     * Exception while creating user OU.
     *
     * @throws Exception
     */
    @Test
    public void Save_UserOUAddFail_ThrowException() throws Exception {
        Account account = new Account();
        account.setId("98765test");
        account.setName("s3test");

        PowerMockito.doThrow(new LDAPException()).when(
                LDAPUtils.class, "add", Mockito.refEq(userEntry));
        exception.expect(DataAccessException.class);

        accountImpl.save(account);

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(Matchers.refEq(accountEntry));
    }

    /**
     * Exception while creating role OU.
     *
     * @throws Exception
     */
    @Test
    public void Save_RoleOUAddFail_ThrowException() throws Exception {
        Account account = new Account();
        account.setId("98765test");
        account.setName("s3test");

        PowerMockito.doThrow(new LDAPException()).when(
                LDAPUtils.class, "add", Mockito.refEq(roleEntry));
        exception.expect(DataAccessException.class);

        accountImpl.save(account);

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(Matchers.refEq(accountEntry));

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(Matchers.refEq(userEntry));
    }
}
