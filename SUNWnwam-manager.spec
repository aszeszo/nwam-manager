#
# spec file for package SUNWnwam-manager
#
# includes module(s): nwam-manager
#
# Copyright (c) 2007 Sun Microsystems, Inc.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# Owner: Darren, Lin
#
%include Solaris.inc

%use nwam_manager = nwam-manager.spec

Name:                    SUNWnwam-manager
Summary:                 Nwam
Version:                 %{default_pkg_version}
#Source:                  %{name}-manpages-0.1.tar.gz
SUNW_BaseDir:            %{_prefix}
BuildRoot:               %{_tmppath}/%{name}-%{version}-build

%include default-depend.inc
BuildRequires: SUNWgnome-libs-devel
Requires: SUNWgnome-libs
Requires: SUNWgnome-base-libs

%package root
Summary:                 %{summary} - / filesystem
SUNW_BaseDir:            /
%include default-depend.inc

%package devel
Summary:                 %{summary} - development files
SUNW_BaseDir: %{_basedir}
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
#cd %{_builddir}/%name-%version
#gzcat %SOURCE0 | tar xf -

%build
export CFLAGS="%optflags"
export CXXFLAGS="%cxx_optflags"
%if %cc_is_gcc
%else
%endif
export RPM_OPT_FLAGS="$CFLAGS"

%nwam_manager.build -d %name-%version

export PKG_CONFIG_PATH=../nwam-manager-%{nwam-manager.version}

%install
rm -rf $RPM_BUILD_ROOT
%nwam_manager.install -d %name-%version
#cd %{_builddir}/%name-%version/sun-manpages
#make install DESTDIR=$RPM_BUILD_ROOT

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

%post root
( echo 'test -x /usr/bin/gconftool-2 || {';
  echo '  echo "ERROR: gconftool-2 not found"';
  echo '  exit 0';
  echo '}';
  echo 'umask 0022';
  echo 'GCONF_CONFIG_SOURCE=xml:merged:/etc/gconf/gconf.xml.defaults';
  echo 'export GCONF_CONFIG_SOURCE';
  echo '/usr/bin/gconftool-2 --makefile-install-rule %{_sysconfdir}/gconf/schemas/*.schemas'
) | $BASEDIR/var/lib/postrun/postrun -u -c JDS_wait

%preun root
test -x $BASEDIR/var/lib/postrun/postrun || exit 0
( echo 'test -x $PKG_INSTALL_ROOT/usr/bin/gconftool-2 || {';
  echo '  echo "WARNING: gconftool-2 not found; not uninstalling gconf schemas"';
  echo '  exit 0';
  echo '}';
  echo 'umask 0022';
  echo 'GCONF_CONFIG_SOURCE=xml:merged:$BASEDIR/etc/gconf/gconf.xml.defaults';
  echo 'GCONF_BACKEND_DIR=$PKG_INSTALL_ROOT/usr/lib/GConf/2';
  echo 'LD_LIBRARY_PATH=$PKG_INSTALL_ROOT/usr/lib';
  echo 'export GCONF_CONFIG_SOURCE GCONF_BACKEND_DIR LD_LIBRARY_PATH';
  echo 'SDIR=$BASEDIR%{_sysconfdir}/gconf/schemas';
  echo 'schemas="$SDIR/gstreamer-%{gst_minmaj}.schemas"';
  echo '$PKG_INSTALL_ROOT/usr/bin/gconftool-2 --makefile-uninstall-rule $schemas'
) | $BASEDIR/var/lib/postrun/postrun -i -c JDS -a

%files
%defattr (-, root, bin)
%dir %attr (0755, root, bin) %{_bindir}
%{_bindir}/*
%dir %attr (0755, root, sys) %{_datadir}
%dir %attr (0755, root, bin) %{_datadir}/nwam-manager
%dir %attr (0755, root, bin) %{_datadir}/nwam-manager/icons
#%dir %attr (0755, root, other) %{_datadir}/applications
#%{_datadir}/applications/*
%attr (-, root, other) %{_datadir}/nwam-manager/*.*
%attr (-, root, other) %{_datadir}/nwam-manager/icons/*
#%attr (-, root, other) %{_datadir}/locale
#%dir %attr(0755, root, bin) %{_mandir}
#%dir %attr(0755, root, bin) %{_mandir}/*
#%{_mandir}/*/*

%files root
%defattr(-, root, sys)
%attr(0755, root, sys) %dir %{_sysconfdir}
%{_sysconfdir}/gconf/schemas/nwam-manager.schemas

%if %build_l10n
%files l10n
%defattr (-, root, bin)
%dir %attr (0755, root, sys) %{_datadir}
%dir %attr (0755, root, other) %{_datadir}/gnome
%attr (-, root, other) %{_datadir}/locale
%{_datadir}/gnome/help/*/[a-z]*
%{_datadir}/omf/*/*-[a-z]*.omf
%endif

%changelog
* Wed Aug 01 2007 - lin.ma@sun.com
- Initial
