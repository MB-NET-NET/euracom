Summary: Ackermann Euracom charge advice logging tool
Name: euracom
Version: 3.0.1
Release: 1
Copyright: GPL v2
Group: Base
Source0: http://www.fgan.de/~bus/personal/euracom/euracom-1.3.0.tar.gz
URL: http://www.fgan.de/~bus/personal/euracom.html
#Distribution: Unknown
Requires: kernel >= 2.0.36
BuildRoot: /var/tmp/euracom

%description
Blah blah blah

%prep
echo "Preparing"

# unpack main sources
%setup
echo "Setup"

%build
echo "Build"

%install
echo "Install"

%clean
echo "Clean"

%files
/dev
/sbin
/usr/bin
/usr/lib/isdn
/usr/lib/libcapi20.a
/usr/include
/usr/X11R6/include/X11/bitmaps
%config /etc/isdn/isdn.conf
%doc /usr/man/man1/*
%dir /var/log/vbox
