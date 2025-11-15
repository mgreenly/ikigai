Name:           ikigai
Version:        __VERSION__
Release:        1%{?dist}
Summary:        Ikigai client and server

License:        MIT
URL:            https://github.com/mgreenly/ikigai
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  libtalloc-devel
BuildRequires:  libcurl-devel
BuildRequires:  libuuid-devel
BuildRequires:  libb64-devel
BuildRequires:  utf8proc-devel
BuildRequires:  check-devel

Requires:       libtalloc
Requires:       libcurl
Requires:       libuuid
Requires:       libb64
Requires:       utf8proc

%description
Ikigai provides both a client (ikigai) and server (ikigai-server)
for network communication and task management.

%prep
%setup -q

%build
make all BUILD=release

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT PREFIX=/usr

%files
%{_bindir}/ikigai
%{_bindir}/ikigai-server
%doc README.md
%license LICENSE

%changelog
* Fri Nov 07 2025 Michael Greenly <mgreenly@example.com> - __VERSION__-1
- Initial release
