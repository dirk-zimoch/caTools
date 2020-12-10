Summary:        Epics CA Tools extensions
Name:           epics-ext-caTools
Version:        1.0
Release:        1.0.4g%{?dist}
License:        Commercial
Group:          Development/Languages
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-%(%{__id_u} -n)
Vendor:         PSI GFA Controls
Packager:       Rene Kapeller <rene.kapeller@psi.ch>
URL:            git@git.psi.ch:epics_support_apps/catools.git

%define prog_folder                          /usr/local/epics/extensions/caTools
%define git_folder                           %{_sourcedir}/%{name}-%{version}

%define debug_package                        %{nil}

%define _use_internal_dependency_generator   0
%define __find_provides                      %{nil}
%define __find_requires                      %{nil}
%define proglinks cado caget cagets cainfo camon caput caputq cawait

%ifarch x86_64
%if 0%{?rhel} == 7
%define epics_host_arch RHEL7-x86_64
%endif
%if 0%{?rhel} == 6
%define epics_host_arch SL6-x86_64
%endif
%endif
%ifarch i386 i585 i686
%define epics_host_arch SL6-x86
%endif

%description
Epics CA Tools extensions

%package links
Summary:    epics-ext-caTools-links installs all symbolic links under /usr/local/bin/
Requires:   %{name} = %{version}-%{release}
Conflicts:  epics-ext-tcl-links

%description links
The 'links' part of epics-ext-caTools is contained in a package named 'epics-ext-caTools-links'

%prep
%{__rm} -rf %{git_folder}
git clone %{url} %{git_folder}

%build
cd %{git_folder}
make CROSS_COMPILER_TARGET_ARCHS=

%install
cd %{git_folder}
%{__rm} -rf $RPM_BUILD_ROOT
%{__mkdir_p} $RPM_BUILD_ROOT%{prog_folder}
pwd
%{__cp} -a bin/%{epics_host_arch} $RPM_BUILD_ROOT%{prog_folder}/bin

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%post links
for pl in %{proglinks}; do
  ln -sf %{prog_folder}/bin/caTools /usr/local/bin/$pl
done

%preun links
if [ "$1" = "0" ] ; then # last uninstall
  for pl in %{proglinks}; do
    %{__rm} -f /usr/local/bin/$pl
  done
fi

%files
%defattr(-,root,root,0755)
%{prog_folder}

%files links
%defattr(-,root,root,0755)
%{prog_folder}

%changelog

* Thu Sep 17 2020 rene@kapeller.ch - 1.0-1.0.4g
- https://jira.psi.ch/browse/CTRLIT-8152
* Wed Jan 29 2020 rene@kapeller.ch - 1.0-1.0.4f
- Don't print timeout when write access is denied
* Tue Apr 30 2018 rene@kapeller.ch - 1.0-1.0.4e
- fix EPICS errors without line terminator
* Tue Mar 21 2017 rene@kapeller.ch - 1.0-1.0.4d
- added support for RHEL7
- added distribution tag to RPM name
* Tue Feb 21 2017 rene@kapeller.ch - 1.0-1.0.4c
- clone from git in spec file
* Mon Feb 20 2017 rene@kapeller.ch - 1.0-1.0.4b
- working on packaging issue
* Mon Feb 20 2017 rene@kapeller.ch - 1.0-1.0.4a
- working on packaging issue
* Thu Feb 16 2017 rene@kapeller.ch - 1.0-1.0.4
- update from dirk.zimoch@psi.ch
- aaffc1dc · do not truncate value (char arrarys)
* Tue Jan 17 2017 rene@kapeller.ch - 1.0-1.0.3
* Wed Nov 23 2016 rene@kapeller.ch - 1.0-1.0.2
- new caTools option "--version"
- align caTools and rpm version
* Tue Nov 15 2016 rene@kapeller.ch - 1.0-1
- Initial build
