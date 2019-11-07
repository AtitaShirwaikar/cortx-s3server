# mero version
%define h_mero_version %(rpm -q --queryformat '%{VERSION}-%{RELEASE}' mero)

# build number
%define build_num  %( test -n "$build_number" && echo "$build_number" || echo 1 )

%global py_ver 3.6

# pybasever without the dot:
%global py_short_ver 36

# XXX For strange reason setup.py uses /usr/lib
# but %{_libdir} resolves to /usr/lib64 with python3.6
#%global py36_sitelib %{_libdir}/python%{py_ver}
%global py36_sitelib /usr/lib/python%{py_ver}/site-packages

Name:       s3server
Version:    %{_s3_version}
Release:    %{build_num}_%{_s3_git_ver}_%{?dist:el7}
Summary:    s3server for Mero

Group:      Development/Tools
License:    Seagate
URL:        http://gerrit.mero.colo.seagate.com:8080/s3server
Source:     %{name}-%{version}-%{_s3_git_ver}.tar.gz

BuildRequires: automake
BuildRequires: bazel
BuildRequires: cmake >= 2.8.12
BuildRequires: libtool
BuildRequires: mero mero-devel
BuildRequires: openssl openssl-devel
BuildRequires: java-1.8.0-openjdk
BuildRequires: java-1.8.0-openjdk-devel
BuildRequires: maven
BuildRequires: libxml2 libxml2-devel
BuildRequires: libyaml libyaml-devel
BuildRequires: yaml-cpp yaml-cpp-devel
BuildRequires: gflags gflags-devel
BuildRequires: glog glog-devel
BuildRequires: gtest gtest-devel
BuildRequires: gmock gmock-devel
BuildRequires: git
BuildRequires: log4cxx_eos log4cxx_eos-devel
BuildRequires: hiredis hiredis-devel
BuildRequires: python3-rpm-macros
BuildRequires: python%{py_short_ver}
BuildRequires: python%{py_short_ver}-devel
BuildRequires: python%{py_short_ver}-setuptools

Requires: mero = %{h_mero_version}
Requires: libxml2
Requires: libyaml
#Supported openssl versions -- CentOS 7 its 1.0.2k, RHEL8 its 1.1.1
Requires: openssl
Requires: yaml-cpp
Requires: gflags
Requires: glog
Requires: pkgconfig
Requires: log4cxx_eos log4cxx_eos-devel
Requires: python%{py_short_ver}
Requires: python%{py_short_ver}-yaml
# Java used by Auth server
Requires: java-1.8.0-openjdk-headless
Requires: PyYAML
Requires: hiredis

%description
S3 server provides S3 REST API interface support for Mero object storage.

%prep
%setup -n %{name}-%{version}-%{_s3_git_ver}

%build
./rebuildall.sh --no-check-code --no-install
mkdir -p %{_builddir}/%{name}-%{version}-%{_s3_git_ver}/s3backgrounddelete/build/lib/s3backgrounddelete
# Build the background delete python module.
cd s3backgrounddelete/s3backgrounddelete
python%{py_ver} -m compileall -b *.py
cp  *.pyc %{_builddir}/%{name}-%{version}-%{_s3_git_ver}/s3backgrounddelete/build/lib/s3backgrounddelete
echo "build complete"

%install
rm -rf %{buildroot}
./installhelper.sh %{buildroot}
# Install the background delete python module.
cd %{_builddir}/%{name}-%{version}-%{_s3_git_ver}/s3backgrounddelete
python%{py_ver} setup.py install --single-version-externally-managed -O1 --root=$RPM_BUILD_ROOT

%clean
bazel clean
cd auth
./mvnbuild.sh clean
cd ..
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%config(noreplace) /opt/seagate/auth/resources/authserver.properties
%config(noreplace) /opt/seagate/auth/resources/authserver-log4j2.xml
%config(noreplace) /opt/seagate/auth/resources/authencryptcli-log4j2.xml
%config(noreplace) /opt/seagate/auth/resources/keystore.properties
%config(noreplace) /opt/seagate/auth/resources/static/saml-metadata.xml
%config(noreplace) /opt/seagate/s3/conf/s3config.yaml
%config(noreplace) /opt/seagate/s3/conf/s3server_audit_log.properties
%config(noreplace) /opt/seagate/s3/conf/s3_obj_layout_mapping.yaml
%config(noreplace) /opt/seagate/s3/conf/s3stats-whitelist.yaml
%config(noreplace) /opt/seagate/auth/resources/defaultAclTemplate.xml
%config(noreplace) /opt/seagate/auth/resources/AmazonS3.xsd

%attr(4600, root, root) /opt/seagate/auth/resources/authserver.properties
%attr(4600, root, root) /opt/seagate/auth/resources/authserver-log4j2.xml
%attr(4600, root, root) /opt/seagate/auth/resources/authencryptcli-log4j2.xml
%attr(4600, root, root) /opt/seagate/auth/resources/keystore.properties
%attr(4600, root, root) /opt/seagate/auth/resources/defaultAclTemplate.xml
%attr(4600, root, root) /opt/seagate/auth/resources/AmazonS3.xsd

