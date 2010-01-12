# -*- mode: rpm-spec -*-

Summary: A lightweight image viewer.
Name: pho
Version: 0.9
Release: 1
Copyright: GPL
Group: Applications/Multimedia
Source: http://www.shallowsky.com/software/pho/pho-%{version}.tar.gz
BuildRoot: /var/tmp/%{name}-root
Packager: Akkana Peck <dev@shallowsky.com>
Requires: gtk+ >= 1.2
Requires: gdk-pixbuf >= 0.14
Buildrequires: gtk+-devel >= 1.2
Buildrequires: gdk-pixbuf-devel >= 0.14

%description
Pho is a lightweight image browser.
For more information, please see http://www.shallowsky.com/software/pho/

%prep
%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/man/man1
install -s -m 755 pho $RPM_BUILD_ROOT/usr/bin/pho
install -m 644 pho.1 $RPM_BUILD_ROOT/usr/man/man1/pho.1
ls -lR $RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/bin/pho
/usr/man/man1/pho.1.gz

%changelog
* Sat Oct 15 2002 Akkana Peck <dev@shallowsky.com>
- First RPM spec.

