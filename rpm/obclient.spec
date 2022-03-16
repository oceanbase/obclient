Name: %NAME
Version: %VERSION
Release: %(echo %RELEASE)%{?dist}
License: GPL
Group: applications/database
buildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Autoreq: no
Prefix: /u01/obclient
Requires: libobclient >= 2.0.0
Summary: Oracle 5.6 and some patches from Oceanbase

%description
The MySQL(TM) software delivers a very fast, multi-threaded, multi-user,
and robust SQL (Structured Query Language) database server. MySQL Server
is intended for mission-critical, heavy-load production systems as well
as for embedding into mass-deployed software.

%define MYSQL_USER root
%define MYSQL_GROUP root
%define __os_install_post %{nil}
%define base_dir /u01/mysql
%define file_dir /app/mariadb


%prep
cd $OLDPWD/../

#%setup -q

%build
cd $OLDPWD/../
./build.sh --prefix %{prefix} --version %{version}

%install
cd $OLDPWD/../
make DESTDIR=$RPM_BUILD_ROOT install
find $RPM_BUILD_ROOT -name '.git' -type d -print0|xargs -0 rm -rf
#for dir in `ls $RPM_BUILD_ROOT%{file_dir} | grep -v "bin\|share\|include\|lib"`
for dir in `ls $RPM_BUILD_ROOT%{file_dir} | grep -v "bin"`
do
        rm -rf $RPM_BUILD_ROOT%{file_dir}/${dir}
done
mkdir -p $RPM_BUILD_ROOT%{prefix}
mv $RPM_BUILD_ROOT%{file_dir}/* $RPM_BUILD_ROOT%{prefix}
rm -rf $RPM_BUILD_ROOT%{prefix}/bin/mariadb*
%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, %{MYSQL_USER}, %{MYSQL_GROUP})
%attr(755, %{MYSQL_USER}, %{MYSQL_GROUP}) %{prefix}/*
%dir %attr(755,  %{MYSQL_USER}, %{MYSQL_GROUP}) %{prefix}

%pre

%post
if [ -d %{base_dir} ]; then
    cp -rf %{prefix}/* %{base_dir}
else
    cp -rf %{prefix} %{base_dir}
fi
rm -rf /usr/bin/obclient
rm -rf /usr/bin/mysql_config
ln -s %{prefix}/bin/obclient /usr/bin/obclient
ln -s %{prefix}/bin/mysql_config /usr/bin/mysql_config


%preun
if [ "$1" = "0" ]; then
    rm /usr/bin/obclient
    rm /usr/bin/mysql_config
fi

%changelog