%dir /opt/seagate/
%dir /opt/seagate/auth
%dir /opt/seagate/auth/resources
%dir /opt/seagate/auth/resources/static
%dir /opt/seagate/s3
%dir /opt/seagate/s3/addb-plugin
%dir /opt/seagate/s3/bin
%dir /opt/seagate/s3/conf
%dir /opt/seagate/s3/libevent
%dir /opt/seagate/s3/libevent/pkgconfig
%dir /opt/seagate/s3/nodejs
%dir /opt/seagate/s3/resources
%dir /var/log/seagate/
%dir /var/log/seagate/auth
%dir /var/log/seagate/s3
/etc/cron.hourly/s3logfilerollover.sh
/lib/systemd/system/s3authserver.service
/lib/systemd/system/s3server@.service
/lib/systemd/system/s3producer.service
/lib/systemd/system/s3consumer.service
/opt/seagate/auth/AuthServer-1.0-0.jar
/opt/seagate/auth/AuthPassEncryptCLI-1.0-0.jar
/opt/seagate/auth/startauth.sh
/opt/seagate/auth/scripts/enc_ldap_passwd_in_cfg.sh
/opt/seagate/s3/addb-plugin/libs3addbplugin.so
/opt/seagate/s3/bin/cloviskvscli
/opt/seagate/s3/bin/s3server
/opt/seagate/s3/libevent/libevent-2.1.so.6
/opt/seagate/s3/libevent/libevent-2.1.so.6.0.4
/opt/seagate/s3/libevent/libevent.a
/opt/seagate/s3/libevent/libevent.la
/opt/seagate/s3/libevent/libevent.so
/opt/seagate/s3/libevent/libevent_core-2.1.so.6
/opt/seagate/s3/libevent/libevent_core-2.1.so.6.0.4
/opt/seagate/s3/libevent/libevent_core.a
/opt/seagate/s3/libevent/libevent_core.la
/opt/seagate/s3/libevent/libevent_core.so
/opt/seagate/s3/libevent/libevent_extra-2.1.so.6
/opt/seagate/s3/libevent/libevent_extra-2.1.so.6.0.4
/opt/seagate/s3/libevent/libevent_extra.a
/opt/seagate/s3/libevent/libevent_extra.la
/opt/seagate/s3/libevent/libevent_extra.so
/opt/seagate/s3/libevent/libevent_openssl-2.1.so.6
/opt/seagate/s3/libevent/libevent_openssl-2.1.so.6.0.4
/opt/seagate/s3/libevent/libevent_openssl.a
/opt/seagate/s3/libevent/libevent_openssl.la
/opt/seagate/s3/libevent/libevent_openssl.so
/opt/seagate/s3/libevent/libevent_pthreads-2.1.so.6
/opt/seagate/s3/libevent/libevent_pthreads-2.1.so.6.0.4
/opt/seagate/s3/libevent/libevent_pthreads.a
/opt/seagate/s3/libevent/libevent_pthreads.la
/opt/seagate/s3/libevent/libevent_pthreads.so
/opt/seagate/s3/libevent/pkgconfig/libevent.pc
/opt/seagate/s3/libevent/pkgconfig/libevent_openssl.pc
/opt/seagate/s3/libevent/pkgconfig/libevent_pthreads.pc
/opt/seagate/s3/libevent/pkgconfig/libevent_core.pc
/opt/seagate/s3/libevent/pkgconfig/libevent_extra.pc
/opt/seagate/s3/resources/s3_error_messages.json
/opt/seagate/s3/s3startsystem.sh
/opt/seagate/s3/s3stopsystem.sh
/etc/rsyslog.d/rsyslog-tcp-audit.conf
%{_bindir}/s3backgroundconsumer
%{_bindir}/s3backgroundproducer
%{py36_sitelib}/s3backgrounddelete/config/*.yaml
%{py36_sitelib}/s3backgrounddelete/*.pyc
%{py36_sitelib}/s3backgrounddelete-%{version}-py?.?.egg-info
%exclude %{py36_sitelib}/s3backgrounddelete/__pycache__/*
%exclude %{py36_sitelib}/s3backgrounddelete/*.py
%exclude %{py36_sitelib}/s3backgrounddelete/s3backgroundconsumer
%exclude %{py36_sitelib}/s3backgrounddelete/s3backgroundproducer

%post
systemctl daemon-reload
systemctl enable s3authserver
systemctl restart rsyslog
openssl_version=`rpm -q --queryformat '%{VERSION}' openssl`
if [ "$openssl_version" != "1.0.2k" ] && [ "$openssl_version" != "1.1.1" ]; then
  echo "Warning: Unsupported (untested) openssl version [$openssl_version] is installed which may work."
  echo "Supported openssl versions are [1.0.2k, 1.1.1]"
fi
