Name: cosu-trainer
Version: 0.3.0
Release: 1
Summary: Change various parameters of an osu! map easily
BuildArch: x86_64 %{ix86}
License: GPL3
URL: https://github.com/hwsmm/cosutrainer

Source0: cosu-trainer_%version.tar.xz

BuildRequires: meson gcc bash
BuildRequires: pkgconfig(libmpg123) pkgconfig(sndfile) pkgconfig(soundtouch)

%if 0%{?suse_version}
BuildRequires: pkgconfig(lame)
%else
BuildRequires: lame-devel
%endif

%description
cosu-trainer is a replacement for FunOrange's osu-trainer, but for Linux. Unlike the original osu-trainer, cosu-trainer doesn't have GUI.
You can use all of its features with commands, so you can even put the command in your DE/WM key shortcuts for easy conversion on the fly.

%prep
%setup -q -n %{name}

%build
bash ./build.sh

%install
install -D -m755 "./bin/cosu-trainer" "%buildroot/%_bindir/cosu-trainer"
install -D -m755 "./bin/osumem" "%buildroot/%_bindir/osumem"
install -D -m755 "./bin/cleanup" "%buildroot/%_bindir/cosu-cleanup"

%files
%defattr(-,root,root,-)
%dir %_bindir
%_bindir/cosu-trainer
%_bindir/osumem
%_bindir/cosu-cleanup
