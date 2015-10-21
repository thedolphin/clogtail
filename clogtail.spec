%define source_repo https://github.com/thedolphin/clogtail.git

Name:           clogtail
Version:        0.2.1
Release:        1%{?dist}
Summary:        log follower for periodic jobs
Group:          Applications/File
License:        BSD
BuildRequires:  git gcc

%description
Log follower for periodic jobs

%prep
%setup -c -T
git clone %{source_repo} .

%build
make

%install
%{__install} -D -m0755 clogtail ${RPM_BUILD_ROOT}/usr/bin/clogtail

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/bin/clogtail

%changelog
* Thu Mar 26 2015 Alexander Rumyantsev <arum@1c.ru>
- Initial release
