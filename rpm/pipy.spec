Name:		pipy-oss
Version: 	%{getenv:VERSION}
Release: 	%{getenv:REVISION}%{?dist}

Summary: 	A brand new proxy powered by Flomesh

License: 	NEU License
Source0: 	pipy.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{revision}-root-%(%{__id_u} -n)
BuildRequires: 	cmake3
#AutoReqProv: no
%define revision %{release}
%define prefix /usr/local

%global debug_package %{nil}

%description
Pipy is a tiny, high performance, highly stable, programmable proxy.

%prep
%setup -c -q -n %{name}-%{version} -T -a0

%build
rm -fr pipy/build
%{__mkdir} pipy/build
cd pipy/gui
npm install
npm run build
cd ../build
CXX=clang++ CC=clang cmake3 -DPIPY_GUI=OFF -DPIPY_TUTORIAL=ON -DCMAKE_BUILD_TYPE=Release ..
make -j$(getconf _NPROCESSORS_ONLN)

%preun
if [ $1 -eq 0 ] ; then
        # Package removal, not upgrade 
        systemctl --no-reload disable pipy.service > /dev/null 2>&1 || true
        systemctl stop pipy.service > /dev/null 2>&1 || true
fi


%pre
getent group pipy >/dev/null || groupadd -r pipy
getent passwd pipy >/dev/null || useradd -r -g pipy -G pipy -d /etc/pipy -s /sbin/nologin -c "pipy" pipy


%install
mkdir -p %{buildroot}%{prefix}/bin
mkdir -p %{buildroot}/etc/pipy
cp pipy/bin/pipy %{buildroot}%{prefix}/bin
cp -r pipy/etc/* %{buildroot}/etc/
chrpath --delete %{buildroot}%{prefix}/bin/pipy

%post
systemctl daemon-reload

%postun
if [ $1 -eq 0 ] ; then
        userdel pipy 2> /dev/null || true
fi

%clean
rm -rf %{buildroot}

%files
%attr(755, pipy, pipy) %{prefix}/bin/pipy
%attr(755, pipy, pipy) /etc/pipy/
%attr(644, root, root) /etc/systemd/system/pipy.service
%attr(644, root, root) /etc/sysconfig/pipy

%config /etc/pipy/pipy.js

%changelog
