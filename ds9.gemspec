lib = File.expand_path("../lib", __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require "ds9/version"

Gem::Specification.new do |s|
  s.name = "ds9"
  s.version = DS9::VERSION
  s.authors = ["Aaron Patterson", "Yuta Iwama"]
  s.email = ["tenderlove@ruby-lang.org", "ganmacs@gmail.com"]

  s.summary = "This library allows you to write HTTP/2 clients and servers"
  s.description = "This library allows you to write HTTP/2 clients and servers. It is a wrapper\naround nghttp2."
  s.homepage = "https://github.com/tenderlove/ds9"
  s.licenses = ["MIT"]

  s.files = `git ls-files -z`.split("\x0")
  s.require_paths = ["lib"]
  s.extensions = ["ext/ds9/extconf.rb"]

  s.required_ruby_version = Gem::Requirement.new(">= 2.3.0")
  s.rubygems_version = "2.4.8"
  s.required_rubygems_version = Gem::Requirement.new(">= 0") if s.respond_to? :required_rubygems_version=
  s.extra_rdoc_files = ["CHANGELOG.md", "README.md"]
  s.rdoc_options = ["--main", "README.md"]
  s.date = "2015-07-06"

  s.add_runtime_dependency "mini_portile2", ">= 2.2.0"

  s.add_development_dependency "bundler"
  s.add_development_dependency "minitest", "~> 5.7"
  s.add_development_dependency "rdoc", "~> 4.0"
  s.add_development_dependency "rake-compiler", ">= 1.0.5"
end
