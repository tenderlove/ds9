require "bundler/gem_tasks"
require "rake/extensiontask"

task default: :compile

Rake::ExtensionTask.new("ds9") do |t|
  t.name = "ds9"
  t.ext_dir = "ext/ds9"
  t.lib_dir = "lib"
end

# vim: syntax=ruby
