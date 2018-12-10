require "bundler/gem_tasks"
require "rake/extensiontask"
require "rake/testtask"

task default: :compile

Rake::ExtensionTask.new("ds9") do |t|
  t.name = "ds9"
  t.ext_dir = "ext/ds9"
  t.lib_dir = "lib"
end

Rake::TestTask.new('test' => 'compile') do |t|
  t.libs << 'test'
  t.verbose = true
end

# vim: syntax=ruby
