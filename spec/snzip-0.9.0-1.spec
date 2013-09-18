Summary:snzip snappy packager, written by kubo(http://github.com/kubo/snzip) packaged by Xianglei
Name: snzip
Version: 0.9.0
Release: 1
License: New BSD
Group: System Environment
Packager: Xianglei
BuildRequires: make gcc-c++ zlib
Source: snzip-0.9.0.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Url: http://www.phphiveadmin.net
BuildArchitectures: x86_64
Requires: snappy > 1.0

%description
Snzip is a compress/decompress cli tool

%prep
%setup -q

%build
export DESTDIR=%{buildroot}
%configure --prefix=/usr --libdir=/usr/lib --includedir=/usr/include --datarootdir=/usr/share --sbindir=/usr/sbin --libexecdir=/usr/libexec --mandir=/usr/man --with-snappy=/usr
%{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
make install DESTDIR=%{buildroot} INSTALLDIRS=vendor
 
%post
/sbin/ldconfig > /dev/null

%postun
/sbin/ldconfig > /dev/null

%clean
rm -rf %{buildroot}

%files
%defattr(-, root, root, 0775)
/usr/bin/snzip
/usr/share/doc/snzip/COPYING
/usr/share/doc/snzip/ChangeLog
/usr/share/doc/snzip/INSTALL
/usr/share/doc/snzip/NEWS
/usr/share/doc/snzip/README
