#
# spec file for package nwam-manager
#
# Copyright (c) 2007 Sun Microsystems, Inc.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# Owner: Darren, Lin
#
%include l10n.inc
Name:         nwam-manager
License:      CDDL
Group:        System/GUI/GNOME
Version:      0.1
Release:      0.1
Distribution: Java Desktop System
Vendor:       Sun Microsystems, Inc.
Summary:      Network Auto-Magic
Source:       http://opensolaris.org/%{name}-%{version}.tar.gz
#Source1:      %{name}-po-sun-%{po_sun_version}.tar.bz2
URL:          http://www.opensolaris.org
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
%if %build_l10n
%endif

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
aclocal $ACLOCAL_FLAGS
autoheader
automake -a -c -f
autoconf

export PKG_CONFIG_PATH="%{_libdir}/pkgconfig:%{_datadir}/pkgconfig"
CFLAGS="$RPM_OPT_FLAGS"	\
./configure \
	--prefix=%{_prefix} \
	--sysconfdir=%{_sysconfdir} \
        --libdir=%{_libdir}         \
        --bindir=%{_bindir}         \
	--libexecdir=%{_libexecdir} \
	--mandir=%{_mandir}         \
	--localstatedir=/var/lib

#error when parallel make
#make -j $CPUS
make

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
* Wed Aug 01 2007 - lin.ma@sun.com
- Initial
