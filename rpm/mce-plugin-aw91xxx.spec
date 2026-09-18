Name:       mce-plugin-aw91xxx
Summary:    AW91XXX LED plugin for Mode Control Entity
Version:    1.0.0
Release:    1
License:    LGPLv2
URL:        https://github.com/mer-hybris/mce-plugin-aw91xxx
Source0:    %{name}-%{version}.tar.bz2

Conflicts:        mce-plugin-libhybris
Requires:         mce >= 1.12.10
Requires:         systemd
Requires(pre):    systemd
Requires(post):   systemd
Requires(preun):  systemd
Requires(postun): systemd

BuildRequires:  pkgconfig(glib-2.0) >= 2.18.0

%description
This package contains mce led plugin for devices that use aw91xxx

%prep
%autosetup -n %{name}-%{version}

%build
%make_build

%install
%make_install _LIBDIR=%{_libdir}

%post
# upgrade or install
systemctl restart mce.service || :

%files
%license LICENSES/*.txt
%{_libdir}/mce/modules/hybris.so
