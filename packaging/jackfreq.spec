Summary:        Jack CPU Frequency Daemon
Name:           jackfreq
Version:        0.2.0
Release:        0
License:        GPL
URL:            https://github.com/oleg68/jackfreqd
Group:          System Environment/Daemons
Source0:        %{name}-%{version}.tar.xz

BuildRequires:  jack-audio-connection-kit-devel
BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  systemd

%global debug_package %{nil}

%description
Switches CPU frequency based on the jack DSP load

%prep
%setup -q

%build
make

%install
make install DESTDIR=%{buildroot} SBIN_DIR=%{_sbindir} SYS_CONF_DIR=%{_sysconfdir} SYSTEMD_UNIT_DIR=%{_unitdir} INITD_DIR=%{_initddir} MAN_DIR=%{_mandir}

%files
%{_sbindir}/jackfreqd
%{_unitdir}/jackfreq.service
%{_mandir}/man1/jackfreqd.1.gz


%changelog
* Mon Feb 24 2020 Oleg Samarin <osamarin68@gmail.com> - 0.2.0
- added support of the intel_pstate governor driver
- added a systemd service
- added rpm build




