# coding: utf-8
lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "ds9/version"

Gem::Specification.new do |s|
  s.name = "ds9"
  s.version = Ds9::VERSION

  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
  s.require_paths = ["lib"]
  s.authors = ["Aaron Patterson"]
  s.date = "2015-07-06"
  s.description = "This library allows you to write HTTP/2 clients and servers.  It is a wrapper\naround nghttp2."
  s.email = ["tenderlove@ruby-lang.org"]
  s.extensions = ["ext/ds9/extconf.rb"]
  s.extra_rdoc_files = ["CHANGELOG.md", "Manifest.txt", "README.md", "CHANGELOG.md", "README.md"]
  s.files = [".autotest", ".gemtest", "CHANGELOG.md", "Manifest.txt", "README.md", "Rakefile", "ext/ds9/ds9.c", "ext/ds9/ds9.h", "ext/ds9/ds9_frames.c", "ext/ds9/extconf.rb", "lib/ds9.rb", "test/helper.rb", "test/test_client.rb", "test/test_ds9.rb"]
  s.homepage = "https://github.com/tenderlove/ds9"
  s.licenses = ["MIT"]
  s.rdoc_options = ["--main", "README.md"]
  s.required_ruby_version = Gem::Requirement.new(">= 2.2.2")
  s.rubygems_version = "2.4.8"
  s.summary = "This library allows you to write HTTP/2 clients and servers"

  s.add_development_dependency(%q<minitest>, ["~> 5.7"])
  s.add_development_dependency(%q<rdoc>, ["~> 4.0"])
  s.add_development_dependency(%q<rake-compiler>, [">= 0.4.1"])
  s.add_development_dependency(%q<hoe>, ["~> 3.13"])
end
