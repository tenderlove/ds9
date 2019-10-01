require 'mkmf'

def using_system_libraries?
  arg_config('--use-system-libraries', !!ENV['DS9_USE_SYSTEM_LIBRARIES'])
end

dir_config('ds9')

if using_system_libraries?
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
else
  message "Building nghttp2\n"
  require 'rubygems'
  require 'mini_portile2'
  recipe = MiniPortile.new('nghttp2', 'v1.34.0')
  recipe.configure_options = recipe.configure_options + ['--with-pic', '--disable-python-bindings']

  recipe.files << {
    url: 'https://github.com/nghttp2/nghttp2/releases/download/v1.34.0/nghttp2-1.34.0.tar.gz',
    sha1: '2d0ad93a254a7a6e6c737f2fd2e10e4810a41f74',
  }
  recipe.cook

  # `recipe.activate` uses invalid path for this package.
  $LIBPATH = ["#{recipe.path}/lib"] + $LIBPATH
  $CPPFLAGS << " -I#{recipe.path}/include"
  $LIBS << " -lstdc++"
end

abort 'nghttp2/nghttp2.h not found' unless have_header('nghttp2/nghttp2.h')
abort 'libnghttp2 not found' unless have_library('nghttp2')

create_makefile 'ds9'
