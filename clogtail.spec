Name:           clogtail
Version:        0.3.0
Release:        2%{?dist}
Summary:        log follower for periodic jobs
Group:          Applications/File
License:        BSD
BuildRequires:  git gcc

%description
Log follower for periodic jobs

%prep
%setup -c -T
curl -fL https://api.github.com/repos/thedolphin/clogtail/tarball/master |
tar -xzvf - --strip 1

%build
make

%install
%{__install} -D -m0755 clogtail ${RPM_BUILD_ROOT}/usr/bin/clogtail

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/bin/clogtail

%changelog
* Fri Dec 18 2015 Alexander Rumyantsev <arum@1c.ru>
- switched to getopt

* Tue Oct 20 2015 Alexander Rumyantsev <arum@1c.ru>
- switched to mmap

* Thu Mar 26 2015 Alexander Rumyantsev <arum@1c.ru>
- Initial release
