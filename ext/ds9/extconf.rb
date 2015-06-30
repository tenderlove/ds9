require 'mkmf'

if with_config("static")
  ldflags = pkg_config 'libnghttp2', 'libs-only-L'

  archive = File.join ldflags.sub(/^-L/, ''), 'libnghttp2.a'
  if File.exist? archive
    $LDFLAGS << " #{archive}"
  else
    raise "couldn't find archive"
  end
else
  pkg_config 'libnghttp2'
end

create_makefile 'ds9'
