#
# spec file for package SUNWnwam-manager
#
# includes module(s): nwam-manager
#
# Copyright 2009 Sun Microsystems, Inc.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
%define owner lin
#
%include Solaris.inc

%use nwam_manager = nwam-manager.spec

Name:                    SUNWnwam-manager
Summary:                 Network Auto-Magic User Interface
Version:                 %{default_pkg_version}
Source:                  %{name}-manpages-0.1.tar.gz
SUNW_BaseDir:            %{_prefix}
SUNW_Copyright:          %{name}.copyright
BuildRoot:               %{_tmppath}/%{name}-%{version}-build

%include default-depend.inc
BuildRequires: SUNWlibgnomecanvas-devel
BuildRequires: SUNWgnome-libs-devel
# Disable these until NWAM 0.5 integrated into OpenSolaris
# BuildRequires: SUNWnwamintu
# BuildRequires: SUNWnwamintr
BuildRequires: SUNWcslr
Requires: SUNWlibgnomecanvas
Requires: %{name}-root
Requires: SUNWgnome-libs
Requires: SUNWgnome-session
Requires: SUNWcslr
Requires: SUNWdesktop-cache

%package root
Summary:                 %{summary} - / filesystem
SUNW_BaseDir:            /
%include default-depend.inc

%if %build_l10n
%package l10n
Summary:                 %{summary} - l10n files
SUNW_BaseDir:            %{_basedir}
%include default-depend.inc
Requires:                %{name}
%endif

%prep
rm -rf %name-%version
mkdir %name-%version
%nwam_manager.prep -d %name-%version
cd %{_builddir}/%name-%version
gzcat %SOURCE0 | tar xf -

%build
%nwam_manager.build -d %name-%version

%install
rm -rf $RPM_BUILD_ROOT
%nwam_manager.install -d %name-%version
cd %{_builddir}/%name-%version/sun-manpages
make install DESTDIR=$RPM_BUILD_ROOT

# RBAC related
mkdir $RPM_BUILD_ROOT/etc/security

# exec_attr(4)
cat >> $RPM_BUILD_ROOT/etc/security/exec_attr <<EOF
Network Autoconf Admin:solaris:cmd:::/usr/bin/nwam-manager-properties:
EOF

%if %build_l10n
%else
# REMOVE l10n FILES
rm -rf $RPM_BUILD_ROOT%{_datadir}/locale
rm -rf $RPM_BUILD_ROOT%{_datadir}/gnome/help/*/[a-z]*
rm -f $RPM_BUILD_ROOT%{_datadir}/omf/*/*-[a-z][a-z].omf
rm -f $RPM_BUILD_ROOT%{_datadir}/omf/*/*-??_??.omf
%endif

%{?pkgbuild_postprocess: %pkgbuild_postprocess -v -c "%{version}:%{jds_version}:%{name}:$RPM_ARCH:%(date +%%Y-%%m-%%d):unsupported" $RPM_BUILD_ROOT}

%clean
rm -rf $RPM_BUILD_ROOT

%post
%restart_fmri icon-cache gconf-cache

%files
%defattr (-, root, bin)
%dir %attr (0755, root, bin) %{_bindir}
%{_bindir}/*
%dir %attr (0755, root, bin) %{_libexecdir}
%{_libexecdir}/*
%dir %attr (0755, root, sys) %{_datadir}
%dir %attr (0755, root, other) %{_datadir}/applications
%{_datadir}/applications/*
%dir %attr (0755, root, bin) %{_datadir}/nwam-manager
%dir %attr (0755, root, bin) %{_datadir}/nwam-manager/icons
%attr (-, root, bin) %{_datadir}/nwam-manager/*.*
%attr (-, root, bin) %{_datadir}/nwam-manager/icons/*
%dir %attr (0755, root, other) %{_datadir}/icons
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/16x16
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/16x16/status
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/24x24
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/24x24/apps
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/24x24/status
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/24x24/emblems
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/32x32
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/32x32/apps
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/32x32/status
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/32x32/emblems
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/48x48
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/48x48/apps
%dir %attr (0755, root, other) %{_datadir}/icons/hicolor/48x48/status
%attr (-, root, other) %{_datadir}/icons/hicolor/16x16/status/*
%attr (-, root, other) %{_datadir}/icons/hicolor/24x24/apps/*
%attr (-, root, other) %{_datadir}/icons/hicolor/24x24/status/*
%attr (-, root, other) %{_datadir}/icons/hicolor/24x24/emblems/*
%attr (-, root, other) %{_datadir}/icons/hicolor/32x32/apps/*
%attr (-, root, other) %{_datadir}/icons/hicolor/32x32/status/*
%attr (-, root, other) %{_datadir}/icons/hicolor/32x32/emblems/*
%attr (-, root, other) %{_datadir}/icons/hicolor/48x48/apps/*
%attr (-, root, other) %{_datadir}/icons/hicolor/48x48/status/*
%dir %attr (0755, root, other) %{_datadir}/gnome
%{_datadir}/gnome/help/nwam-manager/C
%{_datadir}/omf/nwam-manager/*-C.omf
%dir %attr(0755, root, bin) %{_mandir}
%dir %attr(0755, root, bin) %{_mandir}/*
%{_mandir}/*/*
%doc -d nwam-manager-%{nwam_manager.version} AUTHORS README 
%doc(bzip2) -d nwam-manager-%{nwam_manager.version} COPYING ChangeLog
%dir %attr (0755, root, other) %{_datadir}/doc

%files root
%defattr(-, root, sys)
%attr(0755, root, sys) %dir %{_sysconfdir}
%{_sysconfdir}/gconf/schemas/nwam-manager.schemas
%dir %attr (-, root, sys) %{_sysconfdir}/xdg
%dir %attr (-, root, sys) %{_sysconfdir}/xdg/autostart
%attr (-, root, sys) %{_sysconfdir}/xdg/autostart/*
%config %class (rbac) %attr (0644, root, sys) /etc/security/exec_attr

%if %build_l10n
%files l10n
%defattr (-, root, bin)
%dir %attr (0755, root, sys) %{_datadir}
%attr (-, root, other) %{_datadir}/locale
%dir %attr (0755, root, other) %{_datadir}/gnome
%{_datadir}/gnome/help/*/[a-z]*
%{_datadir}/omf/*/*-[a-z]*.omf
%endif

%changelog
* Mon Jan  4 2009 - laca@sun.com
- Add 
* Fri Apr  3 2009 - laca@sun.com
- use desktop-cache instead of postrun
* Mon Sep 15 2008 - darren.kenny@sun.com
- Update copyright (thanks to Matt for the changes).
* Mon Sep 8 2008 - darren.kenny@sun.com
- Fix some icon file attributes.
* Fri Sep 5 2008 - darren.kenny@sun.com
- Comment more l10n dirs until there is something to deliver, it's
  causing build failures.
* Thu Sep 4 2008 - darren.kenny@sun.com
- Fix some issues in spec, add preun for schema and fix l10n build..
* Wed Sep 3 2008 - darren.kenny@sun.com
- Initial delivery.

