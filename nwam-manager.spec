#
# spec file for package nwam-manager
#
# Copyright 2008 Sun Microsystems, Inc.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
%define owner dkenny
#

%define OSR developed in the open, no OSR needed:0

%include l10n.inc
Name:         nwam-manager
License:      CDDL
Group:        System/GUI/GNOME
Version:      0.99.92.1
Vendor:       Sun Microsystems, Inc.
Summary:      Network Auto-Magic User Interface
#Source:       http://src.opensolaris.org/source/raw/jds/nwam-manager/branches/phase-0.5/tarballs/%{name}-%{version}.tar.gz
Source:       http://www.opensolaris.org/os/project/nwam/picea/%{name}-%{version}.tar.gz
%if %build_l10n
Source1:      l10n-configure.sh
%endif
URL:          http://www.opensolaris.org/os/project/nwam
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
Docdir:       %{_defaultdocdir}/%{name}
Autoreqprov:  on

%define	libgnomeui_version		2.1.5
%define	glib_version			2.6.0
%define gconf_version			2.6.0
%define libglade_version		2.6.0
%define gtk_version			2.6.0
%define libnotify_version		0.3.0

Requires:       libgnomeui >= %{libgnomeui_version}
Requires:       glib >= %{glib_version}
Requires:       gconf >= %{gconf_version}
Requires:       libglade >= %{libglade_version}
Requires:       gtk >= %{gtk_version}
Requires:       libnotify >= %{libnotify_version}
BuildRequires:	libgnomeui-devel >= %{libgnomeui_version}
BuildRequires:	glib >= %{glib_version}
BuildRequires:	gconf >= %{gconf_version}
BuildRequires:	libglade >= %{libglade_version}
BuildRequires:	gtk >= %{gtk_version}
BuildRequires:	libnotify >= %{libnotify_version}

%description
Nwam-manager is a GUI of Nwam http://www.opensolaris.org/os/project/nwam

%prep
%setup -q

%build
%ifos linux
if [ -x /usr/bin/getconf ]; then
  CPUS=`getconf _NPROCESSORS_ONLN`
fi
%else
  CPUS=`/usr/sbin/psrinfo | grep on-line | wc -l | tr -d ' '`
%endif
if test "x$CPUS" = "x" -o $CPUS = 0; then
  CPUS=1
fi

libtoolize --force
intltoolize --force --copy --automake

%if %build_l10n
sh %SOURCE1 --enable-copyright
%endif

aclocal $ACLOCAL_FLAGS
autoheader
automake -a -c -f
autoconf

export PKG_CONFIG_PATH="%{_libdir}/pkgconfig:%{_datadir}/pkgconfig"
CFLAGS="%optflags"	\
./configure \
	--prefix=%{_prefix} \
	--sysconfdir=%{_sysconfdir} \
        --libdir=%{_libdir}         \
        --bindir=%{_bindir}         \
	--libexecdir=%{_libexecdir} \
	--mandir=%{_mandir}         \
	--localstatedir=/var/lib

make -j $CPUS

%install
export GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL=1
make install DESTDIR=$RPM_BUILD_ROOT
unset GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr (-, root, root)
%{_bindir}/*
%{_sysconfdir}/gconf/schemas
%{_libdir}/*
%{_libexecdir}/*
%{_datadir}/applications
%{_datadir}/gnome/help/nwam/*
%{_datadir}/locale/*/LC_MESSAGES/*.mo
%{_datadir}/omf/nwam/*
%{_datadir}/pixmaps/*
%{_datadir}/nwam/*
%{_mandir}/man1/nwam*
%{_includedir}/nwam/*

%changelog
* Tue Feb 24 2009 - lin.ma@sun.com
- Fixed defect 6044.
* Tue Dec 23 2008 - darren.kenny@sun.com
- Bump to 0.5.13 and remove upsteam patches.
* Thu Nov 13 2008 - darren.kenny@sun.com
- Added patch nwam-manager-02-new-aps.diff to fix bug#6764186 where on some
  machines there are a lot of notifications of new wireless networks being
  found.
* Mon Nov 10 2008 - darren.kenny@sun.com
- Added patch to fix bug#4677 where WPA passwords are not being passed to NWAM
  when requested.
* Mon Oct 20 2008 - takao.fujiwara@sun.com
- Added l10n-configure.sh for l10n copyright.
* Mon Oct 20 2008 - darren.kenny@sun.com
- Bump to 0.5.11 to get translations (Bug#6761343) and also fix an issue where
  NWAM Manager is show messages too early in the login cycle (Bug#6761297).
* Fri Sep 19 2008 - darren.kenny@sun.com
- Bump to 0.5.10 to get a fix for bug#6750461 where things were crashing when
  a hot-pluggable wireless interface was plugged in.
  Also fixed a minor issue where the status icon was being incorrectly updated
  in this case too.
* Thu Sep 18 2008 - darren.kenny@sun.com
- Bump to 0.5.9 handle change in some libnwam symbol names.
* Wed Sep 17 2008 - darren.kenny@sun.com
- Bump to 0.5.8 to get the following fixes:
  - Fix bug#6744179, always update the notification if user put mouse
    pointer over message area.
  - Fix bug#6749315 to allow people to differentiate between an ad-hoc
    network and AP.
  - Fix bug#6748692 - Don't show unnecessary messages (for lower-prio
    interfaces some messages shouldn't be shown).
  - Several other minor fixes.
* Fri Sep 12 2008 - darren.kenny@sun.com
- Bump to 0.5.7 to fix issue where wireless connection to a network resulted
  in more than one attempt to connect. Also fixed issue where wifi_net objects
  were initialised to connected state incorrectly.
* Fri Sep 12 2008 - darren.kenny@sun.com
- Bump to 0.5.6 to fix a race-condition where sometimes nwam-manager ceased to
  work with wireless networks if nwamd was in a specific state.
* Thu Sep 11 2008 - darren.kenny@sun.com
- Bump to 0.5.5 to get fix for 6747318, and other minor fixes w.r.t. messages.
* Wed Sep 10 2008 - darren.kenny@sun.com
- Bump to 0.5.4 to fix a stupid mistake with non-population of menu.
* Wed Sep 10 2008 - darren.kenny@sun.com
- Bump to 0.5.3 to get several minor fixes from feedback on nwam-discuss.
* Tue Sep 9 2008 - darren.kenny@sun.com
- Change tarball location to somewhere that I know it will be visible
  immediately on upload.
* Mon Sep 8 2008 - darren.kenny@sun.com
- Bump to 0.5.2, to get fixes for bugs#6745722,6745720,6745719
* Fri Sep 5 2008 - darren.kenny@sun.com
- Bump to 0.5.1
* Thu Sep 4 2008 - darren.kenny@sun.com
- Fix some issues in spec 
* Wed Sep 3 2008 - darren.kenny@sun.com
- Initial delivery.
